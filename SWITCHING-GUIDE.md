# 🔄 คู่มือสลับแหล่งข้อมูล (Supabase ↔ Server อาจารย์)

เอกสารนี้บอก **ทุกจุดที่ต้องแก้** เวลาสลับระหว่างแผนหลัก (Supabase) กับแผนสำรอง (Server อาจารย์)

## แนวคิดสำคัญ: มีแค่ 2 ฝั่งที่ต้องแก้
```
   [ฝั่ง ESP32]  ─ ส่งข้อมูลขึ้น ─→  [Cloud]  ─ ดึงมาแสดง ─→  [ฝั่ง Dashboard]
    แก้ 1 จุด                                                    แก้ 1 จุด
```
เปลี่ยน cloud = แก้แค่ **2 ไฟล์** เท่านั้น: โค้ด ESP32 (1 ไฟล์) + Dashboard (1 ไฟล์)

---

## ตารางสรุป: แต่ละโหมดใช้ไฟล์ไหน / แก้อะไร

| | 🟢 Supabase (แผนหลัก) | 🟡 Server อาจารย์ (แผนสำรอง) | ⚪ Mock (ทดสอบจอ) |
|---|---|---|---|
| **โค้ด ESP32** | `3-supabase-primary/Full-Version/Full-Version.ino` | `4-server-backup/esp32-server-test.ino` | — (ไม่ต้องมีอุปกรณ์) |
| **Dashboard `CONFIG.mode`** | `'supabase'` | `'server'` | `'mock'` |
| **ต้องกรอกใน Dashboard** | `CONFIG.supabase = {url, anonKey}` | `CONFIG.apiBase = '...'` | — |
| **ฝั่ง cloud ต้องเตรียม** | สร้าง Supabase project, รัน schema | ติดตั้ง PHP+MySQL, รัน db.sql | — |
| **Telegram / 6 โหมด / admin** | Edge Functions (`supabase/`) | PHP (`4-server-backup/api` + `lib`) | — |

---

## 🟢 สลับไปใช้ SUPABASE (แผนหลัก)

### จุดที่ 1 — Dashboard: `2-dashboard/index.html`
หา `const CONFIG` (ราวบรรทัด 427) แล้วแก้ **2 ที่**:
```js
const CONFIG = {
  mode: 'supabase',                    // ← (1) เปลี่ยนเป็น 'supabase'
  device: 'buoy-01',
  supabase: {                          // ← (2) เอาค่าจาก Supabase > Project Settings > API
    url:     "https://xxxx.supabase.co",
    anonKey: "eyJhbGciOi....",
  },
  ...
};
```
> ค่าใน `supabase{}` เอามาจากไหน → ดู `3-supabase-primary/README.md` ขั้นที่ 3

### จุดที่ 2 — ESP32: ใช้ไฟล์ `3-supabase-primary/Full-Version/Full-Version.ino`
แก้ค่าซิม + host/key (host + anon key ใส่ไว้ให้แล้ว แก้เฉพาะถ้าเปลี่ยนโปรเจกต์):
```cpp
const char APN[]   = "www.dtac.co.th";   // DTAC=www.dtac.co.th, AIS/True=internet
const char GUSER[] = "";
const char GPASS[] = "";
#define USE_FAKE 0                        // 0 = อ่าน sensor จริง, 1 = ค่าปลอมทดสอบ
```

### ✅ Checklist Supabase
- [ ] สร้าง Supabase project + รัน `3-supabase-primary/schema.sql` แล้ว
- [ ] (ฝั่งบก) รัน `supabase/schema-telegram.sql` + `schema-settings.sql` + deploy 3 ฟังก์ชัน
- [ ] Dashboard: `mode = 'supabase'` และกรอก `supabase{}` ครบ
- [ ] ESP32: ตั้ง APN ตรงค่ายซิม + `USE_FAKE=0`
- [ ] (ถ้าใช้ /admin) ตั้ง `ADMIN_USER`/`ADMIN_PASS` secret + Rewrite `/admin`→`/index.html` บน Render

---

## 🟡 สลับไปใช้ SERVER อาจารย์ (แผนสำรอง)

