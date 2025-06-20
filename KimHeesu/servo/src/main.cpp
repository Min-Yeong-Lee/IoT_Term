#include <Arduino.h>
#include <IO7F32.h>
#include <ESP32Servo.h>

String user_html = ""
    "<p><input type='text' name='meta.yourVar' placeholder='Your Custom Config'>";
int customVar1;

char* ssid_pfix = (char*)"IOTfinalCurtain";
unsigned long lastPublishMillis = -pubInterval;

const int SERVO_PIN = 13;
Servo myServo;

void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");
    data["angle"] = myServo.read();
    serializeJson(root, msgBuffer);
    client.publish(evtTopic, msgBuffer);
}

void handleUserMeta() {
    customVar1 = cfg["meta"]["yourVar"];
}

void handleUserCommand(char* topic, JsonDocument* root) {
    JsonObject d = (*root)["d"];
    Serial.println(topic);
     if (d.containsKey("angle")) {
        int angle = constrain(d["angle"].as<int>(), 0, 180);
        delay(100);
        if (angle == 0) {
          delay(100);
          myServo.write(0);
          Serial.printf("Servo angle set to: %d\n", angle);
        }
        else if (angle == 180) {
          myServo.write(180);
          Serial.printf("Servo angle set to: %d\n", angle);
        }
        lastPublishMillis = -pubInterval;
    }
}

void setup() {
    Serial.begin(115200);
    initDevice();
    delay(1000); 
    ledcDetachPin(SERVO_PIN); 
    bool ok = myServo.attach(SERVO_PIN, 500, 2400);
    /*Serial.printf("Servo attached? %s\n", ok ? "YES" : "NO");
    if (ok) {
      myServo.write(0);
      delay(1000);
      myServo.write(180);
      delay(1000);
      myServo.write(90);
    }*/
    delay(100);
    myServo.write(0);

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

    userMeta = handleUserMeta;
    delay(100);
    userCommand = handleUserCommand;
    delay(100);
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