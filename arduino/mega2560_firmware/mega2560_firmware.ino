#include <DHT.h>

/*
  ProjetBadis - Arduino Mega 2560 firmware

  Hardware:
  - LED luminaire          -> D8
  - LED alerte température -> D9
  - DHT22 / AM2302 DATA    -> D2

  Serial commands from GUI:
  - LED_ON
  - LED_OFF
  - BLINK_ON
  - BLINK_OFF
  - FREQ:500
  - SEUIL:30
  - GET_TEMP
*/

const int LED_LUMIERE = 8;
const int LED_ALERTE = 9;

#define DHT_PIN 2
#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);

bool blinkMode = false;
bool ledState = false;

unsigned long lastBlink = 0;
unsigned long blinkInterval = 500;

unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 2500;

float seuilTemperature = 30.0;
float temperature = 0.0;
float humidite = 0.0;

bool capteurOk = false;

void setup() {
  pinMode(LED_LUMIERE, OUTPUT);
  pinMode(LED_ALERTE, OUTPUT);

  digitalWrite(LED_LUMIERE, LOW);
  digitalWrite(LED_ALERTE, LOW);

  Serial.begin(9600);

  dht.begin();
  delay(2000);

  Serial.println("ARDUINO_READY");
}

void loop() {
  lireCommandeSerie();
  gererClignotement();
  lireCapteurTemperature();
  gererAlerteTemperature();
}

void lireCommandeSerie() {
  if (!Serial.available()) {
    return;
  }

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd == "LED_ON") {
    blinkMode = false;
    ledState = true;
    digitalWrite(LED_LUMIERE, HIGH);
    Serial.println("OK:LED_ON");
  }

  else if (cmd == "LED_OFF") {
    blinkMode = false;
    ledState = false;
    digitalWrite(LED_LUMIERE, LOW);
    Serial.println("OK:LED_OFF");
  }

  else if (cmd == "BLINK_ON") {
    blinkMode = true;
    Serial.println("OK:BLINK_ON");
  }

  else if (cmd == "BLINK_OFF") {
    blinkMode = false;
    ledState = false;
    digitalWrite(LED_LUMIERE, LOW);
    Serial.println("OK:BLINK_OFF");
  }

  else if (cmd.startsWith("FREQ:")) {
    int nouvelleFrequence = cmd.substring(5).toInt();

    if (nouvelleFrequence >= 100 && nouvelleFrequence <= 5000) {
      blinkInterval = nouvelleFrequence;
      Serial.print("FREQ_OK:");
      Serial.println(blinkInterval);
    } else {
      Serial.println("ERR:FREQ");
    }
  }

  else if (cmd.startsWith("SEUIL:")) {
    float nouveauSeuil = cmd.substring(6).toFloat();

    if (nouveauSeuil >= -20.0 && nouveauSeuil <= 80.0) {
      seuilTemperature = nouveauSeuil;
      Serial.print("SEUIL_OK:");
      Serial.println(seuilTemperature);
      envoyerTemperature();
    } else {
      Serial.println("ERR:SEUIL");
    }
  }

  else if (cmd == "GET_TEMP") {
    lireCapteurMaintenant();
    envoyerTemperature();
  }

  else {
    Serial.print("ERR:CMD:");
    Serial.println(cmd);
  }
}

void gererClignotement() {
  if (!blinkMode) {
    return;
  }

  unsigned long maintenant = millis();

  if (maintenant - lastBlink >= blinkInterval) {
    lastBlink = maintenant;
    ledState = !ledState;
    digitalWrite(LED_LUMIERE, ledState);
  }
}

void lireCapteurTemperature() {
  unsigned long maintenant = millis();

  if (maintenant - lastSensorRead >= sensorInterval) {
    lastSensorRead = maintenant;
    lireCapteurMaintenant();
    envoyerTemperature();
  }
}

void lireCapteurMaintenant() {
  float nouvelleTemperature = dht.readTemperature();
  float nouvelleHumidite = dht.readHumidity();

  if (isnan(nouvelleTemperature) || isnan(nouvelleHumidite)) {
    capteurOk = false;
    Serial.println("ERR:DHT22");
    return;
  }

  temperature = nouvelleTemperature;
  humidite = nouvelleHumidite;
  capteurOk = true;
}

void gererAlerteTemperature() {
  if (capteurOk && temperature >= seuilTemperature) {
    digitalWrite(LED_ALERTE, HIGH);
  } else {
    digitalWrite(LED_ALERTE, LOW);
  }
}

void envoyerTemperature() {
  if (!capteurOk) {
    return;
  }

  Serial.print("TEMP:");
  Serial.println(temperature);

  Serial.print("HUM:");
  Serial.println(humidite);

  Serial.print("SEUIL:");
  Serial.println(seuilTemperature);
}
