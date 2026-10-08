# 3-supabase-primary — แผนหลัก (Supabase / PostgreSQL)

เส้นทางข้อมูล:
```
ESP32 (4G/WiFi) --HTTPS POST--> Supabase (ตาราง readings) --realtime--> Dashboard
```
Supabase = Backend-as-a-Service บน PostgreSQL (ข้อมูลเป็นตารางเหมือน Excel, query ด้วย SQL ได้)

---

## ขั้นที่ 1 — สร้าง Supabase Project
1. เข้า https://supabase.com → New project (ล็อกอิน GitHub/Google)
2. ตั้งชื่อ เช่น `buoy-monitor`, ตั้งรหัส database, เลือก region **Southeast Asia (Singapore)**
3. รอสร้างเสร็จ ~2 นาที

## ขั้นที่ 2 — สร้างตาราง + สิทธิ์ + realtime
1. เมนูซ้าย **SQL Editor** → New query
2. วางเนื้อหาไฟล์ `schema.sql` ทั้งหมด → กด **Run**
3. จะได้ตาราง `readings` (ค่าน้ำทุกคอลัมน์ + `sensor_ok` heartbeat + `lat`/`lon` — คอลัมน์ chl/orp/oil/algae ยังมีแต่ทุ่นไม่ได้ส่ง เพราะไม่ได้ติดโพรบ) + RLS policy (anon insert/read) + เปิด realtime + view รายวัน
> ฝั่งบก (Telegram/admin/6 โหมด) รัน `supabase/schema-telegram.sql` + `supabase/schema-settings.sql` เพิ่ม (ดู [`../supabase/README.md`](../supabase/README.md))

## ขั้นที่ 3 — เอา URL + anon key มาใส่ Dashboard
1. เมนูซ้าย **Project Settings (เฟือง)** → **API**
2. ก๊อป 2 ค่า:
   - **Project URL** → `https://xxxx.supabase.co`
   - **anon public** key (ยาว ขึ้นต้น `eyJ...`)
3. ใส่ใน `2-dashboard/index.html` ที่ `CONFIG.supabase`:
   ```js
   supabase: {
     url:     "https://xxxx.supabase.co",
     anonKey: "eyJhbGciOi....",
   },
   ```
4. เปลี่ยน `CONFIG.mode` เป็น `'supabase'`

## ขั้นที่ 4 — ทดสอบก่อนมี ESP32 (ไม่ต้องมีฮาร์ดแวร์!)
1. เมนูซ้าย **Table Editor** → ตาราง `readings` → **Insert row**
2. ใส่ค่าเช่น `device=buoy-01, do_val=6.2, do_pct=90, temp=30, ph=8.1, sal=32, cond=50, turb=8` → Save
3. เปิด `2-dashboard/index.html` → **ค่าต้องขึ้นทันที** และถ้า insert แถวใหม่ Dashboard เปลี่ยน**สดๆ** = realtime ทำงาน ✅

## ขั้นที่ 5 — ต่อ ESP32
| ไฟล์ | ใช้ตอนไหน | เน็ต |
|------|-----------|------|
| **`Full-Version/Full-Version.ino`** ⭐ | **ของจริงบนทุ่น** (อ่าน 7 ค่า + GPS + heartbeat + 4G) | โมดูล 4G (LilyGO T-Call-A7670 V1.0 / A7670E) |
| `Backup/esp32-supabase-test.ino` | ทดสอบบนโต๊ะ (ก่อนได้โมดูล 4G) | WiFi |
| `Backup/esp32-gps-test.ino` | ทดสอบ GPS อย่างเดียว | — |

**เวอร์ชันของจริง (`Full-Version/Full-Version.ino`) — บนทุ่น:**
- ฮาร์ดแวร์: **LilyGO T-Call-A7670 V1.0** (โมดูล A7670E — LTE Cat-1 + GNSS ในตัว) + เซนเซอร์ผ่าน **RS485-to-TTL**
  ⚠️ พิน V1.0 ต่างจากรุ่นทั่วไป: TX=26, **RX=25**, PWRKEY=4, **RST=27 (active LOW)**
