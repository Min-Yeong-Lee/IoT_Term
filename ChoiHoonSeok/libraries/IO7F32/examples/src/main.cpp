#include <Arduino.h>
#include <IO7F32.h>

String user_html =
    ""
    // USER CODE EXAMPLE : your custom config variable
    // in meta.XXXXX, XXXXX should match to ArduinoJson index to access
    "<p><input type='text' name='meta.yourVar' placeholder='Your Custom Config'>";
;
// for meta.XXXXX, this var is the C variable to hold the XXXXX
int customVar1;
// USER CODE EXAMPLE : your custom config variable

char* ssid_pfix = (char*)"IOTValve";
unsigned long lastPublishMillis = -pubInterval;
const int RELAY = 15;

void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");

    // USER CODE EXAMPLE : command handling
    data["valve"] = digitalRead(RELAY) == 1 ? "on" : "off";
    // USER CODE EXAMPLE : command handling

    serializeJson(root, msgBuffer);
    client.publish(evtTopic, msgBuffer);
}

void handleUserMeta() {
    // USER CODE EXAMPLE : meta data update
    // If any meta data updated on the Internet, it can be stored to local variable to use for the logic
    // in cfg["meta"]["XXXXX"], XXXXX should match to one in the user_html
    customVar1 = cfg["meta"]["yourVar"];
    // USER CODE EXAMPLE
}

void handleUserCommand(char* topic, JsonDocument* root) {
    JsonObject d = (*root)["d"];

    // USER CODE EXAMPLE : status/change update
    // code if any of device status changes to notify the change
    Serial.println(topic);
    if (d.containsKey("valve")) {
        if (strstr(d["valve"], "on")) {
            digitalWrite(RELAY, HIGH);
        } else {
            digitalWrite(RELAY, LOW);
        }
        lastPublishMillis = -pubInterval;
    }
    // USER CODE EXAMPLE
}

void setup() {
    Serial.begin(115200);
    // USER CODE EXAMPLE : Initialization
    pinMode(RELAY, OUTPUT);
    // USER CODE EXAMPLE

    initDevice();
    // USER CODE EXAMPLE : meta data to local variable
    JsonObject meta = cfg["meta"];
    pubInterval = meta.containsKey("pubInterval") ? meta["pubInterval"] : 0;
    lastPublishMillis = -pubInterval;
    // USER CODE EXAMPLE

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    // main setup
    Serial.printf("\nIP address : ");
    Serial.println(WiFi.localIP());

    // if there is any user Meta Data, implement the funtcion and assign it to userMeta
    userMeta = handleUserMeta;
    // if there is any user Command, implement the funtcion and assign it to userCommand
    userCommand = handleUserCommand;
    set_iot_server();
    iot_connect();
}

void loop() {
    if (!client.connected()) {
        iot_connect();
    }
    // USER CODE EXAMPLE : main loop
    //     you can put any main code here, for example,
    //     the continous data acquistion and local intelligence can be placed here
    // USER CODE EXAMPLE
    client.loop();
    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }
}