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

## 5.4 รันแบบ Local (Supabase CLI + Docker)
จำลอง backend ทั้งชุด (DB + REST + Realtime + Edge Functions + Studio) บนเครื่องตัวเอง ไว้พัฒนา/ทดสอบโดยไม่แตะ cloud
**cloud ยังเป็น production เหมือนเดิม** — local กับ cloud แยกกันคนละฐานข้อมูล

| ไฟล์ | หน้าที่ |
|---|---|
| `supabase/config.toml` | ตั้งค่า local (`project_id = "buoy-local"`, พอร์ต, `verify_jwt = false` ทั้ง 3 ฟังก์ชันเหมือน cloud) |
| `supabase/migrations/20261014120000_init.sql` | schema = 3 ไฟล์ SQL ของ cloud รวมกัน (+ GRANT ที่ local ต้องมี) — **แก้ schema ต้องแก้ให้ตรงกันทั้งสองที่** |
| `supabase/seed.sql` | ข้อมูลตั้งต้น: `mode=recreation` + ค่าจำลองย้อนหลัง 3 ชม. (ทุก 15 วิ) + trigger แทน Database Webhook → `telegram-alert` (**local เท่านั้น**) |
| `supabase/.env.local.example` | secrets ของ Edge Functions แบบ local (ค่าปลอดภัย) → คัดลอกเป็น `supabase/.env.local` |
| `1-sensor-tools/simulate_buoy.py` | ทุ่นจำลอง POST ค่าทุก 15 วิ (JSON เดียวกับ ESP32) |

### เตรียม (ครั้งเดียว)
1. ติดตั้ง **Docker Desktop** แล้วเปิดไว้ (รอจน Engine running) + Supabase CLI (มีแล้วจากขั้นที่ 2)
2. คัดลอก secrets ตัวอย่าง: `copy supabase\.env.local.example supabase\.env.local`

### รัน (จาก root ของ repo)
```bash
supabase start                 # ครั้งแรกโหลด image นานหน่อย · สร้าง DB + รัน migrations + seed.sql
supabase status                # ดู URL และ key (anon/publishable, service_role/secret) · -o env = แบบตัวแปร
supabase functions serve --env-file supabase/.env.local    # เปิด Edge Functions (ค้างหน้าต่างนี้ไว้ ดู log ที่นี่)
```
| บริการ | URL (ค่าเริ่มต้น) |
|---|---|
| API (REST / Realtime / Functions) | `http://127.0.0.1:54321` · ฟังก์ชัน `http://127.0.0.1:54321/functions/v1/<ชื่อ>` |
| Studio (ดูตาราง/รัน SQL) | `http://127.0.0.1:54323` |
| Postgres | `postgresql://postgres:postgres@127.0.0.1:54322/postgres` |

ส่งค่าจำลอง + เปิดเว็บ (อีกหน้าต่าง):
```bash
cd 1-sensor-tools && py simulate_buoy.py          # ทุก 15 วิ · --once ครั้งเดียว · --red ทดสอบเตือน (DO ต่ำ)
cd 2-dashboard && py -m http.server 8000          # เปิด http://localhost:8000
```
> เปิดเว็บที่ `localhost` / `127.0.0.1` แล้วแดชบอร์ดจะใช้ Supabase local ให้อัตโนมัติ (เปิดจาก onrender.com = cloud)

คำสั่งอื่นที่ใช้บ่อย:
```bash
supabase db reset              # ล้าง DB local แล้วรัน migrations + seed.sql ใหม่ (ใช้หลังแก้ schema/seed)
supabase stop                  # ปิดทั้งหมด (ข้อมูลยังอยู่ใน volume) · --no-backup = ลบทิ้ง
```

### ทดสอบฟังก์ชันแบบ local
- **admin:** `curl -X POST http://127.0.0.1:54321/functions/v1/admin -H "content-type: application/json" -d "{\"action\":\"login\",\"user\":\"admin\",\"pass\":\"localdev123\"}"` → `{"ok":true,"token":...}`
- **telegram-alert:** `py simulate_buoy.py --once --red` → trigger ใน DB ยิงไปที่ฟังก์ชันเอง ดู log ในหน้าต่าง `functions serve` (ไม่มีผู้สมัคร/token ปลอม = ไม่มีข้อความจริงออก) · ผลฝั่ง DB: Studio → SQL `select * from net._http_response order by created desc limit 5;`
- **telegram-bot:** ยิงจำลองข้อความด้วย `curl` ใส่ header `x-telegram-bot-api-secret-token: local-hook-secret` และ body แบบ Telegram update (`{"message":{"chat":{"id":111111111},"text":"/status"}}`)

### ข้อจำกัดของ local
- **ทุ่นจริง (4G)** ส่งเข้า `127.0.0.1` ไม่ได้ — firmware ชี้ cloud (`SB_HOST`) เสมอ · ทดสอบ local ใช้ `simulate_buoy.py` หรือ `sensor_to_supabase.py` (แก้ `SB_URL`/`SB_ANON`)
- **Telegram webhook** เรียก localhost ไม่ได้ — ต้องใช้ tunnel (เช่น ngrok / cloudflared) + บอททดสอบแยก **อย่า** `setWebhook` บอท production มาที่เครื่อง
- Database Webhook ของ cloud ไม่ถูก export → local ใช้ trigger ใน `seed.sql` แทน (ชี้ `http://kong:8000/...` ในเครือข่าย docker)
- ข้อมูล local/cloud ไม่ซิงก์กัน · ห้าม `supabase db push --include-seed` (seed เป็นข้อมูลจำลอง)
