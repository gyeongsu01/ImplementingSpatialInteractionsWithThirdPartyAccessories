/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
NearbyInteraction 액세서리 사용자 경험을 제공하는 뷰 컨트롤러입니다.
*/

import UIKit
import NearbyInteraction
import os.log

// MARK: - 메시지 프로토콜 정의
// 앱과 액세서리 간의 통신을 위한 예제 메시징 프로토콜입니다.
// 앱에서는 이 열거형을 앱의 사용자 경험에 맞게 수정하거나 확장하고,
// 액세서리를 그에 맞게 구성하세요.
enum MessageId: UInt8 {
    // 액세서리에서 오는 메시지
    case accessoryConfigurationData = 0x1 // 액세서리 구성 데이터
    case accessoryUwbDidStart = 0x2      // UWB 세션 시작 알림
    case accessoryUwbDidStop = 0x3       // UWB 세션 중지 알림
    
    // 액세서리로 보내는 메시지
    case initialize = 0xA                // 초기화 요청
    case configureAndStart = 0xB         // 구성 및 시작 명령
    case stop = 0xC                      // 중지 명령
}

// MARK: - 액세서리 데모 뷰 컨트롤러
class AccessoryDemoViewController: UIViewController {
    // 블루투스 통신 채널
    var dataChannel = DataCommunicationChannel()
    
    // NearbyInteraction 세션 - U1 칩을 사용한 UWB 거리 측정 관리
    var niSession = NISession()
    var configuration: NINearbyAccessoryConfiguration?
    
    // 액세서리 연결 상태
    var accessoryConnected = false
    var connectedAccessoryName: String?
    
    // 토큰에서 이름으로의 매핑 (여러 액세서리 지원용)
    var accessoryMap = [NIDiscoveryToken: String]()

    // 로깅을 위한 객체
    let logger = os.Logger(subsystem: "com.example.apple-samplecode.NINearbyAccessorySample", category: "AccessoryDemoViewController")

    // UI 요소
    @IBOutlet weak var connectionStateLabel: UILabel! // 연결 상태 표시
    @IBOutlet weak var uwbStateLabel: UILabel!       // UWB 상태 표시
    @IBOutlet weak var infoLabel: UILabel!           // 정보 메시지 표시
    @IBOutlet weak var distanceLabel: UILabel!       // 거리 측정값 표시
    @IBOutlet weak var actionButton: UIButton!       // 액션 버튼
    
    // MARK: - 뷰 생명주기
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        // NearbyInteraction 프레임워크의 세션 업데이트를 위한 위임자 설정
        niSession.delegate = self
        
        // 데이터 통신 채널 준비
        dataChannel.accessoryConnectedHandler = accessoryConnected
        dataChannel.accessoryDisconnectedHandler = accessoryDisconnected
        dataChannel.accessoryDataHandler = accessorySharedData
        dataChannel.start()
        
