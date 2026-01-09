//////////////////////////////////////////////////////////////////////
////////////// ssgommi-leipzig by Martin Mittrenga ///////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>           /* Please use the TFT library provided in the library. */

#define BUTTON_PIN                    35

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  3540        /* Time ESP32 will go to sleep (in seconds) */

uint32_t cpu_frequency;

const char* ssid = "Karli68";
const char* password = "Karli_68_Yi10yBdQOu";
const char* mqtt_broker = "eu1.cloud.thethings.network";
const int mqtt_port = 1883;
const char* mqtt_user = "ttgommi-leipzig@ttn";
const char* mqtt_pass = "NNSXS.LAJKJIOFYTWTTY57SM55CW5ATZPNY5PSMLPMAEQ.HRHNQ7IEQKE3YOS6MX6LJEPLMFHWTT3HCC4UY3LEMZ7HCNKHAXWQ";
const char* topic = "v3/ttgommi-leipzig@ttn/devices/ttgommi3/up";

int charge = 0;
int sensor = 0;
float voltage = 0.0;
char buf_application_id[32];
char buf_device_id[32];
char buf_device_addr[32];
char buf_voltage[16];
char buf_charge[16];
char buf_sensor[16];
char buf_frequency[16];
char buf_bandwidth[16];
char buf_sf[16];
char buf_cr[16];
char buf_rssi[16];
char buf_snr[16];
char buf_gateway_id[16];
char buf_eui[32];
char buf_forwarder[32];
char buf_timestamp[32];

unsigned long lastDisplayPart = 0;
unsigned long lastButtonChanged = 0;
unsigned long lastTurnOff = 0;

unsigned long frequency = 0;

bool buttonState = false;

int step = 1;
int waitShort = 2000;
int waitLong = 3000;
int debounceTime = 300;
int displayOffTime = 600000;

//////////////////////////////////////////////////////////////////////

TFT_eSPI tft = TFT_eSPI();
WiFiClient espClient;
PubSubClient client(espClient);

//////////////////////////////////////////////////////////////////////

/*

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Topic: "); Serial.println(topic);
  Serial.print("Message: ");
  for (int i=0; i<length; i++) Serial.print((char)payload[i]);
  Serial.println();
}

*/

