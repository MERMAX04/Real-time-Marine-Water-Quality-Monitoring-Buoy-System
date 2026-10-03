# supabase/ — ฝั่งบก (Edge Functions) 🏝️

Telegram (แจ้งเตือน + คำสั่งแชท), การประเมินตาม **6 โหมดมาตรฐานน้ำทะเลไทย** และ **สิทธิ์ admin** ทำที่ **Supabase** ทั้งหมด — ทุ่น ESP32 ทำแค่ อ่าน sensor → POST
ทำให้ทุ่น **เบา/ประหยัดไฟ/เสถียร** และ `/status` **ตอบเร็ว (<1 วิ)** เพราะอ่านค่าล่าสุดจาก DB ตรงๆ

```
ทุ่น ESP32 ──POST──► readings ──(INSERT trigger)──► telegram-alert ──► Telegram (เตือนตามโหมด)
                        ▲
ผู้ใช้ /start,/status,/swim,/mode ──► telegram-bot (webhook) ──► ตอบทันที
เว็บ /admin (ล็อกอิน) ──► admin (login/setmode) ──► เปลี่ยนโหมดใน app_settings
```

| ไฟล์ | หน้าที่ |
|------|---------|
| `schema-telegram.sql` | สร้างตาราง `tg_subscribers`, `alert_state` |
| `schema-settings.sql` | สร้างตาราง `app_settings` (เก็บโหมด) + **ถอนสิทธิ์ anon update** (เปลี่ยนโหมดได้เฉพาะผ่าน Edge Function admin) |
| `functions/telegram-bot/` | รับคำสั่งแชท (/start /status /swim /mode /stop /help) |
| `functions/telegram-alert/` | ประเมินตามโหมดตอนมีแถวใหม่ → เตือนเมื่อสถานะรวม "แดง" (cooldown 30 นาที) |
| `functions/admin/` | ตรวจรหัสผ่าน admin (login) + เปลี่ยนโหมดแบบปลอดภัย (setmode) ด้วย service_role |
| `functions/_shared/water-modes.json` | **config กลาง** เกณฑ์ 6 โหมด (คัดลอกไป `2-dashboard/` และ `4-server-backup/lib/`) |
| `functions/_shared/water-eval.ts` | ตรรกะประเมิน `evalWater` + Model A (วิกฤต→แดง / เฝ้าระวัง→ส้ม) + get/setMode |
| `functions/_shared/telegram.ts` | ส่งข้อความ + จัดรูป /status /swim + สถานะทุ่น (ใช้ร่วม) |

---

## ติดตั้ง (ทำครั้งเดียว ~10 นาที)

### 0) เตรียม
- มี **Bot Token** จาก @BotFather แล้ว
- ติดตั้ง Supabase CLI: `npm i -g supabase` (หรือ `scoop install supabase`) แล้ว `supabase login`
- ผูกโปรเจกต์:  `supabase link --project-ref jjbrgolulggksxnuicgg`

### 1) สร้างตาราง
เปิด **Supabase Studio → SQL Editor** → วางเนื้อหา `schema-telegram.sql` **และ** `schema-settings.sql` → Run (ทีละไฟล์)

### 2) ตั้งความลับ (secrets) ของ Edge Functions
```bash
supabase secrets set TELEGRAM_BOT_TOKEN="123456789:ABC...ใส่ token จริง"
supabase secrets set TELEGRAM_WEBHOOK_SECRET="buoy-hook-2026"   # ตั้งเองอะไรก็ได้ (ลับๆ)
supabase secrets set ALERT_WEBHOOK_SECRET="buoy-alert-2026"     # ตั้งเองอะไรก็ได้ (ลับๆ)
supabase secrets set TG_ADMINS="123456789,987654321"           # chat_id ที่ใช้ /mode ได้ (คั่นด้วย ,)
supabase secrets set ADMIN_USER="admin"                        # ชื่อผู้ใช้ล็อกอินเว็บ /admin
supabase secrets set ADMIN_PASS="ใส่รหัสผ่านที่ปลอดภัย"          # รหัสผ่าน admin (เว็บ /admin)
```
> `SUPABASE_URL` / `SUPABASE_SERVICE_ROLE_KEY` ไม่ต้องตั้ง — Supabase ใส่ให้อัตโนมัติ
> 💡 ตั้ง secret ไม่ติด (403 account privileges)? → `set SUPABASE_ACCESS_TOKEN=sbp_...` (จาก Account Tokens) ในเทอร์มินัลนั้นก่อน

