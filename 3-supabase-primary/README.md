# 3-supabase-primary — แผนหลัก (Supabase / PostgreSQL) + Firmware ทุ่น

เส้นทางข้อมูล:
```
Sensor ─RS485─► RS485-to-TTL ─► ESP32 + 4G (A7670E) ─HTTPS POST ทุก 15 วิ─► Supabase (ตาราง readings) ─realtime─► Dashboard
```
Supabase = Backend-as-a-Service บน PostgreSQL (ตาราง + REST API + realtime ในตัว)

| ไฟล์ | หน้าที่ |
|------|---------|
| `schema.sql` | สร้างตาราง `readings` + index + RLS + realtime + view `readings_daily` |
| `Full-Version/Full-Version.ino` | **firmware ที่ flash ลงทุ่น** — อ่าน 7 ค่า + GPS + heartbeat แล้วส่งขึ้น Supabase ผ่าน 4G |

---

## ขั้นที่ 1 — สร้าง Supabase Project
1. เข้า https://supabase.com → New project
2. ตั้งชื่อ เช่น `buoy-monitor`, ตั้งรหัส database, เลือก region **Southeast Asia (Singapore)**
3. รอสร้างเสร็จ ~2 นาที

## ขั้นที่ 2 — สร้างตาราง + สิทธิ์ + realtime
1. เมนูซ้าย **SQL Editor** → New query
2. วางเนื้อหาไฟล์ `schema.sql` ทั้งหมด → กด **Run**
3. จะได้ตาราง `readings` (13 คอลัมน์: id, device, created_at, ค่าน้ำ 7 ค่า, sensor_ok, lat, lon) + RLS (anon insert/read) + realtime + view รายวัน
> ฝั่งบก (Telegram / admin / 6 โหมด) รัน `supabase/schema-telegram.sql` + `supabase/schema-settings.sql` เพิ่ม (ดู [`../supabase/README.md`](../supabase/README.md))

## ขั้นที่ 3 — เอา URL + anon key มาใส่ Dashboard
1. เมนูซ้าย **Project Settings → API** → ก๊อป **Project URL** และ **anon public key** (ขึ้นต้น `eyJ...`)
2. ใส่ใน `2-dashboard/index.html` ที่ `CONFIG.supabase` และตั้ง `CONFIG.mode = 'supabase'`
   ```js
   supabase: { url: "https://xxxx.supabase.co", anonKey: "eyJhbGciOi...." },
   ```

## ขั้นที่ 4 — ทดสอบก่อนมี ESP32 (ไม่ต้องมีฮาร์ดแวร์)
1. เมนูซ้าย **Table Editor** → ตาราง `readings` → **Insert row**
2. ใส่ค่า เช่น `device=buoy-01, do_val=6.2, do_pct=90, temp=30, ph=8.1, sal=32, cond=50, turb=8` → Save
3. เปิดแดชบอร์ด → ค่าต้องขึ้นทันที และเมื่อ insert แถวใหม่หน้าเว็บเปลี่ยนเอง = realtime ทำงาน ✅

## ขั้นที่ 5 — Flash firmware ลง ESP32 (`Full-Version/Full-Version.ino`)

**ฮาร์ดแวร์:** LilyGO **T-Call-A7670 V1.0** (โมดูล A7670E — LTE Cat-1 + GNSS ในตัว) + เซนเซอร์ผ่านโมดูล **RS485-to-TTL**
| ขา ESP32 | ต่อกับ |
|---|---|
| GPIO 26 / 25 | โมเด็ม TX / RX (V1.0: **RX=25**) |
| GPIO 4 / 27 | โมเด็ม PWRKEY / RESET (V1.0: **RST=27, active LOW**) |
| GPIO 32 / 33 | RS485-to-TTL TXD → ESP RX (32) · ESP TX (33) → RXD |
| 3V3 / GND | ไฟเลี้ยงโมดูล RS485 + **GND ร่วม** กับเซนเซอร์และแหล่งจ่าย |