        // 초기 상태 설정
        updateInfoLabel(with: "액세서리를 스캔 중입니다")
    }
    
    // MARK: - 사용자 인터렉션
    
    // 액션 버튼 터치 시 실행되는 메서드
    @IBAction func buttonAction(_ sender: Any) {
        updateInfoLabel(with: "액세서리에서 구성 데이터 요청 중")
        // 초기화 메시지 전송
        let msg = Data([MessageId.initialize.rawValue])
        sendDataToAccessory(msg)
    }
    
    // MARK: - 데이터 채널 메서드
    
    // 추가 메시지 ID (BLE 시뮬레이션에서 사용)
    private enum SimulatedMessageType: UInt8 {
        case distanceData = 0xD0    // 거리 데이터 메시지
    }
    
    // BLE를 통한 거리 데이터 구조체
    private struct DistanceData {
        var messageType: UInt8      // 메시지 유형 (0xD0)
        var distance: Float         // 거리 (미터)
        var signalStrength: Int8    // 신호 강도 (dBm)
        var status: UInt8           // 상태 (0: 정상, 1: 오류)
    }
    
    // 액세서리에서 공유된 데이터 처리
    func accessorySharedData(data: Data, accessoryName: String) {
        // 각 메시지는 식별자 바이트로 시작합니다.
        // 메시지 길이가 유효 범위 내에 있는지 확인합니다.
        if data.count < 1 {
            updateInfoLabel(with: "액세서리 공유 데이터 길이가 1보다 작습니다.")
            return
        }
        
        // 첫 번째 바이트 확인
        let firstByte = data.first!
        
        // 표준 MessageId인지 확인
        if let messageId = MessageId(rawValue: firstByte) {
            // 표준 메시지 처리
            switch messageId {
            case .accessoryConfigurationData:
                // 메시지 식별자를 건너뛰어 메시지 데이터에 접근합니다.
                assert(data.count > 1)
                let message = data.advanced(by: 1)
                setupAccessory(message, name: accessoryName)
            case .accessoryUwbDidStart:
                handleAccessoryUwbDidStart()
            case .accessoryUwbDidStop:
                handleAccessoryUwbDidStop()
            case .configureAndStart:
                fatalError("액세서리는 'configureAndStart'를 보내면 안됩니다.")
            case .initialize:
                fatalError("액세서리는 'initialize'를 보내면 안됩니다.")
            case .stop:
                fatalError("액세서리는 'stop'을 보내면 안됩니다.")
            }
        } 
        // 시뮬레이션 메시지인지 확인
        else if firstByte == SimulatedMessageType.distanceData.rawValue {
            // 거리 데이터 메시지 처리
            handleSimulatedDistanceData(data)
        }
        else {
            logger.error("\(firstByte)은(는) 알 수 없는 메시지 유형입니다.")
        }
    }
    
    // 시뮬레이션된 거리 데이터 처리
    private func handleSimulatedDistanceData(_ data: Data) {
        // 구조체 크기만큼의 데이터가 있는지 확인
        guard data.count >= MemoryLayout<DistanceData>.size else {
            updateInfoLabel(with: "거리 데이터 길이가 너무 짧습니다.")
            return
        }
        
        // 데이터를 DistanceData 구조체로 변환
        var distanceData = DistanceData(messageType: 0, distance: 0, signalStrength: 0, status: 0)
        data.withUnsafeBytes { rawBufferPointer in
            distanceData = rawBufferPointer.load(as: DistanceData.self)
        }
        
        // 상태 확인
        if distanceData.status != 0 {
            updateInfoLabel(with: "거리 측정 오류 (상태: \(distanceData.status))")
            return
        }
        
        // 거리 정보 업데이트
        let distanceStr = String(format: "'%@'까지의 거리: %.1f 미터 (BLE 시뮬레이션)", 
                                 connectedAccessoryName ?? "액세서리", 
                                 distanceData.distance)
        
        // 거리 레이블 업데이트
        self.distanceLabel.text = distanceStr
        self.distanceLabel.sizeToFit()
        
        // UWB가 꺼져있을 때만 정보 레이블 업데이트
        if uwbStateLabel.text == "꺼짐" {
            updateInfoLabel(with: "BLE 시뮬레이션 모드: 실시간 거리 \(String(format: "%.2f", distanceData.distance)) 미터")
        }
    }
    
    // 액세서리 연결 시 호출되는 메서드
    func accessoryConnected(name: String) {
        accessoryConnected = true
        connectedAccessoryName = name
        actionButton.isEnabled = true
        connectionStateLabel.text = "연결됨"
        updateInfoLabel(with: "'\(name)'에 연결됨")
    }
    
    // 액세서리 연결 해제 시 호출되는 메서드
    func accessoryDisconnected() {
        accessoryConnected = false
        actionButton.isEnabled = false
        connectedAccessoryName = nil
        connectionStateLabel.text = "연결 안됨"
        updateInfoLabel(with: "액세서리 연결 해제됨")
    }
    
    // MARK: - 액세서리 메시지 처리
    
    // 액세서리 설정 - 구성 데이터 수신 후 NI 세션 시작
    func setupAccessory(_ configData: Data, name: String) {
        updateInfoLabel(with: "'\(name)'에서 구성 데이터를 수신했습니다. 세션을 실행합니다.")
        
        // 디버깅을 위해 수신된 구성 데이터 로깅
        let dataString = configData.map { String(format: "0x%02x, ", $0) }.joined()
        logger.info("수신된 구성 데이터 (총 \(configData.count)바이트): \(dataString)")
        
        // 구성 형식 확인 (디버깅용)
        checkConfigurationFormat(configData)
        
        do {
            // 수신된 구성 데이터로 NINearbyAccessoryConfiguration 생성
            configuration = try NINearbyAccessoryConfiguration(data: configData)
            logger.info("NINearbyAccessoryConfiguration 생성 성공")
            
            // 이 액세서리의 업데이트를 상호 연관시키기 위해 토큰을 캐시합니다.
            cacheToken(configuration!.accessoryDiscoveryToken, accessoryName: name)
            
            // NI 세션 시작
            logger.info("NI 세션 시작 중...")
            niSession.run(configuration!)
            logger.info("NI 세션 실행 완료")
        } catch {
            // 수신 데이터가 잘못되었기 때문에 중지하고 문제를 표시합니다.
            let errorDescription = error.localizedDescription
            let errorInfo = (error as NSError).userInfo
            
            // 자세한 오류 정보 로깅
            logger.error("구성 생성 실패 오류: \(error)")
            logger.error("오류 세부정보: \(errorInfo)")
            
            // 사용자에게 오류 표시
            updateInfoLabel(with: "'\(name)'에 대한 NINearbyAccessoryConfiguration 생성 실패. 오류: \(errorDescription)")
            
            // BLE 시뮬레이션 모드 활성화 알림
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
                self.updateInfoLabel(with: "UWB 구성 실패. BLE 기반 시뮬레이션 모드로 전환됩니다.")
            }
            return
        }
    }
    
    // 구성 데이터 형식 체크 (디버깅용)
    private func checkConfigurationFormat(_ data: Data) {
        // 데이터 길이 확인
        guard data.count > 4 else {
            logger.error("구성 데이터가 너무 짧습니다: \(data.count) 바이트")
            return
        }
        
        // 첫 4바이트가 "UWB1" 매직 스트링인지 확인
        if data.count >= 4 {
            let firstFourBytes = data.prefix(4)
            if let magicString = String(data: firstFourBytes, encoding: .ascii) {
                logger.info("매직 스트링: \(magicString)")
                
                if magicString == "UWB1" {
                    logger.info("UWB1 형식 감지됨, 유효한 형식일 수 있음")
                }
            }
        }
        
        // 데이터 시작 부분에 다른 특징적인 패턴이 있는지 확인
        if data.count >= 2 {
            let firstTwoBytes = [data[0], data[1]]
            logger.info("첫 2바이트: 0x\(String(format: "%02X%02X", firstTwoBytes[0], firstTwoBytes[1]))")
            
            // FiRa 컨소시엄 ID 패턴 확인
            if data.count >= 20 {
                if data[18] == 0x0D && data[19] == 0xF0 {
                    logger.info("FiRa 블록 ID (0x0DF0) 감지됨, 위치 18에서")
                }
                
                // 또 다른 가능한 식별자 패턴 확인
                for i in 0..<data.count-1 {
                    if data[i] == 0x2D && data[i+1] == 0x01 {
                        logger.info("U2 프로토콜 ID (0x2D01) 감지됨, 위치 \(i)에서")
                    }
                }
            }
        }
    }
    
    // 액세서리 UWB 세션 시작 처리
    func handleAccessoryUwbDidStart() {
        updateInfoLabel(with: "액세서리 세션이 시작되었습니다.")
        actionButton.isEnabled = false
        self.uwbStateLabel.text = "켜짐"
    }
    
    // 액세서리 UWB 세션 중지 처리
    func handleAccessoryUwbDidStop() {
        updateInfoLabel(with: "액세서리 세션이 중지되었습니다.")
        if accessoryConnected {
            actionButton.isEnabled = true
        }
        self.uwbStateLabel.text = "꺼짐"
    }
}

