#include <Arduino.h>
#include <IO7F32.h>

String user_html = ""
    "<p><input type='text' name='meta.yourVar' placeholder='Your Custom Config'>";
int customVar1;

char* ssid_pfix = (char*)"IOTfinalLED";
unsigned long lastPublishMillis = -pubInterval;

const int LED = 5;

void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");
    data["valve"] = digitalRead(LED) == 1 ? "on" : "off";
    serializeJson(root, msgBuffer);
    client.publish(evtTopic, msgBuffer);
}

void handleUserMeta() {
    customVar1 = cfg["meta"]["yourVar"];
}

void handleUserCommand(char* topic, JsonDocument* root) {
    JsonObject d = (*root)["d"];
    Serial.println(topic);
    if (d.containsKey("valve")) {
        digitalWrite(LED, strstr(d["valve"], "on") ? HIGH : LOW);
        lastPublishMillis = -pubInterval;
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);

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