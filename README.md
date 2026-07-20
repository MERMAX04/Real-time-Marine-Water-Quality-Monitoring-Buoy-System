# 🌊 ทุ่นตรวจคุณภาพน้ำทะเลแบบ Realtime (โปรเจคจบ)

ระบบตรวจวัดคุณภาพน้ำทะเลด้วยทุ่นลอย — เซนเซอร์อ่านค่า → ESP32 + 4G ส่งขึ้น cloud → แสดงผลบน Dashboard แบบเรียลไทม์ พร้อมพิกัด GPS และ AI ช่วยวิเคราะห์

## เส้นทางข้อมูล
```
  Sensor  ──RS485/Modbus──►  ESP32 + โมดูล 4G (A7670E) + GPS  ──HTTPS──►  Cloud  ──realtime──►  Dashboard
                                                                           │
                                          แผนหลัก  = Supabase (PostgreSQL) ─┤
                                          แผนสำรอง = Server อาจารย์ (PHP+MySQL)
```
📊 ดูแผนผังการทำงานเต็ม: [docs/system-flowchart.html](docs/system-flowchart.html)

## สถานะ (อัปเดต 2026-07-20)
| ส่วน | สถานะ |
|------|-------|
| อ่าน sensor จริง (RS485) บน PC | ✅ อ่านค่าได้ครบ 8 พารามิเตอร์ |
| Dashboard (8 การ์ด + AI + การ์ดตำแหน่ง) | ✅ ใช้งานได้ |
| Supabase (ตาราง + realtime) | ✅ ต่อครบ |
| ESP32 + 4G (A7670E) → Supabase | ✅ ส่งขึ้น cloud ได้ (HTTP 201 ผ่าน AIS) |
| GPS (A7670E GNSS) | ✅ ได้พิกัดจริง |
| **เหลือ:** ต่อ sensor จริงเข้า ESP32 ผ่านโมดูล RS485-TTL | ⏳ ด่านสุดท้าย |

## โครงสร้างโฟลเดอร์
| โฟลเดอร์ | คืออะไร |
|----------|---------|
| **0-manuals/** | คู่มือ PDF (sensor / RS485 / solar controller) — *ไม่ commit ขึ้น git (ไฟล์ใหญ่)* |
| **1-sensor-tools/** | เครื่องมือ Python อ่าน sensor ผ่าน RS485 (read_sensor, console, bridge) |
| **2-dashboard/** | หน้าเว็บแสดงผล (สลับแหล่งข้อมูลได้ mock/supabase/server) |
| **3-supabase-primary/** | ★ แผนหลัก ★ schema.sql + โค้ด ESP32 (4G / WiFi test / GPS test) |
| **4-server-backup/** | แผนสำรอง: PHP+MySQL API + โค้ด ESP32 |
| **5-extensions/** | ต่อยอด: AI ผู้ช่วย, แจ้งเตือน LINE, เทรนด์, หลายทุ่น |
| **docs/** | แผนผังระบบ + ดีไซน์ LINE Rich Menu |

> 🔄 **สลับ Supabase ↔ Server?** → [SWITCHING-GUIDE.md](SWITCHING-GUIDE.md)

## ค่าที่วัด (8 พารามิเตอร์ — เฉพาะ probe ที่ sensor จริงอ่านได้)
ออกซิเจนละลายน้ำ (DO) · ออกซิเจน %อิ่มตัว · อุณหภูมิ · ความเป็นกรด-ด่าง (pH) · ความเค็ม · การนำไฟฟ้า (Conductivity) · สารละลายรวม (TDS) · ความขุ่น
> คลอโรฟิลล์/ORP/น้ำมัน/สาหร่าย = ยังไม่ได้ติด probe (อ่านได้ 0) จึงซ่อนไว้ — คอลัมน์ยังอยู่ใน Supabase เปิดคืนได้ทันที

## ฮาร์ดแวร์ที่ใช้
- **ESP32:** LilyGO **T-Call-A7670 V1.0** (โมดูล A7670E — LTE Cat-1 + GNSS ในตัว)
- **Sensor:** Online Multi-parameter Sensor (Modbus RTU, 9600 8N1)
- **ไฟ:** แบต 12V + โซลาร์ (OLYS controller) — โมเด็มต้องการไฟแน่น (เสียบแบต LiPo)
- ⚠️ พิน T-Call-A7670 **V1.0** ต่างจากรุ่นทั่วไป: TX=26, **RX=25**, PWRKEY=4, **RST=27**

## เริ่มใช้งาน
1. **ดู Dashboard:** เปิด `2-dashboard/index.html` (โหมด mock เห็นค่าขยับ realtime)
2. **อ่าน sensor จริง:** ดู `1-sensor-tools/README.md` (รัน `read_sensor.py`)
3. **ตั้ง Supabase:** รัน `3-supabase-primary/schema.sql` แล้วตั้ง `CONFIG.mode='supabase'` ใน dashboard
4. **ESP32 + 4G:** อัปโหลด `3-supabase-primary/esp32-supabase-4g.ino` (ใส่ APN — AIS = `internet`)

## หมายเหตุการเชื่อมต่อ (บทเรียนสำคัญ)
- **A7670E ต่อ HTTPS Supabase:** ใช้ HTTP application ในตัวโมเด็ม (AT+HTTP...) + ตั้ง SSL ต้อง **เปิด `enableSNI`** ไม่งั้น handshake fail (error 715) เพราะ Supabase อยู่หลัง Cloudflare
- **ไลบรารี TinyGSM:** ใช้ macro `TINY_GSM_MODEM_A7672X` และต้องอยู่ path อังกฤษ (`C:\Arduino`)
