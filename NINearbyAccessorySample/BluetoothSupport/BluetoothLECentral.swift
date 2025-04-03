/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
블루투스를 사용하여 액세서리와의 연결 및 데이터 전송을 관리하는 클래스입니다.
*/

import Foundation
import CoreBluetooth
import os

// MARK: - 블루투스 통신 서비스 및 특성 UUID 정의
// 여기서 정의된 UUID는 ESP32-UWB-DW3000 모듈과의 통신에 사용됩니다.
// 아래 UUID들은 표준 Nordic UART 서비스(NUS) UUID를 사용합니다.
struct TransferService {
    // 서비스 UUID: 모든 통신의 기본 채널
    static let serviceUUID = CBUUID(string: "6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
    // RX 특성 UUID: 아이폰에서 ESP32로 데이터를 보내는 채널
    static let rxCharacteristicUUID = CBUUID(string: "6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
    // TX 특성 UUID: ESP32에서 아이폰으로 데이터를 받는 채널
    static let txCharacteristicUUID = CBUUID(string: "6E400003-B5A3-F393-E0A9-E50E24DCCA9E")
}

// MARK: - 블루투스 통신 오류 정의
enum BluetoothLECentralError: Error {
    case noPeripheral // 연결된 주변 장치가 없을 때 발생하는 오류
}

// MARK: - 데이터 통신 채널 클래스
// 이 클래스는 블루투스를 통한 데이터 송수신을 담당합니다.
class DataCommunicationChannel: NSObject {
    // 블루투스 중앙 관리자 - 블루투스 작업을 관리하는 핵심 객체
    var centralManager: CBCentralManager!

    // 발견된 주변 장치 정보
    var discoveredPeripheral: CBPeripheral?
    var discoveredPeripheralName: String?
    
    // 데이터 송수신에 사용되는 특성
    var rxCharacteristic: CBCharacteristic? // 데이터 송신용 (아이폰 -> ESP32)
    var txCharacteristic: CBCharacteristic? // 데이터 수신용 (ESP32 -> 아이폰)
    
    // 작업 완료 카운터
    var writeIterationsComplete = 0
    var connectionIterationsComplete = 0
    
    // 주변 장치 스캔 재시도 횟수
    // 앱의 테스트 사용 사례에 따라 이 값을 변경하세요.
    let defaultIterations = 5
    
    // 콜백 핸들러
    var accessoryDataHandler: ((Data, String) -> Void)? // 데이터 수신 시 호출됨
    var accessoryConnectedHandler: ((String) -> Void)? // 연결 성공 시 호출됨
    var accessoryDisconnectedHandler: (() -> Void)? // 연결 해제 시 호출됨
    
    // 블루투스 상태 관련 플래그
    var bluetoothReady = false
    var shouldStartWhenReady = false

    // 로깅을 위한 객체
    let logger = os.Logger(subsystem: "com.example.apple-samplecode.NINearbyAccessorySample", category: "DataChannel")

    // 초기화 메서드
    override init() {
        super.init()
        // 블루투스 중앙 관리자 초기화 및 위임자 설정
        centralManager = CBCentralManager(delegate: self, queue: nil, options: [CBCentralManagerOptionShowPowerAlertKey: true])
    }
    
    // 소멸자 - 스캐닝 중지
    deinit {
        centralManager.stopScan()
        logger.info("스캐닝 중지됨")
    }
    
    // 액세서리에 데이터 전송 메서드
    func sendData(_ data: Data) throws {
        if discoveredPeripheral == nil {
            throw BluetoothLECentralError.noPeripheral
        }
        writeData(data)
    }
    
    // 블루투스 스캔 시작 메서드
    func start() {
        if bluetoothReady {
            retrievePeripheral()
        } else {
            shouldStartWhenReady = true
        }
    }

    // MARK: - 헬퍼 메서드

    /*
     * 연결된 주변 장치를 확인하고, 없는 경우 서비스의 128비트 CBUUID를 사용하여
     * 주변 장치를 스캔합니다.
     */
    private func retrievePeripheral() {
        // 지정된 서비스 UUID로 이미 연결된 주변 장치 목록 가져오기
        let connectedPeripherals: [CBPeripheral] = (centralManager.retrieveConnectedPeripherals(withServices: [TransferService.serviceUUID]))

        logger.info("전송 서비스를 가진 연결된 주변 장치 발견: \(connectedPeripherals)")

        if let connectedPeripheral = connectedPeripherals.last {
            logger.info("주변 장치에 연결 중: \(connectedPeripheral)")
            self.discoveredPeripheral = connectedPeripheral
            centralManager.connect(connectedPeripheral, options: nil)
        } else {
            logger.info("연결된 장치 없음, 스캔 시작.")
            // 앱이 피어에 연결되어 있지 않으므로 주변 장치 스캔 시작
            centralManager.scanForPeripherals(withServices: [TransferService.serviceUUID],
                                          options: [CBCentralManagerScanOptionAllowDuplicatesKey: true])
        }
    }

    /*
     * 오류가 발생하거나 완료된 연결을 중지합니다. 참고: `didUpdateNotificationStateForCharacteristic`
     * 는 구독자가 있으면 연결을 취소합니다.
     */
    private func cleanup() {
        // 연결되어 있지 않으면 아무 작업도 수행하지 않음
        guard let discoveredPeripheral = discoveredPeripheral,
              case .connected = discoveredPeripheral.state else { return }

        // 알림 구독 중인 특성에 대해 구독 해제
        for service in (discoveredPeripheral.services ?? [] as [CBService]) {
            for characteristic in (service.characteristics ?? [] as [CBCharacteristic]) {
                if characteristic.uuid == TransferService.rxCharacteristicUUID && characteristic.isNotifying {
                    // 알림이 활성화된 경우 구독 해제
                    self.discoveredPeripheral?.setNotifyValue(false, for: characteristic)
                }
            }
        }

        // 구독자 없이 연결이 존재하는 경우 연결 해제만 수행
        centralManager.cancelPeripheralConnection(discoveredPeripheral)
    }
    
    // 주변 장치에 데이터 전송
    private func writeData(_ data: Data) {
        guard let discoveredPeripheral = discoveredPeripheral,
              let transferCharacteristic = rxCharacteristic
        else { return }

        // 최대 전송 가능 크기(MTU) 확인
        let mtu = discoveredPeripheral.maximumWriteValueLength(for: .withResponse)
        
        // 전송할 바이트 수를 MTU와 데이터 크기 중 작은 값으로 설정
        let bytesToCopy: size_t = min(mtu, data.count)
        
        // 데이터 패킷 준비
        var rawPacket = [UInt8](repeating: 0, count: bytesToCopy)
        data.copyBytes(to: &rawPacket, count: bytesToCopy)
        let packetData = Data(bytes: &rawPacket, count: bytesToCopy)

        // 디버깅을 위한 데이터 로깅
        let stringFromData = packetData.map { String(format: "0x%02x, ", $0) }.joined()
        logger.info("\(bytesToCopy) 바이트 쓰기: \(String(describing: stringFromData))")

        // 주변 장치로 데이터 전송
        discoveredPeripheral.writeValue(packetData, for: transferCharacteristic, type: .withResponse)
        
        // 쓰기 작업 완료 카운터 증가
        writeIterationsComplete += 1
    }
}

// MARK: - CBCentralManagerDelegate 구현
// 블루투스 중앙 관리자의 이벤트를 처리하는 확장
extension DataCommunicationChannel: CBCentralManagerDelegate {
    /*
     * 블루투스가 켜졌을 때 블루투스 작업을 시작합니다.
     *
     * 프로토콜은 `centralManagerDidUpdateState` 구현을 요구합니다.
     * 중앙 관리자의 상태가 `poweredOn`인지 확인하여 사용 가능한지 확인합니다.
     * 앱은 현재 기기가 블루투스 LE를 지원하는지 등의 다른 상태도 확인할 수 있습니다.
     */
    internal func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
            
        // 주변 장치와 통신 시작
        case .poweredOn:
            logger.info("블루투스 관리자 전원 켜짐")
            bluetoothReady = true
            if shouldStartWhenReady {
                start()
            }
        // 앱에서 필요에 따라 다음 상태를 처리하세요.
        case .poweredOff:
            logger.error("블루투스 관리자가 꺼져 있음")
            return
        case .resetting:
            logger.error("블루투스 관리자 재설정 중")
            return
        case .unauthorized:
            handleCBUnauthorized()
            return
        case .unknown:
            logger.error("블루투스 관리자 상태 알 수 없음")
            return
        case .unsupported:
            logger.error("이 기기에서는 블루투스가 지원되지 않음")
            return
        @unknown default:
            logger.error("이전에 알려지지 않은 중앙 관리자 상태 발생")
            return
        }
    }

