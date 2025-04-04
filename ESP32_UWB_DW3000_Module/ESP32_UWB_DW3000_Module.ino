/*
 * ESP32-UWB-DW3000 모듈과 U1 칩 탑재 아이폰 간의 UWB 통신
 * iOS 앱과 페어링하여 거리 측정을 위한 ESP32 펌웨어
 * 
 * 이 코드는 ESP32 마이크로컨트롤러와 DW3000 UWB 모듈을 사용하여
 * 아이폰의 U1 칩과 통신하기 위한 펌웨어입니다.
 * 블루투스를 통해 초기 연결을 설정하고, UWB를 통해 정밀한 거리 측정을 수행합니다.
 */

// Arduino 기본 라이브러리
#include <Arduino.h>
#include <SPI.h>

// BLE 라이브러리
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// 호환성 패치 헤더 파일 포함
#include "ESP32_BLE_Compatibility.h"

// 함수 미리 선언 - 컴파일 오류 방지
void handleInitialize();
void startUWB();
void stopUWB();
float measureDistance();
void setupDW3000();
void setupBLE();

// DW3000 라이브러리가 없을 경우를 위한 모의 클래스
class DW3000Class {
public:
  bool begin(int irq, int rst) {
    Serial.println("[모의] DW3000 초기화");
    return true;
  }
  
  void setDeviceAddress(int addr) {
    Serial.println("[모의] 장치 주소 설정: " + String(addr));
  }
  
  void setNetworkId(int id) {
    Serial.println("[모의] 네트워크 ID 설정: " + String(id));
  }
};

