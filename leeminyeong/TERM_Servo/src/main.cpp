#include <Arduino.h>
#include <IO7F32.h>
#include <ESP32Servo.h>

#define SERVO_PIN        18   // ← 부트 핀 아님, PWM 안정 핀으로 변경
#define OPEN_ANGLE       45
#define CLOSE_ANGLE      0
#define OPEN_DURATION_MS 2000UL

char* ssid_pfix = (char*)"sonic_my";
Servo garageServo;
bool opened = false;
unsigned long openTs = 0;

void handleUserCommand(char* topic, JsonDocument* root) {
  const char* cmd = (*root)["d"]["cmd"];
  Serial.printf("📩 %s\n", cmd);
  if (strcmp(cmd,"arrived")==0) {
    garageServo.write(OPEN_ANGLE);
    opened = true; openTs = millis();
    Serial.println("🚪 Open");
  } else if (strcmp(cmd,"departed")==0) {
    garageServo.write(CLOSE_ANGLE);
    opened = false;
    Serial.println("🚪 Close");
  }
}

void setup() {
  Serial.begin(115200);
  initDevice();

  garageServo.attach(SERVO_PIN, 500, 2000);

  // 토픽 설정
  cfg["device"] = ssid_pfix;
  cfg["event"]  = "evt";
  cfg["broker"] = "54.197.59.70";
  cfg["port"]   = 1883;
  cfg["meta"]["tls"] = false;

  WiFi.begin((const char*)cfg["ssid"],(const char*)cfg["w_pw"]);
  while(WiFi.status()!=WL_CONNECTED){delay(200);Serial.print(".");}

  userCommand = handleUserCommand;
  set_iot_server();
  iot_connect();
  Serial.println("▶ Ready");
}

void loop() {
  if (!client.connected()) iot_connect();
  client.loop();
  if (opened && millis()-openTs>=OPEN_DURATION_MS) {
    garageServo.write(CLOSE_ANGLE);
    opened = false;
  }
}
