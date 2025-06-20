// io7 IOT Device
#include <ConfigPortal32.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <PubSubClient.h>
#include <HTTPUpdate.h>
#include <ESPmDNS.h>

const char compile_date[] = __DATE__ " " __TIME__;
char cmdTopic[200]      = "iot3/%s/cmd/+/fmt/+";
char evtTopic[200]      = "iot3/%s/evt/status/fmt/json";
char connTopic[200]     = "iot3/%s/evt/connection/fmt/json";
char logTopic[200]      = "iot3/%s/mgmt/device/status";
char metaTopic[200]     = "iot3/%s/mgmt/device/meta";
char updateTopic[200]   = "iot3/%s/mgmt/device/update";
char rebootTopic[200]   = "iot3/%s/mgmt/initiate/device/reboot";
char resetTopic[200]    = "iot3/%s/mgmt/initiate/device/factory_reset";
char upgradeTopic[200]  = "iot3/%s/mgmt/initiate/firmware/update";

String user_config_html = ""
    "<p><input type='text' name='broker' placeholder='Broker'>"
    "<div style='margin-left:50px'>"
    "<h3>Use SSL : <input type='checkbox' name='ssl' value='true'/></h3>"
    "</div>"
    "<p><input type='text' name='devId' placeholder='Device Id'>"
    "<p><input type='text' name='token' placeholder='Device Token'>"
    "<p><input type='text' name='meta.pubInterval' placeholder='Publish Interval'>";

String cert_html = ""
    "<html><head><title>MQTT Server Certificate</title></head><body>"
        "<style>"
            "input {font-size:3em; width:90%%; text-align:center;}"
            "textarea {font-size:1em; width:90%%; text-align:left; height:10em;}"
            "button { border:0;border-radius:0.3rem;background-color:#1fa3ec;"
            "color:#fff; line-height:2em;font-size:3em;width:90%%;}"
        "</style>"
        "<center><h1>Certificate</h1>"
        "<form action='/cert_save' method='post'>"
            "<p><textarea type='text' name='cert' placeholder='Default io7lab certificate'>%s</textarea>"
            "<p><button type='submit'>Save</button>"
        "</form></center>"
    "</body></html>";

String new_redirect_html = ""
    "<html><head><meta http-equiv='refresh' content='0; URL=http:/check_ssl' /></head>"
    "<body><p>Redirecting</body></html>";

extern String       user_html;

WebServer           server(80);
WiFiClientSecure    wifiClientSecure;
WiFiClient          wifiClient;
PubSubClient        client;
char                iot_server[100];
char                msgBuffer[JSON_CHAR_LENGTH];
unsigned long       pubInterval;
int                 mqttPort;

char                caFile[] = "/ca.pem";
String              ca = ""
   "-----BEGIN CERTIFICATE-----\n"
   "MIIDhjCCAm4CCQC5R8/qqp9yLjANBgkqhkiG9w0BAQsFADCBhDELMAkGA1UEBhMC\n"
   "S1IxDjAMBgNVBAgMBVNlb3VsMQ4wDAYDVQQHDAVTZW91bDEPMA0GA1UECgwGaW83\n"
   "bGFiMQwwCgYDVQQLDANFZHUxEzARBgNVBAMMCmlvN2xhYi5jb20xITAfBgkqhkiG\n"
   "9w0BCQEWEnlvb25zZW9rQGdtYWlsLmNvbTAeFw0yMzA3MjAwMDU0NDlaFw0yODA3\n"
   "MTgwMDU0NDlaMIGEMQswCQYDVQQGEwJLUjEOMAwGA1UECAwFU2VvdWwxDjAMBgNV\n"
   "BAcMBVNlb3VsMQ8wDQYDVQQKDAZpbzdsYWIxDDAKBgNVBAsMA0VkdTETMBEGA1UE\n"
   "AwwKaW83bGFiLmNvbTEhMB8GCSqGSIb3DQEJARYSeW9vbnNlb2tAZ21haWwuY29t\n"
   "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuw1+N46J6LYTzvgW4iwi\n"
   "GcqcmYoOokKoCGTUK1n6ZzvRIZGM3rhiNciyGYxZlSao8SnnRu/xgXgOV/dZ+cqg\n"
   "dCEYlhQoc59pU1qSNqpT4g5qbWCljsx1GOM2DXSPHWb7w6c3SAZTUTRdSJ1h5ieh\n"
   "eNw/9Ia1Na90ihWLXmRKl8RSyMOknWJmz0bOZ0wR4qsJZMY0blN1O9n2Z4w2kA7c\n"
   "pYZmZAJcrP9jCAdIx6777O9zVSljKsq3Y3k29tO3VcAfd1Ce1N5UfmB8mo9R+qdk\n"
   "OedRcq9sf6nW03WSMsVtiXJszZVVo5pgmNp9u4tlb8rqqhkfttDeaXmHSHo+D/b3\n"
   "KwIDAQABMA0GCSqGSIb3DQEBCwUAA4IBAQCKqhEUXF4tDs7VQMWerJL4AbVCWN2P\n"
   "VdXQlQ0a6wkzfIhFByKoA83Uhyh/4cFleT8ZOw/S1RmQNhUwlIqzRYkdxKm7VR/A\n"
   "RMcnmUA0xNRmGG35G+4zf81Bk9BzOQ2pAmvVuuAbluAv7FxAP/z/aE367B7xafxR\n"
   "5B8laBZisqUSUc2RizT6Dabf/D7912wLGqgLPPOYJ/kn+3+G4sKW9/77Xb6tZMJi\n"
   "IRB+42iMBj73Qwn795SWdxq9M/egAfG+ykNL2Ytkz8S68r+LzTw4DK0JKqVlawjo\n"
   "GL8aK8vD+nZo5ec66yYL/CxPYn/K8fMH+r1X92ftitY/wgOstV8xKcwu\n"
   "-----END CERTIFICATE-----\n";