### 3) deploy ทั้ง 3 ฟังก์ชัน
```bash
supabase functions deploy telegram-bot   --no-verify-jwt
supabase functions deploy telegram-alert --no-verify-jwt
supabase functions deploy admin          --no-verify-jwt
```

### 4) ชี้ Telegram webhook มาที่ telegram-bot
(แทน `<TOKEN>` และใช้ค่า secret เดียวกับข้อ 2)
```bash
curl "https://api.telegram.org/bot<TOKEN>/setWebhook?url=https://jjbrgolulggksxnuicgg.supabase.co/functions/v1/telegram-bot&secret_token=buoy-hook-2026"
```
ได้ `{"ok":true,...}` = สำเร็จ (เช็คได้ที่ `/getWebhookInfo`)

### 5) ตั้ง Database Webhook ให้เตือนตอนมีข้อมูลใหม่
Supabase Studio → **Database → Webhooks → Create**
- Table: `public.readings` · Events: **Insert**
- Type: **HTTP Request** · Method **POST**
- URL: `https://jjbrgolulggksxnuicgg.supabase.co/functions/v1/telegram-alert`
- HTTP Headers: เพิ่ม `x-alert-secret` = `buoy-alert-2026` (ให้ตรงข้อ 2)

---

## ทดสอบ
1. ทักบอท `/start` → ได้ข้อความต้อนรับ (แถว `tg_subscribers` เพิ่มขึ้น)
2. พิมพ์ `/status` → บอทตอบค่าล่าสุด **ทันที**
2b. พิมพ์ `/swim` → บอทตอบ **ธงลงเล่นน้ำ** (เขียว/เหลือง/แดง) + เหตุผล — ดูเกณฑ์ที่ [`5-extensions/05-blueflag-swim-safety.md`](../5-extensions/05-blueflag-swim-safety.md)
3. ทดสอบเตือน: Studio → SQL Editor รันแทรกค่าที่เข้าเกณฑ์ เช่น
   ```sql
   insert into readings (device, do_val, ph, temp, turb) values ('buoy-01', 2.0, 8.0, 30, 5);
   ```
   → ทุก subscriber ต้องได้ 🌊 แจ้งเตือน (DO ต่ำ)
4. ดู log: `supabase functions logs telegram-alert`

## แก้เกณฑ์ 6 โหมด (ไม่ต้อง flash ทุ่น!)
เกณฑ์ทั้ง 6 โหมดอยู่ที่ **`functions/_shared/water-modes.json`** (config กลาง) — แก้แล้ว deploy ฟังก์ชันที่ใช้ร่วมใหม่
> ⚠️ ไฟล์นี้มี 3 ชุดต้องตรงกัน: `supabase/functions/_shared/`, `2-dashboard/`, `4-server-backup/lib/` — แก้แล้วคัดลอกให้ครบทั้ง 3 ที่
(ตรรกะประเมินอยู่ที่ `functions/_shared/water-eval.ts` · cooldown อยู่ที่ `functions/telegram-alert/index.ts` → `COOLDOWN_MS`)

## เปลี่ยนโหมดมาตรฐานน้ำทะเล
- **ผ่านเว็บ:** เปิด `…/admin` → ล็อกอิน (ADMIN_USER/ADMIN_PASS) → เลือกโหมดจาก dropdown
- **ผ่าน Telegram:** พิมพ์ `/mode` (เฉพาะ chat_id ใน `TG_ADMINS`) → เลือกโหมด
- ทั้งสองทางเขียนลง `app_settings.key='mode'` ตัวเดียวกัน → มีผลทันทีทั้งเว็บ/บอท/แจ้งเตือน
