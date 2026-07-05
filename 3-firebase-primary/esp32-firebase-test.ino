/* =========================================================================
   esp32-firebase-test.ino
   ทดสอบส่งค่า "ปลอม" จาก ESP32 ขึ้น Firebase เพื่อพิสูจน์ว่าทั้งวงจรทำงาน
   (ESP32 -> Firebase -> Dashboard) โดยยังไม่ต้องต่อ sensor จริง

   ต้องติดตั้งไลบรารีก่อน (Arduino IDE > Library Manager):
     - "Firebase Arduino Client Library for ESP8266 and ESP32"  โดย Mobizt

   ขั้นตอน:
     1) แก้ WIFI_SSID / WIFI_PASS ให้เป็นของคุณ
     2) แก้ API_KEY / DATABASE_URL ให้ตรงกับ Firebase project (ดู 3-firebase-primary/README.md)
     3) อัปโหลดลง ESP32 แล้วเปิด Serial Monitor 115200
     4) เปิด Dashboard (mode = 'firebase') ค่าจะขยับทุก 5 วิ
   หมายเหตุ: ตอนนี้ใช้ WiFi (ง่ายกว่า) พอวงจรครบค่อยเปลี่ยนไปโมดูล 4G ทีหลัง
   ========================================================================= */

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ---------- แก้ค่าตรงนี้ ----------
#define WIFI_SSID     "ชื่อ_WiFi"
#define WIFI_PASS     "รหัส_WiFi"
#define API_KEY       "AIza..."                 // จาก Firebase config
#define DATABASE_URL  "https://buoy-monitor-default-rtdb.asia-southeast1.firebasedatabase.app"
#define DEVICE_ID     "buoy-01"
// ----------------------------------

FirebaseData   fbdo;
FirebaseAuth   auth;
FirebaseConfig config;

unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);

  // ต่อ WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("กำลังต่อ WiFi");
  while (WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(300); }
  Serial.println("\nต่อ WiFi สำเร็จ IP: " + WiFi.localIP().toString());

  // ต่อ Firebase (ใช้ anonymous sign-in — ใช้ได้ตอน test mode)
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) Serial.println("Firebase: sign-in สำเร็จ");
  else Serial.printf("Firebase sign-in error: %s\n", config.signer.signupError.message.c_str());

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

// สุ่มค่าปลอมให้ดูสมจริง (แทนที่ด้วยค่าจริงจาก sensor ทีหลัง)
float rnd(float lo, float hi) { return lo + (random(0, 1000) / 1000.0) * (hi - lo); }

void loop() {
  if (Firebase.ready() && (millis() - lastSend > 5000)) {   // ส่งทุก 5 วิ
    lastSend = millis();

    FirebaseJson json;
    json.set("do",    rnd(4, 9));
    json.set("sal",   rnd(28, 35));
    json.set("turb",  rnd(1, 25));
    json.set("chl",   rnd(0.5, 12));
    json.set("orp",   rnd(150, 400));
    json.set("oil",   rnd(0, 30));
    json.set("algae", rnd(0.2, 15));
    json.set("ts/.sv", "timestamp");   // ให้ Firebase ใส่เวลา server ให้

    String pathLatest  = String(DEVICE_ID) + "/latest";
    String pathHistory = String(DEVICE_ID) + "/history";

    // 1) เขียนค่าล่าสุด (Dashboard ฟังตรงนี้)
    if (Firebase.RTDB.setJSON(&fbdo, pathLatest.c_str(), &json))
      Serial.println("ส่งค่าล่าสุดสำเร็จ");
    else
      Serial.println("ผิดพลาด: " + fbdo.errorReason());

    // 2) เก็บลงประวัติ (push = เพิ่มต่อท้าย)
    Firebase.RTDB.pushJSON(&fbdo, pathHistory.c_str(), &json);
  }
}