- อ่านค่า: เฟรม bulk `0x2600` ครั้งเดียว ใช้ DO, ความขุ่น, EC, pH, อุณหภูมิ, ความเค็ม, DO% (ช่อง ORP/CHL/OIW-BGA ไม่ใช้ เพราะไม่ได้ติดโพรบ)
- ถ้าเฟรมผ่านแต่ pH/DO/EC เป็น 0 ทั้งหมด (sensor ยังไม่พร้อม) = ถือว่าอ่านไม่สำเร็จ ส่ง heartbeat แทน
- **heartbeat:** อ่าน sensor ไม่สำเร็จ → ส่ง `sensor_ok=false` (แยก "sensor เสีย" ออกจาก "ทุ่น/4G หลุด")
- โหมดทดสอบ: ตั้ง `USE_FAKE=1` เพื่อสร้างค่าปลอมครบ 7 ค่า (+TDS) (ตอนใช้จริงตั้ง `USE_FAKE=0`)
- ไลบรารี **TinyGSM** → ใช้ macro `TINY_GSM_MODEM_A7672X` (ครอบคลุม A7670E); ต้องอยู่ path อังกฤษ (`C:\Arduino`)
- แก้: `APN` ของค่ายซิม (DTAC=`www.dtac.co.th`, AIS/True=`internet`), host + anon key ใส่ให้แล้ว
- **HTTPS:** A7670E ต่อ TLS socket ไม่เสถียร → ใช้ **HTTP application ในตัวโมเด็ม** (AT+HTTP...) + **เปิด `enableSNI`** (ไม่งั้น error 715 เพราะ Supabase อยู่หลัง Cloudflare)
- 🎯 **GPS:** A7670E มี GNSS ในตัว — `AT+CGNSSPWR=1` + `AT+CGNSSINFO` เอาพิกัดจริงมาโชว์ (ทำในโค้ดแล้ว)
- 📲 **แจ้งเตือน/บอท Telegram ไม่อยู่บนทุ่นแล้ว** — ย้ายไปฝั่งคลาวด์ (ดู [`../supabase/`](../supabase/)) ทุ่นแค่อ่าน sensor แล้ว POST

**ผลที่ควรได้ (ทั้ง 2 เวอร์ชัน):** Serial Monitor ขึ้น **HTTP 201** = insert สำเร็จ
- **401/403** = RLS หรือ anon key ผิด (เช็ค schema.sql รันครบ)
- **400** = คอลัมน์ผิด (JSON field ไม่ตรงตาราง)
- **ต่อ https ไม่ได้** = เน็ต 4G/เสาอากาศ/APN

---

## ⚠️ 3 จุดที่วางแผนไว้แล้ว (กันเสียเวลาทีหลัง)
| จุด | จัดการแล้วที่ |
|-----|--------------|
| **HTTPS** บน ESP32 | HTTP-app ในตัวโมเด็ม + `enableSNI` (ไม่ใช่ TLS socket) |
| **RLS** (ไม่งั้น 403) | policy anon insert/read ใน `schema.sql` |
| **anon key** | ใช้ public key + RLS จำกัดสิทธิ์ |

## หมายเหตุ
- Free tier จะ **pause หลังไม่ใช้งาน 7 วัน** — ทุ่นที่ส่งข้อมูลตลอดจะไม่ pause; ถ้าปิดทุ่นนานเกินสัปดาห์ให้กด unpause
- ต่อจริงแล้วอยากปลอดภัยขึ้น: จำกัด RLS ให้ insert อย่างเดียว หรือใช้ Postgres role แยก
- ทำกราฟย้อนหลัง: query `readings` หรือ view `readings_daily` ด้วย supabase-js ได้เลย
