/*
 * ESP32-UWB-DW3000 모듈과 U1 칩 탑재 아이폰 간의 UWB 통신
 * iOS 앱과 페어링하여 거리 측정을 위한 ESP32 펌웨어
 * 
 * 이 코드는 ESP32 마이크로컨트롤러와 DW3000 UWB 모듈을 사용하여
 * 아이폰의 U1 칩과 통신하기 위한 펌웨어입니다.
 * 블루투스를 통해 초기 연결을 설정하고, UWB를 통해 정밀한 거리 측정을 수행합니다.
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <SPI.h>
#include "DW3000.h"

// BLE 서비스 및 특성 UUID (iOS 앱과 동일)
// 아래 UUID들은 표준 Nordic UART 서비스(NUS) UUID를 사용합니다.
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // 서비스 UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // RX 특성 (ESP32가 수신)
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // TX 특성 (ESP32가 송신)

// 메시지 ID 정의 (iOS 앱과 동일)
// 앱과 액세서리 간의 통신 프로토콜입니다.
enum MessageId {
  // 액세서리에서 보내는 메시지
  ACCESSORY_CONFIGURATION_DATA = 0x1, // 액세서리 구성 데이터
  ACCESSORY_UWB_DID_START = 0x2,      // UWB 세션 시작 알림
  ACCESSORY_UWB_DID_STOP = 0x3,       // UWB 세션 중지 알림
  
  // 앱에서 보내는 메시지
  INITIALIZE = 0xA,                   // 초기화 요청
  CONFIGURE_AND_START = 0xB,          // 구성 및 시작 명령
  STOP = 0xC                          // 중지 명령
};

// DW3000 핀 설정
// ESP32와 DW3000 모듈 간의 연결 핀 정의
const uint8_t PIN_RST = 27; // 리셋 핀
const uint8_t PIN_IRQ = 34; // 인터럽트 핀
const uint8_t PIN_SS = 4;   // SPI 선택 핀

// BLE 변수
BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
bool deviceConnected = false;    // 현재 연결 상태
bool oldDeviceConnected = false; // 이전 연결 상태

// UWB 관련 변수
byte configData[100]; // UWB 구성 데이터 저장용 배열
volatile bool receivedConfig = false; // 구성 데이터 수신 여부
volatile bool uwbRunning = false;     // UWB 세션 실행 여부

// 디바이스 연결 콜백 클래스
// BLE 연결/연결 해제 이벤트를 처리합니다.
class MyServerCallbacks: public BLEServerCallbacks {
  // 연결 이벤트 처리
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("디바이스가 연결되었습니다");
  }

  // 연결 해제 이벤트 처리
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("디바이스가 연결 해제되었습니다");
    
    // UWB가 실행 중이면 중지
    if (uwbRunning) {
      stopUWB();
    }
  }
};

// BLE 수신 데이터 콜백 클래스
// BLE를 통해 수신된 데이터를 처리합니다.
class MyCallbacks: public BLECharacteristicCallbacks {
  // 데이터 수신 시 호출되는 메서드
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0) {
      Serial.print("수신된 값: ");
      for (int i = 0; i < rxValue.length(); i++) {
        Serial.print(rxValue[i], HEX);
        Serial.print(" ");
      }
      Serial.println();

      // 첫 바이트가 메시지 ID
      uint8_t messageId = rxValue[0];
      
      // 메시지 ID에 따른 처리
      switch (messageId) {
        case INITIALIZE:
          // 초기화 요청 처리
          handleInitialize();
          break;
        
        case CONFIGURE_AND_START:
          // 메시지의 나머지 부분이 UWB 구성 데이터
          if (rxValue.length() > 1) {
            // 구성 데이터 저장
            memset(configData, 0, sizeof(configData));
            memcpy(configData, rxValue.data() + 1, rxValue.length() - 1);
            receivedConfig = true;
            // UWB 세션 시작
            startUWB();
          }
          break;
        
        case STOP:
          // UWB 세션 중지
          stopUWB();
          break;
      }
    }
  }
};

// 초기 설정
void setup() {
  // 시리얼 통신 초기화
  Serial.begin(115200);
  Serial.println("ESP32-UWB-DW3000 모듈 시작 중...");

  // DW3000 UWB 칩 초기화
  setupDW3000();
  
  // BLE 초기화
  setupBLE();
}

// 메인 루프
void loop() {
  // UWB 통신이 활성화된 경우 거리 측정 실행
  if (uwbRunning) {
    // 실제 구현에서는 여기서 DW3000 API를 통해 거리 측정 수행
    float distance = measureDistance();
    
    // 거리 출력 (디버깅용)
    Serial.print("거리: ");
    Serial.print(distance);
    Serial.println(" m");
    
    // 필요한 경우 거리 데이터를 BLE를 통해 전송할 수 있음
    // 하지만 iOS 앱은 NINearbyObject를 통해 직접 거리를 받으므로 일반적으로 필요하지 않음
  }

  // BLE 연결 상태 변경 처리
  // 연결되었다가 연결 해제된 경우 광고 재시작
  if (deviceConnected && !oldDeviceConnected) {
    // 연결됨
    oldDeviceConnected = deviceConnected;
  }

  if (!deviceConnected && oldDeviceConnected) {
    // 연결 해제됨
    delay(500); // 연결 해제 후 서버 재시작을 위한 지연
    pServer->startAdvertising(); // 재광고 시작
    Serial.println("광고 시작");
    oldDeviceConnected = deviceConnected;
  }
  
  delay(100); // 루프 딜레이 - 너무 빠른 실행 방지
}

// BLE 설정
void setupBLE() {
  // BLE 디바이스 초기화
  BLEDevice::init("ESP32-UWB-DW3000");
  
  // BLE 서버 생성
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // BLE 서비스 생성
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  // BLE 특성 생성 - TX (ESP32에서 아이폰으로)
  pTxCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_TX,
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
  pTxCharacteristic->addDescriptor(new BLE2902());

  // BLE 특성 생성 - RX (아이폰에서 ESP32로)
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
                                           CHARACTERISTIC_UUID_RX,
                                           BLECharacteristic::PROPERTY_WRITE
                                         );
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // 서비스 시작
  pService->start();
  
  // 광고 설정 및 시작
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // iPhone 연결 문제 해결을 위한 함수
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println("BLE 설정 완료, 클라이언트 연결 대기 중...");
}

// DW3000 UWB 칩 설정
void setupDW3000() {
  // SPI 초기화 - ESP32와 DW3000 간의 통신 설정
  SPI.begin(18, 19, 23, PIN_SS); // SCK, MISO, MOSI, SS
  
  // DW3000 리셋 핀 설정
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, LOW);
  delay(2);
  digitalWrite(PIN_RST, HIGH);
  delay(10);
  
  // IRQ 핀 설정
  pinMode(PIN_IRQ, INPUT);
  
  // DW3000 초기화
  if (DW3000.begin(PIN_IRQ, PIN_RST)) {
    Serial.println(F("DW3000이 성공적으로 초기화되었습니다!"));
  } else {
    Serial.println(F("DW3000 초기화 실패!"));
    while (1) ; // 초기화 실패 시 무한 루프
  }
  
  // DW3000 기본 설정
  DW3000.setDeviceAddress(1); // 장치 주소 설정
  DW3000.setNetworkId(10);    // 네트워크 ID 설정
  
  // 기타 필요한 DW3000 설정
  // 주파수 채널, PRF, 데이터 레이트 등 설정
  
  Serial.println("DW3000 설정 완료");
}

// UWB 초기화 요청 처리
void handleInitialize() {
  Serial.println("초기화 요청 처리 중");
  
  // 구성 데이터 생성 (실제 구현에서는 DW3000에서 가져온 정보로 구성)
  uint8_t configMsg[32]; // 실제 구성 데이터 크기에 맞게 조정
  
  // 첫 바이트는 메시지 ID
  configMsg[0] = ACCESSORY_CONFIGURATION_DATA;
  
  // 나머지 바이트는 구성 데이터 (예시)
  // 실제 구현에서는 NINearbyAccessoryConfiguration에 필요한 바이너리 데이터 포맷으로 정확히 구성해야 함
  for (int i = 1; i < 32; i++) {
    configMsg[i] = i; // 임시 예시 데이터
  }
  
  // 구성 데이터 전송
  if (deviceConnected) {
    pTxCharacteristic->setValue(configMsg, sizeof(configMsg));
    pTxCharacteristic->notify();
    Serial.println("구성 데이터 전송됨");
  }
}

// UWB 세션 시작
void startUWB() {
  Serial.println("UWB 시작");
  
  // 실제 구현에서는 여기서 iOS에서 받은 configData를 파싱하여 DW3000 설정에 적용
  
  // UWB 세션 시작 알림
  if (deviceConnected) {
    uint8_t startMsg[1] = {ACCESSORY_UWB_DID_START};
    pTxCharacteristic->setValue(startMsg, sizeof(startMsg));
    pTxCharacteristic->notify();
    Serial.println("UWB 시작 알림 전송됨");
  }
  
  uwbRunning = true;
}

// UWB 세션 중지
void stopUWB() {
  Serial.println("UWB 중지");
  
  // UWB 세션 중지 알림
  if (deviceConnected) {
    uint8_t stopMsg[1] = {ACCESSORY_UWB_DID_STOP};
    pTxCharacteristic->setValue(stopMsg, sizeof(stopMsg));
    pTxCharacteristic->notify();
    Serial.println("UWB 중지 알림 전송됨");
  }
  
  uwbRunning = false;
}

// 거리 측정 함수 (실제 구현 필요)
float measureDistance() {
  // 이 함수는 DW3000 API를 사용하여 실제 거리 측정 구현이 필요함
  // 실제 구현에서는 TWR(Two-Way Ranging) 또는 다른 거리 측정 알고리즘을 사용
  
  // === 실제 거리 측정 코드는 여기에 구현 필요 ===
  // 1. DW3000에서 UWB 신호 송신
  // 2. 반사된 신호 수신 또는 응답 신호 수신
  // 3. 왕복 시간 계산
  // 4. 거리 계산 (시간 * 광속)
  
  // 예시로 랜덤한 거리 값을 반환 (테스트용)
  return random(10, 200) / 100.0;
}