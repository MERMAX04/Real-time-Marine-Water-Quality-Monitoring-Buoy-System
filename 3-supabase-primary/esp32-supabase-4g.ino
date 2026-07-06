/* =========================================================================
   esp32-supabase-4g.ino
   ส่งค่าขึ้น Supabase ผ่าน "โมดูล 4G" (LilyGO T-A7670G) — ไม่ใช่ WiFi
   ใช้ TinyGSM คุยกับโมเด็ม A7670 แล้วยิง HTTPS POST เข้า Supabase REST

   📦 ต้องติดตั้งไลบรารี (Arduino IDE > Library Manager):
      - "TinyGSM"  โดย Volodymyr Shymanskyy   (แนะนำเวอร์ชันล่าสุด — รองรับ A7670)
        ถ้าตัวใน Library Manager เก่า ให้ลงจาก GitHub: github.com/vshymanskyy/TinyGSM
      - (ออปชัน) "StreamDebugger" ไว้ดู AT command ตอน debug

   🔌 ต่อสาย: ใช้บอร์ด LilyGO T-A7670G (ESP32+โมเด็ม+ซิม+GPS ในตัว) เสียบเสา LTE + GPS + ซิม
   ⚡ ไฟ: จ่ายจากพอร์ต USB หรือแบต LiPo/โซลาร์เข้าช่องของบอร์ด (โมเด็มกินไฟพีคสูง ต้องไฟแน่น)

   ทดสอบ: ส่งค่าปลอมทุก 15 วิ — ดู Serial Monitor 115200 (ควรได้ HTTP 201)
   ========================================================================= */

// ---- ต้องประกาศ "ก่อน" include TinyGSM ----
#define TINY_GSM_MODEM_A7670          // รุ่นโมเด็ม (A7670G ใช้ชุดคำสั่งนี้)
#define SerialAT       Serial1        // ESP32 คุยกับโมเด็มผ่าน Serial1
#define TINY_GSM_RX_BUFFER 1024
#include <TinyGsmClient.h>

// ===================== แก้ค่าตรงนี้ =====================
const char APN[]  = "internet";       // APN ของค่ายซิม: AIS/True = "internet", DTAC = "www.dtac.co.th"
const char GUSER[]= "";
const char GPASS[]= "";
#define SB_HOST  "jjbrgolulggksxnuicgg.supabase.co"     // host ของ Supabase (ไม่มี https://)
#define SB_ANON  "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImpqYnJnb2x1bGdna3N4bnVpY2dnIiwicm9sZSI6ImFub24iLCJpYXQiOjE3ODMyNjc0ODAsImV4cCI6MjA5ODg0MzQ4MH0.0coKRuYMMOwLq9yJFsOff99ya9RBwyLBzqxN2YU1JWg"
#define DEVICE_ID "buoy-01"

// ---- ขา LilyGO T-A7670 (ค่ามาตรฐาน; ถ้าต่อไม่ติดให้เทียบกับ utilities.h ของ LilyGO รุ่นบอร์ดจริง) ----
#define MODEM_BAUD    115200
#define PIN_TX        26      // ESP32 TX -> โมเด็ม RX
#define PIN_RX        27      // ESP32 RX <- โมเด็ม TX
#define PIN_PWRKEY    4       // ปุ่มเปิดโมเด็ม
#define PIN_POWERON   12      // จ่ายไฟภาคโมเด็ม (ต้อง HIGH)
#define PIN_RST       5       // reset
#define PIN_DTR       25
// ======================================================

TinyGsm        modem(SerialAT);
TinyGsmClientSecure client(modem);    // client แบบ SSL (สำหรับ HTTPS)

unsigned long lastSend = 0;
float rnd(float lo, float hi){ return lo + (random(0,1000)/1000.0)*(hi-lo); }

void modemPowerOn(){
  pinMode(PIN_POWERON, OUTPUT); digitalWrite(PIN_POWERON, HIGH);   // เปิดไฟภาคโมเด็ม
  pinMode(PIN_RST, OUTPUT);     digitalWrite(PIN_RST, HIGH);
  pinMode(PIN_PWRKEY, OUTPUT);
  digitalWrite(PIN_PWRKEY, LOW);  delay(100);
  digitalWrite(PIN_PWRKEY, HIGH); delay(1000);                     // กดปุ่ม PWRKEY ~1 วิ
  digitalWrite(PIN_PWRKEY, LOW);
}