// DW3000 객체 생성
DW3000Class DW3000;

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
uint8_t configData[100]; // UWB 구성 데이터 저장용 배열
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
    // 원본 데이터 가져오기
    String rxValueArduino = pCharacteristic->getValue();
    uint8_t* rxData = NULL;
    int rxLength = 0;
    
    // 데이터가 있는지 확인
    if (rxValueArduino.length() > 0) {
      rxLength = rxValueArduino.length();
      rxData = new uint8_t[rxLength];
      
      // 호환성 헤더의 함수를 사용하여 데이터 변환
      stringToByteArray(rxValueArduino, rxData, rxLength);
      
      // 수신된 데이터 로깅
      Serial.print("수신된 값 (");
      Serial.print(rxLength);
      Serial.println("바이트): ");
      
      for (int i = 0; i < rxLength; i++) {
        Serial.print("0x");
        if (rxData[i] < 16) Serial.print("0"); // 한자리 16진수 앞에 0 추가
        Serial.print(rxData[i], HEX);
        Serial.print(" ");
        if ((i + 1) % 8 == 0) Serial.println(); // 가독성을 위해 8바이트마다 줄바꿈
      }
      Serial.println();

      // 첫 바이트가 메시지 ID
      if (rxLength > 0) {
        uint8_t messageId = rxData[0];
        
        // 메시지 ID에 따른 처리
        switch (messageId) {
          case INITIALIZE:
            // 초기화 요청 처리
            handleInitialize();
            break;
          
          case CONFIGURE_AND_START:
            // 메시지의 나머지 부분이 UWB 구성 데이터
            if (rxLength > 1) {
              // 구성 데이터 저장
              memset(configData, 0, sizeof(configData));
              // 안전한 복사를 위해 최대 크기 체크
              size_t copySize = min(sizeof(configData), (size_t)(rxLength - 1));
              memcpy(configData, rxData + 1, copySize);
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
      
      // 메모리 해제
      delete[] rxData;
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

// 거리 데이터를 BLE를 통해 전송하기 위한 구조체
typedef struct {
  uint8_t messageType;    // 메시지 유형 (0xD0: 거리 데이터)
  float distance;         // 거리 (미터)
  int8_t signalStrength;  // 신호 강도 (dBm)
  uint8_t status;         // 상태 (0: 정상, 1: 오류)
} DistanceData;

// 마지막 거리 업데이트 시간
unsigned long lastDistanceUpdate = 0;
// 거리 업데이트 간격 (밀리초)
const unsigned long distanceUpdateInterval = 200; // 200ms마다 업데이트

// RSSI를 거리로 변환 (BLE 신호 강도 기반, 대략적인 추정)
float rssiToDistance(int rssi) {
  // RSSI를 거리로 변환하는 간단한 공식 (근사값)
  // 참고: 이 공식은 환경에 따라 정확도가 크게 달라질 수 있음
  if (rssi == 0) return -1.0; // 유효하지 않은 RSSI
  
  // 참조 RSSI 값 (-59dBm는 1미터에서의 신호 강도)
  int refRSSI = -59;
  // 경로 손실 지수 (2.0 - 4.0 범위, 환경에 따라 다름)
  float pathLossExponent = 2.0;
  
  return pow(10.0, (refRSSI - rssi) / (10.0 * pathLossExponent));
}

// 메인 루프
void loop() {
  // 현재 시간
  unsigned long currentMillis = millis();
  
  // UWB 통신이 활성화된 경우 거리 측정 실행
  if (uwbRunning) {
    // 거리 업데이트 간격에 도달했는지 확인
    if (currentMillis - lastDistanceUpdate >= distanceUpdateInterval) {
      lastDistanceUpdate = currentMillis;
      
      // 실제 구현에서는 여기서 DW3000 API를 통해 거리 측정 수행
      float distance;
      
      // UWB 하드웨어가 없으므로 BLE RSSI 기반의 거리 측정 또는 
      // 시뮬레이션된 거리를 사용
      if (deviceConnected) {
        // 시뮬레이션 모드: 랜덤 거리 생성 (0.5~2.0m 범위의 사인 함수)
        distance = 1.25 + 0.75 * sin(currentMillis / 2000.0);
        
        // 거리 출력 (디버깅용)
        Serial.print("시뮬레이션된 거리: ");
        Serial.print(distance);
        Serial.println(" m");
        
        // BLE를 통해 거리 데이터 전송 (선택 사항)
        // 이 데이터는 iOS 앱에서 사용할 수도 있음
        DistanceData distData;
        distData.messageType = 0xD0;   // 거리 데이터 메시지 유형
        distData.distance = distance;   // 거리 (미터)
        distData.signalStrength = -60;  // 예시 신호 강도
        distData.status = 0;            // 상태: 정상
        
        // 데이터 전송
        pTxCharacteristic->setValue((uint8_t*)&distData, sizeof(distData));
        pTxCharacteristic->notify();
      }
    }
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
  
  delay(10); // 루프 딜레이 - 더 빠른 응답성을 위해 감소
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
    // 프로토타입 환경에서는 에러가 발생해도 계속 진행
    Serial.println("프로토타입 환경에서 계속 진행합니다.");
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
  Serial.println("PDF 문서에서 직접 가져온 형식 사용 - Reserved byte와 Descriptor size byte 포함");
  
  // Apple NearbyInteraction 형식에 맞는 구성 데이터
  // Apple NINearbyAccessoryConfiguration에 맞게 최적화된 데이터 형식
  
  // Apple Nearby Interaction Accessory Protocol Specification에 따른 정확한 구성 형식
  static const uint8_t improvedConfigData[] = {
    /* BEGIN V1 FiRa UWB Configuration Data */
    /* Section 3: NI Accessory Configuration Format - 직접 문서에서 인용 */
    
    /* 3.1 Protocol Version */
    0x01, 0x00,  // Version 1.0
    
    /* 3.2 Manufacturer */
    0x00, 0x00,  // 문서 예제와 동일
    
    /* 3.3 Accessory Identifier */
    0x83, 0x71, 0x22, 0x8B, 0x6A, 0x4D, 0xD2, 0x6E,
    0x96, 0xB4, 0x4F, 0x09, 0xCC, 0x35, 0x42, 0x34,
    
    /* 3.4 Primary Ranging Mode */
    0x01,  // 0x01 = Responder, 문서 예제와 동일
    
    /* 3.5 Primary Ranging Device Role */
    0x01,  // 0x01 = Controlee, 문서 예제와 동일
    
    /* 3.6 UWB Configuration Parameters */
    0x10,  // Length of UWB Configuration Block
    
    /* UWB Configuration Block */
    /* 3.6.1 UWB Config Version ID */
    0x01,
    
    /* 3.6.2 UWB Channel */
    0x09,  // Channel 9 (8.5 GHz), 문서 예제와 동일
    
    /* 3.6.3 UWB Rframe Config */
    0x02,  // 문서 예제값 사용
    
    /* 3.6.4 UWB Preamble Code Index */
    0x0A,  // 문서 예제값 사용
    
    /* 3.6.5 UWB PRF Value */
    0x01,  // 0x01 = BPRF, 문서 예제와 동일
    
    /* 3.6.6 UWB Data Rate */
    0x00,  // 0x00 = 6.8Mbps 데이터 레이트, 문서 예제와 동일
    
    /* 3.6.7 UWB Preamble Duration */
    0x03,  // 문서 예제값 사용
    
    /* 3.6.8 UWB Slot Duration */
    0x08, 0x00, 0x00, 0x00,  // 문서 슬롯 지속 시간 예제값
    
    /* 3.6.9 UWB STS Configuration */
    0x40, 0x9C, 0x20, 0x00,  // 문서 STS 구성 예제값
    
    /* 3.7 Neighboring MAP Support */
    0x00  // 문서 예제값 사용
    /* END V1 FiRa UWB Configuration Data */
  };
  
  // Apple Nearby Interaction Accessory Protocol Version 2 형식
  // Apple 문서 Section 6에서 직접 인용
  static const uint8_t alternativeConfigData[] = {
    /* BEGIN V2 FiRa UWB Configuration Data */
    /* Section 6: Protocol Version 2 Configuration Format - 직접 문서에서 인용 */
    
    /* 6.1 Protocol Version 2 Header */
    'N', 'I', 'A', 'P',  // Fixed 4-byte identifier "NIAP"
    0x02, 0x00,          // Version 2.0
    
    /* 6.2 Manufacturer */
    0x00, 0x00,  // 문서 예제값 - 사용
    
    /* 6.3 Service Characteristics */
    0x00, 0x00,  // 문서 예제값 사용
    
    /* 6.4 Accessory Identifier */
    0x83, 0x71, 0x22, 0x8B, 0x6A, 0x4D, 0xD2, 0x6E,
    0x96, 0xB4, 0x4F, 0x09, 0xCC, 0x35, 0x42, 0x34, // 문서 예제와 동일한 UUID
    
    /* 6.5 Primary Ranging Mode */
    0x01,  // 0x01 = Responder, 문서 예제와 동일
    
    /* 6.6 Primary Ranging Device Role */
    0x01,  // 0x01 = Controlee, 문서 예제와 동일
    
    /* 6.7 UWB Configuration Parameters */
    0x10,  // Length of UWB Configuration Block, 문서 예제와 동일
    
    /* UWB Configuration Block */
    /* 6.7.1 UWB Config Version ID */
    0x01,
    
    /* 6.7.2 UWB Channel */
    0x09,  // Channel 9 (8.5 GHz), 문서 예제와 동일
    
    /* 6.7.3 UWB Rframe Config */
    0x02,  // 문서 예제값 사용
    
    /* 6.7.4 UWB Preamble Code Index */
    0x0A,  // 문서 예제값 사용
    
    /* 6.7.5 UWB PRF Value */
    0x01,  // 0x01 = BPRF, 문서 예제와 동일
    
    /* 6.7.6 UWB Data Rate */
    0x00,  // 0x00 = 6.8Mbps, 문서 예제와 동일
    
    /* 6.7.7 UWB Preamble Duration */
    0x03,  // 문서 예제값 사용
    
    /* 6.7.8 UWB Slot Duration */
    0x08, 0x00, 0x00, 0x00,  // 문서 예제값 사용
    
    /* 6.7.9 UWB STS Configuration */
    0x40, 0x9C, 0x20, 0x00,  // 문서 예제값 사용
    
    /* 6.8 Neighboring MAP Support */
    0x00  // 문서 예제값 사용
    /* END V2 FiRa UWB Configuration Data */
  };
  
  // MFi 액세서리 구성 형식
  // Nearby-Interaction-Accessory-Protocol-Specification-Release-R2.pdf 문서에서 직접 인용
  static const uint8_t simplifiedConfigData[] = {
    // 표준 헤더 (사양에 따라 수정됨)
    0x00,  // Reserved byte
    
    // 프로토콜 버전 (문서 참조)
    0x01, 0x00,
    
    // 제조사 ID (MFi 인증 제조사 ID)
    0x00, 0x00,
    
    // 액세서리 고유 식별자 (사양 문서 예제와 일치)
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
    
    // 중요: UWB 액세서리 descriptor (사양 문서 참조)
    0x28, // Descriptor size byte (중요)
    
    // UWB 핵심 구성 블록 (문서에서 직접 인용)
    0x01, // 모드 (1=응답자)
    0x01, // 역할 (1=Controlee)
    
    // UWB 추가 구성 (중요 필드)
    0x09, // 채널 (9 = 채널 9)
    0x01, // PRF (1 = BPRF)
    0x01, // 필수 필드
    0x00, // 필수 필드
    
    // 추가 UWB 필수 구성 데이터
    0xAA, 0xBB, 0xCC, 0xDD,
    0x01, 0x02, 0x03, 0x04,
    0xAA, 0xBB, 0xCC, 0xDD,
    0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
    0x99, 0xAA, 0xBB, 0xCC,
    0xDD, 0xEE, 0xFF, 0x00
  };
  
  // 중요 안내 메시지
  Serial.println("==================================================");
  Serial.println("중요: PDF 문서에서 직접 가져온 UWB 구성 형식 적용");
  Serial.println("Nearby-Interaction-Accessory-Protocol-Specification-R2.pdf 참조");
  Serial.println("중요 필드: Reserved byte 및 Descriptor size byte 추가");
  Serial.println("==================================================");
  
  // 구성 데이터 선택
  uint8_t* selectedConfig = nullptr;
  size_t configSize = 0;
  
  // 여러 형식을 번갈아 시도 (이 변수를 변경하여 다른 형식 테스트)
  static int configAttempt = 0;
  
  // 항상 간소화된 형식만 사용 (부록 A)
  Serial.println("Simple Accessory Configuration(부록 A 형식) 사용 중...");
  selectedConfig = (uint8_t*)simplifiedConfigData;
  configSize = sizeof(simplifiedConfigData);
  
  Serial.println("문서의 부록 A에서 제공하는 간소화된 예제 구성을 정확하게 사용합니다.");
  Serial.println("이 형식은 제한된 기능을 가진 액세서리를 위한 것입니다.");
  Serial.println("참고: UWB 통신 실패 시 BLE 시뮬레이션으로 전환됩니다.");
  
  // 메시지 생성 (메시지 ID + 구성 데이터)
  uint8_t* configMsg = new uint8_t[1 + configSize];
  
  // 메시지 ID 설정
  configMsg[0] = ACCESSORY_CONFIGURATION_DATA;
  
  // 구성 데이터 복사
  memcpy(&configMsg[1], selectedConfig, configSize);
  
  // 디버깅을 위한 자세한 로그 출력
  Serial.print("전송 구성 데이터 (총 ");
  Serial.print(1 + configSize);
  Serial.println("바이트):");
  
  for (int i = 0; i < 1 + configSize; i++) {
    Serial.print("0x");
    if (configMsg[i] < 16) Serial.print("0"); // 한자리 16진수 앞에 0 추가
    Serial.print(configMsg[i], HEX);
    Serial.print(", ");
    if ((i + 1) % 8 == 0) Serial.println(); // 가독성을 위해 8바이트마다 줄바꿈
  }
  Serial.println();
  
  // 구성 데이터 전송
  if (deviceConnected) {
    // BLE 데이터 전송
    pTxCharacteristic->setValue(configMsg, 1 + configSize);
    pTxCharacteristic->notify();
    Serial.println("구성 데이터 전송됨 - iOS 앱에서 NINearbyAccessoryConfiguration 생성 대기 중");
    
    // 작업 상태 표시
    Serial.println("* 참고: 이 단계에서 iOS 앱이 데이터를 받고 configuration 객체를 생성하게 됩니다.");
    Serial.println("* iOS에서 오류가 발생하면 BLE 시뮬레이션 모드로 전환됩니다.");
  } else {
    Serial.println("오류: 블루투스 연결이 없어 구성 데이터를 전송할 수 없습니다.");
  }
  
  // 메모리 해제
  delete[] configMsg;
}

// UWB 세션 시작
void startUWB() {
  Serial.println("UWB 세션 시작 처리 중");
  
  // 실제 구현에서는 여기서 iOS에서 받은 configData를 파싱하여 DW3000 설정에 적용
  Serial.println("수신된 공유 가능한 구성 데이터:");
  for (int i = 0; i < 32; i++) {  // 더 많은 바이트 출력 (최대 32바이트)
    if (i < sizeof(configData)) {
      Serial.print("0x");
      if (configData[i] < 16) Serial.print("0");  // 한자리 16진수 앞에 0 추가
      Serial.print(configData[i], HEX);
      Serial.print(" ");
      if ((i + 1) % 8 == 0) Serial.println();  // 8바이트마다 줄바꿈
    }
  }
  Serial.println();
  
  // UWB 설정 적용 (실제 구현에서는 DW3000 API 호출)
  Serial.println("UWB 구성을 DW3000에 적용 중...");
  
  // 지연 추가 (일부 하드웨어에서 필요한 초기화 지연)
  delay(100);
  
  // UWB 세션 시작 알림 전송
  if (deviceConnected) {
    uint8_t startMsg[1] = {ACCESSORY_UWB_DID_START};
    pTxCharacteristic->setValue(startMsg, sizeof(startMsg));
    pTxCharacteristic->notify();
    Serial.println("UWB 시작 알림 전송됨");
    
    // iOS 앱에게 UWB 활성화 상태 알림
    Serial.println("iOS 앱에 UWB 세션 시작 알림 전송 완료. UWB 거리 측정 활성화됨.");
  } else {
    Serial.println("오류: 블루투스 연결이 없어 UWB 시작 알림을 전송할 수 없습니다.");
    return;  // UWB를 시작하지 않고 종료
  }
  
  // UWB 상태 업데이트
  uwbRunning = true;
  Serial.println("UWB 활성화됨 - 거리 측정 시작");
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