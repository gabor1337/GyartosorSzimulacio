
/*
  ESP32-S3 - Gyartosor szimulacio
  LED-ek:
    Zold: 17
    Sarga: 16
    Piros: 4

  Gombok:
    START: 15
    RESET: 7
    STOP: 6
    VESZALLJ: 5
*/

const int GREEN_LED = 17;
const int YELLOW_LED = 16;
const int RED_LED = 4;

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

unsigned long lastProductionTime = 0;
unsigned long lastStatusTime = 0;

const unsigned long PRODUCTION_INTERVAL = 2000;
const unsigned long DEBOUNCE_TIME = 40;

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

bool buttonPressed(int index) {
  bool reading = digitalRead(buttonPins[index]);

  if (reading != lastReading[index]) {
    lastReading[index] = reading;
    lastChange[index] = millis();
  }

  if (millis() - lastChange[index] >= DEBOUNCE_TIME) {
    if (reading != stableState[index]) {
      stableState[index] = reading;

      // INPUT_PULLUP: LOW means pressed
      if (stableState[index] == LOW) {
        return true;
      }
    }
  }

  return false;
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

void setup() {
  Serial.begin(115200);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  for (int i = 0; i < 4; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  updateLEDs();

  Serial.println();
  Serial.println("=== GYARTOSOR SZIMULACIO ===");
  Serial.println("Allapot: LEALLITVA");
  Serial.println("Nyomd meg a START gombot!");
}

void loop() {
  bool startPressed = buttonPressed(0);
  bool resetPressed = buttonPressed(1);
  bool stopPressed = buttonPressed(2);
  bool emergencyPressed = buttonPressed(3);

  // A veszalj minden mas muveletnel fontosabb
  if (emergencyPressed) {
    machineState = EMERGENCY_STOP;
    Serial.println("VESZLEALLITAS! A gyartosor megallt.");
  }
  else if (machineState == EMERGENCY_STOP) {
    if (resetPressed) {
      machineState = STOPPED;
      Serial.println("Veszallapot torolve. A gyartosor LEALLITVA.");
    }
    else if (startPressed) {
      Serial.println("Nem indithato! Eloszor RESET szukseges.");
    }
  }
  else {
    if (stopPressed) {
      machineState = STOPPED;
      Serial.println("STOP: A gyartosor leallt.");
    }
    else if (resetPressed) {
      machineState = STOPPED;
      totalProduced = 0;
      Serial.println("RESET: A darabszam nullazva.");
    }
    else if (startPressed) {
      machineState = RUNNING;
      lastProductionTime = millis();
      Serial.println("START: A gyartas elindult.");
    }
  }

  updateLEDs();

  // Virtualis termeles: 1 darab minden 2. masodpercben
  if (machineState == RUNNING &&
      millis() - lastProductionTime >= PRODUCTION_INTERVAL) {
    lastProductionTime = millis();
    totalProduced++;

    Serial.print("Legyartott darabok: ");
    Serial.println(totalProduced);
  }

  // Allapotjelentes 5 masodpercenkent
  if (millis() - lastStatusTime >= 5000) {
    lastStatusTime = millis();

    Serial.print("Allapot: ");

    if (machineState == RUNNING) {
      Serial.print("MUKODIK");
    }
    else if (machineState == STOPPED) {
      Serial.print("LEALLITVA");
    }
    else {
      Serial.print("VESZLEALLITAS");
    }

    Serial.print(" | Darabszam: ");
    Serial.println(totalProduced);
  }
}