// MARK: - NISessionDelegate 구현
// NearbyInteraction 세션 이벤트 처리
extension AccessoryDemoViewController: NISessionDelegate {

    // 공유 가능한 구성 데이터가 생성되었을 때 호출
    func session(_ session: NISession, didGenerateShareableConfigurationData shareableConfigurationData: Data, for object: NINearbyObject) {
        // 요청한 액세서리인지 확인
        guard object.discoveryToken == configuration?.accessoryDiscoveryToken else { return }
        
        // 액세서리에 보낼 메시지 준비
        var msg = Data([MessageId.configureAndStart.rawValue])
        msg.append(shareableConfigurationData)
        
        // 디버깅을 위한 데이터 로깅
        let str = msg.map { String(format: "0x%02x, ", $0) }.joined()
        logger.info("공유 가능한 구성 바이트 전송 중: \(str)")
        
        let accessoryName = accessoryMap[object.discoveryToken] ?? "알 수 없음"
        
        // 액세서리에 메시지 전송
        sendDataToAccessory(msg)
        updateInfoLabel(with: "'\(accessoryName)'에 공유 가능한 구성 데이터를 전송했습니다.")
    }
    
    // 주변 객체 업데이트 시 호출 - 거리 측정값 수신
    func session(_ session: NISession, didUpdate nearbyObjects: [NINearbyObject]) {
        guard let accessory = nearbyObjects.first else { return }
        guard let distance = accessory.distance else { return }
        guard let name = accessoryMap[accessory.discoveryToken] else { return }
        
        // 거리 정보 표시 업데이트
        self.distanceLabel.text = String(format: "'%@'까지의 거리: %0.1f 미터", name, distance)
        self.distanceLabel.sizeToFit()
    }
    
