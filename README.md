# 🌊 ระบบทุ่นตรวจวัดคุณภาพน้ำทะเลแบบเรียลไทม์
**Real-time Marine Water Quality Monitoring Buoy System** — ปริญญานิพนธ์

ทุ่นลอยน้ำวัดคุณภาพน้ำทะเล **7 ค่า** ทุก 15 วินาที ส่งขึ้นคลาวด์ผ่าน **4G** แล้วประเมินตาม **มาตรฐานคุณภาพน้ำทะเลของไทย 6 ประเภท** (เลือกโหมดได้) แสดงผลบน **เว็บแดชบอร์ด** แบบเรียลไทม์ และแจ้งเตือน/ตอบคำสั่งผ่าน **Telegram**

- 🌐 แดชบอร์ด: https://real-time-marine-water-quality.onrender.com (แอดมิน: เติม `/admin` ท้าย URL)
- 📘 เอกสารสำหรับนักพัฒนา: [DevelopmentDoc/](DevelopmentDoc/README.md)

---

## ระบบทำงานอย่างไร
```
  🌊 ทะเล                                               🏝️ คลาวด์ (ทำงานเอง 24/7)
  เซนเซอร์ ─RS485─► RS485-to-TTL ─► ESP32 + 4G + GPS ─HTTPS─► Supabase (PostgreSQL)
  (4 หัววัด)                          ทุก 15 วินาที              │  ├─ realtime ─► เว็บแดชบอร์ด (Render)
                                                                 │  ├─ แถวใหม่ ─► ประเมินตามโหมด → แจ้งเตือน Telegram
                                                                 │  └─ คำสั่งแชท /status /swim /mode ─► ตอบทันที
                                                                 └─ แผนสำรอง: PHP + MySQL (ตรรกะเดียวกัน)
```
1. **ทุ่น** อ่านเซนเซอร์ (Modbus RTU) → ส่ง JSON 7 ค่า + พิกัด GPS ขึ้น Supabase · อ่านเซนเซอร์ไม่ได้จะส่ง heartbeat (`sensor_ok=false`)
2. **คลาวด์** เก็บข้อมูล + ประเมินตามโหมดมาตรฐานที่เลือก → สีรายค่า (🟢 ปลอดภัย / 🟠 ระวัง / 🔴 อันตราย) + สถานะรวม
3. **เว็บ** แสดงค่าแบบเรียลไทม์ · **Telegram** ตอบคำสั่งและเตือนเมื่อสถานะรวมเป็น "แดง"
4. ทุ่นทำแค่อ่านและส่ง — ตรรกะทั้งหมดอยู่บนคลาวด์ จึง**แก้เกณฑ์ได้โดยไม่ต้อง flash ทุ่นใหม่**

📊 แผนผังแบบภาพ: [docs/system-flowchart.html](docs/system-flowchart.html)

## ค่าที่วัด (7 ค่า จาก 4 หัววัด)
| หัววัด | ค่าที่ได้ |
|---|---|
| DO (Y504-B) | ออกซิเจนละลายน้ำ (mg/L) · ออกซิเจนอิ่มตัว (%) |
| TUR (Y510-C) | ความขุ่น (NTU) |
| CT/SAL (Y521-B) | การนำไฟฟ้า (mS/cm) · ความเค็ม (ppt) |
| pH (Y532-B) | ความเป็นกรด-ด่าง |
| เซนเซอร์อุณหภูมิในตัว | อุณหภูมิ (°C) |

## ความสามารถหลัก
| ส่วน | รายละเอียด |
|---|---|
| ระบบ 6 โหมดมาตรฐานน้ำทะเลไทย | อนุรักษ์ · ปะการัง · เพาะเลี้ยง · นันทนาการ · อุตสาหกรรม · ชุมชน — เกณฑ์/สี/สถานะ/การแจ้งเตือนเปลี่ยนตามโหมด |
| สถานะรวม (Model A) | ค่าระดับ "วิกฤต" แดง → รวมแดง · ค่าระดับ "เฝ้าระวัง" แดง/ส้ม → รวมส้ม |
| เว็บแดชบอร์ด | การ์ด 7 ค่า · สถานะรวม · ดัชนี WQI · กราฟ/ตาราง · พยากรณ์แนวโน้ม · Google Map ตามพิกัด · Export CSV |
| สถานะทุ่น | ออนไลน์ (< 45 วิ) / ข้อมูลค้าง / ออฟไลน์ (> 5 นาที) + แยก "เซนเซอร์ขัดข้อง" ด้วย heartbeat |
| สิทธิ์ผู้ใช้ 2 ระดับ | user ดูค่า · admin เข้า `/admin` ล็อกอินแล้วเปลี่ยนโหมด (ตรวจสิทธิ์ฝั่งเซิร์ฟเวอร์ + RLS) |
| Telegram | `/status` `/swim` `/mode` `/start` `/stop` `/help` + แจ้งเตือนอัตโนมัติ (cooldown 30 นาที) |

