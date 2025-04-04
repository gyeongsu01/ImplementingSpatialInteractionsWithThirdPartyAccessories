/*
 * ESP32 BLE 호환성 패치 헤더 파일
 * 
 * 이 헤더 파일은 ESP32 Arduino Core 3.2.0 이상에서 발생할 수 있는
 * BLE 라이브러리 관련 호환성 문제를 해결하기 위한 코드를 제공합니다.
 * 
 * ESP32_UWB_DW3000_Module.ino에 의해 포함됩니다.
 */

#ifndef ESP32_BLE_COMPATIBILITY_H
#define ESP32_BLE_COMPATIBILITY_H

// BLE에서 문자열 데이터 처리 도우미 함수
// 문자열을 바이트 배열로 변환하는 함수
void stringToByteArray(const String &str, uint8_t *byteArray, size_t maxLength) {
  size_t copyLength = min(str.length(), maxLength);
  for (size_t i = 0; i < copyLength; i++) {
    byteArray[i] = (uint8_t)str[i];
  }
}

// 바이트 배열을 16진수 문자열로 변환하는 함수
String byteArrayToHexString(const uint8_t *byteArray, size_t length) {
  String result = "";
  for (size_t i = 0; i < length; i++) {
    if (byteArray[i] < 16) result += "0";
    result += String(byteArray[i], HEX);
    result += " ";
  }
  return result;
}

// BLE 핸들러 메시지 로깅 도우미 함수
void logBLEMessage(const char *prefix, const uint8_t *data, size_t length) {
  String hexStr = byteArrayToHexString(data, length);
  Serial.print(prefix);
  Serial.print(hexStr);
  Serial.println();
}

// 구조체 데이터를 바이트 배열로 변환하는 도우미 함수
template <typename T>
void structToByteArray(const T &data, uint8_t *byteArray) {
  memcpy(byteArray, &data, sizeof(T));
}

#endif // ESP32_BLE_COMPATIBILITY_H