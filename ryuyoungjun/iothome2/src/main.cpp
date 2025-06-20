#include <Arduino.h>
#include <IO7F32.h>
#include <ESP32Servo.h>
#include <BH1750.h>
#include <HTTPClient.h>

#define SDA_PIN 5
#define SCL_PIN 4

#define BUZZER_PIN 23   // 부저 연결 핀 (변경 가능)

String user_html = "";

char* ssid_pfix = (char*)"IOTValveServo";
unsigned long lastPublishMillis = -pubInterval;

Servo valveServo;
const int SERVO_PIN = 15;  // 서보모터 제어 핀

// 조도센서 관련
BH1750 lightMeter;           // BH1750 인스턴스
#define LIGHT_THRESHOLD 5000          // 밝기 임계값 (환경에 맞게 조정)
bool fireDetected = false;            // 화재 발생 상태 저장

// 서보 위치 정의 (각도 조정은 실제 밸브에 맞게)
const int ANGLE_OPEN = 90;
const int ANGLE_CLOSE = 0;

void sendFireAlertToNodeRED() {
    HTTPClient http;
    http.begin("http://54.197.59.70:1880/fire_alert");  // Node-RED 서버 주소
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST("{\"event\":\"fire\"}");
    Serial.print("Node-RED 응답코드: "); Serial.println(httpResponseCode);
    http.end();
}

void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");

    int pos = valveServo.read();
    data["valve"] = (pos > 45) ? "on" : "off";
    data["lux"] = lightMeter.readLightLevel();   // 현재 밝기도 같이 전송

    serializeJson(root, msgBuffer);
    client.publish(evtTopic, msgBuffer);
}

void handleUserCommand(char* topic, JsonDocument* root) {
    JsonObject d = (*root)["d"];

    Serial.println(topic);
    if (d.containsKey("valve")) {
        if (strstr(d["valve"], "on")) {
            valveServo.write(ANGLE_OPEN);
        } else if (strstr(d["valve"], "off")) {
            valveServo.write(ANGLE_CLOSE);
        }
        lastPublishMillis = -pubInterval;
    }
}

void setup() {
    Serial.begin(115200);
    valveServo.attach(SERVO_PIN);

    pinMode(BUZZER_PIN, OUTPUT);       // 부저 출력 모드
    digitalWrite(BUZZER_PIN, LOW);     // 부저 OFF

    // BH1750 초기화
    Wire.begin(SDA_PIN, SCL_PIN);
    if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
        Serial.println("Error initializing BH1750");
        while (1);  // 초기화 실패시 멈춤
    }
    Serial.println("BH1750 조도센서 초기화 완료");

    initDevice();
    JsonObject meta = cfg["meta"];
    pubInterval = meta.containsKey("pubInterval") ? meta["pubInterval"] : 0;
    lastPublishMillis = -pubInterval;

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.printf("\nIP address : ");
    Serial.println(WiFi.localIP());

    userCommand = handleUserCommand;
    set_iot_server();
    iot_connect();
}

void loop() {
    if (!client.connected()) {
        iot_connect();
    }
    client.loop();

    // 밝기 측정 및 화재 감지
    float lux = lightMeter.readLightLevel();
    if (lux >= LIGHT_THRESHOLD && !fireDetected) {
        Serial.println("임계값 초과: 화재 감지! 밸브 잠금, 부저 ON");
        valveServo.write(ANGLE_CLOSE);
        fireDetected = true;
        digitalWrite(BUZZER_PIN, HIGH);   // 부저 ON
        sendFireAlertToNodeRED();
    } else if (lux < LIGHT_THRESHOLD && fireDetected) {
        fireDetected = false;
        digitalWrite(BUZZER_PIN, LOW);    // 부저 OFF
    }

    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }

    delay(1000);  // 1초마다 측정
}