//////////////////////////////////////////////////////////////////////

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("----------- MQTT Message -----------");

  // Statische JSON Dokumentgröße: sollte groß genug für TTN Payload sein
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("JSON Parsing failed: ");
    Serial.println(error.c_str());
    return;
  }

  // Anwendung und Device ID
  const char* application_id = doc["end_device_ids"]["application_ids"]["application_id"] | "n/a";
  const char* device_id      = doc["end_device_ids"]["device_id"] | "n/a";
  const char* device_addr       = doc["end_device_ids"]["dev_addr"] | "";

  strncpy(buf_application_id, application_id, sizeof(buf_application_id) - 1);
  buf_application_id[sizeof(buf_application_id) - 1] = '\0';
  strncpy(buf_device_id, device_id, sizeof(buf_device_id) - 1);
  buf_device_id[sizeof(buf_device_id) - 1] = '\0';
  strncpy(buf_device_addr, device_addr, sizeof(buf_device_addr) - 1);
  buf_device_addr[sizeof(buf_device_addr) - 1] = '\0';

  Serial.print("Application ID: "); Serial.println(application_id);
  Serial.print("Device ID: ");      Serial.println(device_id);
  Serial.print("Device Addr: ");    Serial.println(device_addr);

  // Decoded Payload
  float voltage = doc["uplink_message"]["decoded_payload"]["voltage"] | 0.0;
  int charge    = doc["uplink_message"]["decoded_payload"]["charge"] | 0;
  int sensor    = doc["uplink_message"]["decoded_payload"]["sensor"] | 0;

  snprintf(buf_voltage, sizeof(buf_voltage), "%.3f V", voltage);
  snprintf(buf_charge, sizeof(buf_charge), "%d %%", charge);
  snprintf(buf_sensor, sizeof(buf_sensor), "%d", sensor);

  Serial.print("Voltage: "); Serial.println(voltage);
  Serial.print("Charge: ");  Serial.println(charge);
  Serial.print("Sensor: ");  Serial.println(sensor);

  // LoRa Einstellungen
  const char* freqStr = doc["uplink_message"]["settings"]["frequency"].as<const char*>();
  long bandwidth      = doc["uplink_message"]["settings"]["data_rate"]["lora"]["bandwidth"] | 0;
  int sf              = doc["uplink_message"]["settings"]["data_rate"]["lora"]["spreading_factor"] | 0;
  const char* coding_rate = doc["uplink_message"]["settings"]["data_rate"]["lora"]["coding_rate"].as<const char*>();

  unsigned long frequency = 0;
  if (freqStr != nullptr) {
    frequency = strtoul(freqStr, nullptr, 10); // in Hz
    // In MHz umrechnen und ins buf schreiben
    float freq_mhz = frequency / 1000000.0;
    snprintf(buf_frequency, sizeof(buf_frequency), "%.3f MHz", freq_mhz);
  } else {
      frequency = 0;
      snprintf(buf_frequency, sizeof(buf_frequency), "n/a");
  }

  // Bandwidth, SF, Coding Rate in Buffer
  // Bandwidth in kHz, formatiert als 125.000 kHz
  float bw_khz = bandwidth / 1000.0;  // 125000 Hz -> 125.0 kHz
  snprintf(buf_bandwidth, sizeof(buf_bandwidth), "%.3f kHz", bw_khz);

  // Spreading Factor
  snprintf(buf_sf, sizeof(buf_sf), "%d", sf);

  // Coding Rate
  if (coding_rate != nullptr) {
      strncpy(buf_cr, coding_rate, sizeof(buf_cr) - 1);
      buf_cr[sizeof(buf_cr) - 1] = '\0';
  } else {
      strncpy(buf_cr, "n/a", sizeof(buf_cr) - 1);
      buf_cr[sizeof(buf_cr) - 1] = '\0';
  }

  // Debug
  Serial.print("Frequency: "); Serial.print(frequency); Serial.println(" MHz");
  Serial.print("Bandwidth: "); Serial.print(bandwidth); Serial.println(" kHz");
  Serial.print("SF: "); Serial.println(sf);
  Serial.print("CR: "); Serial.println(buf_cr);

  // Metadata (erster Gateway)
  if(doc["uplink_message"]["rx_metadata"].size() > 0){
    JsonObject meta = doc["uplink_message"]["rx_metadata"][0];

    const char* gateway_id = meta["gateway_ids"]["gateway_id"] | "n/a";
    const char* eui        = meta["gateway_ids"]["eui"] | "n/a";
    const char* forwarder  = meta["packet_broker"]["forwarder_gateway_id"] | "n/a";

    // Gateway ID, EUI, Forwarder in Buffers kopieren
    strncpy(buf_gateway_id, gateway_id, 15);
    buf_gateway_id[sizeof(buf_gateway_id)-1] = '\0';

    if(!eui) eui = "n/a";
    strncpy(buf_eui, eui, sizeof(buf_eui)-1);
    buf_eui[sizeof(buf_eui)-1] = '\0';

    if(!forwarder) forwarder = "n/a";
    strncpy(buf_forwarder, forwarder, 16);
    buf_forwarder[16] = '\0';

    // Timestamp (nur bis Minute)
    const char* ts_meta = meta["time"].as<const char*>();
    const char* ts_pb   = meta["packet_broker"]["time"].as<const char*>();

    const char* timestamp = nullptr;

    if (ts_meta && strlen(ts_meta) > 0) {
        timestamp = ts_meta;
    } else if (ts_pb && strlen(ts_pb) > 0) {
        timestamp = ts_pb;
    } else {
        timestamp = "n/a";
    }

    // nur die ersten 16 Zeichen
    strncpy(buf_timestamp, timestamp, 16);
    buf_timestamp[16] = '\0';

    int rssi = meta["rssi"] | 0;
    float snr = meta["snr"] | 0.0;

    snprintf(buf_rssi, sizeof(buf_rssi), "%.2d dbm", rssi);
    snprintf(buf_snr, sizeof(buf_snr), "%.2f db", snr);

    Serial.print("Gateway ID: "); Serial.println(gateway_id);
    Serial.print("EUI: "); Serial.println(eui);
    Serial.print("Forwarder: "); Serial.println(forwarder);
    Serial.print("Time: "); Serial.println(timestamp);
    Serial.print("RSSI: "); Serial.print(rssi); Serial.println(" dBm");
    Serial.print("SNR: "); Serial.print(snr); Serial.println(" dB");
  }

}

//////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);

  setCpuFrequencyMhz(80);               // Set CPU Frequenz 240, 160, 80, 40, 20, 10 Mhz
  
  cpu_frequency = getCpuFrequencyMhz();
  Serial.println(" "); Serial.print("Cpu Frequenz: "); Serial.println(cpu_frequency);

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0,0); //1 = true, 0 = false

  //esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  pinMode(BUTTON_PIN, INPUT);

  //tft.init();
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  //tft.setSwapBytes(true);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);

  lastTurnOff = millis();

  Serial.println("------- WiFi and TTN Connect -------");
  Serial.println("Things Network");
  Serial.println("Weatherstation");
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Things Network", 20, 20, 4); 
  tft.drawString("Weatherstation", 20, 50, 4); 
  delay(1000);

  Serial.println("LoRaWAN MQTT");
  Serial.println("Subscriber");
  tft.fillScreen(TFT_BLACK);
  tft.drawString("LoRaWAN MQTT", 20, 20, 4); 
  tft.drawString("Subscriber", 20, 50, 4); 
  delay(1000);
  
  Serial.println("Connecting WiFi...");
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.println("WiFi connected");
  tft.fillScreen(TFT_BLACK);
  tft.drawString("WiFi connected", 20, 20, 4); 
  delay(1000);

  Serial.println(WiFi.localIP());
  tft.fillScreen(TFT_BLACK);
  tft.drawString(WiFi.localIP().toString(), 20, 20, 4);
  delay(1000);

  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);

  client.setBufferSize(4096);

  while(!client.connected()) {
    Serial.println("Connecting TTN...");
    if(client.connect("esp32-sub", mqtt_user, mqtt_pass)) {
      Serial.println("TTN connected");
      tft.fillScreen(TFT_BLACK);
      tft.drawString("TTN connected", 20, 20, 4); 
      delay(1000);
    } else {
      Serial.print("Failed: "); Serial.println(client.state());
      delay(2000);
    }
  }

  client.subscribe(topic);
}

