
#define TINY_GSM_MODEM_A7672X
#define SerialAT        Serial1
#define TINY_GSM_RX_BUFFER 1024
#include <TinyGsmClient.h>

// ---- ขา LilyGO T-Call-A7670 V1.0 (RX=25, RST=27 ต่างจากรุ่น T-A7670!) ----
#define MODEM_BAUD    115200
#define PIN_TX        26      // ESP32 TX -> modem RX
#define PIN_RX        25      // ESP32 RX <- modem TX  (V1.0 = 25 ไม่ใช่ 27)
#define PIN_PWRKEY    4
#define PIN_RST       27      // V1.0 reset = 27 (active LOW)

TinyGsm modem(SerialAT);
bool gnssOK = false;

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

// ส่งคำสั่ง AT แล้วคืน response ทั้งก้อน (อ่านจาก SerialAT ตรงๆ)
String atCmd(const String& cmd, uint32_t timeout=3000){
  while(SerialAT.available()) SerialAT.read();       // เคลียร์ buffer เก่า
  SerialAT.println(cmd);
  String r=""; uint32_t t0=millis();
  while(millis()-t0 < timeout){
    while(SerialAT.available()) r += (char)SerialAT.read();
  }
  return r;
}

// ดึง field ที่ idx จากสตริงคั่นด้วย comma
String field(const String& s, int idx){
  int start=0, count=0;
  for(int i=0;i<=(int)s.length();i++){
    if(i==(int)s.length() || s[i]==','){
      if(count==idx) return s.substring(start,i);
      count++; start=i+1;
    }
  }
  return "";
}

// firmware A7670E-FASE คืนพิกัดเป็น "องศาทศนิยม" อยู่แล้ว (เช่น 7.198357) — แค่ใส่เครื่องหมายตามซีกโลก
double toDeg(const String& v, const String& hemi){
  double d = v.toDouble();
  if(hemi=="S" || hemi=="W") d = -d;
  return d;
}

void setup(){
  Serial.begin(115200);
  delay(500);
  Serial.println("\n========== ทดสอบ GPS/GNSS (A7670E) ==========");

  modemPowerOn();
  SerialAT.begin(MODEM_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

  Serial.print("รอโมเด็มบูต + ทดสอบ AT ");
  int retry = 0;
  while(!modem.testAT(1000)){
    Serial.print(".");
    if(++retry > 15){                 // ~15 วิ ยังไม่ตอบ -> กด PWRKEY ใหม่
      Serial.println("\n  ↻ ยังไม่ตอบ — กด PWRKEY ซ้ำ (ถ้ายังไม่ติด = ไฟไม่พอ ให้เสียบแบต LiPo!)");
      modemPowerOn();
      retry = 0;
    }
  }
  Serial.println(" ✅ โมเด็มตอบแล้ว!");
  Serial.println("รุ่นโมเด็ม: " + modem.getModemName());

  // ---- เปิด GNSS ----
  Serial.print("เปิด GNSS (AT+CGNSSPWR=1)...");
  String r = atCmd("AT+CGNSSPWR=1", 5000);
  if(r.indexOf("ERROR") >= 0){
    Serial.println(" ❌ ตอบ ERROR");
    Serial.println("➜ โมดูล A7670E ตัวนี้ 'ไม่มี GNSS' ในตัว");
    Serial.println("   ทางแก้: ใช้ 'พิกัดคงที่' ของจุดวางทุ่น (ทุ่นจอดที่เดิม) — ง่ายและแม่นพอ");
    return;
  }
  Serial.println(" ✅ ส่งคำสั่งแล้ว");
  Serial.println("รอ GNSS บูต + จับดาวเทียม... (เสา GPS ต้องเห็นท้องฟ้า)");
  Serial.println("ครั้งแรกอาจใช้เวลา 30 วิ – 2-3 นาที\n");
  delay(4000);
  gnssOK = true;
}

void loop(){
  if(!gnssOK){ delay(5000); return; }

  String r = atCmd("AT+CGNSSINFO", 3000);
  int i = r.indexOf("+CGNSSINFO:");
  if(i < 0){
    Serial.println("⏳ ยังไม่ตอบ CGNSSINFO (GNSS กำลังบูต) รอต่อ...");
    delay(3000); return;
  }

  String line = r.substring(i + 11);
  line.trim();
  Serial.println("   [debug] RAW = " + line);    // <-- โชว์ข้อความดิบเพื่อดู field จริง
  // หา N/S และ E/W (แข็งแรงกว่าเดาตำแหน่ง เพราะ firmware ต่างรุ่นมี field ดาวเทียมนำหน้าไม่เท่ากัน)
  int nsIdx=-1, ewIdx=-1;
  for(int k=0;k<20;k++){
    String f = field(line,k);
    if(f=="N" || f=="S") nsIdx=k;
    else if(f=="E" || f=="W") ewIdx=k;
  }

  if(nsIdx < 1 || ewIdx < 1){
    Serial.println("⏳ ยังไม่ล็อกพิกัด (ยังจับดาวไม่พอ) — ให้เสา GPS เห็นฟ้ามากขึ้น แล้วรอ...");
  } else {
    double lat = toDeg(field(line,nsIdx-1), field(line,nsIdx));   // ตัวเลขก่อนหน้า N/S = latitude
    double lon = toDeg(field(line,ewIdx-1), field(line,ewIdx));   // ตัวเลขก่อนหน้า E/W = longitude
    String date = field(line,ewIdx+1), utc = field(line,ewIdx+2);
    Serial.println("========================================");
    Serial.println("✅ ได้พิกัดแล้ว!");
    Serial.printf("   lat = %.6f\n", lat);
    Serial.printf("   lon = %.6f\n", lon);
    Serial.printf("   วันที่(ddmmyy)=%s  เวลา(UTC)=%s\n", date.c_str(), utc.c_str());
    Serial.printf("   🗺️ https://maps.google.com/?q=%.6f,%.6f\n", lat, lon);
    Serial.println("========================================");
  }
  delay(3000);
}
