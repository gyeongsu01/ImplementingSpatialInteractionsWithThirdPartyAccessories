# ESP32-UWB-DW3000 모듈과 U1 칩 아이폰 통신 통합 가이드

이 가이드는 ESP32-UWB-DW3000 모듈과 U1 칩이 탑재된 아이폰 간의 UWB 통신을 설정하고 테스트하는 방법을 설명합니다.

## 준비 과정

### 하드웨어 준비
1. ESP32 개발 보드
2. DW3000 UWB 모듈
3. U1 칩이 탑재된 아이폰 (iPhone 11 이상)
4. 연결 케이블 및 점퍼 와이어

### 소프트웨어 준비
1. Arduino IDE
2. Xcode (최신 버전)
3. DW3000 라이브러리 (아래 설치 방법 참조)

## ESP32-UWB-DW3000 모듈 설정

### 1. 하드웨어 연결
ESP32_UWB_DW3000_Accessory_README.md 파일에 있는 배선 가이드를 따라 ESP32와 DW3000 모듈을 연결합니다.

### 2. Arduino 라이브러리 설치
아래 라이브러리를 Arduino IDE에 설치합니다:
- ESP32 Arduino Core
- BLE 라이브러리 (기본 포함)
- DW3000 라이브러리 (GitHub에서 다운로드하거나 아래 명령어로 설치)

```
git clone https://github.com/thotro/arduino-dw1000.git
```
* 참고: DW1000 라이브러리를 DW3000에 맞게 수정해야 할 수 있습니다.

### 3. 펌웨어 업로드
1. ESP32_UWB_DW3000_Module.ino 파일을 Arduino IDE에서 엽니다.
2. ESP32 보드 타입을 선택합니다 (예: ESP32 Dev Module).
3. 올바른 포트를 선택합니다.
4. Verify 버튼을 클릭하여 코드를 컴파일합니다.
5. Upload 버튼을 클릭하여 ESP32에 펌웨어를 업로드합니다.
6. 시리얼 모니터를 열어 (115200 baud) 디버그 메시지를 확인합니다.

## iOS 앱 설정

### 1. 프로젝트 빌드
1. Xcode에서 NINearbyAccessorySample.xcodeproj 프로젝트를 엽니다.
2. 개발자 계정을 설정합니다 (서명 필요).
3. 대상 기기로 U1 칩이 탑재된 아이폰을 선택합니다.
4. Build 및 Run을 클릭하여 앱을 설치하고 실행합니다.

### 2. 앱 권한 설정
앱이 처음 실행될 때:
1. Bluetooth 권한 요청을 허용합니다.
2. Nearby Interaction 권한 요청을 허용합니다.

## 통신 테스트

### 1. 연결 설정
1. ESP32를 전원에 연결하고 시리얼 모니터에서 "ESP32-UWB-DW3000 Module Starting..." 메시지를 확인합니다.
2. iOS 앱을 실행합니다.
3. 앱이 자동으로 ESP32-UWB-DW3000 기기를 스캔하고 발견하면 연결합니다.
4. 연결되면 앱 화면에 "Connected"가 표시되고 ESP32 시리얼 모니터에 "Device connected" 메시지가 표시됩니다.

### 2. UWB 세션 시작
1. iOS 앱에서 "Request Configuration" 버튼을 누릅니다.
2. ESP32는 구성 데이터를 생성하여 iOS 앱으로 전송합니다.
3. iOS 앱은 이 데이터를 처리하고 공유 가능한 구성 데이터를 다시 ESP32로 보냅니다.
4. ESP32는 UWB 세션을 시작하고 "UWB session started" 메시지를 전송합니다.
5. 이제 앱 화면에 "UWB: ON"이 표시되고 거리 측정이 시작됩니다.

### 3. 거리 측정 테스트
1. 앱은 ESP32-UWB-DW3000 모듈과의 거리를 지속적으로 측정하고 화면에 표시합니다.
2. 아이폰과 ESP32를 다양한 거리에 배치하여 측정이 정확한지 테스트합니다.
3. 거리 변화가 앱에 실시간으로 반영되는지 확인합니다.

## 문제 해결

### 블루투스 연결 문제
- ESP32와 아이폰이 가까이 있는지 확인합니다.
- ESP32를 재부팅합니다.
- 앱을 완전히 종료하고 다시 실행합니다.
- iOS 설정에서 Bluetooth를 껐다가 다시 켭니다.

### UWB 세션 시작 실패
- ESP32의 시리얼 로그를 확인하여 오류 메시지를 확인합니다.
- iOS 앱에 표시되는 정보 메시지를 확인합니다.
- 구성 데이터 형식이 올바른지 확인합니다.

### 거리 측정 부정확
- DW3000 안테나 위치와 방향을 조정합니다.
- 금속 물체나 전자파 간섭 소스에서 멀리 테스트합니다.
- ESP32 코드의 거리 측정 로직을 점검합니다.

## 개발 및 커스터마이징

### 펌웨어 수정
ESP32_UWB_DW3000_Module.ino 파일의 다음 부분을 수정하여 기능을 확장할 수 있습니다:
- `measureDistance()` 함수: 실제 DW3000 API를 사용한 정확한 거리 측정 구현
- 메시지 처리 로직: 추가 메시지 유형이나 데이터 포맷 지원
- 전력 관리: 배터리 수명 연장을 위한 절전 모드 추가

### iOS 앱 수정
AccessoryDemoViewController.swift 파일을 수정하여:
- UI 개선: 더 직관적인 거리 표시나 그래픽 추가
- 추가 기능: 방향 정보나 위치 추적 기능 구현
- 데이터 로깅: 거리 측정 데이터 저장 및 분석 기능 추가

## 추가 리소스
- [Apple NearbyInteraction 프레임워크 문서](https://developer.apple.com/documentation/nearbyinteraction)
- [DW3000 데이터시트 및 사용자 매뉴얼](https://www.qorvo.com/products/p/DW3000)
- [ESP32 Bluetooth 프로그래밍 가이드](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/index.html)

## 결론
이 가이드를 따라 설정하면 ESP32-UWB-DW3000 모듈과 U1 칩이 탑재된 아이폰 간의 UWB 통신이 가능해집니다. 이 기술은 정밀한 실내 위치 추적, 근접성 기반 애플리케이션, IoT 장치 제어 등 다양한 응용 분야에 활용될 수 있습니다.