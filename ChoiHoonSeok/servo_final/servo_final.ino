#include <Arduino.h>               // 아두이노 기본 라이브러리
#include <IO7F32.h>                // AWS IoT 통신용 라이브러리
#include <ESP32Servo.h>            // ESP32 서보모터 제어 라이브러리

String user_html = "";             // 웹 인터페이스용 HTML 문자열

char* ssid_pfix = (char*)"IOTServoController";  // WiFi 접속점 이름 접두사
unsigned long lastPublishMillis = -pubInterval; // 마지막 데이터 전송 시간

// 서보모터 핀 설정
#define SERVO_PIN       18         // 서보모터 신호 핀 (PWM 출력)
Servo doorServo;                   // 서보모터 객체 생성

// LED 핀 설정 (상태 표시용)
#define LED_GREEN       2          // 초록색 LED 핀 (문 열림 표시)
#define LED_RED         4          // 빨간색 LED 핀 (문 잠김 표시)

// 도어락 상태 변수
bool doorLocked = true;            // 도어 잠금 상태 (true=잠김, false=열림)
int servoPosition = 0;             // 서보모터 현재 위치 (0도=잠김, 90도=열림)
unsigned long doorOpenTime = 0;    // 문이 열린 시간 기록
const unsigned long AUTO_CLOSE_DELAY = 3000; // 3초 후 자동 닫기

void publishData() {
    StaticJsonDocument<512> root;     // JSON 문서 생성 (512바이트 크기)
    JsonObject data = root.createNestedObject("d");  // "d" 객체 생성

    data["door_status"] = doorLocked ? "locked" : "unlocked";  // 도어 상태 추가
    data["servo_position"] = servoPosition;    // 서보모터 위치 추가
    data["device_id"] = WiFi.macAddress();     // 디바이스 MAC 주소 추가
    data["device_type"] = "servo_controller";  // 디바이스 타입 지정

    serializeJson(root, msgBuffer);    // JSON을 문자열로 변환
    client.publish(evtTopic, msgBuffer); // AWS IoT로 메시지 전송
    Serial.println("Door action logged to AWS IoT"); // 전송 완료 로그
}

void handleUserCommand(char* topic, JsonDocument* root) {
    JsonObject d = (*root)["d"];      // JSON에서 "d" 객체 추출
    
    Serial.println("Received command:");     // 명령 수신 로그
    Serial.println(topic);                  // 수신된 토픽 출력
    
    // RFID 리더기에서 오는 접근 승인 신호 처리
    if (d.containsKey("access_granted") && d.containsKey("device_type")) {
        String deviceType = d["device_type"]; // 디바이스 타입 확인
        
        // RFID 리더기에서 온 신호인지 확인
        if (deviceType == "rfid_reader") {
            bool accessGranted = d["access_granted"]; // 접근 허용 여부 확인
            
            if (accessGranted) {              // 접근이 허용된 경우
                Serial.println(" Access granted from RFID - Opening door");
                openDoorTemporarily();        // 문 열고 자동으로 닫기
            } else {                          // 접근이 거부된 경우
                Serial.println(" Access denied from RFID reader");
                showAccessDenied();           // 접근 거부 LED 표시
            }
        }
    }
    
    // 원격 제어 명령 처리 (Node-RED에서 오는 수동 명령)
    if (d.containsKey("door_control")) {
        String command = d["door_control"];   // 명령 내용 추출
        
        if (command == "auto_open") {         // 자동문 열기 명령 (3초 후 자동 닫힘)
            openDoorTemporarily();
            Serial.println(" Door opened remotely (auto-close in 3 sec)");
            
        } else if (command == "manual_open") { // 수동 열기 명령 (자동 닫힘 없음)
            manualOpenDoor();
            Serial.println(" Door opened manually (stays open)");
            
        } else if (command == "manual_close") { // 수동 닫기 명령
            manualCloseDoor();
            Serial.println(" Door closed manually");
            
        } else if (command == "lock") {       // 강제 잠금 명령 (기존)
            lockDoor();
            Serial.println(" Door locked remotely");
            
        } else if (command == "unlock") {     // 강제 열림 유지 명령 (기존)
            unlockDoor();
            doorOpenTime = 0;                 // 자동 닫기 비활성화
            Serial.println(" Door unlocked permanently");
        }
    }
    
    // 서보모터 직접 제어 (테스트용)
    if (d.containsKey("servo_angle")) {
        int angle = d["servo_angle"];         // 각도 값 추출
        angle = constrain(angle, 0, 180);     // 0~180도 범위로 제한
        doorServo.write(angle);               // 서보모터 각도 설정
        servoPosition = angle;                // 현재 위치 업데이트
        doorLocked = (angle < 45);            // 45도 미만이면 잠김으로 간주
        Serial.println("🔧 Servo moved to: " + String(angle) + " degrees");
        publishData();                        // 상태 변경 로그 전송
    }
}

