#include <Arduino.h>
#include <IO7F32.h>
#include <ESP32Servo.h>

String user_html = "";  // 필요시 웹설정 UI

char* ssid_pfix = (char*)"IOTWINDOW";
unsigned long lastPublishMillis = -pubInterval;
const int SERVO_PIN = 15;

// SG90 기준 각도 (0: 닫힘, 90: 열림)
const int ANGLE_OPEN = 90;
const int ANGLE_CLOSE = 0;
Servo windowServo;

// 현재 상태 캐싱(추가 정보 표시시)
String windowState = "close";

void publishData() {
    StaticJsonDocument<256> root;
    JsonObject data = root.createNestedObject("d");
    int pos = windowServo.read();
    data["window"] = (pos > 45) ? "open" : "close";
    serializeJson(root, msgBuffer);
    Serial.print("[상태전송] "); Serial.println(msgBuffer); // ★ 이 라인 추가
    client.publish(evtTopic, msgBuffer);
}

void handleUserCommand(char* topic, JsonDocument* root) {
    JsonObject d = (*root)["d"];
    Serial.print("[명령수신] "); Serial.println(topic);

    if (d.containsKey("window")) {
        const char* windowCmd = d["window"];
        if (strstr(windowCmd, "open")) {
            windowServo.write(ANGLE_OPEN);
            windowState = "open";
        } else if (strstr(windowCmd, "close")) {
            windowServo.write(ANGLE_CLOSE);
            windowState = "close";
        }
        lastPublishMillis = -pubInterval;
        Serial.printf("→ 창문 상태: %s\n", windowState.c_str());
    }
}

void setup() {
    Serial.begin(115200);
    initDevice();
    windowServo.attach(SERVO_PIN);

    
    JsonObject meta = cfg["meta"];
    pubInterval = meta.containsKey("pubInterval") ? meta["pubInterval"] : 0;
    lastPublishMillis = -pubInterval;

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500); Serial.print(".");
    }
    Serial.printf("\nIP address : "); Serial.println(WiFi.localIP());

    userCommand = handleUserCommand;
    set_iot_server();
    iot_connect();
}


void loop() {
    if (!client.connected()) iot_connect();
    client.loop();

    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }
}