    // 블루투스 제한의 다양한 원인에 대응
    internal func handleCBUnauthorized() {
        switch CBManager.authorization {
        case .denied:
            // 앱에서 사용자에게 설정으로 이동하여 권한을 변경하도록 안내하는 것을 고려하세요.
            logger.error("사용자가 블루투스 접근을 거부했습니다.")
        case .restricted:
            logger.error("블루투스가 제한됨")
        default:
            logger.error("예상치 못한 권한")
        }
    }

    // 전송 서비스 UUID 발견에 반응합니다.
    // 연결을 시도하기 전에 RSSI 값을 확인하는 것을 고려하세요.
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                    advertisementData: [String: Any], rssi RSSI: NSNumber) {
        logger.info("\( String(describing: peripheral.name)) 발견됨, 신호 강도: \(RSSI.intValue)")
        
        // 앱이 범위 내의 주변 장치를 인식하는지 확인합니다.
        if discoveredPeripheral != peripheral {
            
            // Core Bluetooth가 할당을 해제하지 않도록 주변 장치의 로컬 복사본을 저장합니다.
            discoveredPeripheral = peripheral
            
            // 주변 장치에 연결합니다.
            logger.info("주변 장치에 연결 중: \(peripheral)")
            
            // 광고 데이터에서 장치 이름을 가져옵니다.
            let name = advertisementData[CBAdvertisementDataLocalNameKey] as? String
            discoveredPeripheralName = name ?? "알 수 없음"
            centralManager.connect(peripheral, options: nil)
        }
    }

    // 연결 실패에 대응
    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        logger.error("\(peripheral)에 연결 실패. \( String(describing: error))")
        cleanup()
    }

    // 주변 장치 연결 후 'TransferService' 특성을 찾기 위한 서비스 및 특성 탐색
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        // 연결 성공 콜백 호출
        if let didConnectHandler = accessoryConnectedHandler {
            didConnectHandler(discoveredPeripheralName!)
        }
        
        logger.info("주변 장치 연결됨")
        
        // 스캔 중지
        centralManager.stopScan()
        logger.info("스캔 중지됨")
        
        // 반복 정보 설정
        connectionIterationsComplete += 1
        writeIterationsComplete = 0
        
        // 서비스 탐색을 위한 `CBPeripheral` 위임자 설정
        peripheral.delegate = self
        
        // 서비스 UUID와 일치하는 서비스만 검색
        peripheral.discoverServices([TransferService.serviceUUID])
    }

    // 연결 해제 후 주변 장치의 로컬 복사본 정리
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        logger.info("주변 장치 연결 해제됨")
        discoveredPeripheral = nil
        discoveredPeripheralName = nil
        
        // 연결 해제 콜백 호출
        if let didDisconnectHandler = accessoryDisconnectedHandler {
            didDisconnectHandler()
        }
        
        // 연결 해제 후 스캔 재개
        if connectionIterationsComplete < defaultIterations {
            retrievePeripheral()
        } else {
            logger.info("연결 반복 완료")
        }
    }
}

