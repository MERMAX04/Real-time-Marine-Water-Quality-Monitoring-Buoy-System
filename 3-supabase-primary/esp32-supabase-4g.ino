/* =========================================================================
   esp32-supabase-4g.ino  ★ FULL VERSION (อ่าน sensor จริง) ★
   บอร์ด: LilyGO T-Call-A7670 V1.0 (โมดูล A7670E)
   งาน: อ่านค่าจริงจาก sensor (RS485/Modbus) -> ส่งขึ้น Supabase ผ่าน 4G + อ่านพิกัด GPS
        ★ ทุ่นทำแค่ "อ่าน sensor แล้ว POST" เท่านั้น — เบา/ประหยัดไฟ/เสถียร (Telegram ย้ายไปฝั่งบก) ★

   📦 ไลบรารี (Arduino IDE > Library Manager): "TinyGSM" โดย Volodymyr Shymanskyy
      - ใช้ macro TINY_GSM_MODEM_A7672X (ครอบคลุม A7670E) — ไลบรารีต้องอยู่ path อังกฤษ (C:\Arduino)

   🔌 การต่อสาย:
      โมเด็ม/GPS : เสา LTE→MAIN, เสา GPS→GPS, ซิม nano (ปิด PIN lock), APN=DTAC(www.dtac.co.th)
      Sensor RS485 (ผ่านโมดูล TTL485-V2.0 = auto-direction ไม่มีขา DE/RE):
        TTL485 TXD -> ESP32 GPIO32 (RX)      TTL485 RXD -> ESP32 GPIO33 (TX)
        TTL485 VCC -> 3V3   GND -> GND ร่วม  (โมดูลนี้คุมทิศเอง จึงตั้ง PIN_485_DE=-1)
        TTL485 A(+) -> sensor เขียว(485_A)   TTL485 B(-) -> sensor ขาว(485_B)
        sensor แดง(+)/ดำ(-) -> ไฟ 12V แยก, GND ดำต้องต่อถึง GND ร่วม (ESP32+TTL485)!
      ⚠️ จ่ายไฟ TTL485 ที่ 3V3 เพื่อให้ขา TXD เป็น 3.3V (ปลอดภัยกับ ESP32)
      ⚡ ไฟ: โมเด็มกินไฟพีคสูง — ควรเสียบแบต LiPo (USB อย่างเดียวอาจไม่พอตอนยิง 4G)
   📲 แจ้งเตือน + คำสั่งแชท Telegram (/status ฯลฯ): ย้ายไปทำ "ฝั่งบก" แล้ว
      (Supabase Edge Functions + PHP server อ่านค่าล่าสุดจาก DB ส่งเอง) — ดู 5-extensions/02-alerts-notification.md
   🔑 HTTPS ใช้ได้เพราะเปิด SNI ใน SSL context (Supabase อยู่หลัง Cloudflare)

   ทดสอบ: เปิด Serial Monitor 115200 — setup จะลองอ่าน sensor 1 ครั้งให้ดู
           แล้ว loop ส่งค่าจริงทุก 15 วิ (ควรได้ HTTP 201)
   ========================================================================= */

// ---- ต้องประกาศ "ก่อน" include TinyGSM ----
#define TINY_GSM_MODEM_A7672X         // TinyGSM ใช้ชื่อ A7672X ครอบคลุม A7670E (ชุดคำสั่งเดียวกัน)
#define SerialAT       Serial1        // ESP32 คุยกับโมเด็มผ่าน Serial1
#define TINY_GSM_RX_BUFFER 1024
#include <TinyGsmClient.h>

// ===================== แก้ค่าตรงนี้ =====================
const char APN[]  = "www.dtac.co.th";  // DTAC (ถ้าต่อไม่ติดลอง "internet"); AIS/True = "internet" — user/pass เว้นว่าง
const char GUSER[]= "";
const char GPASS[]= "";
#define SB_HOST  "jjbrgolulggksxnuicgg.supabase.co"     // host ของ Supabase (ไม่มี https://)
#define SB_ANON  "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImpqYnJnb2x1bGdna3N4bnVpY2dnIiwicm9sZSI6ImFub24iLCJpYXQiOjE3ODMyNjc0ODAsImV4cCI6MjA5ODg0MzQ4MH0.0coKRuYMMOwLq9yJFsOff99ya9RBwyLBzqxN2YU1JWg"
#define DEVICE_ID "buoy-01"

// ===== โหมดทดสอบ =====
#define USE_FAKE  0     // 1 = ใช้ค่าปลอม (ทดสอบ 4G โดยไม่ต้องต่อ sensor), 0 = อ่าน sensor จริง

