/* =========================================================================
   esp32-server-test.ino
   ทดสอบส่งค่า "ปลอม" จาก ESP32 ขึ้น server อาจารย์ (PHP+MySQL) ผ่าน HTTP POST
   ใช้คู่กับ 4-server-backup/api/save.php

   ต้องติดตั้ง: ไม่ต้อง (ใช้ HTTPClient/WiFi ที่มากับ ESP32 core อยู่แล้ว)

   ขั้นตอน:
     1) แก้ WIFI_SSID / WIFI_PASS
     2) แก้ SERVER_URL ให้เป็น URL ของ save.php บน server อาจารย์
     3) แก้ API_KEY ให้ตรงกับใน config.php
     4) อัปโหลด แล้วเปิด Serial Monitor 115200
   หมายเหตุ: ใช้ WiFi ทดสอบก่อน พอวงจรครบค่อยเปลี่ยนไปโมดูล 4G ทีหลัง
   ========================================================================= */

#include <WiFi.h>
#include <HTTPClient.h>

// ---------- แก้ค่าตรงนี้ ----------
#define WIFI_SSID   "ชื่อ_WiFi"
#define WIFI_PASS   "รหัส_WiFi"
#define SERVER_URL  "https://server-อาจารย์.ac.th/buoy/api/save.php"
#define API_KEY     "buoy-secret-2026"     // ต้องตรงกับ config.php
#define DEVICE_ID   "buoy-01"
// ----------------------------------

unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("กำลังต่อ WiFi");
  while (WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(300); }
  Serial.println("\nต่อ WiFi สำเร็จ IP: " + WiFi.localIP().toString());
}

float rnd(float lo, float hi) { return lo + (random(0, 1000) / 1000.0) * (hi - lo); }

void loop() {
  if (WiFi.status() == WL_CONNECTED && (millis() - lastSend > 15000)) {   // ทุก 15 วิ (เท่าทุ่นจริง)
    lastSend = millis();

    // สร้าง URL แบบ GET query (ง่ายและ save.php รองรับ)
    // ส่งครบ 7 ค่าเหมือนทุ่นจริง (DO, DO%, อุณหภูมิ, pH, ความเค็ม, EC, ความขุ่น)
    String url = String(SERVER_URL) + "?key=" + API_KEY + "&device=" + DEVICE_ID
      + "&sensor_ok=1"
      + "&do_val=" + String(rnd(4, 9), 2)
      + "&do_pct=" + String(rnd(70, 120), 1)
      + "&temp="   + String(rnd(26, 31), 2)
      + "&ph="     + String(rnd(7.5, 8.5), 2)
      + "&sal="    + String(rnd(28, 35), 2)
      + "&cond="   + String(rnd(40, 55), 2)
      + "&turb="   + String(rnd(1, 25), 2);

    HTTPClient http;
    http.begin(url);                 // ถ้าเป็น https จะใช้ TLS ให้อัตโนมัติ
    int code = http.GET();
    if (code > 0) {
      Serial.printf("HTTP %d : %s\n", code, http.getString().c_str());  // ควรได้ {"ok":true,...}
    } else {
      Serial.printf("ส่งไม่สำเร็จ: %s\n", http.errorToString(code).c_str());
    }
    http.end();
  }
}
