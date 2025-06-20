#include <Arduino.h>               // 아두이노 기본 라이브러리full json
#include <IO7F32.h>                // AWS IoT 통신용 라이브러리

String user_html = "";             // 웹 인터페이스용 HTML 문자열

char* ssid_pfix = (char*)"IOTVoiceController";  // WiFi 접속점 이름 접두사
unsigned long lastPublishMillis = -pubInterval; // 마지막 데이터 전송 시간

// ⭐ ISD1820 음성 모듈 핀 설정
#define ISD1820_PLAYE_PIN   17     // ISD1820 PLAYE 핀 (음성 재생 트리거)
#define ISD1820_REC_PIN     16    // ISD1820 REC 핀 (녹음 트리거) - 선택사항
#define ISD1820_FT_PIN      18     // ISD1820 FT 핀 (Feed Through) - 선택사항

// LED 핀 설정 (상태 표시용)
#define STATUS_LED      2          // 상태 표시 LED (음성 재생 시 3초간 점등)

// 음성 재생 상태 변수
bool isPlayingVoice = false;       // 음성 재생 중인지 여부
unsigned long voiceStartTime = 0;  // 음성 재생 시작 시간
const unsigned long LED_DURATION = 3000;   // LED 점등 지속 시간 (3초)

// ⭐ 기본 음성 재생 함수
void playVoiceMessage() {
    Serial.println("🔊 Playing voice message...");
    
    // 상태 LED 켜기 (3초간 점등)
    digitalWrite(STATUS_LED, HIGH);
    
    // ISD1820 재생 트리거 (LOW 신호로 트리거)
    digitalWrite(ISD1820_PLAYE_PIN, LOW);   
    delay(100);                             // 100ms 트리거 신호 유지
    digitalWrite(ISD1820_PLAYE_PIN, HIGH);  // 트리거 신호 해제
    
    // 음성 재생 상태 업데이트
    isPlayingVoice = true;
    voiceStartTime = millis();
    
    Serial.println("🎵 Voice message triggered - ISD1820 will play full recording");
    Serial.println("💡 Status LED ON for 3 seconds");
    publishVoiceStatus("playing");
}

// ⭐ 접근 허용시 음성 재생 함수
void playAccessGrantedMessage() {
    Serial.println("✅ Playing 'Access Granted' message...");
    
    // 음성 재생
    playVoiceMessage();
}

// ⭐ 접근 거부시 음성 재생 함수  
void playAccessDeniedMessage() {
    Serial.println("❌ Playing 'Access Denied' message...");
    
    // 음성 재생
    playVoiceMessage();
}

// ⭐ 환영 메시지 재생 함수
void playWelcomeMessage() {
    Serial.println("👋 Playing 'Welcome' message...");
    
    // 음성 재생
    playVoiceMessage();
}

// ⭐ 긴급 알림 메시지 재생 함수
void playEmergencyMessage() {
    Serial.println("🚨 Playing 'Emergency' message...");
    
    // 음성 재생
    playVoiceMessage();
}

// ⭐ 음성 재생 상태를 AWS IoT로 전송
void publishVoiceStatus(String status) {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");
    
    data["voice_status"] = status;              // 음성 상태
    data["device_id"] = WiFi.macAddress();      // 디바이스 MAC 주소
    data["device_type"] = "voice_controller";   // 디바이스 타입
    data["timestamp"] = millis();               // 타임스탬프
    
    serializeJson(root, msgBuffer);
    bool result = client.publish(evtTopic, msgBuffer);
    
    if (result) {
        Serial.println("📡 Voice status sent to AWS IoT: " + status);
    } else {
        Serial.println("❌ Failed to send voice status");
    }
}