void openDoorTemporarily() {
    Serial.println(" Opening door temporarily...");
    
    // 문 열기
    doorServo.write(90);              // 서보모터를 90도로 회전 (문 열림)
    servoPosition = 90;               // 현재 위치 변수 업데이트
    doorLocked = false;               // 잠금 상태를 해제로 변경
    digitalWrite(LED_GREEN, HIGH);    // 초록 LED 켜기 (열림 상태 표시)
    digitalWrite(LED_RED, LOW);       // 빨간 LED 끄기
    
    doorOpenTime = millis();          // 문이 열린 시간 기록 (자동 닫기용)
    
    delay(500);                       // 서보모터 동작 완료까지 0.5초 대기
    Serial.println(" Door OPENED - Will auto-close in 3 seconds");
    
    publishData();                    // 문 열림 로그 전송
}

void unlockDoor() {
    Serial.println(" Unlocking door permanently...");
    doorServo.write(90);              // 서보모터를 90도로 회전 (문 열림)
    servoPosition = 90;               // 현재 위치 변수 업데이트
    doorLocked = false;               // 잠금 상태를 해제로 변경
    digitalWrite(LED_GREEN, HIGH);    // 초록 LED 켜기 (열림 상태 표시)
    digitalWrite(LED_RED, LOW);       // 빨간 LED 끄기
    delay(500);                       // 서보모터 동작 완료까지 0.5초 대기
    Serial.println(" Door UNLOCKED permanently");
    publishData();                    // 상태 변경 로그 전송
}

void manualOpenDoor() {
    Serial.println(" Opening door manually...");
    
    // 문 열기
    doorServo.write(90);              // 서보모터를 90도로 회전 (문 열림)
    servoPosition = 90;               // 현재 위치 변수 업데이트
    doorLocked = false;               // 잠금 상태를 해제로 변경
    digitalWrite(LED_GREEN, HIGH);    // 초록 LED 켜기 (열림 상태 표시)
    digitalWrite(LED_RED, LOW);       // 빨간 LED 끄기
    
    doorOpenTime = 0;                 // 자동 닫기 타이머 비활성화 (수동이므로)
    
    delay(500);                       // 서보모터 동작 완료까지 0.5초 대기
    Serial.println(" Door OPENED manually - Will stay open until manually closed");
    
    publishData();                    // 문 열림 로그 전송
}

void manualCloseDoor() {
    Serial.println(" Closing door manually...");
    
    doorServo.write(0);               // 서보모터를 0도로 회전 (문 잠김)
    servoPosition = 0;                // 현재 위치 변수 업데이트
    doorLocked = true;                // 잠금 상태를 잠김으로 변경
    digitalWrite(LED_GREEN, LOW);     // 초록 LED 끄기
    digitalWrite(LED_RED, HIGH);      // 빨간 LED 켜기 (잠김 상태 표시)
    doorOpenTime = 0;                 // 자동 닫기 타이머 초기화
    
    delay(500);                       // 서보모터 동작 완료까지 0.5초 대기
    Serial.println(" Door CLOSED manually");
    
    publishData();                    // 문 닫힘 로그 전송
}

