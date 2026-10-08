# 🌊 ทุ่นตรวจคุณภาพน้ำทะเลแบบ Realtime (ปริญญานิพนธ์)

ระบบตรวจวัดคุณภาพน้ำทะเลด้วยทุ่นลอย — เซนเซอร์อ่านค่า → ESP32 + 4G ส่งขึ้น cloud → ประเมินตาม **มาตรฐานน้ำทะเลไทย 6 ประเภท** → แสดงผลบน Dashboard แบบเรียลไทม์ พร้อม **Google Map**, สถานะสีรายค่า, สถานะรวม, สิทธิ์ผู้ใช้ 2 ระดับ และ **แจ้งเตือน/บอท Telegram ที่ทำฝั่งคลาวด์**

## เส้นทางข้อมูล
```
  🌊 ทะเล                                          🏝️ ฝั่งบก (cloud ทำงานเอง 24/7)
  Sensor ─RS485─►[RS485-to-TTL]─►ESP32 + 4G(A7670E) + GPS ─HTTPS─► Cloud DB ─realtime─► Dashboard
   (อ่านค่า)                        (อ่าน + POST เท่านั้น)          │  │
                                                                    │  └─(แถวใหม่)─► ประเมินตามโหมด → เตือน Telegram
                                                                    └─ ผู้ใช้ /status /swim /mode → อ่าน/ตั้งค่า → ตอบ
        แผนหลัก  = Supabase (PostgreSQL + Edge Functions)
        แผนสำรอง = Server อาจารย์ (PHP + MySQL)
```
📊 ดูแผนผังการทำงานเต็ม: [docs/system-flowchart.html](docs/system-flowchart.html) · 📘 สรุประบบฉบับอ้างอิง (สำหรับเล่ม): [5-extensions/06-system-documentation.md](5-extensions/06-system-documentation.md)

## สถานะ (อัปเดต 2026-10-08)
| ส่วน | สถานะ |
|------|-------|
| อ่าน sensor จริง (RS485/Modbus) เข้า ESP32 | ✅ อ่านค่าได้ **7 พารามิเตอร์ จาก 4 โพรบที่ติดตั้ง** |
| ESP32 + 4G (A7670E) → Supabase | ✅ HTTP 201 (SNI + HTTP-app ในตัวโมเด็ม) |
| GPS (A7670E GNSS) + Google Map ตามพิกัด | ✅ ได้พิกัดจริง / แผนที่ตามตำแหน่ง (ไม่มีพิกัด = แจ้งเตือน ไม่ hard-code) |
| ระบบ **6 โหมดมาตรฐานน้ำทะเลไทย** (เลือกได้ มีผลต่อสี/สถานะ/แจ้งเตือน) | ✅ ใช้งานจริง (config กลาง `water-modes.json`) |
| สถานะสีรายค่า (เขียว/ส้ม/แดง) + สถานะรวม (Model A) | ✅ ใช้งานจริง |
| Dashboard (การ์ด 7 ค่า + สถานะทุ่น + Google Map + Export CSV) | ✅ ใช้งานได้ |
| สิทธิ์ผู้ใช้ 2 ระดับ (user ดูค่า / admin เปลี่ยนโหมดผ่าน `/admin`) | ✅ ใช้งานจริง (Edge Function + RLS) |
| Telegram `/status` `/swim` `/mode` (admin) + แจ้งเตือนตามโหมด | ✅ deploy Supabase Edge Functions แล้ว |
| สถานะทุ่น online/stale/offline (liveness) + heartbeat (`sensor_ok`) | ✅ ใช้งานจริง |
| **เหลือ:** เก็บงาน + ทดสอบภาคสนามจริง | ⏳ |

