# ESP32-UWB-DW3000 모듈과 U1 칩 아이폰 통신 가이드

이 프로젝트는 ESP32-UWB-DW3000 모듈을 사용하여 U1 칩이 탑재된 아이폰과 UWB(Ultra Wide Band) 통신을 구현하는 방법을 설명합니다.

## 하드웨어 요구사항

- ESP32 개발 보드 (ESP32-WROOM, ESP32-WROVER 등)
- DW3000 UWB 모듈 (Decawave DW3000 기반)
- U1 칩이 탑재된 아이폰 (iPhone 11 이상)

## 소프트웨어 요구사항

- Arduino IDE
- ESP32 Arduino Core
- [DW3000 Arduino 라이브러리](https://github.com/thotro/arduino-dw1000) (DW3000용으로 수정 필요)
- iOS 앱 (NINearbyAccessorySample)

## 배선 가이드

ESP32와 DW3000 모듈의 연결:

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

## 작동 원리

1. **블루투스 연결 설정**:
   - ESP32는 블루투스 LE를 통해 아이폰과 연결됩니다.
   - 지정된 서비스 UUID와 특성 UUID를 사용하여 통신합니다.

2. **UWB 구성 교환**:
   - 초기화 요청 이후, ESP32는 구성 데이터를 아이폰에 전송합니다.
   - 아이폰은 이 데이터를 사용해 NINearbyAccessoryConfiguration을 생성합니다.
   - 아이폰은 공유 가능한 구성 데이터를 다시 ESP32로, UWB 세션을 시작합니다.

3. **거리 측정**:
   - UWB 세션이 활성화되면, 아이폰은 U1 칩을 사용해 ESP32-UWB-DW3000 모듈과의 거리를 측정합니다.
   - iOS 앱은 이 거리 정보를 사용자에게 표시합니다.

## 주의사항

- DW3000 라이브러리는 아직 개발 중일 수 있으므로, DW1000 라이브러리를 DW3000에 맞게 수정해야 할 수 있습니다.
- 정확한 구성 데이터 형식은 Apple의 NINearbyAccessoryConfiguration 사양을 따라야 합니다.
- UWB 통신은 라인 오브 사이트(Line of Sight)에서 가장 효과적입니다.

## 추가 개발 및 참조

- 더 정확한 거리 측정을 위해 DW3000의 TWR(Two-Way Ranging) 기능을 구현하세요.
- 배터리 효율성을 위한 전력 관리 기능을 추가하세요.
- 실제 환경에서 테스트하고 필요에 따라 안테나 위치를 조정하세요.

## 트러블슈팅

- 연결 문제가 발생하면 블루투스 설정과 UUID가 iOS 앱과 일치하는지 확인하세요.
- 거리 측정이 정확하지 않으면 DW3000 설정과 안테나 배치를 확인하세요.
- UWB 세션이 시작되지 않으면 구성 데이터 형식이 올바른지 확인하세요.