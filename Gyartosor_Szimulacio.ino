#include <WiFi.h>
#include <PubSubClient.h>
#include <string.h>
#include <esp_system.h>
#include "secrets.h"

// Wi-Fi konfiguráció
const char* WIFI_SSID = SECRET_WIFI_SSID;
const char* WIFI_PASSWORD = SECRET_WIFI_PASSWORD;
const char* MQTT_SERVER = SECRET_MQTT_SERVER;

const int MQTT_PORT = 1883;

// MQTT témakörök
const char* TOPIC_STATUS = "gyartosor/esp32/status";
const char* TOPIC_EVENT = "gyartosor/esp32/event";
const char* TOPIC_COMMAND = "gyartosor/esp32/command";

// LED-ek GPIO-kiosztása
const int GREEN_LED = 17;
const int YELLOW_LED = 16;
const int RED_LED = 4;

// Fizikai gombok GPIO-kiosztása
const int START_BUTTON = 15;
const int RESET_BUTTON = 7;
const int STOP_BUTTON = 6;
const int EMERGENCY_BUTTON = 5;

// Gyártósor állapotai
enum MachineState {
  STOPPED,
  RUNNING,
  EMERGENCY_STOP
};

MachineState machineState = STOPPED;

// Webes parancsok
enum RemoteCommand {
  NO_COMMAND,
  REMOTE_START,
  REMOTE_STOP,
  REMOTE_RESET,
  REMOTE_EMERGENCY_STOP
};

RemoteCommand pendingCommand = NO_COMMAND;

// Wi-Fi és MQTT objektumok
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Időzítők
unsigned long lastProductionTime = 0;
unsigned long lastStatusTime = 0;
unsigned long lastWiFiAttempt = 0;
unsigned long lastMQTTAttempt = 0;

// Időintervallumok
const unsigned long PRODUCTION_INTERVAL = 2000;
const unsigned long DEBOUNCE_TIME = 40;
const unsigned long STATUS_INTERVAL = 5000;
const unsigned long RECONNECT_INTERVAL = 3000;

// Gombok pergésmentesítése
unsigned long lastChange[4] = {0, 0, 0, 0};

bool lastReading[4] = {
  HIGH, HIGH, HIGH, HIGH
};

bool stableState[4] = {
  HIGH, HIGH, HIGH, HIGH
};

const int buttonPins[4] = {
  START_BUTTON,
  RESET_BUTTON,
  STOP_BUTTON,
  EMERGENCY_BUTTON
};

// Termelési adatok
unsigned long totalProduced = 0;
unsigned long goodProducts = 0;
unsigned long defectiveProducts = 0;
unsigned long messageNumber = 0;

// Gyártási ciklusidő adatok
unsigned long lastCycleTime = 0;
unsigned long totalCycleTime = 0;
unsigned long averageCycleTime = 0;
unsigned long cycleCount = 0;

// Selejtgyártási valószínűség százalékban
const int DEFECTIVE_PROBABILITY = 5;

// Utoljára elküldött állapot
MachineState lastPublishedState = STOPPED;
unsigned long lastPublishedCount = 0;
bool firstStatusPublished = false;


// Fizikai gombok olvasása
bool buttonPressed(int index) {
  bool reading = digitalRead(buttonPins[index]);

  if (reading != lastReading[index]) {
    lastReading[index] = reading;
    lastChange[index] = millis();
  }

  if (millis() - lastChange[index] >= DEBOUNCE_TIME) {
    if (reading != stableState[index]) {
      stableState[index] = reading;

      if (stableState[index] == LOW) {
        return true;
      }
    }
  }

  return false;
}


// Aktuális állapot nevének lekérdezése
const char* getStateName() {
  switch (machineState) {
    case RUNNING:
      return "RUNNING";

    case STOPPED:
      return "STOPPED";

    case EMERGENCY_STOP:
      return "EMERGENCY_STOP";
  }

  return "UNKNOWN";
}


// LED-ek vezérlése
void updateLEDs() {
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);

  switch (machineState) {
    case RUNNING:
      digitalWrite(GREEN_LED, HIGH);
      break;

    case STOPPED:
      digitalWrite(YELLOW_LED, HIGH);
      break;

    case EMERGENCY_STOP:
      digitalWrite(RED_LED, HIGH);
      break;
  }
}