## โครงสร้างโฟลเดอร์
| โฟลเดอร์ | คืออะไร |
|----------|---------|
| **0-manuals/** | คู่มือ PDF (sensor / RS485 / solar) — *ไม่ commit ขึ้น git (ไฟล์ใหญ่)* |
| **1-sensor-tools/** | เครื่องมือ Python อ่าน sensor ผ่าน RS485 (read_sensor, console, bridge) |
| **2-dashboard/** | หน้าเว็บแสดงผล + `water-modes.json` — *Render ใช้เป็น Publish Directory* |
| **3-supabase-primary/** | ★ แผนหลัก ★ `schema.sql` + firmware ESP32 (`Full-Version/Full-Version.ino`) |
| **4-server-backup/** | แผนสำรอง: PHP+MySQL API + Telegram + 6 โหมด (`lib/water-modes.json`) |
| **5-extensions/** | ต่อยอด + **เอกสารเล่ม** (06 ระบบ, 07 บท 3–4 + diagram, 08 บทคัดย่อ/บท 1/บท 5, 09 แผนโพรบเสริม A/B) |
| **supabase/** | ★ ฝั่งบก ★ Edge Functions (telegram-bot + telegram-alert + **admin**) + schema + config กลาง `functions/_shared/water-modes.json` |
| **docs/** | แผนผังการทำงานของระบบ (system-flowchart.html) |

> 🔄 **สลับ Supabase ↔ Server?** → [SWITCHING-GUIDE.md](SWITCHING-GUIDE.md)
> 📲 **ตั้ง Telegram (แจ้งเตือน + /status /swim /mode)?** → [supabase/README.md](supabase/README.md)
> 🏖️ **เกณฑ์ 6 โหมด + "ลงเล่นน้ำได้ไหม" (คพ.ไทย / Blue Flag + แหล่งอ้างอิง)** → [5-extensions/05-blueflag-swim-safety.md](5-extensions/05-blueflag-swim-safety.md)
> 🔐 **สิทธิ์ผู้ใช้ admin/user (`/admin`)** → [5-extensions/06-system-documentation.md](5-extensions/06-system-documentation.md) ข้อ 8

## ค่าที่วัด (7 พารามิเตอร์ จาก 4 โพรบที่ติดตั้ง)
| โพรบที่ติดตั้ง | ค่าที่ได้ |
|---|---|
| DO (Y504-B) | ออกซิเจนละลายน้ำ (DO, mg/L) · ออกซิเจนอิ่มตัว (DO%) |
| TUR (Y510-C) | ความขุ่น (Turbidity, NTU) |
| CT/SAL (Y521-B) | การนำไฟฟ้า (Conductivity, mS/cm) · ความเค็ม (Salinity, ppt) |
| pH (Y532-B) | ความเป็นกรด-ด่าง (pH) |
| – (วัดจากตัวเครื่อง) | อุณหภูมิ (Temp, °C) |

> ⚠️ **ไม่มี ORP / คลอโรฟิลล์ (CHL) / น้ำมัน (OIW) / สาหร่าย (BGA)** — เซนเซอร์รุ่นนี้เลือกติดโพรบได้ แต่ชุดที่ใช้ไม่ได้ติดตั้ง 4 โพรบนี้ (ข้อมูลจริง 597 แถวอ่านได้ 0.000 ทุกแถว) จึงตัดออกจากระบบ — คอลัมน์ในฐานข้อมูลยังเก็บไว้รองรับการติดเพิ่มในอนาคต · รายละเอียด [5-extensions/09-probe-plan-A-B.md](5-extensions/09-probe-plan-A-B.md)
> ⚠️ **ไม่มี TDS** — TDS เป็นค่าที่คำนวณจาก Conductivity ไม่ใช่ค่าจากโพรบจริง

## ฮาร์ดแวร์ที่ใช้
- **ESP32:** LilyGO **T-Call-A7670 V1.0** (โมดูล A7670E — LTE Cat-1 + GNSS ในตัว)
- **Sensor:** Online Multi-parameter Sensor (Modbus RTU, 9600 8N1) ผ่านโมดูล **RS485-to-TTL**
- **ไฟ:** โซล่าเซลล์ → Control (OLYS charge controller) → แบต 12V → Delay → 12V (เลี้ยงเซนเซอร์) + Step-down 5V (เลี้ยง ESP32)
- ⚠️ พิน T-Call-A7670 **V1.0** ต่างจากรุ่นทั่วไป: TX=26, **RX=25**, PWRKEY=4, **RST=27**

## เริ่มใช้งาน
1. **ดู Dashboard:** เปิด `2-dashboard/index.html` (โหมด mock เห็นค่าขยับ realtime)
2. **อ่าน sensor จริง:** ดู `1-sensor-tools/README.md` (รัน `read_sensor.py`)
3. **ตั้ง Supabase:** รัน `3-supabase-primary/schema.sql` + `supabase/schema-settings.sql` + `supabase/schema-telegram.sql`
4. **ESP32 + 4G:** อัปโหลด `3-supabase-primary/Full-Version/Full-Version.ino` (ใส่ APN — DTAC = `www.dtac.co.th`, AIS = `internet`)
5. **ฝั่งบก (Telegram + admin):** ทำตาม `supabase/README.md` (แผนหลัก) หรือ `4-server-backup/README.md` (แผนสำรอง)

## หมายเหตุการเชื่อมต่อ (บทเรียนสำคัญ)
- **A7670E ต่อ HTTPS Supabase:** ใช้ **HTTP application ในตัวโมเด็ม** (AT+HTTP...) + ตั้ง SSL ต้อง **เปิด `enableSNI`** ไม่งั้น handshake fail (error 715) เพราะ Supabase อยู่หลัง Cloudflare — *TLS socket (`TinyGsmClientSecure`) ของ A7670E ไม่เสถียร ใช้ไม่ได้*
- **ไลบรารี TinyGSM:** ใช้ macro `TINY_GSM_MODEM_A7672X` และต้องอยู่ path อังกฤษ (`C:\Arduino`)
- **GND ร่วม:** ต้องต่อ common ground ที่ GND BAR ครบทุกตัว (ESP32 / RS485-to-TTL / sensor / แหล่งจ่าย) ไม่งั้นสัญญาณ RS485 เพี้ยน เซนเซอร์ไม่ตอบ
- **ทุ่นเบา:** ตรรกะประเมิน/แจ้งเตือน/แสดงผล อยู่ฝั่งคลาวด์ทั้งหมด — ทุ่นแค่อ่าน sensor + GPS แล้ว POST (ปรับเกณฑ์/เพิ่มฟีเจอร์ได้โดยไม่ต้อง flash ทุ่นใหม่)
