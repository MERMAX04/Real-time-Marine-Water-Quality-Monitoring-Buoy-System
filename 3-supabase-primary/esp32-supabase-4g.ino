/* =========================================================================
   esp32-supabase-4g.ino
   บอร์ด: LilyGO T-Call-A7670 V1.0 (โมดูล A7670E) — ส่งค่าขึ้น Supabase ผ่าน 4G
   + อ่านพิกัด GPS + แจ้งเตือนวิกฤตเข้า Telegram (ทั้งหมดผ่าน HTTP-app ในตัวโมเด็ม)

   📦 ไลบรารี (Arduino IDE > Library Manager): "TinyGSM" โดย Volodymyr Shymanskyy
      - ใช้ macro TINY_GSM_MODEM_A7672X (ครอบคลุม A7670E) — ไลบรารีต้องอยู่ path อังกฤษ (C:\Arduino)

   🔌 ต่อ: เสา LTE→MAIN, เสา GPS→GPS, ซิม nano (ปิด PIN lock), APN=internet (AIS)
   ⚡ ไฟ: โมเด็มกินไฟพีคสูง — ควรเสียบแบต LiPo (USB อย่างเดียวอาจไม่พอตอนยิง 4G)
   🔑 HTTPS ใช้ได้เพราะเปิด SNI ใน SSL context (Supabase/Telegram อยู่หลัง Cloudflare)

   ทดสอบ: ส่งค่าปลอมทุก 15 วิ — ดู Serial Monitor 115200 (ควรได้ HTTP 201)
   ========================================================================= */

// ---- ต้องประกาศ "ก่อน" include TinyGSM ----
#define TINY_GSM_MODEM_A7672X         // TinyGSM ใช้ชื่อ A7672X ครอบคลุม A7670E (ชุดคำสั่งเดียวกัน)
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

// ===== Telegram Bot (แจ้งเตือนวิกฤต — ESP32 ยิงตรง) =====
// วิธีเอา token/chat_id: ดู 5-extensions/02-alerts-notification.md (ทัก @BotFather 5 นาที)
#define TG_ENABLE   1                     // 0 = ปิดแจ้งเตือน Telegram
#define TG_TOKEN    "ใส่_BOT_TOKEN"        // จาก @BotFather (รูปแบบ 123456789:ABCdef...)
#define TG_CHAT     "ใส่_CHAT_ID"          // chat id ของคุณ/กลุ่ม (ดูจาก getUpdates)
const unsigned long ALERT_COOLDOWN = 30UL*60UL*1000UL;  // กันเตือนซ้ำ 30 นาที/รายการ

// ---- ขา LilyGO T-Call-A7670 V1.0 (RX=25, RST=27 ต่างจากรุ่น T-A7670! อ้างอิง utilities.h ของ LilyGO) ----
#define MODEM_BAUD    115200
#define PIN_TX        26      // ESP32 TX -> โมเด็ม RX
#define PIN_RX        25      // ESP32 RX <- โมเด็ม TX  (V1.0 = 25)
#define PIN_PWRKEY    4       // ปุ่มเปิดโมเด็ม
#define PIN_RST       27      // reset (V1.0 = 27, active LOW)
// ======================================================

TinyGsm        modem(SerialAT);        // ส่ง HTTPS ผ่าน HTTP-app ในตัวโมเด็ม (ไม่ใช้ TLS socket)

unsigned long lastSend = 0;
float rnd(float lo, float hi){ return lo + (random(0,1000)/1000.0)*(hi-lo); }

void modemPowerOn(){
  // รีเซ็ตโมเด็ม (V1.0 reset = active LOW)
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, HIGH); delay(100);
  digitalWrite(PIN_RST, LOW);  delay(2600);   // assert reset
  digitalWrite(PIN_RST, HIGH);                // release
  // กดปุ่ม PWRKEY เปิดโมเด็ม (pulse 100 ms ตามสเปก LilyGO)
  pinMode(PIN_PWRKEY, OUTPUT);
  digitalWrite(PIN_PWRKEY, LOW);  delay(100);
  digitalWrite(PIN_PWRKEY, HIGH); delay(100);
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

