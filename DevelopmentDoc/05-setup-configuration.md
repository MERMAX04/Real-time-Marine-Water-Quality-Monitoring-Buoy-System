# 5. Setup & Configuration Guide

## 5.1 ค่าที่ต้องตั้ง (Environment variables / Configuration)

### Supabase Edge Function secrets (ตั้งด้วย `supabase secrets set NAME="value"`)
| ชื่อ | ใช้ใน | ความหมาย | ตัวอย่าง |
|---|---|---|---|
| `TELEGRAM_BOT_TOKEN` | telegram-bot, telegram-alert | token ของบอทจาก @BotFather | `123456789:ABC...` |
| `TELEGRAM_WEBHOOK_SECRET` | telegram-bot | ค่าลับที่ Telegram แนบมาใน header (ตั้งตอน `setWebhook`) | ตั้งเอง |
| `ALERT_WEBHOOK_SECRET` | telegram-alert | ค่าลับใน header `x-alert-secret` ของ Database Webhook | ตั้งเอง |
| `TG_ADMINS` | telegram-bot | chat_id ที่เปลี่ยนโหมดผ่าน `/mode` ได้ (คั่นด้วย `,`) | `123456789,987654321` |
| `ADMIN_USER` | admin | ชื่อผู้ใช้สำหรับล็อกอินเว็บ `/admin` | `admin` |
| `ADMIN_PASS` | admin | รหัสผ่านแอดมิน (ใช้สร้าง token ด้วย) | ตั้งให้ปลอดภัย |
| `SUPABASE_URL`, `SUPABASE_SERVICE_ROLE_KEY` | ทุกฟังก์ชัน | **Supabase ใส่ให้อัตโนมัติ** ไม่ต้องตั้ง | – |

> ห้าม commit token จริงลง git — ใช้สคริปต์ส่วนตัว `supabase/set-secrets.local.ps1` (ถูก `.gitignore` ไว้แล้ว)

### Firmware (`3-supabase-primary/Full-Version/Full-Version.ino` บรรทัดต้นไฟล์)
| ค่า | ความหมาย | ค่าปัจจุบัน |
|---|---|---|
| `APN` / `GUSER` / `GPASS` | APN ของซิม 4G | `internet` (AIS/True) · DTAC ใช้ `www.dtac.co.th` |
| `SB_HOST` | host ของ Supabase (ไม่มี `https://`) | `jjbrgolulggksxnuicgg.supabase.co` |
| `SB_ANON` | anon public key | ใส่แล้ว |
| `DEVICE_ID` | รหัสทุ่น | `buoy-01` |
| `USE_FAKE` | `0` อ่านเซนเซอร์จริง / `1` ส่งค่าจำลอง | `0` |
| `SENSOR_ADDR`, `SENSOR_BAUD` | Modbus address / baud | `0x01`, `9600` |
| `PIN_485_RX`, `PIN_485_TX` | ขา UART ไปโมดูล RS485 | `32`, `33` |

### เว็บแดชบอร์ด (`2-dashboard/index.html` → `const CONFIG`)
| ค่า | ความหมาย |
|---|---|
| `mode` | `'supabase'` (ข้อมูลจริง) · `'server'` (แผนสำรอง PHP) · `'mock'` (ข้อมูลจำลอง) |
| `device` | รหัสทุ่นที่จะแสดง (`buoy-01`) |
| `supabase.url`, `supabase.anonKey` | URL และ anon key ของโปรเจกต์ Supabase |
| `apiBase` | URL โฟลเดอร์ `api` ของแผนสำรอง (ใช้เมื่อ `mode:'server'`) |

### Edge Function (ค่าคงที่ในโค้ด)
| ค่า | ไฟล์ | ค่าปัจจุบัน |
|---|---|---|
| `DEVICE` | `supabase/functions/_shared/telegram.ts` | `buoy-01` |
| `COOLDOWN_MS` | `supabase/functions/telegram-alert/index.ts` | 30 นาที |

### แผนสำรอง PHP (`4-server-backup/config.php`)
`DB_HOST`, `DB_NAME`, `DB_USER`, `DB_PASS`, `API_KEY`, `DEVICE_ID`, `TELEGRAM_BOT_TOKEN`, `TELEGRAM_WEBHOOK_SECRET`, `TG_ADMINS`

### เกณฑ์ 6 โหมด (`water-modes.json`)
ต้นฉบับ `supabase/functions/_shared/water-modes.json` — **ต้องคัดลอกให้ตรงกัน 3 ที่** (`2-dashboard/` และ `4-server-backup/lib/`)

