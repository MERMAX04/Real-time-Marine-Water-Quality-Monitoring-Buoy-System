# 3. API Documentation

ระบบมี API 3 กลุ่ม
- **A. Supabase REST + Realtime** — สร้างอัตโนมัติจากตาราง ใช้โดยทุ่นและเว็บ
- **B. Supabase Edge Functions** — `telegram-bot`, `telegram-alert`, `admin`
- **C. PHP API (แผนสำรอง)** — `save.php`, `latest.php`, `history.php`, `tg-webhook.php`

Base URL แผนหลัก: `https://jjbrgolulggksxnuicgg.supabase.co`
(`<anon-key>` = anon public key จาก Supabase → Project Settings → API · เป็น public key ที่ถูกจำกัดสิทธิ์ด้วย RLS)

---

## สรุป endpoint ทั้งหมด
| # | Method | Endpoint | ผู้เรียก | หน้าที่ |
|---|---|---|---|---|
| A1 | POST | `/rest/v1/readings` | ทุ่น ESP32 | บันทึกค่าที่วัดได้ 1 ครั้ง |
| A2 | GET | `/rest/v1/readings?...` | เว็บ, เครื่องมือ | อ่านค่าล่าสุด/ย้อนหลัง |
| A3 | GET | `/rest/v1/app_settings?key=eq.mode` | เว็บ | อ่านโหมดมาตรฐานปัจจุบัน |
| A4 | GET | `/rest/v1/readings_daily?...` | (ใช้วิเคราะห์) | ค่าเฉลี่ยรายวัน |
| A5 | WebSocket | Realtime `postgres_changes` | เว็บ | รับแถวใหม่ / โหมดที่เปลี่ยน แบบสด |
| B1 | POST | `/functions/v1/telegram-bot` | Telegram (webhook) | รับคำสั่งแชทแล้วตอบ |
| B2 | POST | `/functions/v1/telegram-alert` | Supabase Database Webhook | ประเมินแถวใหม่และแจ้งเตือน |
| B3 | POST | `/functions/v1/admin` | เว็บ | ล็อกอินแอดมิน / เปลี่ยนโหมด |
| C1 | POST/GET | `/api/save.php` | ทุ่น (แผนสำรอง) | บันทึกค่า + แจ้งเตือน |
| C2 | GET | `/api/latest.php` | เว็บ (โหมด server) | ค่าล่าสุด |
| C3 | GET | `/api/history.php` | (ใช้วิเคราะห์) | ค่าย้อนหลัง |
| C4 | POST | `/api/tg-webhook.php` | Telegram (webhook) | รับคำสั่งแชท |

---

## A. Supabase REST + Realtime

### A1. บันทึกค่าที่วัดได้ — `POST /rest/v1/readings`
**Headers**
```
apikey: <anon-key>
Authorization: Bearer <anon-key>
Content-Type: application/json
Prefer: return=minimal
```
**Request body (ค่าปกติ)**
```json
{
  "device": "buoy-01",
  "sensor_ok": true,
  "do_val": 6.45, "do_pct": 0.9, "temp": 30.10, "ph": 7.96,
  "sal": 32.10, "cond": 50.20, "turb": 4.24,
  "lat": 7.191844, "lon": 100.610184
}
```
**Request body (heartbeat — อ่านเซนเซอร์ไม่ได้)**
```json
{ "device": "buoy-01", "sensor_ok": false }
```
| field | ชนิด | บังคับ | ความหมาย |
|---|---|---|---|
| `device` | text | ไม่ (ค่าเริ่มต้น `buoy-01`) | รหัสทุ่น |
| `sensor_ok` | boolean | ไม่ (ค่าเริ่มต้น `true`) | `false` = ESP ทำงานแต่อ่านเซนเซอร์ไม่ได้ |
| `do_val` | real | ไม่ | ออกซิเจนละลายน้ำ (mg/L) |
| `do_pct` | real | ไม่ | ออกซิเจนอิ่มตัว (%) — บางเซนเซอร์ส่งเป็นสัดส่วน (0.90) ระบบแปลงเป็น % ให้ตอนแสดงผล |
| `temp` | real | ไม่ | อุณหภูมิ (°C) |
| `ph` | real | ไม่ | pH |
| `sal` | real | ไม่ | ความเค็ม (ppt) |
| `cond` | real | ไม่ | การนำไฟฟ้า (mS/cm) |
| `turb` | real | ไม่ | ความขุ่น (NTU) |
| `lat`, `lon` | double | ไม่ | พิกัด GPS (ส่งเมื่อจับพิกัดได้) |