bool connect4G(){
  Serial.print("รอสัญญาณเครือข่าย...");
  if(!modem.waitForNetwork(60000)){ Serial.println(" ❌ ไม่เจอเครือข่าย"); return false; }
  Serial.println(" ✅");
  Serial.print("ต่อ 4G (APN "); Serial.print(APN); Serial.print(")...");
  if(!modem.gprsConnect(APN, GUSER, GPASS)){ Serial.println(" ❌ ต่อไม่ได้ (เช็ค APN/ซิม/เน็ต)"); return false; }
  Serial.println(" ✅ ออนไลน์ IP=" + modem.getLocalIP());
  return true;
}

void setup(){
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32 + 4G (A7670) -> Supabase ===");
  modemPowerOn();
  SerialAT.begin(MODEM_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  delay(3000);
  Serial.print("เริ่มต้นโมเด็ม...");
  if(!modem.init()){ Serial.println(" ❌ ไม่ตอบ (เช็คขา/ไฟ/เสา)"); }
  else Serial.println(" ✅ " + modem.getModemName());
  client.setInsecure();          // ข้ามตรวจใบรับรอง TLS (พอสำหรับงานนี้)
  connect4G();
}

void loop(){
  if(millis() - lastSend < 15000) return;    // ส่งทุก 15 วิ (ผ่าน 4G อย่าถี่)
  lastSend = millis();

  if(!modem.isGprsConnected() && !connect4G()) return;

  // สร้าง JSON (คอลัมน์ตรงกับ probe ที่ sensor จริงอ่านได้ = ที่ Dashboard แสดง)
  String body = "{";
  body += "\"device\":\"" DEVICE_ID "\",";
  body += "\"do_val\":" + String(rnd(4,9),2)     + ",";
  body += "\"do_pct\":" + String(rnd(70,120),1)  + ",";
  body += "\"temp\":"   + String(rnd(26,31),2)   + ",";
  body += "\"ph\":"     + String(rnd(7.5,8.5),2) + ",";
  body += "\"sal\":"    + String(rnd(28,35),2)   + ",";
  body += "\"cond\":"   + String(rnd(40,55),2)   + ",";
  body += "\"tds\":"    + String(rnd(28,40),2)   + ",";
  body += "\"turb\":"   + String(rnd(1,25),2)    + "}";

  Serial.print("ต่อ Supabase (https)...");
  if(!client.connect(SB_HOST, 443)){ Serial.println(" ❌ ต่อไม่ได้"); return; }
  Serial.println(" ✅");

  // เขียน HTTP request เอง (POST /rest/v1/readings)
  client.print(F("POST /rest/v1/readings HTTP/1.1\r\n"));
  client.print(F("Host: " SB_HOST "\r\n"));
  client.print(F("apikey: " SB_ANON "\r\n"));
  client.print(F("Authorization: Bearer " SB_ANON "\r\n"));
  client.print(F("Content-Type: application/json\r\n"));
  client.print(F("Prefer: return=minimal\r\n"));
  client.print("Content-Length: " + String(body.length()) + "\r\n");
  client.print(F("Connection: close\r\n\r\n"));
  client.print(body);

  // อ่านบรรทัดแรกของ response (เช่น "HTTP/1.1 201 Created")
  unsigned long t0 = millis(); String status = "";
  while(millis()-t0 < 10000 && !client.available()) delay(10);
  if(client.available()) status = client.readStringUntil('\n');
  Serial.println("ตอบกลับ: " + status);
  if(status.indexOf("201") >= 0) Serial.println("✅ insert สำเร็จ : " + body);
  else Serial.println("⚠️ ไม่ใช่ 201 — 401/403=RLS/anon ผิด, 400=คอลัมน์ผิด, อื่นๆ=เน็ต/TLS");

  client.stop();
}

/* ---------------------------------------------------------------------------
   🎯 โบนัส GPS (A7670G มี GNSS ในตัว) — เอาพิกัดจริงมาโชว์ในการ์ด "ตำแหน่งทุ่น"
   วิธีเปิดใช้:
     1) ในตาราง Supabase เพิ่มคอลัมน์:  lat float8, lon float8
     2) เรียก readGPS(lat,lon) แล้วใส่ "lat":..., "lon":... ต่อท้าย body ก่อนปิด }
     3) ปรับ Dashboard ให้อ่าน lat/lon จาก row (แจ้งผมเดี๋ยวปรับให้)
   ---------------------------------------------------------------------------
bool readGPS(float &lat, float &lon){
  modem.enableGPS();
  for(int i=0;i<30;i++){                    // รอจับดาว ~ไม่เกิน 30 ครั้ง
    if(modem.getGPS(&lat,&lon)) { modem.disableGPS(); return true; }
    delay(2000);
  }
  modem.disableGPS(); return false;
}
--------------------------------------------------------------------------- */