void handleIOTCommand(char* topic, byte* payload, unsigned int payloadLength);
void (*userCommand)(char* topic, JsonDocument* root) = NULL;
void (*userMeta)() = NULL;

bool subscribeTopic(const char* topic) {
    if (client.subscribe(topic)) {
        Serial.printf("Subscription to %s OK\n", topic);
        return true;
    } else {
        Serial.printf("Subscription to %s Failed\n", topic);
        return false;
    }
}

void setDevId(char* topic, const char* devId) {
    char buffer[200];
    sprintf(buffer, topic, devId);
    strcpy(topic, buffer);
}

void initDevice() {
    loadConfig();
    if (LittleFS.exists(caFile)) {
        File f = LittleFS.open(caFile, "r");
        ca = f.readString();
        ca.trim();
        f.close();
    }

    if (!cfg.containsKey("config") || strcmp((const char*)cfg["config"], "done")) {
// MODIFY HERE FOR CERTIFICATE
        webServer.on("/check_ssl", [](){
            if (cfg.containsKey("ssl") &&  strcmp((const char*)cfg["ssl"], "true") == 0) {
                char buffer[2000];
                if (LittleFS.exists(caFile)) {
                    sprintf(buffer, cert_html.c_str(), ca.c_str());
                } else {
                    sprintf(buffer, cert_html.c_str(), "");
                }
                webServer.send(200, "text/html", buffer);
            } else {
                webServer.send(200, "text/html", postSave_html);
            }
        });
        webServer.on("/cert_save", HTTP_POST, [](){
            if (webServer.arg("cert").length() > 0) {
                File f = LittleFS.open(caFile, "w");
                f.print(webServer.arg("cert"));
                f.close();
            } else {
                LittleFS.remove(caFile);
            }
            webServer.send(200, "text/html", postSave_html);
        });
        redirect_html = new_redirect_html;
        configDevice();
    } else {
        const char* devId = (const char*)cfg["devId"];
        const char* ssl = "false";
        if (cfg.containsKey("ssl")) {
            ssl = (const char*)cfg["ssl"];
        }
        setDevId(evtTopic, devId);
        setDevId(logTopic, devId);
        setDevId(connTopic, devId);
        setDevId(metaTopic, devId);
        setDevId(cmdTopic, devId);
        setDevId(updateTopic, devId);
        setDevId(rebootTopic, devId);
        setDevId(resetTopic, devId);
        setDevId(upgradeTopic, devId);
        sprintf(iot_server, "%s", (const char*)cfg["broker"]);
        if (strcmp(ssl, "true") == 0) {
            wifiClientSecure.setCACert(ca.c_str());
            client.setClient(wifiClientSecure);
            mqttPort = 8883;
        } else {
            client.setClient(wifiClient);
            mqttPort = 1883;
        }
    }
}

void set_iot_server() {
    client.setCallback(handleIOTCommand);
    if (mqttPort == 8883) {
        if (!wifiClientSecure.connect(iot_server, mqttPort)) {
            Serial.println("ssl connection failed");
            return;
        }
    } else {
        if (!wifiClient.connect(iot_server, mqttPort)) {
            Serial.println("connection failed");
            return;
        }
    }
    client.setServer(iot_server, mqttPort);  // IOT
}

void pubMeta() {
    JsonObject meta = cfg["meta"];
    StaticJsonDocument<512> root;
    JsonObject d = root.createNestedObject("d");
    JsonObject metadata = d.createNestedObject("metadata");
    for (JsonObject::iterator it = meta.begin(); it != meta.end(); ++it) {
        metadata[it->key().c_str()] = it->value();
    }
    JsonObject supports = d.createNestedObject("supports");
    supports["deviceActions"] = true;
    serializeJson(root, msgBuffer);
    Serial.printf("publishing device metadata: %s\n", msgBuffer);
    client.publish(metaTopic, msgBuffer, true);
}