//////////////////////////////////////////////////////////////////////

void loop() {
  client.loop();

  if ((millis() - lastDisplayPart > waitShort) && (step == 1)) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("AppID:", 20, 20, 4); tft.drawString(buf_application_id, 20, 50, 4); 
    step = 2;
    lastDisplayPart = millis();
  }
  
  if ((millis() - lastDisplayPart > waitShort) && (step == 2)) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("DevID:", 20, 20, 4); tft.drawString(buf_device_id, 20, 50, 4); 
    step = 3;
    lastDisplayPart = millis();
  }

  if ((millis() - lastDisplayPart > waitShort) && (step == 3)) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("DevADD:", 20, 20, 4); tft.drawString(buf_device_addr, 20, 50, 4); 
    step = 4;
    lastDisplayPart = millis();
  }

  if ((millis() - lastDisplayPart > waitShort) && (step == 4)) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Voltage:", 20, 20, 4); tft.drawString(buf_voltage, 20, 50, 4); 
    step = 5;
    lastDisplayPart = millis();
  }

  if ((millis() - lastDisplayPart > waitShort) && (step == 5)) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Charge:", 20, 20, 4); tft.drawString(buf_charge, 20, 50, 4); 
    step = 6;
    lastDisplayPart = millis();
  }

  if ((millis() - lastDisplayPart > waitShort) && (step == 6)) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Sensor:", 20, 20, 4); tft.drawString(buf_sensor, 20, 50, 4); 
    step = 7;
    lastDisplayPart = millis();
  }

  if ((millis() - lastDisplayPart > waitShort) && (step == 7)) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Frequency:", 20, 20, 4); tft.drawString(buf_frequency, 20, 50, 4); 
    step = 8;
    lastDisplayPart = millis();
  }

  if ((millis() - lastDisplayPart > waitShort) && (step == 8)) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Bandwidth:", 20, 20, 4); tft.drawString(buf_bandwidth, 20, 50, 4); 
    step = 9;
    lastDisplayPart = millis();
  }

  if ((millis() - lastDisplayPart > waitShort) && (step == 9)) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Spreading Factor:", 20, 20, 4); tft.drawString(buf_sf, 20, 50, 4); 
    step = 10;
    lastDisplayPart = millis();
  }

  if ((millis() - lastDisplayPart > waitShort) && (step == 10)) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Coding Rate:", 20, 20, 4); tft.drawString(buf_cr, 20, 50, 4); 
    step = 11;
    lastDisplayPart = millis();
  }

  if ((millis() - lastDisplayPart > waitShort) && (step == 11)) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Gateway ID:", 20, 20, 4); tft.drawString(buf_gateway_id, 20, 50, 4); 
    step = 12;
    lastDisplayPart = millis();
  }

  if ((millis() - lastDisplayPart > waitShort) && (step == 12)) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("EUI:", 20, 20, 4); tft.drawString(buf_eui, 20, 50, 4); 
    step = 13;
    lastDisplayPart = millis();
  }

  if ((millis() - lastDisplayPart > waitShort) && (step == 13)) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Forwarder:", 20, 20, 4); tft.drawString(buf_forwarder, 20, 50, 4); 
    step = 14;
    lastDisplayPart = millis();
  }

  if ((millis() - lastDisplayPart > waitShort) && (step == 14)) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Time:", 20, 20, 4); tft.drawString(buf_timestamp, 20, 50, 4); 
    step = 15;
    lastDisplayPart = millis();
  }

  if ((millis() - lastDisplayPart > waitShort) && (step == 15)) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("RSSI:", 20, 20, 4); tft.drawString(buf_rssi, 20, 50, 4); 
    step = 16;
    lastDisplayPart = millis();
  }

  if ((millis() - lastDisplayPart > waitShort) && (step == 16)) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("SNR:", 20, 20, 4); tft.drawString(buf_snr, 20, 50, 4); 
    step = 17;
    lastDisplayPart = millis();
  }

  // Go back to first Step
  if ((millis() - lastDisplayPart > 1) && (step == 17)) {
    lastDisplayPart = millis();
    step = 1;
  }

  // Read Button and do anything
  if (millis() - lastButtonChanged > debounceTime) {
    buttonState = digitalRead(BUTTON_PIN);
    lastButtonChanged = millis(); 

    if (buttonState == false) {

      tft.fillScreen(TFT_BLACK);
      Serial.println("Going to sleep now because of button push");
      esp_deep_sleep_start();
    }
  }

  // Function turn off the Display after some time
  if ((millis() - lastTurnOff > displayOffTime)) {

    tft.fillScreen(TFT_BLACK);
    Serial.println("Going to sleep now because of timer");
    esp_deep_sleep_start();
  }

}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////