// ===== Sensor RS485/Modbus (Online Multi-parameter, 9600 8N1, slave 0x01) =====
#define SENSOR_ADDR  0x01
#define SENSOR_BAUD  9600
#define PIN_485_RX   32     // ESP32 RX  <- ขา TXD ของ TTL485 module
#define PIN_485_TX   33     // ESP32 TX  -> ขา RXD ของ TTL485 module
#define PIN_485_DE   -1     // TTL485-V2.0 = auto-direction (คุมทิศเอง) ไม่มีขา DE/RE จึงตั้ง -1
HardwareSerial SerialRS(2); // ใช้ UART2 สำหรับ sensor (โมเด็มใช้ UART1 อยู่ — ไม่ชนกัน)

// (Telegram: ไม่มีในทุ่นแล้ว — ทำฝั่งบก อ่านค่าล่าสุดจาก Supabase/DB ส่งเอง)

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

// ---- ค่าอ่านล่าสุดจาก sensor (11 ค่า ตามลำดับ frame 0x2600) ----
float sDO, sTurb, sCond, sPH, sTemp, sORP, sChl, sOIW, sSal, sTDS, sDOpct;
double gLat=1000, gLon=1000;       // พิกัด GPS ล่าสุด (1000 = ยังไม่ล็อกดาว)

// ---- forward declarations (atCmd/waitFor + atCmd มี default arg) ----
String atCmd(const String& cmd, uint32_t timeout=3000);
String waitFor(const char* token, uint32_t timeout);

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