## โครงสร้างโฟลเดอร์
| โฟลเดอร์ | คืออะไร |
|----------|---------|
| [`1-sensor-tools/`](1-sensor-tools/) | เครื่องมือ Python อ่านเซนเซอร์ผ่าน USB-RS485 + สรุป protocol |
| [`2-dashboard/`](2-dashboard/) | เว็บแดชบอร์ด (HTML/CSS/JS) — Render ใช้เป็น Publish Directory |
| [`3-supabase-primary/`](3-supabase-primary/) | ★ แผนหลัก: `schema.sql` + firmware ทุ่น `Full-Version/Full-Version.ino` |
| [`4-server-backup/`](4-server-backup/) | แผนสำรอง: PHP + MySQL API + Telegram + 6 โหมด |
| [`5-extensions/`](5-extensions/) | เอกสารระบบ/ประกอบเล่ม + แนวทางพัฒนาต่อ |
| [`supabase/`](supabase/) | ★ ฝั่งคลาวด์: Edge Functions (`telegram-bot`, `telegram-alert`, `admin`) + schema + เกณฑ์กลาง `water-modes.json` |
| [`docs/`](docs/) | แผนผังการทำงานของระบบ (HTML) |
| [`DevelopmentDoc/`](DevelopmentDoc/) | เอกสารสำหรับนักพัฒนา 7 หัวข้อ |

## ติดตั้งและรัน (สรุป)
> ขั้นตอนละเอียดทั้งหมด: [DevelopmentDoc/05-setup-configuration.md](DevelopmentDoc/05-setup-configuration.md)

1. **ฐานข้อมูล Supabase** — SQL Editor รัน `3-supabase-primary/schema.sql`, `supabase/schema-settings.sql`, `supabase/schema-telegram.sql`
2. **Edge Functions** — ตั้ง secrets แล้ว deploy `telegram-bot`, `telegram-alert`, `admin` (ดู [`supabase/README.md`](supabase/README.md))
3. **เว็บ** — ใส่ URL + anon key ใน `2-dashboard/index.html` (`CONFIG.supabase`) แล้ว deploy โฟลเดอร์ `2-dashboard` บน Render (Static Site)
4. **ทุ่น** — Arduino IDE ติดตั้ง ESP32 core + ไลบรารี TinyGSM แล้ว flash `3-supabase-primary/Full-Version/Full-Version.ino` (ตั้ง APN ของซิม)

## ตัวอย่างการใช้งานเบื้องต้น
**ดูเว็บในเครื่อง (ไม่ต้องมีทุ่น)**
```
cd 2-dashboard
py -m http.server 8000
```
เปิด http://localhost:8000 · ถ้าต้องการข้อมูลจำลอง ตั้ง `CONFIG.mode = 'mock'` ใน `index.html`
> ต้องเปิดผ่าน web server แบบนี้ — ถ้าดับเบิลคลิกเปิดไฟล์ตรงๆ เบราว์เซอร์จะโหลดเกณฑ์ `water-modes.json` ไม่ได้

**อ่านเซนเซอร์จริงจากคอมพิวเตอร์** (ต่อ USB-RS485 + ไฟ 12V)
```
cd 1-sensor-tools
py -m pip install pyserial
py read_sensor.py
```

**ทดสอบระบบโดยไม่ต้องมีทุ่น** — Supabase → Table Editor → `readings` → Insert row
`device=buoy-01, do_val=6.2, do_pct=90, temp=30, ph=8.1, sal=32, cond=50, turb=8` → เว็บอัปเดตทันที

**Telegram** — ทักบอท `/start` (สมัครรับแจ้งเตือน) · `/status` (ค่าล่าสุดแยกสี) · `/swim` (ทำกิจกรรมได้ไหม) · `/mode` (ดูโหมด)

## เอกสารที่เกี่ยวข้อง
| หัวข้อ | ไฟล์ |
|---|---|
| เอกสารนักพัฒนา (สถาปัตยกรรม, API, ฐานข้อมูล, ติดตั้ง, มาตรฐานโค้ด, ข้อจำกัด) | [DevelopmentDoc/](DevelopmentDoc/README.md) |
| สลับ Supabase ↔ Server PHP | [SWITCHING-GUIDE.md](SWITCHING-GUIDE.md) |
| เกณฑ์ 6 โหมด + แหล่งอ้างอิงมาตรฐาน | [5-extensions/05-blueflag-swim-safety.md](5-extensions/05-blueflag-swim-safety.md) |
| สรุประบบฉบับอ้างอิง | [5-extensions/06-system-documentation.md](5-extensions/06-system-documentation.md) |
| Protocol เซนเซอร์ | [1-sensor-tools/SENSOR-PROTOCOL.md](1-sensor-tools/SENSOR-PROTOCOL.md) |