// ⭐ AWS IoT에서 오는 명령 처리
void handleUserCommand(char* topic, JsonDocument* root) {
    JsonObject d = (*root)["d"];
    
    Serial.println("📨 Received command:");
    Serial.println(topic);
    
    // 전체 JSON 출력 (디버그)
    String jsonString;
    serializeJson(*root, jsonString);
    Serial.println("📋 Full JSON: " + jsonString);
    
    // RFID 리더기에서 오는 접근 신호 처리
    if (d.containsKey("access_granted") && d.containsKey("device_type")) {
        String deviceType = d["device_type"];
        
        // RFID 리더기에서 온 신호인지 확인
        if (deviceType == "rfid_reader") {
            bool accessGranted = d["access_granted"];
            String cardUID = d.containsKey("card_uid") ? d["card_uid"].as<String>() : "unknown";
            
            Serial.println("🔍 RFID Signal - Card: " + cardUID + ", Access: " + String(accessGranted));
            
            if (accessGranted) {
                Serial.println("✅ Access granted from RFID - Playing welcome message");
                // 직접 playVoiceMessage() 호출로 변경
                Serial.println("🧪 DIRECT VOICE TEST - BYPASSING playAccessGrantedMessage()");
                playVoiceMessage();
            } else {
                Serial.println("❌ Access denied from RFID - Playing denied message");
                playVoiceMessage();
            }
        }
    }
    
    // 직접 음성 제어 명령 처리
    if (d.containsKey("voice_command")) {
        String voiceCmd = d["voice_command"];
        
        Serial.println("🎵 Voice command received: " + voiceCmd);
        Serial.println("🧪 DIRECT VOICE TEST - BYPASSING ALL WRAPPER FUNCTIONS");
        
        // 모든 명령에 대해 직접 playVoiceMessage() 호출
        playVoiceMessage();
        
        /*
        if (voiceCmd == "welcome") {
            playWelcomeMessage();
        } else if (voiceCmd == "access_granted") {
            playAccessGrantedMessage();
        } else if (voiceCmd == "access_denied") {
            playAccessDeniedMessage();
        } else if (voiceCmd == "emergency") {
            playEmergencyMessage();
        } else if (voiceCmd == "test") {
            Serial.println("🧪 Voice test command");
            playVoiceMessage();
        } else {
            Serial.println("❓ Unknown voice command: " + voiceCmd);
        }
        */
    }
    
    // LED 테스트 명령
    if (d.containsKey("led_test")) {
        String ledTest = d["led_test"];
        Serial.println("💡 LED test: " + ledTest);
        
        if (ledTest == "on") {
            digitalWrite(STATUS_LED, HIGH);
            delay(1000);
            digitalWrite(STATUS_LED, LOW);
        } else if (ledTest == "test") {
            // 3초간 점등 테스트
            digitalWrite(STATUS_LED, HIGH);
            delay(3000);
            digitalWrite(STATUS_LED, LOW);
        }
    }
    
    // 볼륨 제어 (ISD1820이 지원하는 경우)
    if (d.containsKey("volume_control")) {
        String volumeCmd = d["volume_control"];
        Serial.println("🔊 Volume control: " + volumeCmd);
        // 볼륨 제어 로직은 ISD1820 모델에 따라 다름
    }
}

// ⭐ 음성 재생 완료 처리
void checkVoicePlayback() {
    if (isPlayingVoice) {
        // 음성 재생 시간이 끝났는지 확인 (3초)
        if (millis() - voiceStartTime >= LED_DURATION) {
            isPlayingVoice = false;
            digitalWrite(STATUS_LED, LOW);  // 상태 LED 끄기
            Serial.println("🎵 Voice playback completed");
            Serial.println("💡 Status LED OFF");
            publishVoiceStatus("completed");
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("🎵 ESP32-ISD1820 Voice Controller Starting...");
    
    // ⭐ ISD1820 핀 초기화
    pinMode(ISD1820_PLAYE_PIN, OUTPUT);
    pinMode(ISD1820_REC_PIN, OUTPUT);     // 녹음 기능용 (선택사항)
    pinMode(ISD1820_FT_PIN, OUTPUT);      // Feed Through용 (선택사항)
    
    // ISD1820 초기 상태 설정 (모든 핀 HIGH = 비활성화)
    digitalWrite(ISD1820_PLAYE_PIN, HIGH);
    digitalWrite(ISD1820_REC_PIN, HIGH);
    digitalWrite(ISD1820_FT_PIN, HIGH);
    
    Serial.println("🔊 ISD1820 voice module initialized");
    
    // LED 핀 초기화 (GPIO 2번만)
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);  // 초기 상태는 꺼짐
    
    // 초기 LED 테스트 (시스템 시작 표시)
    digitalWrite(STATUS_LED, HIGH);
    delay(500);
    digitalWrite(STATUS_LED, LOW);
    delay(500);
    digitalWrite(STATUS_LED, HIGH);
    delay(500);
    digitalWrite(STATUS_LED, LOW);
    
    // IO7F32 라이브러리 초기화
    initDevice();
    JsonObject meta = cfg["meta"];
    pubInterval = 0; // heartbeat 비활성화 - 명령 수신 시에만 응답
    lastPublishMillis = -pubInterval;
    
    // WiFi 연결
    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    
    Serial.print("📡 Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println();
    Serial.println("📶 IP address: " + WiFi.localIP().toString());
    Serial.println("🔗 MAC address: " + WiFi.macAddress());
    
    // AWS IoT 연결
    userCommand = handleUserCommand;
    set_iot_server();
    iot_connect();
    
    Serial.println("🎵 Voice Controller Ready!");
    Serial.println("🔊 ISD1820 Voice System Online!");
    Serial.println("⏳ Waiting for voice commands...");
    
    // 시스템 시작 알림 (2초 후)
    delay(2000);
    Serial.println("🎉 Playing system startup message...");
    playWelcomeMessage();
}

void loop() {
    // AWS IoT 연결 상태 확인
    if (!client.connected()) {
        Serial.println("⚠️ AWS IoT disconnected! Reconnecting...");
        iot_connect();
    }
    client.loop();
    
    // 음성 재생 상태 확인
    checkVoicePlayback();
    
    // 불필요한 heartbeat 제거 - 명령 수신 시에만 응답
    
    delay(50);  // CPU 부하 감소
}