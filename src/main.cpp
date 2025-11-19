#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "dlink";
const char* password = "UnserWlanIstSchoen2020!";
const char* mqtt_broker = "eu1.cloud.thethings.network";
const int mqtt_port = 1883;
const char* mqtt_user = "ttgommi-leipzig@ttn";
const char* mqtt_pass = "NNSXS.LAJKJIOFYTWTTY57SM55CW5ATZPNY5PSMLPMAEQ.HRHNQ7IEQKE3YOS6MX6LJEPLMFHWTT3HCC4UY3LEMZ7HCNKHAXWQ";
const char* topic = "v3/ttgommi-leipzig@ttn/devices/ttgommi3/up";

WiFiClient espClient;
PubSubClient client(espClient);

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Topic: "); Serial.println(topic);
  Serial.print("Message: ");
  for (int i=0; i<length; i++) Serial.print((char)payload[i]);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while(!client.connected()) {
    Serial.println("Connecting MQTT...");
    if(client.connect("esp32-sub", mqtt_user, mqtt_pass)) {
      Serial.println("Connected to TTN MQTT");
    } else {
      Serial.print("Failed: "); Serial.println(client.state());
      delay(2000);
    }
  }

  client.subscribe(topic);
}

void loop() {
  client.loop();
}