## 5.2 Dependencies ที่ต้องติดตั้ง
| ส่วน | Dependency | วิธีติดตั้ง |
|---|---|---|
| Firmware | Arduino IDE 2.x | https://www.arduino.cc/en/software |
| | ESP32 board package (Espressif, ทดสอบกับ 3.3.x) | Board Manager URL: `https://espressif.github.io/arduino-esp32/package_esp32_index.json` |
| | ไลบรารี **TinyGSM** (ทดสอบกับ 0.12.0) | Library Manager — โฟลเดอร์ไลบรารีต้องอยู่ path ภาษาอังกฤษ เช่น `C:\Arduino` |
| Edge Functions | Supabase CLI | `npm i -g supabase` แล้ว `supabase login` |
| | Deno (เฉพาะถ้าต้องการตรวจ type ในเครื่อง) | `deno check supabase/functions/telegram-bot/index.ts` |
| เว็บ | ไม่มี (HTML/JS ล้วน) · supabase-js โหลดจาก CDN | – |
| เครื่องมือเซนเซอร์ | Python 3 + pyserial | `py -m pip install pyserial` |
| แผนสำรอง | PHP 8 + MySQL/MariaDB (PDO MySQL) | ตามผู้ให้บริการเซิร์ฟเวอร์ |
| ฮาร์ดแวร์ทดสอบ | สาย USB-to-RS485 · แหล่งจ่าย 12V ≥1A สำหรับเซนเซอร์ | – |

## 5.3 ขั้นตอน Deployment

### ขั้นที่ 1 — ฐานข้อมูล Supabase
1. สร้างโปรเจกต์ที่ https://supabase.com (region Singapore)
2. SQL Editor → รันทีละไฟล์: `3-supabase-primary/schema.sql` → `supabase/schema-settings.sql` → `supabase/schema-telegram.sql`
3. ตรวจ: Table Editor ต้องมี `readings`, `app_settings` (มีแถว `mode=recreation`), `tg_subscribers`, `alert_state`

### ขั้นที่ 2 — Edge Functions
```bash
supabase login
supabase link --project-ref <project-ref>
supabase secrets set TELEGRAM_BOT_TOKEN="..." TELEGRAM_WEBHOOK_SECRET="..." ALERT_WEBHOOK_SECRET="..."
supabase secrets set TG_ADMINS="<chat_id>" ADMIN_USER="admin" ADMIN_PASS="<รหัส>"
supabase functions deploy telegram-bot   --no-verify-jwt
supabase functions deploy telegram-alert --no-verify-jwt
supabase functions deploy admin          --no-verify-jwt
```
> ถ้า `secrets set` ขึ้น 401/403: ล้างค่า `SUPABASE_ACCESS_TOKEN` ที่ค้างในเทอร์มินัล (`set SUPABASE_ACCESS_TOKEN=`) แล้ว `supabase login` ใหม่ด้วยบัญชีเจ้าของโปรเจกต์

### ขั้นที่ 3 — เชื่อม Telegram และ Database Webhook
1. ชี้ webhook ของบอท:
   ```
   https://api.telegram.org/bot<TOKEN>/setWebhook?url=https://<project-ref>.supabase.co/functions/v1/telegram-bot&secret_token=<TELEGRAM_WEBHOOK_SECRET>
   ```
2. Supabase Studio → Database → Webhooks → Create: table `public.readings` · event **Insert** · HTTP POST ไป `https://<project-ref>.supabase.co/functions/v1/telegram-alert` · header `x-alert-secret: <ALERT_WEBHOOK_SECRET>`

### ขั้นที่ 4 — เว็บแดชบอร์ดบน Render
1. ใส่ `CONFIG.supabase.url` / `anonKey` ใน `2-dashboard/index.html` แล้ว push ขึ้น GitHub
2. Render → New **Static Site** → เชื่อม repo · Branch `main` · Build Command: ว่าง · **Publish Directory: `2-dashboard`**
3. Settings → **Redirects/Rewrites**: เพิ่ม Rewrite `/admin` → `/index.html` (ให้หน้า `/admin` เปิดได้)
4. หลังจากนี้ push ขึ้น `main` แล้ว Render deploy ให้อัตโนมัติ

### ขั้นที่ 5 — Firmware ทุ่น
1. ต่อวงจรตาม [`5-extensions/07-thesis-chapters.md`](../5-extensions/07-thesis-chapters.md) หัวข้อ 3.5.1 (GND ร่วมครบทุกตัว)
2. Arduino IDE เปิด `Full-Version.ino` → ตั้ง APN ของซิม → เลือกบอร์ด ESP32 → Upload
3. Serial Monitor 115200 ต้องเห็น `✅ insert สำเร็จ (201)` ทุก 15 วินาที

### ขั้นที่ 6 — ตรวจทั้งระบบ
| ตรวจ | ผลที่ต้องได้ |
|---|---|
| เปิดเว็บ | การ์ด 7 ค่าอัปเดต · สถานะ "ทุ่นออนไลน์" · แผนที่ขึ้นตำแหน่ง |
| เว็บ `/admin` ล็อกอิน | เห็นแถบเลือกโหมด · เปลี่ยนแล้ว Telegram `/mode` เห็นโหมดใหม่ |
| Telegram `/start` → `/status` | ได้ค่าล่าสุดแยกสี |
| insert แถว `do_val=2` (โหมดนันทนาการ) | ผู้สมัครได้แจ้งเตือนสถานะรวมแดง |

### แผนสำรอง (PHP + MySQL)
ดูขั้นตอนใน [`4-server-backup/README.md`](../4-server-backup/README.md) และวิธีสลับระบบใน [`SWITCHING-GUIDE.md`](../SWITCHING-GUIDE.md)
