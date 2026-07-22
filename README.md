# 🌊 ทุ่นตรวจคุณภาพน้ำทะเลแบบ Realtime (โปรเจคจบ)

ระบบตรวจวัดคุณภาพน้ำทะเลด้วยทุ่นลอย — เซนเซอร์อ่านค่า → ESP32 + 4G ส่งขึ้น cloud → แสดงผลบน Dashboard แบบเรียลไทม์ พร้อมพิกัด GPS, AI ช่วยวิเคราะห์ และ **แจ้งเตือน/บอท Telegram ที่ทำฝั่งคลาวด์**

## เส้นทางข้อมูล
```
  🌊 ทะเล                                          🏝️ ฝั่งบก (cloud ทำงานเอง 24/7)
  Sensor ─RS485─► ESP32 + 4G(A7670E) + GPS ─HTTPS─► Cloud DB ─realtime─► Dashboard
   (อ่านค่า)          (อ่าน + POST เท่านั้น)          │  │
                                                      │  └─(แถวใหม่)─► เช็คเกณฑ์ → เตือน Telegram
                                                      └─ ผู้ใช้ /status → อ่านค่าล่าสุด → ตอบ
        แผนหลัก  = Supabase (PostgreSQL + Edge Functions)
        แผนสำรอง = Server อาจารย์ (PHP + MySQL)
```
📊 ดูแผนผังการทำงานเต็ม: [docs/system-flowchart.html](docs/system-flowchart.html)

## สถานะ (อัปเดต 2026-07-21)
| ส่วน | สถานะ |
|------|-------|
| อ่าน sensor จริง (RS485) เข้า ESP32 | ✅ อ่านค่าได้ครบ 8 พารามิเตอร์ |
| ESP32 + 4G (A7670E) → Supabase | ✅ HTTP 201 (SNI + HTTP-app ในตัวโมเด็ม) |
| GPS (A7670E GNSS) | ✅ ได้พิกัดจริง |
| Dashboard (8 การ์ด + AI + การ์ดตำแหน่ง) | ✅ ใช้งานได้ |
| Telegram แจ้งเตือนวิกฤต + `/status` (ฝั่งบก) | ✅ deploy Supabase Edge Functions แล้ว |
| **เหลือ:** เก็บงาน + ทดสอบภาคสนามจริง | ⏳ |

## โครงสร้างโฟลเดอร์
| โฟลเดอร์ | คืออะไร |
|----------|---------|
| **0-manuals/** | คู่มือ PDF (sensor / RS485 / solar) — *ไม่ commit ขึ้น git (ไฟล์ใหญ่)* |
| **1-sensor-tools/** | เครื่องมือ Python อ่าน sensor ผ่าน RS485 (read_sensor, console, bridge) |
| **2-dashboard/** | หน้าเว็บแสดงผล (mock/supabase/server) — *Render ใช้เป็น Publish Directory* |
| **3-supabase-primary/** | ★ แผนหลัก ★ `schema.sql` + firmware ESP32 (4G / GPS / test) |
| **4-server-backup/** | แผนสำรอง: PHP+MySQL API + Telegram + test sketch |
| **5-extensions/** | ต่อยอด: AI ผู้ช่วย, แจ้งเตือน Telegram, เทรนด์, หลายทุ่น |
| **supabase/** | ★ Telegram ฝั่งบก ★ Edge Functions (bot + alert) + schema — *ชื่อโฟลเดอร์ต้องเป็น `supabase/` (Supabase CLI บังคับ)* |
| **docs/** | แผนผังการทำงานของระบบ (system-flowchart.html) |

> 🔄 **สลับ Supabase ↔ Server?** → [SWITCHING-GUIDE.md](SWITCHING-GUIDE.md)
> 📲 **ตั้ง Telegram (แจ้งเตือน + /status)?** → [5-extensions/02-alerts-notification.md](5-extensions/02-alerts-notification.md) + [supabase/README.md](supabase/README.md)

## ค่าที่วัด (8 พารามิเตอร์ — เฉพาะ probe ที่ sensor จริงอ่านได้)
ออกซิเจนละลายน้ำ (DO) · ออกซิเจน %อิ่มตัว · อุณหภูมิ · ความเป็นกรด-ด่าง (pH) · ความเค็ม · การนำไฟฟ้า (Conductivity) · สารละลายรวม (TDS) · ความขุ่น
> คลอโรฟิลล์/ORP/น้ำมัน/สาหร่าย = ยังไม่ได้ติด probe (อ่านได้ 0) จึงซ่อนไว้ — คอลัมน์ยังอยู่ใน Supabase เปิดคืนได้ทันที

## ฮาร์ดแวร์ที่ใช้
- **ESP32:** LilyGO **T-Call-A7670 V1.0** (โมดูล A7670E — LTE Cat-1 + GNSS ในตัว)
- **Sensor:** Online Multi-parameter Sensor (Modbus RTU, 9600 8N1) ผ่านโมดูล **TTL485-V2.0** (auto-direction)
- **ไฟ:** แบต 12V + โซลาร์ (OLYS controller) — โมเด็มต้องการไฟแน่น (เสียบแบต LiPo)
- ⚠️ พิน T-Call-A7670 **V1.0** ต่างจากรุ่นทั่วไป: TX=26, **RX=25**, PWRKEY=4, **RST=27**

## เริ่มใช้งาน
1. **ดู Dashboard:** เปิด `2-dashboard/index.html` (โหมด mock เห็นค่าขยับ realtime)
2. **อ่าน sensor จริง:** ดู `1-sensor-tools/README.md` (รัน `read_sensor.py`)
3. **ตั้ง Supabase:** รัน `3-supabase-primary/schema.sql` แล้วตั้ง `CONFIG.mode='supabase'` ใน dashboard
4. **ESP32 + 4G:** อัปโหลด `3-supabase-primary/esp32-supabase-4g.ino` (ใส่ APN — DTAC = `www.dtac.co.th`, AIS = `internet`)
5. **Telegram ฝั่งบก:** ทำตาม `supabase/README.md` (แผนหลัก) หรือ `4-server-backup/README.md` (แผนสำรอง)

## หมายเหตุการเชื่อมต่อ (บทเรียนสำคัญ)
- **A7670E ต่อ HTTPS Supabase:** ใช้ **HTTP application ในตัวโมเด็ม** (AT+HTTP...) + ตั้ง SSL ต้อง **เปิด `enableSNI`** ไม่งั้น handshake fail (error 715) เพราะ Supabase อยู่หลัง Cloudflare — *TLS socket (`TinyGsmClientSecure`) ของ A7670E ไม่เสถียร ใช้ไม่ได้*
- **ไลบรารี TinyGSM:** ใช้ macro `TINY_GSM_MODEM_A7672X` และต้องอยู่ path อังกฤษ (`C:\Arduino`)
- **ทุ่นเบา:** Telegram/แจ้งเตือน ไม่อยู่บน ESP32 แล้ว (ย้ายไปคลาวด์) — ทุ่นแค่อ่าน sensor แล้ว POST
