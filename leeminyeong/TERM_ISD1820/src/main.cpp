#include <Arduino.h>
#include <IO7F32.h>
#include <WiFi.h>        // Wi-Fi API
#include <TFT_eSPI.h>    // TFT 라이브러리

#define VOICE_PIN 43

TFT_eSPI tft = TFT_eSPI();

// ——— 설정 포털 SSID 접두어 ———
String user_html = "";
char*  ssid_pfix  = (char*)"CarDisplay";  

// ——— 원격 명령 콜백 ———
// JSON payload 예: { "d":{ "status":"arrived" } }
void handleUserCommand(char* topic, JsonDocument* root) {
  JsonObject d = (*root)["d"];
  Serial.printf("수신된 topic: %s\n", topic);

  if (d.containsKey("status")) {
    const char* status = d["status"];
    Serial.printf("명령 상태: %s\n", status);
    
    if (strcmp(status, "arrived") == 0) {
      // TFT 출력
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(3);
      tft.drawString("car arrived", 60, 50);

      // ISD1820 음성 재생
      digitalWrite(VOICE_PIN, LOW);
      delay(500);
      digitalWrite(VOICE_PIN, HIGH);
      
    } else if (strcmp(status, "departed") == 0) {
      // 차량 출발
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(3);
      tft.drawString("car departed",  60, 50); // x좌표는 필요에 따라 조정
    }
  }
}

void setup() {
  Serial.begin(115200);

  // 1) TFT 초기화
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawString("Waiting...", 80,50);


  // 2) ISD1820 핀 초기화
  pinMode(VOICE_PIN, OUTPUT);
  digitalWrite(VOICE_PIN, HIGH);  // ← idle 상태

  // PLAYE (edge-trigger) 기준 예시
  digitalWrite(VOICE_PIN, LOW);   // ↓ 펄스 시작 (재생)
  delay(800);
  digitalWrite(VOICE_PIN, HIGH);  // ↑ 펄스 끝 (idle)


  // 3) LittleFS의 config.json 에서 설정 불러오기
  initDevice();

  // 3) LittleFS로 불러온 뒤, 아래 두 줄을 이렇게 바꿔
  cfg["device"] = "sonic_my";   // tft_my → sonic_my
  cfg["event"]  = "evt";        // cmd    → evt



  // 4) Wi-Fi 연결 (STA 모드)
  WiFi.mode(WIFI_STA);
  WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWiFi OK, IP: %s\n", WiFi.localIP().toString().c_str());

  // 5) MQTT 콜백 등록 및 연결
  userCommand = handleUserCommand;
  set_iot_server();  // config.json(meta)에서 broker/port/tls 읽음
  iot_connect();

  Serial.println("▶ CarDisplay ready");
}

void loop() {
  // MQTT 재연결 & 유지
  if (!client.connected()) {
    iot_connect();
  }
  client.loop();
}
