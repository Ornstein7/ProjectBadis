#include <DHT.h>

#define DHT_PIN 3
#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);

void setup() {
  Serial.begin(9600);
  dht.begin();

  delay(2000);

  Serial.println("TEST_DHT22_START");
}

void loop() {
  float temperature = dht.readTemperature();
  float humidite = dht.readHumidity();

  if (isnan(temperature) || isnan(humidite)) {
    Serial.println("ERR:DHT22");
  } else {
    Serial.print("TEMP:");
    Serial.println(temperature);

    Serial.print("HUM:");
    Serial.println(humidite);
  }

  delay(2500);
}