void iot_connect() {
    while (!client.connected()) {
        int mqConnected = 0;
        mqConnected = client.connect((const char*)cfg["devId"],
                                     (const char*)cfg["devId"],
                                     (const char*)cfg["token"],
                                     connTopic,
                                     0,
                                     true,
                                     "{\"d\":{\"status\":\"offline\"}}",
                                     true);
        if (mqConnected) {
            Serial.println("MQ connected");
        } else {
            if (digitalRead(RESET_PIN) == 0) {
                reboot();
            }
            if (WiFi.status() == WL_CONNECTED) {
                if (client.state() == -2) {
                    set_iot_server();
                } else {
                    Serial.printf("MQ Connection fail RC = %d, try again in 5 seconds\n", client.state());
                }
                delay(5000);
            } else {
                Serial.println("Reconnecting to WiFi");
                WiFi.disconnect();
                WiFi.begin();
                int i = 0;
                while (WiFi.status() != WL_CONNECTED) {
                    Serial.print("*");
                    delay(5000);
                    if (i++ > 10) reboot();
                }
            }
        }
    }
    if (!subscribeTopic(rebootTopic)) return;
    if (!subscribeTopic(resetTopic)) return;
    if (!subscribeTopic(updateTopic)) return;
    if (!subscribeTopic(upgradeTopic)) return;
    if (!subscribeTopic(cmdTopic)) return;
    pubMeta();
    client.publish(connTopic, "{\"d\":{\"status\":\"online\"}}", true);
}

void update_progress(int cur, int total) {
    Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
    Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

void handleIOTCommand(char* topic, byte* payload, unsigned int payloadLength) {
    byte2buff(msgBuffer, payload, payloadLength);
    StaticJsonDocument<512> root;
    deserializeJson(root, String(msgBuffer));
    JsonObject d = root["d"];

    if (strstr(topic, rebootTopic)) {  // rebooting
        reboot();
    } else if (strstr(topic, resetTopic)) {  // clear the configuration and reboot
        reset_config();
        ESP.restart();
    } else if (strstr(topic, updateTopic)) {
        JsonArray fields = d["fields"];
        for (JsonArray::iterator it = fields.begin(); it != fields.end(); ++it) {
            DynamicJsonDocument field = *it;
            const char* fieldName = field["field"];
            if (strstr(fieldName, "metadata")) {
                JsonObject fieldValue = field["value"];
                cfg.remove("meta");
                JsonObject meta = cfg.createNestedObject("meta");
                for (JsonObject::iterator fv = fieldValue.begin(); fv != fieldValue.end(); ++fv) {
                    meta[(char*)fv->key().c_str()] = fv->value();
                }
                save_config_json();
            }
        }
        pubMeta();
        pubInterval = cfg["meta"]["pubInterval"];
        if (userMeta) {
            userMeta();
        }
    } else if (strstr(topic, upgradeTopic)) {
        JsonObject upgrade = d["upgrade"];
        String response = "{\"OTA\":{\"status\":";
        if(upgrade.containsKey("fw_url")) {
		    Serial.println("firmware upgrading");
            const char* fw_url = upgrade["fw_url"];
            WiFiClient cli;
            HTTPUpdate httpUpdate;
            httpUpdate.onProgress(update_progress);
            httpUpdate.onError(update_error);
            client.publish(logTopic,"{\"info\":{\"upgrade\":\"Device will be upgraded.\"}}" );
            t_httpUpdate_return ret = httpUpdate.update(cli, fw_url);
	            switch(ret) {
		            case HTTP_UPDATE_FAILED:
                        response += "\"[update] Update failed. " + String(fw_url) + "\"}}";
                        client.publish(logTopic, (char*)response.c_str());
                        Serial.println(response);
		                break;
		            case HTTP_UPDATE_NO_UPDATES:
                        response += "\"[update] Update no Update.\"}}";
                        client.publish(logTopic, (char*) response.c_str());
                        Serial.println(response);
		                break;
		            case HTTP_UPDATE_OK:
		                Serial.println("[update] Update ok."); // may not called we reboot the ESP
		                break;
	            }
        } else {
            response += "\"OTA Information Error\"}}";
            client.publish(logTopic, (char*) response.c_str());
            Serial.println(response);
        }
    } else if (strstr(topic, "/cmd/")) {
        if (d.containsKey("config")) {
            char maskBuffer[JSON_CHAR_LENGTH];
            cfg["compile_date"] = compile_date;
            maskConfig(maskBuffer);
            cfg.remove("compile_date");
            String info = String("{\"config\":") + String(maskBuffer) + String("}");
            client.publish(logTopic, info.c_str());
        } else if (userCommand) {
            userCommand(topic, &root);
        }
    }
}