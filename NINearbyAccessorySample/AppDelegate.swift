/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
애플리케이션 생명주기 이벤트에 응답하는 클래스입니다.
*/

import UIKit
import NearbyInteraction

@UIApplicationMain
class AppDelegate: UIResponder, UIApplicationDelegate {

    var window: UIWindow?

    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        // 기기가 NearbyInteraction을 지원하는지 확인합니다.
        var isSupported: Bool
        if #available(iOS 16.0, *) {
            // iOS 16 이상에서는 정밀 거리 측정 지원 여부를 확인합니다.
            isSupported = NISession.deviceCapabilities.supportsPreciseDistanceMeasurement
        } else {
            // 이전 iOS 버전에서는 일반적인 지원 여부를 확인합니다.
            isSupported = NISession.isSupported
        }
        
        // 지원되지 않는 기기인 경우 오류 메시지를 표시합니다.
        if !isSupported {
            print("지원되지 않는 기기입니다")
            // 기기가 NearbyInteraction을 지원하지 않는 경우 오류 메시지 뷰 컨트롤러를 표시합니다.
            let storyboard = UIStoryboard(name: "Main", bundle: nil)
            window?.rootViewController = storyboard.instantiateViewController(withIdentifier: "unsupportedDeviceMessage")
        }
        return true
    }
}