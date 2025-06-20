#include <Arduino.h>
#include <IO7F32.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const int RELAY = 15;
const char* weatherApiKey = "5e9df23ec1fbf34552059c875a878119";
char* ssid_pfix = (char*)"IOTValve";
const char* city = "Seoul";
float TEMP_LIMIT = 30.0;   // 초기값(슬라이더로 갱신 가능)
float current_temp = 0.0;  // API에서 받아온 온도값
unsigned long lastPublishMillis = 0;

// 릴레이, 온도값 상태 MQTT publish
void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");
    data["valve_relay"] = digitalRead(RELAY) == HIGH ? "on" : "off";
    data["temp"] = current_temp; // 현재온도 같이 전송
    serializeJson(root, msgBuffer);
    client.publish(evtTopic, msgBuffer);
}

// Node-RED 명령 수신 처리 (수동 제어 및 슬라이더 값 반영)
void handleUserCommand(char* topic, JsonDocument* root) {
    JsonObject d = (*root)["d"];
    if (d.containsKey("valve_relay")) {
        const char* cmd = d["valve_relay"].as<const char*>();
        if (strcmp(cmd, "on") == 0) digitalWrite(RELAY, HIGH);
        else if (strcmp(cmd, "off") == 0) digitalWrite(RELAY, LOW);
        lastPublishMillis = -pubInterval; // 상태 즉시 전송
    }
    // 슬라이더에서 설정온도 값이 오면 TEMP_LIMIT을 갱신!
    if (d.containsKey("set_temp")) {
        TEMP_LIMIT = d["set_temp"];
        Serial.print("슬라이더로 설정온도 갱신: ");
        Serial.println(TEMP_LIMIT);
    }
}

// 날씨 API에서 현재온도 받아와 current_temp에 저장
void checkTempAndSetRelay() {
    String url = String("http://api.openweathermap.org/data/2.5/weather?q=") +
                 city + "&appid=" + weatherApiKey + "&units=metric&lang=kr";
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == 200) {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        if (!deserializeJson(doc, payload)) {
            float temp = doc["main"]["temp"];
            current_temp = temp; // 전역에 저장!
            Serial.print("[Weather] Current temp: ");
            Serial.println(current_temp);
        }
    }
    http.end();
}

void setup() {
    Serial.begin(115200);
    pinMode(RELAY, OUTPUT);
    digitalWrite(RELAY, LOW);

    // 1. IO7F32 기반 cfg 로드(포털 진입 필요시 자동 진입)
    initDevice();

    // 2. WiFi 직접 연결
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected.");

    // 3. 부팅 직후 1회 온도 체크
    checkTempAndSetRelay();

    // 4. MQTT, Node-RED 연동
    userCommand = handleUserCommand;
    set_iot_server();
    iot_connect();

    // pubInterval 등 동기화
    JsonObject meta = cfg["meta"];
    pubInterval = meta.containsKey("pubInterval") ? meta["pubInterval"] : 0;
    lastPublishMillis = -pubInterval;
}

void loop() {
    if (!client.connected()) iot_connect();
    client.loop();

    // 주기적으로 날씨 API에서 온도 받아오기 (예: 10분마다)
    static unsigned long lastApiMillis = 0;
    if (millis() - lastApiMillis > 10 * 60 * 1000UL) { // 10분마다(필요시 변경)
        checkTempAndSetRelay();
        lastApiMillis = millis();
    }

    // 주기적으로 릴레이+온도 상태 publish
    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }
}