// MARK: - `CBPeripheralDelegate` 메서드 구현
// 주변 장치 이벤트를 처리하는 확장
extension DataCommunicationChannel: CBPeripheralDelegate {
    
    // 주변 장치 서비스 무효화에 대응
    func peripheral(_ peripheral: CBPeripheral, didModifyServices invalidatedServices: [CBService]) {
        // 전송 서비스가 무효화된 경우 서비스를 다시 발견
        for service in invalidatedServices where service.uuid == TransferService.serviceUUID {
            logger.error("전송 서비스가 무효화됨 - 서비스 재탐색")
            peripheral.discoverServices([TransferService.serviceUUID])
        }
    }

    // 주변 장치 서비스 발견에 대응
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error = error {
            logger.error("서비스 발견 오류: \(error.localizedDescription)")
            cleanup()
            return
        }
        logger.info("서비스 발견됨. 이제 특성 탐색 중")
        
        // 새로 채워진 주변 장치 서비스 배열에서 서비스 확인
        guard let peripheralServices = peripheral.services else { return }
        for service in peripheralServices {
            // 전송 서비스의 RX 및 TX 특성 발견
            peripheral.discoverCharacteristics([TransferService.rxCharacteristicUUID, TransferService.txCharacteristicUUID], for: service)
        }
    }

    // 발견된 특성을 구독하여 해당 특성이 포함하는 데이터를 원한다고 주변 장치에 알림
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        // 오류 처리
        if let error = error {
            logger.error("특성 발견 오류: \(error.localizedDescription)")
            cleanup()
            return
        }

        // 새로 채워진 주변 장치 서비스 배열에서 서비스 확인
        guard let serviceCharacteristics = service.characteristics else { return }
        
        // RX 특성 발견 및 구독
        for characteristic in serviceCharacteristics where characteristic.uuid == TransferService.rxCharacteristicUUID {
            rxCharacteristic = characteristic
            logger.info("특성 발견됨: \(characteristic)")
            peripheral.setNotifyValue(true, for: characteristic)
        }

        // TX 특성 발견 및 구독
        for characteristic in serviceCharacteristics where characteristic.uuid == TransferService.txCharacteristicUUID {
            txCharacteristic = characteristic
            logger.info("특성 발견됨: \(characteristic)")
            peripheral.setNotifyValue(true, for: characteristic)
        }

        // 주변 장치가 데이터를 보낼 때까지 대기
    }

    // 특성 알림을 통한 데이터 도착에 대응
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        // 주변 장치가 오류를 보고했는지 확인
        if let error = error {
            logger.error("특성 발견 오류:\(error.localizedDescription)")
            cleanup()
            return
        }
        guard let characteristicData = characteristic.value else { return }
    
        // 디버깅을 위한 데이터 로깅
        let str = characteristicData.map { String(format: "0x%02x, ", $0) }.joined()
        logger.info("\(characteristicData.count) 바이트 수신: \(str)")
        
        // 데이터 수신 콜백 호출
        if let dataHandler = self.accessoryDataHandler, let accessoryName = discoveredPeripheralName {
            dataHandler(characteristicData, accessoryName)
        }
    }

    // 구독 상태에 대응
    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        // 주변 장치가 오류를 보고했는지 확인
        if let error = error {
            logger.error("알림 상태 변경 오류: \(error.localizedDescription)")
            return
        }

        if characteristic.isNotifying {
            // 알림이 시작되었음을 나타냄
            logger.info("\(characteristic)에서 알림 시작됨")
        } else {
            // 알림이 중지되었으므로 주변 장치와의 연결 해제
            logger.info("\(characteristic)에서 알림 중지됨. 연결 해제 중")
            cleanup()
        }
    }
}