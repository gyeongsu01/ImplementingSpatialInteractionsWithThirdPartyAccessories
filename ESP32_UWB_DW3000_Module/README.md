# ESP32-UWB-DW3000 모듈 펌웨어 및 설정 가이드

이 폴더에는 ESP32와 DW3000 UWB 모듈을 사용하여 U1 칩이 탑재된 아이폰과 통신하기 위한 펌웨어가 포함되어 있습니다.

## 컴파일 시 문제 해결

ESP32 Arduino Core 버전 3.2.0 이상에서는 BLE 라이브러리 관련 호환성 문제가 발생할 수 있습니다. 이를 해결하기 위해 다음 두 파일을 제공합니다:

1. `ESP32_UWB_DW3000_Module.ino`: 메인 펌웨어 코드
2. `ESP32_BLE_Compatibility.h`: BLE 호환성 패치 헤더 파일

## 하드웨어 준비

ESP32와 DW3000 모듈 연결:

| ESP32 핀 | DW3000 핀 |
|----------|-----------|
| GPIO 18  | SCK       |
| GPIO 19  | MISO      |
| GPIO 23  | MOSI      |
| GPIO 4   | CS/SS     |
| GPIO 27  | RST       |
| GPIO 34  | IRQ       |
| 3.3V     | VCC       |
| GND      | GND       |

## 라이브러리 요구사항

이 코드를 실행하려면 다음 라이브러리가 필요합니다:

1. ESP32 Arduino Core - [설치 안내](https://github.com/espressif/arduino-esp32)
2. DW3000 라이브러리 - 현재 개발 중이므로 아래 방법으로 설치
   - [DW3000-Arduino 라이브러리](https://github.com/thotro/arduino-dw1000)를 수정한 버전 또는
   - [Qorvo의 공식 DWM3000 API](https://www.qorvo.com/products/p/DWM3000)

## 코드 설명

이 펌웨어는 다음 세 가지 주요 구성 요소로 이루어져 있습니다:

1. **블루투스 통신 모듈**
   - 아이폰과의 초기 연결 설정
   - 메시지 교환을 위한 서비스 및 특성 설정
   - 연결 상태 관리

2. **UWB 통신 모듈**
   - DW3000 칩 초기화 및 설정
   - 거리 측정 기능 구현
   - UWB 세션 관리

3. **메시지 프로토콜**
   - iOS 앱과 액세서리 간의 통신을 위한 메시지 형식 정의
   - 초기화, 구성, 시작, 중지 등의 명령 처리

## 테스트 준비

1. Arduino IDE에서 보드를 ESP32로 설정
2. 필요한 라이브러리 설치
3. 코드 업로드
4. 시리얼 모니터 열기 (115200 baud)

## 문제 해결

1. **컴파일 오류**
   - ESP32 Arduino Core 버전 확인 (3.2.0 이상인 경우 호환성 문제가 발생할 수 있음)
   - ESP32_BLE_Compatibility.h 파일이 동일 폴더에 있는지 확인
   - DW3000 라이브러리가 올바르게 설치되었는지 확인

2. **블루투스 연결 문제**
   - 아이폰의 블루투스가 켜져 있는지 확인
   - NINearbyAccessorySample 앱이 올바르게 실행 중인지 확인

3. **UWB 초기화 오류**
   - SPI 연결 및 핀 설정 확인
   - DW3000 모듈에 전원이 제대로 공급되는지 확인

## 추가 개발 정보

실제 제품 개발 시에는 다음 사항을 고려하세요:

1. **전력 관리**: 배터리 효율성을 위한 저전력 모드 구현
2. **보안**: 통신 보안 및 데이터 암호화 구현
3. **안정성**: 오류 처리 및 복구 메커니즘 강화

## 참고 자료

- [NearbyInteraction 프레임워크 문서](https://developer.apple.com/documentation/nearbyinteraction)
- [DW3000 데이터시트](https://www.qorvo.com/products/p/DW3000)
- [ESP32 Bluetooth 프로그래밍 가이드](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/index.html)