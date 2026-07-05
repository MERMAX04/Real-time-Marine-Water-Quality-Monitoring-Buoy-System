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
3. จะได้ตาราง `readings` + RLS policy (anon insert/read) + เปิด realtime + view รายวัน

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
2. ใส่ค่าเช่น `device=buoy-01, do_val=6.2, sal=32, turb=8, chl=3, orp=280, oil=1.2, algae=8000` → Save
3. เปิด `2-dashboard/index.html` → **ค่าต้องขึ้นทันที** และถ้า insert แถวใหม่ Dashboard เปลี่ยน**สดๆ** = realtime ทำงาน ✅

## ขั้นที่ 5 — ต่อ ESP32
มี 2 เวอร์ชัน:
| ไฟล์ | ใช้ตอนไหน | เน็ต |
|------|-----------|------|
| `esp32-supabase-test.ino` | **ทดสอบบนโต๊ะ** (ก่อนได้โมดูล 4G) | WiFi |
| `esp32-supabase-4g.ino` | **ของจริงบนทุ่น** | โมดูล 4G (LilyGO T-A7670G) |

**เวอร์ชัน 4G (`esp32-supabase-4g.ino`):**
- ฮาร์ดแวร์แนะนำ: **LilyGO T-A7670G** (ESP32 + 4G LTE Cat-1 + GPS + จัดการไฟ ในบอร์ดเดียว — เสถียรสุด)
- ติดตั้งไลบรารี **TinyGSM** (เวอร์ชันล่าสุดจาก GitHub รองรับ A7670)
- แก้: `APN` ของค่ายซิม (AIS/True=`internet`), host + anon key ใส่ให้แล้ว
- Supabase เป็น HTTPS → ใช้ `TinyGsmClientSecure` + `setInsecure()` (A7670 รองรับ SSL)
- 🎯 บอนัส: A7670G มี **GPS** ในตัว — เปิดใช้เอาพิกัดจริงมาโชว์ได้ (ดูคอมเมนต์ท้ายไฟล์)

**ผลที่ควรได้ (ทั้ง 2 เวอร์ชัน):** Serial Monitor ขึ้น **HTTP 201** = insert สำเร็จ
- **401/403** = RLS หรือ anon key ผิด (เช็ค schema.sql รันครบ)
- **400** = คอลัมน์ผิด (JSON field ไม่ตรงตาราง)
- **ต่อ https ไม่ได้** = เน็ต 4G/เสาอากาศ/APN

---

## ⚠️ 3 จุดที่วางแผนไว้แล้ว (กันเสียเวลาทีหลัง)
| จุด | จัดการแล้วที่ |
|-----|--------------|
| **HTTPS** บน ESP32 | `client.setInsecure()` ในโค้ด .ino |
| **RLS** (ไม่งั้น 403) | policy anon insert/read ใน `schema.sql` |
| **anon key** | ใช้ public key + RLS จำกัดสิทธิ์ |

## หมายเหตุ
- Free tier จะ **pause หลังไม่ใช้งาน 7 วัน** — ทุ่นที่ส่งข้อมูลตลอดจะไม่ pause; ถ้าปิดทุ่นนานเกินสัปดาห์ให้กด unpause
- ต่อจริงแล้วอยากปลอดภัยขึ้น: จำกัด RLS ให้ insert อย่างเดียว หรือใช้ Postgres role แยก
- ทำกราฟย้อนหลัง: query `readings` หรือ view `readings_daily` ด้วย supabase-js ได้เลย
