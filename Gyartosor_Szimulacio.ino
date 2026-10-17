
#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

const char* WIFI_SSID = SECRET_WIFI_SSID;
const char* WIFI_PASSWORD = SECRET_WIFI_PASSWORD;
const char* MQTT_SERVER = SECRET_MQTT_SERVER;

const int MQTT_PORT = 1883;


// MQTT temak
const char* TOPIC_STATUS = "gyartosor/esp32/status";
const char* TOPIC_EVENT = "gyartosor/esp32/event";

// LED-ek
const int GREEN_LED = 17;
const int YELLOW_LED = 16;
const int RED_LED = 4;

// Gombok
const int START_BUTTON = 15;
const int RESET_BUTTON = 7;
const int STOP_BUTTON = 6;
const int EMERGENCY_BUTTON = 5;

enum MachineState {
  STOPPED,
  RUNNING,
  EMERGENCY_STOP
};

MachineState machineState = STOPPED;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastProductionTime = 0;
unsigned long lastStatusTime = 0;
unsigned long lastWiFiAttempt = 0;
unsigned long lastMQTTAttempt = 0;

const unsigned long PRODUCTION_INTERVAL = 2000;
const unsigned long DEBOUNCE_TIME = 40;
const unsigned long STATUS_INTERVAL = 5000;
const unsigned long RECONNECT_INTERVAL = 3000;

unsigned long lastChange[4] = {0, 0, 0, 0};
bool lastReading[4] = {HIGH, HIGH, HIGH, HIGH};
bool stableState[4] = {HIGH, HIGH, HIGH, HIGH};

const int buttonPins[4] = {
  START_BUTTON,
  RESET_BUTTON,
  STOP_BUTTON,
  EMERGENCY_BUTTON
};

unsigned long totalProduced = 0;
unsigned long messageNumber = 0;

MachineState lastPublishedState = STOPPED;
unsigned long lastPublishedCount = 0;
bool firstStatusPublished = false;

bool buttonPressed(int index) {
  bool reading = digitalRead(buttonPins[index]);

  if (reading != lastReading[index]) {
    lastReading[index] = reading;
    lastChange[index] = millis();
  }

  if (millis() - lastChange[index] >= DEBOUNCE_TIME) {
    if (reading != stableState[index]) {
      stableState[index] = reading;

      // INPUT_PULLUP: LOW = lenyomott gomb
      if (stableState[index] == LOW) {
        return true;
      }
    }
  }

  return false;
}

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

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();

  if (now - lastWiFiAttempt < RECONNECT_INTERVAL) {
    return;
  }

  lastWiFiAttempt = now;

  Serial.println("Wi-Fi-csatlakozasi kiserlet...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

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

  Serial.print("MQTT-kapcsolodas... ");

  String clientId = "ESP32-Gyartosor-";
  clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("sikeres!");

    // Uj kapcsolatnal friss allapotot kuldunk
    firstStatusPublished = false;
  } else {
    Serial.print("sikertelen, hibakod: ");
    Serial.println(mqttClient.state());
  }
}

void publishEvent(const char* eventName) {
  if (!mqttClient.connected()) {
    return;
  }

  char payload[160];

  snprintf(
    payload,
    sizeof(payload),
    "{\"event\":\"%s\",\"status\":\"%s\",\"totalProduced\":%lu}",
    eventName,
    getStateName(),
    totalProduced
  );

  if (mqttClient.publish(TOPIC_EVENT, payload)) {
    Serial.print("MQTT-esemeny: ");
    Serial.println(payload);
  } else {
    Serial.println("Az MQTT-esemeny kuldese sikertelen.");
  }
}

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

  char payload[160];

  snprintf(
    payload,
    sizeof(payload),
    "{\"status\":\"%s\",\"totalProduced\":%lu,\"messageNumber\":%lu}",
    getStateName(),
    totalProduced,
    ++messageNumber
  );

  if (mqttClient.publish(TOPIC_STATUS, payload)) {
    Serial.print("MQTT-allapot: ");
    Serial.println(payload);

    lastPublishedState = machineState;
    lastPublishedCount = totalProduced;
    lastStatusTime = now;
    firstStatusPublished = true;
  } else {
    Serial.println("Az MQTT-allapot kuldese sikertelen.");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  for (int i = 0; i < 4; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  updateLEDs();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setBufferSize(256);

  Serial.println();
  Serial.println("=== GYARTOSOR SZIMULACIO MQTT-VEL ===");
  Serial.println("Allapot: STOPPED");
  Serial.println("Nyomd meg a START gombot!");

  lastWiFiAttempt = millis();
}

void loop() {
  // Halozati kapcsolat kezelese
  connectWiFi();
  connectMQTT();

  if (mqttClient.connected()) {
    mqttClient.loop();
  }

  // Gombok olvasasa
  bool startPressed = buttonPressed(0);
  bool resetPressed = buttonPressed(1);
  bool stopPressed = buttonPressed(2);
  bool emergencyPressed = buttonPressed(3);

  bool stateChanged = false;
  const char* eventName = nullptr;

  // A veszalj minden mas muveletnel fontosabb
  if (emergencyPressed) {
    if (machineState != EMERGENCY_STOP) {
      machineState = EMERGENCY_STOP;
      stateChanged = true;
      eventName = "EMERGENCY_STOP";
      Serial.println("VESZLEALLITAS! A gyartosor megallt.");
    }
  }
  else if (machineState == EMERGENCY_STOP) {
    if (resetPressed) {
      machineState = STOPPED;
      stateChanged = true;
      eventName = "EMERGENCY_RESET";
      Serial.println("Veszallapot torolve. A gyartosor LEALLITVA.");
    }
    else if (startPressed) {
      Serial.println("Nem indithato! Eloszor RESET szukseges.");
    }
  }
  else {
    if (stopPressed) {
      if (machineState != STOPPED) {
        machineState = STOPPED;
        stateChanged = true;
        eventName = "STOP";
        Serial.println("STOP: A gyartosor leallt.");
      }
    }
    else if (resetPressed) {
      machineState = STOPPED;
      totalProduced = 0;
      stateChanged = true;
      eventName = "RESET";
      Serial.println("RESET: A darabszam nullazva.");
    }
    else if (startPressed) {
      if (machineState != RUNNING) {
        machineState = RUNNING;
        lastProductionTime = millis();
        stateChanged = true;
        eventName = "START";
        Serial.println("START: A gyartas elindult.");
      }
    }
  }

  updateLEDs();

  // Szimulalt termeles: egy darab ket masodpercenkent
  if (machineState == RUNNING &&
      millis() - lastProductionTime >= PRODUCTION_INTERVAL) {
    lastProductionTime = millis();
    totalProduced++;

    Serial.print("Legyartott darabok: ");
    Serial.println(totalProduced);
  }

  // Az esemenyek es az aktualis allapot tovabbitasa
  if (stateChanged && eventName != nullptr) {
    publishEvent(eventName);
  }

  publishStatus();
}