// Wi-Fi kapcsolat kezelése
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();

  if (now - lastWiFiAttempt < RECONNECT_INTERVAL) {
    return;
  }

  lastWiFiAttempt = now;

  Serial.println("Wi-Fi-csatlakozási kísérlet...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}


// MQTT üzenetek fogadása
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, TOPIC_COMMAND) != 0) {
    return;
  }

  char message[128];

  if (length >= sizeof(message)) {
    Serial.println("Túl hosszú MQTT-parancs!");
    return;
  }

  memcpy(message, payload, length);
  message[length] = '\0';

  Serial.print("Webes parancs érkezett: ");
  Serial.println(message);

  if (strstr(message, "\"command\":\"START\"") != nullptr) {
    pendingCommand = REMOTE_START;
  }
  else if (strstr(message, "\"command\":\"STOP\"") != nullptr) {
    pendingCommand = REMOTE_STOP;
  }
  else if (strstr(message, "\"command\":\"RESET\"") != nullptr) {
    pendingCommand = REMOTE_RESET;
  }
  else if (strstr(message, "\"command\":\"EMERGENCY_STOP\"") != nullptr) {
    pendingCommand = REMOTE_EMERGENCY_STOP;
  }
  else {
    Serial.println("Ismeretlen webes parancs!");
  }
}


// MQTT kapcsolat létrehozása
void connectMQTT() {
  if (WiFi.status() != WL_CONNECTED ||
      mqttClient.connected()) {
    return;
  }

  unsigned long now = millis();

  if (now - lastMQTTAttempt < RECONNECT_INTERVAL) {
    return;
  }

  lastMQTTAttempt = now;

  Serial.print("MQTT-kapcsolódás... ");

  String clientId = "ESP32-Gyartosor-";
  clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("sikeres!");

    if (mqttClient.subscribe(TOPIC_COMMAND)) {
      Serial.println("Webes vezérlés MQTT-feliratkozás sikeres.");
    }
    else {
      Serial.println("MQTT-feliratkozás sikertelen!");
    }

    firstStatusPublished = false;
  }
  else {
    Serial.print("Sikertelen, hibakód: ");
    Serial.println(mqttClient.state());
  }
}


// MQTT esemény küldése
void publishEvent(const char* eventName) {
  if (!mqttClient.connected()) {
    return;
  }

  char payload[384];

  snprintf(
    payload,
    sizeof(payload),
    "{\"event\":\"%s\",\"status\":\"%s\",\"totalProduced\":%lu,\"goodProducts\":%lu,\"defectiveProducts\":%lu,\"lastCycleTimeMs\":%lu,\"averageCycleTimeMs\":%lu,\"cycleCount\":%lu}",
    eventName,
    getStateName(),
    totalProduced,
    goodProducts,
    defectiveProducts,
    lastCycleTime,
    averageCycleTime,
    cycleCount
  );

  if (mqttClient.publish(TOPIC_EVENT, payload)) {
    Serial.print("MQTT-esemény: ");
    Serial.println(payload);
  }
  else {
    Serial.println("Az MQTT-esemény küldése sikertelen.");
  }
}


// MQTT állapot küldése
void publishStatus(bool force = false) {
  if (!mqttClient.connected()) {
    return;
  }

  unsigned long now = millis();

  bool stateChanged =
    machineState != lastPublishedState;

  bool countChanged =
    totalProduced != lastPublishedCount;

  bool intervalElapsed =
    now - lastStatusTime >= STATUS_INTERVAL;

  if (!force &&
      firstStatusPublished &&
      !stateChanged &&
      !countChanged &&
      !intervalElapsed) {
    return;
  }

  char payload[384];

  snprintf(
    payload,
    sizeof(payload),
    "{\"status\":\"%s\",\"totalProduced\":%lu,\"goodProducts\":%lu,\"defectiveProducts\":%lu,\"messageNumber\":%lu,\"lastCycleTimeMs\":%lu,\"averageCycleTimeMs\":%lu,\"cycleCount\":%lu}",
    getStateName(),
    totalProduced,
    goodProducts,
    defectiveProducts,
    ++messageNumber,
    lastCycleTime,
    averageCycleTime,
    cycleCount
  );

  if (mqttClient.publish(TOPIC_STATUS, payload)) {
    Serial.print("MQTT-állapot: ");
    Serial.println(payload);

    lastPublishedState = machineState;
    lastPublishedCount = totalProduced;
    lastStatusTime = now;
    firstStatusPublished = true;
  }
  else {
    Serial.println("Az MQTT-állapot küldése sikertelen.");
  }
}