    // 주변 객체 제거 시 호출
    func session(_ session: NISession, didRemove nearbyObjects: [NINearbyObject], reason: NINearbyObject.RemovalReason) {
        // 피어 타임아웃 시에만 세션을 재시도합니다.
        guard reason == .timeout else { return }
        updateInfoLabel(with: "'\(self.connectedAccessoryName ?? "액세서리")'와의 세션이 시간 초과되었습니다.")
        
        // 세션은 하나의 액세서리로 실행됩니다.
        guard let accessory = nearbyObjects.first else { return }
        
        // 앱의 액세서리 상태를 지웁니다.
        accessoryMap.removeValue(forKey: accessory.discoveryToken)
        
        // 재시도 여부를 결정하기 위해 헬퍼 함수를 참조합니다.
        if shouldRetry(accessory) {
            sendDataToAccessory(Data([MessageId.stop.rawValue]))
            sendDataToAccessory(Data([MessageId.initialize.rawValue]))
        }
    }
    
    // 세션이 일시 중단될 때 호출
    func sessionWasSuspended(_ session: NISession) {
        updateInfoLabel(with: "세션이 일시 중단되었습니다.")
        let msg = Data([MessageId.stop.rawValue])
        sendDataToAccessory(msg)
    }
    
    // 세션 일시 중단이 끝났을 때 호출
    func sessionSuspensionEnded(_ session: NISession) {
        updateInfoLabel(with: "세션 일시 중단이 종료되었습니다.")
        // 일시 중단이 끝나면 액세서리와의 구성 절차를 다시 시작합니다.
        let msg = Data([MessageId.initialize.rawValue])
        sendDataToAccessory(msg)
    }
    
    // 세션이 무효화되었을 때 호출
    func session(_ session: NISession, didInvalidateWith error: Error) {
        switch error {
        case NIError.invalidConfiguration:
            // 예상 형식에 맞게 액세서리 데이터를 디버깅하세요.
            updateInfoLabel(with: "액세서리 구성 데이터가 잘못되었습니다. 디버깅 후 다시 시도하세요.")
        case NIError.userDidNotAllow:
            handleUserDidNotAllow()
        default:
            handleSessionInvalidation()
        }
    }
}

// MARK: - 헬퍼 메서드
extension AccessoryDemoViewController {
    // 정보 레이블 업데이트
    func updateInfoLabel(with text: String) {
        self.infoLabel.text = text
        self.distanceLabel.sizeToFit()
        logger.info("\(text)")
    }
    
    // 액세서리에 데이터 전송
    func sendDataToAccessory(_ data: Data) {
        do {
            try dataChannel.sendData(data)
        } catch {
            updateInfoLabel(with: "액세서리에 데이터 전송 실패: \(error)")
        }
    }
    
    // 세션 무효화 처리
    func handleSessionInvalidation() {
        updateInfoLabel(with: "세션이 무효화되었습니다. 재시작합니다.")
        // 액세서리에게 중지 요청
        sendDataToAccessory(Data([MessageId.stop.rawValue]))

        // 무효화된 세션을 새로운 세션으로 교체
        self.niSession = NISession()
        self.niSession.delegate = self

        // 액세서리에게 초기화 요청
        sendDataToAccessory(Data([MessageId.initialize.rawValue]))
    }
    
    // 세션 재시도 여부 결정
    func shouldRetry(_ accessory: NINearbyObject) -> Bool {
        if accessoryConnected {
            return true
        }
        return false
    }
    
    // 토큰과 액세서리 이름 매핑 저장
    func cacheToken(_ token: NIDiscoveryToken, accessoryName: String) {
        accessoryMap[token] = accessoryName
    }
    
    // 사용자 권한 거부 처리
    func handleUserDidNotAllow() {
        // iOS 15부터 설정에서 영구적인 접근 상태를 관리합니다.
        updateInfoLabel(with: "NearbyInteractions 접근이 필요합니다. 설정에서 NIAccessory에 대한 접근을 변경할 수 있습니다.")
        
        // 사용자에게 설정으로 이동하도록 요청하는 알림 생성
        let accessAlert = UIAlertController(title: "접근 필요",
                                          message: """
                                          NIAccessory는 이 샘플 앱에서 NearbyInteractions에 대한 접근이 필요합니다.
                                          설정에서 NearbyInteractions 접근을 변경하면 어떤 기능이 활성화될지 사용자에게 설명하기 위해
                                          이 문자열을 사용하세요.
                                          """,
                                          preferredStyle: .alert)
        accessAlert.addAction(UIAlertAction(title: "취소", style: .cancel, handler: nil))
        accessAlert.addAction(UIAlertAction(title: "설정으로 이동", style: .default, handler: {_ in
            // 사용자를 앱 설정으로 안내합니다.
            if let settingsURL = URL(string: UIApplication.openSettingsURLString) {
                UIApplication.shared.open(settingsURL, options: [:], completionHandler: nil)
            }
        }))

        // 접근 알림을 표시합니다.
        present(accessAlert, animated: true, completion: nil)
    }
}