// ================= Sensor RS485/Modbus =================
// CRC-16 Modbus (คืนค่า: low byte ส่งก่อน)
uint16_t modbusCRC(const uint8_t* buf, int len){
  uint16_t crc = 0xFFFF;
  for(int i=0;i<len;i++){
    crc ^= buf[i];
    for(int b=0;b<8;b++) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
  }
  return crc;
}
// ส่ง frame ออก RS485 (สลับทิศ DE ถ้ามีขาคุม)
void rs485Send(const uint8_t* buf, int len){
  if(PIN_485_DE >= 0){ digitalWrite(PIN_485_DE, HIGH); delayMicroseconds(50); }
  SerialRS.write(buf, len);
  SerialRS.flush();                         // รอส่งจบก่อนสลับกลับมารับ
  if(PIN_485_DE >= 0) digitalWrite(PIN_485_DE, LOW);
}
// อ่านทุกค่ารวดเดียว: 01 03 26 00 00 16 + CRC -> ตอบ 01 03 2C <44 ไบต์> <CRC> = 49 ไบต์
// float เป็น IEEE754 little-endian (DCBA) → ESP32 little-endian memcpy ได้ตรงๆ
bool readSensor(){
  uint8_t req[8] = {SENSOR_ADDR, 0x03, 0x26, 0x00, 0x00, 0x16, 0, 0};
  uint16_t crc = modbusCRC(req, 6);
  req[6] = crc & 0xFF; req[7] = (crc >> 8) & 0xFF;

  while(SerialRS.available()) SerialRS.read();     // เคลียร์ buffer เก่า
  rs485Send(req, 8);

  const int NEED = 3 + 44 + 2;                      // = 49
  uint8_t resp[64]; int n = 0; uint32_t t0 = millis();
  while(n < NEED && millis() - t0 < 1000){
    while(SerialRS.available() && n < (int)sizeof(resp)) resp[n++] = SerialRS.read();
  }
  if(n < NEED){ Serial.printf("  RS485: ตอบไม่ครบ (ได้ %d/%d ไบต์) — เช็คสาย A/B, ไฟ 12V, GND ร่วม\n", n, NEED); return false; }
  if(resp[0] != SENSOR_ADDR || resp[1] != 0x03 || resp[2] != 44){
    Serial.printf("  RS485: frame ผิด (hdr %02X %02X %02X)\n", resp[0], resp[1], resp[2]); return false;
  }
  uint16_t rc = modbusCRC(resp, 3 + 44);
  if((rc & 0xFF) != resp[47] || ((rc >> 8) & 0xFF) != resp[48]){ Serial.println("  RS485: CRC ไม่ผ่าน (สัญญาณรบกวน/GND?)"); return false; }

  float f[11];
  for(int i=0;i<11;i++) memcpy(&f[i], &resp[3 + i*4], 4);   // ถอด 11 floats
  sDO=f[0]; sTurb=f[1]; sCond=f[2]; sPH=f[3]; sTemp=f[4];
  sORP=f[5]; sChl=f[6]; sOIW=f[7]; sSal=f[8]; sTDS=f[9]; sDOpct=f[10];
  return true;
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

// ================= GPS (GNSS ผ่านคำสั่ง AT) =================
// (gLat/gLon ประกาศเป็น global ด้านบนแล้ว)

String atCmd(const String& cmd, uint32_t timeout){   // default อยู่ที่ forward declaration ด้านบนแล้ว
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
  Serial.println("\n=== ทุ่น buoy-01 : Sensor(RS485) + 4G(A7670) -> Supabase ===");

  // เปิด UART สำหรับ sensor RS485 + ตั้งขาคุมทิศ
  SerialRS.begin(SENSOR_BAUD, SERIAL_8N1, PIN_485_RX, PIN_485_TX);
  if(PIN_485_DE >= 0){ pinMode(PIN_485_DE, OUTPUT); digitalWrite(PIN_485_DE, LOW); }  // เริ่มที่โหมดรับ
#if USE_FAKE
  Serial.println("โหมด: ใช้ค่าปลอม (USE_FAKE=1) — ข้ามการอ่าน sensor");
#else
  Serial.print("ทดสอบอ่าน sensor RS485... ");
  if(readSensor())
    Serial.printf("✅ DO=%.2f mg/L  pH=%.2f  temp=%.2f°C  turb=%.1f NTU\n", sDO, sPH, sTemp, sTurb);
  else
    Serial.println("⚠️ ยังอ่านไม่ได้ — เดี๋ยว loop จะลองใหม่ (เช็ค TXD/RXD, A/B, ไฟ 12V, GND ร่วม)");
#endif

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
  atCmd("AT+CSSLCFG=\"ignorelocaltime\",0,1", 1500); // ข้ามการเช็ววันหมดอายุ
  String _sni = atCmd("AT+CSSLCFG=\"enableSNI\",0,1", 1500);        // ★ เปิด SNI (จำเป็นสำหรับ Cloudflare/Supabase)
  Serial.println(_sni.indexOf("OK") >= 0 ? "✅ (เปิด SNI แล้ว)" : ("⚠️ SNI ตอบ: " + _sni));
  connect4G();
}

void loop(){
  if(millis() - lastSend < 15000) return;    // ส่งทุก 15 วิ (ผ่าน 4G อย่าถี่)
  lastSend = millis();

  if(!modem.isGprsConnected() && !connect4G()) return;

  // ---- อ่านค่าจาก sensor ----
#if USE_FAKE
  sDO=rnd(4,9); sDOpct=rnd(70,120); sTemp=rnd(26,31); sPH=rnd(7.5,8.5);
  sSal=rnd(28,35); sCond=rnd(40,55); sTDS=rnd(28,40); sTurb=rnd(1,25);
#else
  bool ok=false;
  for(int a=0; a<3 && !ok; a++){ ok=readSensor(); if(!ok) delay(250); }   // ลองซ้ำได้ 3 ครั้ง
  if(!ok){ Serial.println("อ่าน sensor ไม่สำเร็จ — ข้ามรอบนี้ (ไม่ส่งค่ามั่ว)"); return; }
  Serial.printf("Sensor: DO=%.2f DO%%=%.0f temp=%.2f pH=%.2f sal=%.2f cond=%.2f tds=%.2f turb=%.1f\n",
                sDO, sDOpct, sTemp, sPH, sSal, sCond, sTDS, sTurb);
#endif

  if(readGPS()) Serial.println("GPS: " + String(gLat,6) + ", " + String(gLon,6));
  else          Serial.println("GPS: ยังไม่ล็อกพิกัด (เสาต้องเห็นฟ้า)");

  // สร้าง JSON (คอลัมน์ตรงกับ probe ที่ sensor จริงอ่านได้ = ที่ Dashboard แสดง)
  String body = "{";
  body += "\"device\":\"" DEVICE_ID "\",";
  body += "\"do_val\":" + String(sDO,2)    + ",";
  body += "\"do_pct\":" + String(sDOpct,1) + ",";
  body += "\"temp\":"   + String(sTemp,2)  + ",";
  body += "\"ph\":"     + String(sPH,2)    + ",";
  body += "\"sal\":"    + String(sSal,2)   + ",";
  body += "\"cond\":"   + String(sCond,2)  + ",";
  body += "\"tds\":"    + String(sTDS,2)   + ",";
  body += "\"turb\":"   + String(sTurb,2);
  if(gLat>=-90 && gLat<=90 && gLon>=-180 && gLon<=180)     // มีพิกัด GPS แล้วค่อยส่ง
    body += ",\"lat\":" + String(gLat,6) + ",\"lon\":" + String(gLon,6);
  body += "}";

  Serial.print("POST Supabase (HTTPS ในตัวโมเด็ม)...");
  int code = httpPost("https://" SB_HOST "/rest/v1/readings", body);
  if(code == 201 || code == 200) Serial.println(" ✅ insert สำเร็จ (" + String(code) + ")\n  " + body);
  else Serial.println(" ⚠️ HTTP " + String(code) + " (401/403=key/RLS, 400=คอลัมน์, ติดลบ=SSL/เน็ต)");
}