// Program inicializálása
void setup() {
  Serial.begin(115200);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  for (int i = 0; i < 4; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  updateLEDs();

  randomSeed(esp_random());

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setBufferSize(384);
  mqttClient.setCallback(mqttCallback);

  Serial.println();
  Serial.println("=== GYÁRTÓSOR SZIMULÁCIÓ MQTT-VEL ===");
  Serial.println("Állapot: STOPPED");
  Serial.println("Nyomd meg a START gombot!");
  Serial.println("A webes vezérlés is aktív.");

  lastWiFiAttempt = millis();
}


// Fő programciklus
void loop() {
  connectWiFi();
  connectMQTT();

  if (mqttClient.connected()) {
    mqttClient.loop();
  }

  // Fizikai gombok olvasása
  bool startPressed = buttonPressed(0);
  bool resetPressed = buttonPressed(1);
  bool stopPressed = buttonPressed(2);
  bool emergencyPressed = buttonPressed(3);

  // Webes parancs kiolvasása
  RemoteCommand remoteCommand = pendingCommand;
  pendingCommand = NO_COMMAND;

  bool remoteStart = remoteCommand == REMOTE_START;
  bool remoteStop = remoteCommand == REMOTE_STOP;
  bool remoteReset = remoteCommand == REMOTE_RESET;
  bool remoteEmergency =
    remoteCommand == REMOTE_EMERGENCY_STOP;

  bool stateChanged = false;
  const char* eventName = nullptr;

  // Vészleállítás
  if (emergencyPressed || remoteEmergency) {
    if (machineState != EMERGENCY_STOP) {
      machineState = EMERGENCY_STOP;

      stateChanged = true;
      eventName = "EMERGENCY_STOP";

      Serial.println("VÉSZLEÁLLÍTÁS! A gyártósor megállt.");
    }
  }

  // Vészállapot feloldása
  else if (machineState == EMERGENCY_STOP) {
    if (resetPressed || remoteReset) {
      machineState = STOPPED;

      stateChanged = true;
      eventName = "EMERGENCY_RESET";

      Serial.println("Vészállapot törölve. A gyártósor leállítva.");
    }
    else if (startPressed || remoteStart) {
      Serial.println("Nem indítható! Először RESET szükséges.");
    }
  }

  // Normál üzemi állapotok
  else {
    if (stopPressed || remoteStop) {
      if (machineState != STOPPED) {
        machineState = STOPPED;

        stateChanged = true;
        eventName = "STOP";

        Serial.println("STOP: A gyártósor leállt.");
      }
    }
    else if (resetPressed || remoteReset) {
      machineState = STOPPED;
      totalProduced = 0;
      goodProducts = 0;
      defectiveProducts = 0;

      lastCycleTime = 0;
      totalCycleTime = 0;
      averageCycleTime = 0;
      cycleCount = 0;

      stateChanged = true;
      eventName = "RESET";

      Serial.println("RESET: A termelési és ciklusidő-adatok nullázva.");
    }
    else if (startPressed || remoteStart) {
      if (machineState != RUNNING) {
        machineState = RUNNING;
        lastProductionTime = millis();

        stateChanged = true;
        eventName = "START";

        Serial.println("START: A gyártás elindult.");
      }
    }
  }

  // LED-ek frissítése
  updateLEDs();

  // Szimulált termelés és ciklusidő mérése
  if (machineState == RUNNING &&
      millis() - lastProductionTime >= PRODUCTION_INTERVAL) {

    unsigned long currentTime = millis();

    lastCycleTime = currentTime - lastProductionTime;
    lastProductionTime = currentTime;

    cycleCount++;
    totalCycleTime += lastCycleTime;
    averageCycleTime = totalCycleTime / cycleCount;

    totalProduced++;

    Serial.print("Utolso ciklusido: ");
    Serial.print(lastCycleTime);
    Serial.println(" ms");

    Serial.print("Atlagos ciklusido: ");
    Serial.print(averageCycleTime);
    Serial.println(" ms");

    if (random(100) < DEFECTIVE_PROBABILITY) {
      defectiveProducts++;
      Serial.println("Selejtes termek keszult!");
      publishEvent("PRODUCT_REJECTED");
    }
    else {
      goodProducts++;
      Serial.println("Jo termek keszult.");
      publishEvent("PRODUCT_GOOD");
    }

    Serial.print("Osszes legyartott termek: ");
    Serial.println(totalProduced);

    Serial.print("Jo termekek: ");
    Serial.println(goodProducts);

    Serial.print("Selejtes termekek: ");
    Serial.println(defectiveProducts);
  }

  // Esemény és állapot továbbítása
  if (stateChanged && eventName != nullptr) {
    publishEvent(eventName);
  }

  publishStatus();
}