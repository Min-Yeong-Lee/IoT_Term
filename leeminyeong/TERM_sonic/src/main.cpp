#include <Arduino.h>
#include <IO7F32.h>

const int  TRIG_PIN     = 22;
const int  ECHO_PIN     = 23;
const long DETECTION_TH = 15;  // cm

unsigned long lastPublishMillis;
bool          lastCarDetected   = false;

char* ssid_pfix = (char*)"sonic_my";

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  initDevice();

  // — 토픽/장치명 설정 (evt/status 로 매칭)
  cfg["event"]  = "evt";          
  cfg["device"] = ssid_pfix;

  // 발행 주기 가져오기
  JsonObject meta = cfg["meta"];
  if (meta.containsKey("pubInterval")) {
    pubInterval = meta["pubInterval"].as<unsigned long>();
  } else {
    pubInterval = 200;
  }
  // 즉시 발행을 위해 offset
  lastPublishMillis = millis() - pubInterval;

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());

  // MQTT (명령 없음)
  userCommand = [](char*, JsonDocument*) {};
  set_iot_server();
  iot_connect();

  Serial.println("▶ Initialized");
}

long measureDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);   delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long d = pulseIn(ECHO_PIN, HIGH, 30000);
  return d > 0 ? (d * 0.0343) / 2 : 999;
}

void loop() {
  if (!client.connected()) iot_connect();
  client.loop();

  unsigned long now = millis();
  if (now - lastPublishMillis < pubInterval) return;
  lastPublishMillis = now;

  bool car = (measureDistanceCM() <= DETECTION_TH);
  if (car != lastCarDetected) {
    lastCarDetected = car;

    StaticJsonDocument<200> root;
    JsonObject d = root.createNestedObject("d");
    d["car_status"] = car ? "arrived" : "departed";

    String payload;
    serializeJson(root, payload);

    // evtTopic = …/sonic_my/evt/status/fmt/json
    client.publish(evtTopic, payload.c_str());

    Serial.printf("▶ %s → %s\n", car ? "arrived" : "departed", payload.c_str());
  }
}