void lockDoor() {
    Serial.println(" Locking door...");
    doorServo.write(0);               // 서보모터를 0도로 회전 (문 잠김)
    servoPosition = 0;                // 현재 위치 변수 업데이트
    doorLocked = true;                // 잠금 상태를 잠김으로 변경
    digitalWrite(LED_GREEN, LOW);     // 초록 LED 끄기
    digitalWrite(LED_RED, HIGH);      // 빨간 LED 켜기 (잠김 상태 표시)
    doorOpenTime = 0;                 // 자동 닫기 타이머 초기화
    delay(500);                       // 서보모터 동작 완료까지 0.5초 대기
    Serial.println(" Door LOCKED");
    publishData();                    // 상태 변경 로그 전송
}

void showAccessDenied() {
    // 접근 거부 표시 (빨간 LED 5번 빠르게 깜빡임)
    Serial.println(" Showing access denied signal");
    for (int i = 0; i < 5; i++) {     // 5번 반복
        digitalWrite(LED_RED, !digitalRead(LED_RED)); // LED 상태 토글
        delay(100);                    // 0.1초 대기
    }
    digitalWrite(LED_RED, doorLocked ? HIGH : LOW); // 원래 상태로 복원
}

void setup() {
    Serial.begin(115200);             // 시리얼 통신 시작 (115200 보드레이트)
    
    // 서보모터 초기화
    doorServo.attach(SERVO_PIN);      // 서보모터를 지정된 핀에 연결
    lockDoor();                       // 시스템 시작시 도어를 잠금 상태로 설정
    
    // LED 핀 설정
    pinMode(LED_GREEN, OUTPUT);       // 초록 LED를 출력 모드로 설정
    pinMode(LED_RED, OUTPUT);         // 빨간 LED를 출력 모드로 설정
    digitalWrite(LED_RED, HIGH);      // 초기 상태는 빨간불 켜기 (잠김 상태)
    
    // IO7F32 라이브러리 초기화
    initDevice();                     // 디바이스 설정 초기화
    JsonObject meta = cfg["meta"];    // 설정에서 메타데이터 가져오기
    pubInterval = meta.containsKey("pubInterval") ? meta["pubInterval"] : 0; // 주기적 전송 비활성화 (필요시에만 전송)
    lastPublishMillis = -pubInterval; // 첫 전송을 위한 시간 초기화
    
    // WiFi 연결
    WiFi.mode(WIFI_STA);             // WiFi를 스테이션 모드로 설정
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]); // WiFi 연결 시작
    while (WiFi.status() != WL_CONNECTED) { // 연결될 때까지 대기
        delay(500);                   // 0.5초 대기
        Serial.print(".");           // 연결 진행 상황 표시
    }
    
    Serial.printf("\nIP address: ");        // IP 주소 출력 라벨
    Serial.println(WiFi.localIP());         // 할당받은 IP 주소 출력
    Serial.println("MAC address: " + WiFi.macAddress()); // MAC 주소 출력
    
    // AWS IoT 연결 설정
    userCommand = handleUserCommand;  // 명령 처리 함수 등록
    set_iot_server();                // IoT 서버 설정
    iot_connect();                   // IoT 서버 연결
    
    Serial.println(" Automatic Door Controller Ready!");
    Serial.println(" Waiting for RFID access commands...");
    Serial.println(" Door will open for 3 seconds when access granted");
}

void loop() {
    // AWS IoT 연결 상태 확인 및 유지
    if (!client.connected()) {        // AWS IoT 연결이 끊어졌는지 확인
        iot_connect();                // 연결이 끊어지면 재연결 시도
    }
    client.loop();                    // MQTT 클라이언트 메시지 처리 (명령 수신 확인)
    
    // 자동 닫기 기능 (문이 열린 후 3초 후 자동 잠금)
    if (!doorLocked && doorOpenTime > 0) {    // 문이 열려있고 타이머가 설정되어 있으면
        if (millis() - doorOpenTime >= AUTO_CLOSE_DELAY) { // 3초가 지났으면
            Serial.println(" Auto-closing door after 3 seconds");
            lockDoor();               // 자동으로 문 잠금
        }
    }
    
    delay(100);                       // CPU 부하 감소를 위한 0.1초 대기
}