`id` และ `created_at` ฐานข้อมูลสร้างให้เอง

**Response**
| code | ความหมาย |
|---|---|
| `201 Created` | บันทึกสำเร็จ (body ว่าง เพราะ `return=minimal`) |
| `400` | มี field ที่ไม่มีคอลัมน์รองรับ เช่น `{"code":"PGRST204","message":"Could not find the 'xxx' column ..."}` |
| `401 / 403` | anon key ผิด หรือ RLS ไม่อนุญาต |

**ตัวอย่าง (curl)**
```bash
curl -X POST "https://jjbrgolulggksxnuicgg.supabase.co/rest/v1/readings" \
  -H "apikey: <anon-key>" -H "Authorization: Bearer <anon-key>" \
  -H "Content-Type: application/json" -H "Prefer: return=minimal" \
  -d '{"device":"buoy-01","do_val":6.2,"do_pct":90,"temp":30,"ph":8.1,"sal":32,"cond":50,"turb":8}'
```

### A2. อ่านค่า — `GET /rest/v1/readings`
ใช้ไวยากรณ์ PostgREST (`select`, `eq`, `order`, `limit`)
```bash
# ค่าล่าสุด 1 แถวของทุ่น buoy-01
curl "https://jjbrgolulggksxnuicgg.supabase.co/rest/v1/readings?select=*&device=eq.buoy-01&order=created_at.desc&limit=1" \
  -H "apikey: <anon-key>" -H "Authorization: Bearer <anon-key>"
```
**Response `200`**
```json
[{ "id": 1250, "device": "buoy-01", "created_at": "2026-10-04T10:00:03.23+00:00",
   "do_val": 6.45, "sal": 3.37, "turb": 4.24, "ph": 7.96, "do_pct": 0.9, "temp": 30.1,
   "cond": 6.2, "lat": 7.191844, "lon": 100.610184, "sensor_ok": true }]
```
เว็บใช้ supabase-js ในรูปแบบเดียวกัน:
```js
sb.from('readings').select('*').eq('device', 'buoy-01')
  .order('created_at', { ascending: false }).limit(120)   // ย้อนหลัง 120 แถวตอนเปิดหน้า
```

### A3. อ่านโหมดปัจจุบัน — `GET /rest/v1/app_settings?key=eq.mode`
```json
[{ "key": "mode", "value": "recreation", "updated_at": "..." }]
```
ค่า `value` เป็น 1 ใน 6: `conservation`, `coral`, `aquaculture`, `recreation`, `industrial`, `community`
> anon **อ่านได้อย่างเดียว** — การเปลี่ยนโหมดต้องผ่าน B3 (`admin`) หรือ Telegram `/mode`

### A4. ค่าเฉลี่ยรายวัน — `GET /rest/v1/readings_daily`
```bash
curl ".../rest/v1/readings_daily?select=day,do_avg,sal_avg,turb_avg,n&device=eq.buoy-01" -H "apikey: <anon-key>" ...
```
คอลัมน์: `device, day, do_avg, do_pct_avg, temp_avg, ph_avg, sal_avg, cond_avg, turb_avg, n`