// ================= Telegram แจ้งเตือน =================
// ส่งข้อความเข้า Telegram ผ่าน HTTP application ในตัวโมเด็ม (เสถียรกับ A7670E — ใช้ atCmd/waitFor ด้านล่าง)
bool sendTelegram(const String& text){
  String body = String("{\"chat_id\":\"") + TG_CHAT + "\",\"text\":\"" + text + "\"}";
  atCmd("AT+HTTPTERM", 800);
  if(atCmd("AT+HTTPINIT", 3000).indexOf("OK") < 0){ Serial.println("TG: HTTPINIT fail"); return false; }
  atCmd("AT+HTTPPARA=\"URL\",\"https://api.telegram.org/bot" TG_TOKEN "/sendMessage\"", 2000);
  atCmd("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 1500);
  atCmd("AT+HTTPPARA=\"SSLCFG\",0", 1500);                    // ใช้ SSL context 0 (เปิด SNI ไว้แล้วใน setup)
  while(SerialAT.available()) SerialAT.read();
  SerialAT.println("AT+HTTPDATA=" + String(body.length()) + ",10000");
  if(waitFor("DOWNLOAD", 3000).indexOf("DOWNLOAD") < 0){ atCmd("AT+HTTPTERM",800); return false; }
  SerialAT.print(body);
  waitFor("OK", 5000);
  while(SerialAT.available()) SerialAT.read();
  SerialAT.println("AT+HTTPACTION=1");
  String r = waitFor("+HTTPACTION:", 20000);
  atCmd("AT+HTTPTERM", 1500);
  bool ok = r.indexOf(",200,") >= 0;                          // Telegram ตอบ 200 = ส่งสำเร็จ
  Serial.println(ok ? "TG: ✅ ส่งแล้ว" : "TG: ⚠️ " + r + " (เช็ค BOT_TOKEN/CHAT_ID)");
  return ok;
}

// กันเตือนซ้ำ: แต่ละรายการ (index) ส่งซ้ำได้เมื่อพ้น cooldown
unsigned long lastAlert[5] = {0,0,0,0,0};
bool canAlert(int i){
  unsigned long now = millis();
  if(lastAlert[i]==0 || now - lastAlert[i] > ALERT_COOLDOWN){ lastAlert[i] = now; return true; }
  return false;
}

// ตรวจเกณฑ์วิกฤต (โฟกัสค่าที่อันตรายจริง) แล้วส่ง Telegram ทีเดียวรวมทุกข้อ
void checkAndAlert(float doVal, float ph, float temp, float turb){
#if TG_ENABLE
  String lines = "";                              // "\\n" = ขึ้นบรรทัดใหม่ใน Telegram
  if(doVal < 3.0  && canAlert(0)){ lines += "\\n🔴 ออกซิเจนละลายน้ำต่ำวิกฤต: "; lines += String(doVal,2); lines += " mg/L (สัตว์น้ำเสี่ยงตาย)"; }
  if(ph    < 7.0  && canAlert(1)){ lines += "\\n🔴 น้ำเป็นกรดผิดปกติ: pH ";      lines += String(ph,2); }
  if(ph    > 9.0  && canAlert(2)){ lines += "\\n🔴 น้ำเป็นด่างผิดปกติ: pH ";     lines += String(ph,2); }
  if(temp  > 33.0 && canAlert(3)){ lines += "\\n🟠 อุณหภูมิน้ำสูง: ";            lines += String(temp,1); lines += " °C"; }
  if(turb  > 40.0 && canAlert(4)){ lines += "\\n🟠 ความขุ่นสูงผิดปกติ: ";        lines += String(turb,1); lines += " NTU"; }
  if(lines.length() > 0){
    String msg = "🌊 แจ้งเตือนคุณภาพน้ำ (" DEVICE_ID ")";
    msg += lines;
    sendTelegram(msg);
  }
#endif
}

// ================= GPS (GNSS ผ่านคำสั่ง AT) =================
double gLat=1000, gLon=1000;   // 1000 = ยังไม่มีพิกัด (ยังไม่ล็อกดาว)

String atCmd(const String& cmd, uint32_t timeout=3000){
  while(SerialAT.available()) SerialAT.read();
  SerialAT.println(cmd);
  String r=""; uint32_t t0=millis();
  while(millis()-t0 < timeout){ while(SerialAT.available()) r += (char)SerialAT.read(); }
  return r;
}
String gpsField(const String& s, int idx){
  int start=0, count=0;
  for(int i=0;i<=(int)s.length();i++){
    if(i==(int)s.length() || s[i]==','){
      if(count==idx) return s.substring(start,i);
      count++; start=i+1;
    }
  }
  return "";
}
// อ่านพิกัด (A7670E คืนเป็นองศาทศนิยม) -> อัปเดต gLat/gLon ถ้าล็อกได้
bool readGPS(){
  String r = atCmd("AT+CGNSSINFO", 3000);
  int i = r.indexOf("+CGNSSINFO:");
  if(i < 0) return false;
  String line = r.substring(i + 11); line.trim();
  int nsIdx=-1, ewIdx=-1;
  for(int k=0;k<20;k++){ String f=gpsField(line,k);
    if(f=="N"||f=="S") nsIdx=k; else if(f=="E"||f=="W") ewIdx=k; }
  if(nsIdx < 1 || ewIdx < 1) return false;
  double la=gpsField(line,nsIdx-1).toDouble(); if(gpsField(line,nsIdx)=="S") la=-la;
  double lo=gpsField(line,ewIdx-1).toDouble(); if(gpsField(line,ewIdx)=="W") lo=-lo;
  if(la==0 && lo==0) return false;
  gLat=la; gLon=lo; return true;
}

// ================= HTTPS POST ด้วย "HTTP application ในตัวโมเด็ม" =================
// (A7670E ต่อ HTTPS ผ่าน TLS socket ไม่เสถียร — ใช้ AT+HTTP... เชื่อถือได้กว่า)
String waitFor(const char* token, uint32_t timeout){
  String r=""; uint32_t t0=millis();
  while(millis()-t0 < timeout){
    while(SerialAT.available()) r += (char)SerialAT.read();
    if(r.indexOf(token) >= 0) break;
  }
  return r;
}
int httpPost(const String& url, const String& body){
  atCmd("AT+HTTPTERM", 800);                                  // เคลียร์ session เก่า
  if(atCmd("AT+HTTPINIT", 3000).indexOf("OK") < 0){ Serial.print("(HTTPINIT fail)"); return -1; }
  atCmd("AT+HTTPPARA=\"URL\",\"" + url + "\"", 2000);
  atCmd("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 1500);
  atCmd("AT+HTTPPARA=\"SSLCFG\",0", 1500);                    // ใช้ SSL context 0
  atCmd("AT+HTTPPARA=\"USERDATA\",\"apikey: " SB_ANON "\"", 1500);
  atCmd("AT+HTTPPARA=\"USERDATA\",\"Authorization: Bearer " SB_ANON "\"", 1500);
  atCmd("AT+HTTPPARA=\"USERDATA\",\"Prefer: return=minimal\"", 1500);

  while(SerialAT.available()) SerialAT.read();
  SerialAT.println("AT+HTTPDATA=" + String(body.length()) + ",10000");
  if(waitFor("DOWNLOAD", 3000).indexOf("DOWNLOAD") < 0){ Serial.print("(no DOWNLOAD)"); atCmd("AT+HTTPTERM",800); return -2; }
  SerialAT.print(body);                                        // ส่ง payload
  waitFor("OK", 5000);

  while(SerialAT.available()) SerialAT.read();
  SerialAT.println("AT+HTTPACTION=1");                         // 1 = POST
  String r = waitFor("+HTTPACTION:", 25000);                  // รอ +HTTPACTION: 1,<code>,<len>
  int code=-1, p=r.indexOf("+HTTPACTION:");
  if(p >= 0){ int c1=r.indexOf(',',p), c2=r.indexOf(',',c1+1);
              if(c1>0 && c2>0) code = r.substring(c1+1,c2).toInt(); }
  atCmd("AT+HTTPTERM", 1500);
  return code;
}

void setup(){
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32 + 4G (A7670) -> Supabase ===");
  modemPowerOn();
  SerialAT.begin(MODEM_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  delay(3000);
  Serial.print("รอโมเด็มบูต + ทดสอบ AT ");
  int mretry=0;
  while(!modem.testAT(1000)){
    Serial.print(".");
    if(++mretry>15){ Serial.println("\n  ↻ กด PWRKEY ซ้ำ (ถ้ายังไม่ติด = ไฟไม่พอ ให้เสียบแบต)"); modemPowerOn(); mretry=0; }
  }
  Serial.println(" ✅ โมเด็มตอบแล้ว: " + modem.getModemName());
  Serial.print("เปิด GNSS...");            // เปิด GPS ทิ้งไว้ อ่านพิกัดตอนส่งแต่ละรอบ
  atCmd("AT+CGNSSPWR=1", 5000);
  Serial.println(" ✅");
  // ตั้ง SSL context 0: TLS + ไม่ตรวจใบรับรอง + ข้ามการเช็คเวลา (แก้ error 715 handshake)
  Serial.print("ตั้ง SSL context 0... ");
  atCmd("AT+CSSLCFG=\"sslversion\",0,4", 1500);      // 4 = รองรับทุกเวอร์ชัน TLS
  atCmd("AT+CSSLCFG=\"authmode\",0,0", 1500);        // 0 = ไม่ตรวจใบรับรองเซิร์ฟเวอร์
  atCmd("AT+CSSLCFG=\"ignorelocaltime\",0,1", 1500); // ข้ามการเช็ควันหมดอายุ
  String _sni = atCmd("AT+CSSLCFG=\"enableSNI\",0,1", 1500);        // ★ เปิด SNI (จำเป็นสำหรับ Cloudflare/Supabase)
  Serial.println(_sni.indexOf("OK") >= 0 ? "✅ (เปิด SNI แล้ว)" : ("⚠️ SNI ตอบ: " + _sni));
  connect4G();
}

void loop(){
  if(millis() - lastSend < 15000) return;    // ส่งทุก 15 วิ (ผ่าน 4G อย่าถี่)
  lastSend = millis();

  if(!modem.isGprsConnected() && !connect4G()) return;

  // อ่านค่าจาก sensor ตรงนี้ (ตอนนี้ยังเป็นค่าปลอมเพื่อทดสอบ — พอต่อ RS485 จริงค่อยแทน)
  float doVal=rnd(4,9), doPct=rnd(70,120), temp=rnd(26,31), ph=rnd(7.5,8.5);
  float sal=rnd(28,35), cond=rnd(40,55), tds=rnd(28,40), turb=rnd(1,25);

  if(readGPS()) Serial.println("GPS: " + String(gLat,6) + ", " + String(gLon,6));
  else          Serial.println("GPS: ยังไม่ล็อกพิกัด (เสาต้องเห็นฟ้า)");

  // สร้าง JSON (คอลัมน์ตรงกับ probe ที่ sensor จริงอ่านได้ = ที่ Dashboard แสดง)
  String body = "{";
  body += "\"device\":\"" DEVICE_ID "\",";
  body += "\"do_val\":" + String(doVal,2) + ",";
  body += "\"do_pct\":" + String(doPct,1) + ",";
  body += "\"temp\":"   + String(temp,2)  + ",";
  body += "\"ph\":"     + String(ph,2)    + ",";
  body += "\"sal\":"    + String(sal,2)   + ",";
  body += "\"cond\":"   + String(cond,2)  + ",";
  body += "\"tds\":"    + String(tds,2)   + ",";
  body += "\"turb\":"   + String(turb,2);
  if(gLat>=-90 && gLat<=90 && gLon>=-180 && gLon<=180)     // มีพิกัด GPS แล้วค่อยส่ง
    body += ",\"lat\":" + String(gLat,6) + ",\"lon\":" + String(gLon,6);
  body += "}";

  Serial.print("POST Supabase (HTTPS ในตัวโมเด็ม)...");
  int code = httpPost("https://" SB_HOST "/rest/v1/readings", body);
  if(code == 201 || code == 200) Serial.println(" ✅ insert สำเร็จ (" + String(code) + ")\n  " + body);
  else Serial.println(" ⚠️ HTTP " + String(code) + " (401/403=key/RLS, 400=คอลัมน์, ติดลบ=SSL/เน็ต)");

  // ตรวจเกณฑ์วิกฤต แล้วแจ้งเตือน Telegram (ยิงตรงจาก ESP32)
  checkAndAlert(doVal, ph, temp, turb);
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