### จุดที่ 1 — Dashboard: `2-dashboard/index.html`
```js
const CONFIG = {
  mode: 'server',                                        // ← (1) เปลี่ยนเป็น 'server'
  device: 'buoy-01',
  ...
  apiBase: 'https://server-อาจารย์.ac.th/buoy/api',      // ← (2) URL โฟลเดอร์ api จริง
};
```
> `apiBase` คือที่อยู่ของโฟลเดอร์ `api/` ที่อัปโหลดขึ้น server (ไม่ต้องมี `/` ปิดท้าย)

### จุดที่ 2 — ESP32: ใช้ไฟล์ `4-server-backup/esp32-server-test.ino`
แก้บรรทัดบนสุด:
```cpp
#define SERVER_URL  "https://server-อาจารย์.ac.th/buoy/api/save.php"  // ← ชี้ที่ save.php
#define API_KEY     "buoy-secret-2026"     // ← ต้องตรงกับ config.php
```

### จุดที่ 3 — Server: ตั้งค่า `4-server-backup/config.php`
```php
define('DB_NAME', 'buoy');
define('DB_USER', 'buoy_user');
define('DB_PASS', 'รหัสจริง');
define('API_KEY', 'buoy-secret-2026');   // ← ต้องตรงกับใน ESP32
// Telegram + 6 โหมด: TELEGRAM_BOT_TOKEN, TG_ADMINS, TELEGRAM_WEBHOOK_SECRET
```

### ✅ Checklist Server
- [ ] รัน `db.sql` สร้างฐานข้อมูลแล้ว (readings ครบทุกคอลัมน์ + app_settings + tg_subscribers + alert_state)
- [ ] `config.php`: กรอก DB + ตั้ง API_KEY (+ Telegram token/secret ถ้าใช้)
- [ ] อัปโหลดโฟลเดอร์ขึ้น server แล้ว (เช่น `public_html/buoy/`)
- [ ] Dashboard: `mode = 'server'` และ `apiBase` ถูกต้อง
- [ ] ESP32: `SERVER_URL` ชี้ที่ save.php, `API_KEY` ตรงกับ config.php
- [ ] server มี **public IP/โดเมน** ที่ ESP32 เข้าถึงได้ (สำคัญสุด!)

---

## ⚠️ กับดักที่เจอบ่อยเวลาสลับ

| อาการ | สาเหตุ | แก้ |
|-------|--------|-----|
| Dashboard ค่าไม่ขึ้น (supabase) | `url`/`anonKey` ผิด หรือ `mode` ยังเป็น mock | เช็ค Console (F12) หา error, ตรวจ RLS + realtime เปิดแล้ว |
| Dashboard ค่าไม่ขึ้น (server) | CORS หรือ apiBase ผิด | เปิด `apiBase/latest.php` ตรงๆใน browser ดูว่าได้ JSON ไหม |
| ESP32 ส่งไม่ขึ้น (supabase) | error 715 (SNI) / anon key / APN | เปิด `enableSNI`, ดู Serial Monitor ต้องได้ HTTP 201 |
| ESP32 ส่งไม่ขึ้น (server) | API_KEY ไม่ตรง / server เข้าไม่ถึง | ดู Serial Monitor, ลองเปิด save.php ใน browser |
| ต่อผ่าน 4G ไม่ได้ แต่ WiFi ได้ | server เป็น LAN ภายใน (server อาจารย์) | ต้องมี public IP — Supabase ไม่มีปัญหานี้ |
| เปลี่ยนโหมด 6 ประเภทไม่ได้ | ยังไม่ล็อกอิน admin / secret ไม่ตั้ง | เข้า `/admin` ล็อกอิน หรือใช้ `/mode` จาก chat_id ใน `TG_ADMINS` |

> 💡 **ข้อสังเกต:** `device` (เช่น `'buoy-01'`) ต้องเหมือนกันทั้ง 3 ที่ (ESP32, Dashboard, และข้อมูลใน cloud) เสมอ ไม่ว่าโหมดไหน
> 📘 รายละเอียดระบบเต็ม → [5-extensions/06-system-documentation.md](5-extensions/06-system-documentation.md)