### A5. Realtime (WebSocket)
| channel | event | filter | ใช้ทำอะไร |
|---|---|---|---|
| `readings-<device>` | `INSERT` บน `public.readings` | `device=eq.<device>` | เว็บได้แถวใหม่ทันทีที่ทุ่นส่ง |
| `app_settings-mode` | `*` บน `public.app_settings` | `key=eq.mode` | เปลี่ยนโหมดที่ไหน ทุกหน้าเว็บที่เปิดอยู่อัปเดตตาม |
```js
sb.channel('readings-buoy-01')
  .on('postgres_changes', { event: 'INSERT', schema: 'public', table: 'readings', filter: 'device=eq.buoy-01' },
      payload => console.log(payload.new))
  .subscribe();
```

---

## B. Supabase Edge Functions
deploy ด้วย `--no-verify-jwt` (ไม่ต้องแนบ JWT) และใช้ secret ของแต่ละฟังก์ชันตรวจผู้เรียกแทน

### B1. `POST /functions/v1/telegram-bot`
ผู้เรียก: Telegram (ตั้งด้วย `setWebhook`)
**Header ที่ต้องมี:** `X-Telegram-Bot-Api-Secret-Token: <TELEGRAM_WEBHOOK_SECRET>` (Telegram แนบให้เอง)
**Request body:** Telegram Update object
```json
{ "update_id": 1, "message": { "chat": { "id": 123456789 }, "text": "/status" } }
```
| คำสั่ง | การทำงาน |
|---|---|
| `/start` | เพิ่ม chat_id ลง `tg_subscribers` |
| `/status` | ตอบค่าล่าสุด 7 ค่าแยกสี + สถานะทุ่น + สถานะรวมตามโหมด + พิกัด |
| `/swim` (หรือ `/activity`) | ตอบสถานะรวม "ทำกิจกรรมได้ไหม" ตามโหมด |
| `/mode` | แสดงรายการโหมด · `/mode <1–6 หรือคีย์>` เปลี่ยนโหมด (เฉพาะ chat_id ใน `TG_ADMINS`) |
| `/stop` (หรือ `/unsubscribe`) | ลบ chat_id ออกจาก `tg_subscribers` |
| `/help` (หรือ `/menu`) | เมนูคำสั่ง |

**Response:** `200 ok` เสมอ (ข้อความตอบส่งกลับผ่าน Telegram `sendMessage`) · `401 forbidden` ถ้า secret ไม่ตรง

### B2. `POST /functions/v1/telegram-alert`
ผู้เรียก: Supabase Database Webhook (INSERT บน `public.readings`)
**Header ที่ต้องมี:** `x-alert-secret: <ALERT_WEBHOOK_SECRET>`
**Request body** (Supabase ส่งให้อัตโนมัติ)
```json
{ "type": "INSERT", "table": "readings", "schema": "public",
  "record": { "device": "buoy-01", "do_val": 2.0, "ph": 8.0, "temp": 30, "turb": 5, "...": "..." },
  "old_record": null }
```
**การทำงาน:** อ่านโหมดจาก `app_settings` → `evalWater(record, mode)` → ถ้าสถานะรวมเป็นแดง และไม่อยู่ใน cooldown 30 นาที (คีย์ `buoy-01:red:<mode>:<ค่าที่ตก>`) → ส่งข้อความหาทุกคนใน `tg_subscribers`
| Response | ความหมาย |
|---|---|
| `200 ok (ปกติ ไม่มีเตือน)` | สถานะรวมไม่ใช่แดง |
| `200 ok (ยังอยู่ใน cooldown)` | เคยเตือนชุดนี้ไปแล้วภายใน 30 นาที |
| `200 ok (ส่งแล้ว n/m คน)` | ส่งแจ้งเตือนแล้ว |
| `400` | body ไม่ใช่ JSON / ไม่มี record |
| `401 forbidden` | secret ไม่ตรง |

### B3. `POST /functions/v1/admin`
ผู้เรียก: เว็บแดชบอร์ด (เปิด CORS สำหรับเบราว์เซอร์)

