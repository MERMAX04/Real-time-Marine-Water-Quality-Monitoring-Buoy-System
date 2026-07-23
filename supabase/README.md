# supabase/ — Telegram ฝั่งบก (Edge Functions) 🏝️

Telegram (แจ้งเตือน + คำสั่งแชท) ย้ายมาทำที่ **Supabase** ทั้งหมด — ทุ่น ESP32 ทำแค่ อ่าน sensor → POST
ทำให้ทุ่น **เบา/ประหยัดไฟ/เสถียร** และ `/status` **ตอบเร็ว (<1 วิ)** เพราะอ่านค่าล่าสุดจาก DB ตรงๆ

```
ทุ่น ESP32 ──POST──► readings ──(INSERT trigger)──► telegram-alert ──► Telegram (เตือนวิกฤต)
                        ▲
ผู้ใช้ /start,/status ──► telegram-bot (webhook) ──► ตอบทันที
```

| ไฟล์ | หน้าที่ |
|------|---------|
| `schema-telegram.sql` | สร้างตาราง `tg_subscribers`, `alert_state` |
| `functions/telegram-bot/` | รับคำสั่งแชท (/start /status /stop /help) |
| `functions/telegram-alert/` | เช็คเกณฑ์ตอนมีแถวใหม่ → เตือนวิกฤต (cooldown 30 นาที) |
| `functions/_shared/telegram.ts` | ส่งข้อความ + เกณฑ์ + จัดรูป /status (ใช้ร่วม) |

---

## ติดตั้ง (ทำครั้งเดียว ~10 นาที)

### 0) เตรียม
- มี **Bot Token** จาก @BotFather แล้ว
- ติดตั้ง Supabase CLI: `npm i -g supabase` (หรือ `scoop install supabase`) แล้ว `supabase login`
- ผูกโปรเจกต์:  `supabase link --project-ref jjbrgolulggksxnuicgg`

### 1) สร้างตาราง
เปิด **Supabase Studio → SQL Editor** → วางเนื้อหา `schema-telegram.sql` → Run

### 2) ตั้งความลับ (secrets) ของ Edge Functions
```bash
supabase secrets set TELEGRAM_BOT_TOKEN="123456789:ABC...ใส่ token จริง"
supabase secrets set TELEGRAM_WEBHOOK_SECRET="buoy-hook-2026"   # ตั้งเองอะไรก็ได้ (ลับๆ)
supabase secrets set ALERT_WEBHOOK_SECRET="buoy-alert-2026"     # ตั้งเองอะไรก็ได้ (ลับๆ)
```
> `SUPABASE_URL` / `SUPABASE_SERVICE_ROLE_KEY` ไม่ต้องตั้ง — Supabase ใส่ให้อัตโนมัติ

### 3) deploy ทั้ง 2 ฟังก์ชัน
```bash
supabase functions deploy telegram-bot   --no-verify-jwt
supabase functions deploy telegram-alert --no-verify-jwt
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

## แก้เกณฑ์เตือน (ไม่ต้อง flash ทุ่น!)
เกณฑ์อยู่ที่ `functions/_shared/telegram.ts` → `evalAlerts()` แก้แล้ว `supabase functions deploy` ใหม่
(cooldown อยู่ที่ `functions/telegram-alert/index.ts` → `COOLDOWN_MS`)
