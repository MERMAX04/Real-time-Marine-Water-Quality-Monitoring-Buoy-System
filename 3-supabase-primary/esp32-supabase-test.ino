/* =========================================================================
   esp32-supabase-test.ino
   ทดสอบส่งค่า "ปลอม" จาก ESP32 ขึ้น Supabase (REST API) เพื่อพิสูจน์วงจร
   ESP32 -> Supabase -> Dashboard  โดยยังไม่ต้องต่อ sensor จริง

   ไม่ต้องลงไลบรารีพิเศษ! ใช้ WiFiClientSecure + HTTPClient ที่มากับ ESP32 core

   ขั้นตอน:
     1) รัน schema.sql ใน Supabase ก่อน (สร้างตาราง + RLS + realtime)
     2) แก้ WIFI_SSID / WIFI_PASS
     3) แก้ SUPABASE_URL + SUPABASE_ANON (จาก Project Settings -> API)
     4) อัปโหลด แล้วเปิด Serial Monitor 115200 (ควรได้ HTTP 201 = insert สำเร็จ)
     5) เปิด Dashboard (mode='supabase') ค่าจะขยับทุก 5 วิ
   หมายเหตุ: ใช้ WiFi ทดสอบก่อน พอวงจรครบค่อยเปลี่ยนเป็นโมดูล 4G
   ========================================================================= */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ---------- แก้ค่าตรงนี้ ----------
#define WIFI_SSID     "ชื่อ_WiFi"
#define WIFI_PASS     "รหัส_WiFi"
#define SUPABASE_URL  "https://ใส่_PROJECT_REF.supabase.co"   // จาก Project Settings > API
#define SUPABASE_ANON "ใส่_ANON_PUBLIC_KEY"                    // (public key, ยาวขึ้นต้น eyJ...)
#define DEVICE_ID     "buoy-01"
// ----------------------------------

unsigned long lastSend = 0;
float rnd(float lo, float hi){ return lo + (random(0,1000)/1000.0)*(hi-lo); }

void setup(){
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("กำลังต่อ WiFi");
  while (WiFi.status()!=WL_CONNECTED){ Serial.print("."); delay(300); }
  Serial.println("\nต่อ WiFi สำเร็จ IP: " + WiFi.localIP().toString());
}

void loop(){
  if (WiFi.status()==WL_CONNECTED && millis()-lastSend > 5000){   // ทุก 5 วิ
    lastSend = millis();

    // สร้าง JSON (หน่วยตรงกับ Dashboard: oil=ppm, algae=cells/mL)
    String body = "{";
    body += "\"device\":\"" DEVICE_ID "\",";
    body += "\"do_val\":" + String(rnd(4,9),2)   + ",";
    body += "\"sal\":"    + String(rnd(28,35),2) + ",";
    body += "\"turb\":"   + String(rnd(1,25),2)  + ",";
    body += "\"chl\":"    + String(rnd(0.5,12),2)+ ",";
    body += "\"orp\":"    + String(rnd(150,400),0)+ ",";
    body += "\"oil\":"    + String(rnd(0,3),2)   + ",";
    body += "\"algae\":"  + String(rnd(0,20000),0) + "}";

    WiFiClientSecure client;
    client.setInsecure();                 // ข้ามตรวจใบรับรอง (พอสำหรับงานนี้)

    HTTPClient https;
    https.begin(client, String(SUPABASE_URL) + "/rest/v1/readings");
    https.addHeader("apikey", SUPABASE_ANON);
    https.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON);
    https.addHeader("Content-Type", "application/json");
    https.addHeader("Prefer", "return=minimal");

    int code = https.POST(body);
    if (code == 201) Serial.println("insert สำเร็จ (201) : " + body);
    else Serial.printf("ผิดพลาด HTTP %d : %s\n", code, https.getString().c_str());
    // 401/403 = RLS/anon key ผิด (ดู schema.sql ว่ารัน policy แล้วยัง)
    https.end();
  }
}