**ล็อกอิน**
```json
// request
{ "action": "login", "user": "admin", "pass": "********" }
// response 200
{ "ok": true, "token": "base64-sha256..." }
// response 401
{ "ok": false, "error": "invalid credentials" }
```
**เปลี่ยนโหมด**
```json
// request
{ "action": "setmode", "token": "base64-sha256...", "mode": "coral" }
// response 200
{ "ok": true, "mode": "coral" }
// response 403
{ "ok": false, "error": "unauthorized" }
```
| code | ความหมาย |
|---|---|
| 400 | `bad json` / `unknown action` |
| 401 | ชื่อผู้ใช้/รหัสผ่านไม่ถูก |
| 403 | token ไม่ถูกต้อง |
| 500 | บันทึกไม่สำเร็จ / server error |

**ตัวอย่าง (curl)**
```bash
curl -X POST "https://jjbrgolulggksxnuicgg.supabase.co/functions/v1/admin" \
  -H "Content-Type: application/json" -d '{"action":"login","user":"admin","pass":"<รหัส>"}'
```

---

## C. PHP API (แผนสำรอง)
Base URL ตัวอย่าง: `https://<โดเมน>/buoy/api` · ทุก response เป็น JSON และเปิด CORS (`Access-Control-Allow-Origin: *`)

### C1. `POST|GET /save.php` — บันทึกค่า + ประเมิน + แจ้งเตือน
รับได้ทั้ง POST JSON และ GET query · ต้องมี `key` ตรงกับ `API_KEY` ใน `config.php`
```json
{ "key": "buoy-secret-2026", "device": "buoy-01", "sensor_ok": true,
  "do_val": 6.2, "do_pct": 95, "temp": 28.5, "ph": 8.1, "sal": 32, "cond": 48, "turb": 8,
  "lat": 7.19, "lon": 100.6 }
```
```
GET /save.php?key=buoy-secret-2026&do_val=6.2&do_pct=90&temp=30&ph=8.1&sal=32&cond=50&turb=8
```
(รองรับ `do=` เป็นชื่อเดิมของ `do_val`) · หลังบันทึกจะเรียก `tg_check_and_alert()` เมื่อ `sensor_ok` เป็นจริง
| Response | ความหมาย |
|---|---|
| `200 {"ok":true,"id":123}` | สำเร็จ |
| `401 {"ok":false,"error":"invalid api key"}` | key ไม่ตรง |
| `500 {"ok":false,"error":"..."}` | ฐานข้อมูลผิดพลาด |

### C2. `GET /latest.php?device=buoy-01` — ค่าล่าสุด
```json
{ "do": 6.2, "do_pct": 90, "temp": 30, "ph": 8.1, "sal": 32, "cond": 50, "turb": 8,
  "lat": 7.19, "lon": 100.6, "sensor_ok": true, "ts": "2026-10-08 13:00:00" }
```
ใช้คีย์ `do` (ตรงกับคีย์ของเว็บ) · `do_pct` แปลงเป็น % แล้ว · ไม่มีข้อมูล → `{"ok":false,"error":"no data"}`

### C3. `GET /history.php?device=buoy-01&limit=60` — ค่าย้อนหลัง
`limit` 1–500 (ค่าเริ่มต้น 60) · เรียงเก่า → ใหม่
```json
[{ "ts": "...", "do": 6.2, "sal": 32, "turb": 8, "ph": 8.1, "cond": 50, "do_pct": 90, "temp": 30 }]
```
(`do_pct` แปลงเป็น % แล้ว เหมือน C2)

### C4. `POST /tg-webhook.php` — คำสั่งแชท Telegram
เหมือน B1 ทุกคำสั่ง · ตรวจ header `X-Telegram-Bot-Api-Secret-Token` กับ `TELEGRAM_WEBHOOK_SECRET` · ตอบ `ok` เสมอ (ไม่งั้น Telegram จะส่งซ้ำ)