**ไลบรารี:** **TinyGSM** (Library Manager) — ใช้ macro `TINY_GSM_MODEM_A7672X` (ครอบคลุม A7670E) · โฟลเดอร์ไลบรารีต้องอยู่ path ภาษาอังกฤษ (เช่น `C:\Arduino`) ไม่งั้น compile ไม่ผ่าน
**บอร์ดใน Arduino IDE:** ESP32 (esp32 by Espressif)

**ค่าที่ต้องตั้งก่อน flash** (บรรทัดต้นไฟล์):
| ค่า | ความหมาย |
|---|---|
| `APN` | APN ของค่ายซิม (AIS/True = `internet`, DTAC = `www.dtac.co.th`) |
| `SB_HOST` / `SB_ANON` | host และ anon key ของโปรเจกต์ Supabase (ใส่ไว้แล้ว) |
| `DEVICE_ID` | ชื่อทุ่น (ค่าเริ่มต้น `buoy-01`) |
| `USE_FAKE` | `0` = อ่านเซนเซอร์จริง · `1` = ค่าจำลอง (ทดสอบ 4G โดยไม่ต่อเซนเซอร์) |

**การทำงานทุก 15 วินาที:**
1. อ่านเฟรม Modbus `0x2600` ครั้งเดียว → ใช้ 7 ค่า (DO, DO%, อุณหภูมิ, pH, ความเค็ม, EC, ความขุ่น)
2. อ่านไม่สำเร็จ → ลองใหม่ได้สูงสุด 3 ครั้ง (เว้น 250 ms) · ถ้าเฟรมผ่านแต่ pH/DO/EC เป็น 0 ทั้งหมด = ถือว่าอ่านไม่สำเร็จ
3. อ่านพิกัด GPS (`AT+CGNSSINFO`) ถ้าล็อกดาวได้
4. ส่ง JSON ขึ้น Supabase 1 ครั้ง — อ่านเซนเซอร์ได้ = ส่งค่า + `sensor_ok=true` · อ่านไม่ได้ = ส่ง **heartbeat** `sensor_ok=false` (แยก "เซนเซอร์เสีย" ออกจาก "ทุ่น/4G หลุด")

**HTTPS:** A7670E ต่อ TLS socket ไม่เสถียร → ใช้ **HTTP application ในตัวโมเด็ม** (`AT+HTTP...`) และต้อง **เปิด `enableSNI`** ไม่งั้นได้ error 715 เพราะ Supabase อยู่หลัง Cloudflare

**ผลใน Serial Monitor (115200):**
- `✅ insert สำเร็จ (201)` = บันทึกสำเร็จ
- `HTTP 400` = มี field ใน JSON ที่ไม่มีคอลัมน์รองรับ
- `HTTP 401/403` = anon key ผิด หรือ RLS ยังไม่ตั้ง (รัน `schema.sql` ไม่ครบ)
- ค่าติดลบ = ปัญหาเน็ต 4G / SSL / APN

> แจ้งเตือนและบอท Telegram **ไม่อยู่บนทุ่น** — ทำฝั่งคลาวด์ทั้งหมด (ดู [`../supabase/`](../supabase/)) ทุ่นทำแค่อ่านค่าแล้วส่ง

---

## หมายเหตุ
- Free tier จะ **pause หลังไม่มีการใช้งาน 7 วัน** — ทุ่นที่ส่งข้อมูลต่อเนื่องจะไม่ pause; ถ้าปิดทุ่นนานเกินสัปดาห์ให้กด unpause ในหน้า Supabase
- ความปลอดภัย: anon key เป็น public key — สิทธิ์ถูกจำกัดด้วย RLS (insert/read ตาราง readings เท่านั้น)
- กราฟย้อนหลัง: query ตาราง `readings` หรือ view `readings_daily` (ค่าเฉลี่ยรายวัน)
