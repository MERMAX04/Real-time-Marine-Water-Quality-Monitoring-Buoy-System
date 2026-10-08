# 4-server-backup — แผนสำรอง (Self-hosted PHP + MySQL)

ใช้เมื่อไม่ใช้ Supabase (เช่น ต้องใช้ server ของมหาวิทยาลัย/อาจารย์) — เส้นทางข้อมูลและตรรกะเหมือนแผนหลัก แค่เปลี่ยนที่เก็บข้อมูล

```
ESP32 --HTTP(S) POST--> api/save.php --> MySQL --> api/latest.php --> Dashboard (CONFIG.mode='server')
                             └─ ประเมินตามโหมด → แจ้งเตือน Telegram
Telegram --webhook--> api/tg-webhook.php (/status /swim /mode /start /stop /help)
```

## เทียบความสามารถกับแผนหลัก (Supabase)
| ความสามารถ | แผนหลัก (Supabase) | แผนสำรอง (PHP) |
|---|---|---|
| รับค่า 7 ค่า + GPS | ✅ | ✅ `save.php` |
| heartbeat `sensor_ok` | ✅ | ✅ `save.php` เก็บ / `latest.php` ส่งคืน |
| ประเมิน 6 โหมด (Model A) | ✅ `water-eval.ts` | ✅ `wm_eval()` ใน `lib/telegram.php` (ตรรกะเดียวกัน, config ไฟล์เดียวกัน) |
| แจ้งเตือนอัตโนมัติ + cooldown 30 นาที | ✅ | ✅ `tg_check_and_alert()` |
| คำสั่ง Telegram /status /swim /mode /start /stop /help | ✅ | ✅ |
| เปลี่ยนโหมดผ่าน Telegram (เฉพาะแอดมิน) | ✅ | ✅ `TG_ADMINS` |
| แดชบอร์ดแสดงค่า 7 ค่า + สถานะสี + WQI | ✅ | ✅ |
| เปลี่ยนโหมดผ่านเว็บ `/admin` | ✅ Edge Function `admin` | ❌ ไม่มี — เปลี่ยนผ่าน Telegram `/mode` แทน |
| สถานะทุ่น online/offline บนเว็บ | ✅ realtime | ✅ polling ทุก 15 วิ |
| แผนที่ GPS + ป้าย "sensor ขัดข้อง" บนเว็บ | ✅ | ⚠️ API ส่ง `lat/lon/sensor_ok` ให้แล้ว แต่หน้าเว็บโหมด server ยังไม่นำไปแสดง |

## ต้องเตรียมฝั่ง server
1. มี **PHP 8 + MySQL**
2. มี **public IP / โดเมน** (สำคัญสุด — ESP32 ผ่าน 4G ต้องเข้าถึงได้จากภายนอก)
3. user/pass ของ MySQL และที่วางไฟล์ (cPanel/FTP/SSH) · แนะนำเปิด HTTPS

## ติดตั้ง
1. รัน `db.sql` ใน phpMyAdmin (สร้าง database `buoy` + ตาราง `readings`, `tg_subscribers`, `alert_state`, `app_settings`)
2. แก้ `config.php`: ข้อมูล DB, `API_KEY`, `TELEGRAM_BOT_TOKEN`, `TELEGRAM_WEBHOOK_SECRET`, `TG_ADMINS`
3. อัปโหลดทั้งโฟลเดอร์ขึ้น server (เช่น `public_html/buoy/`)
4. ตั้ง webhook ของบอท:
   ```
   https://api.telegram.org/bot<TOKEN>/setWebhook?url=https://<โดเมน>/buoy/api/tg-webhook.php&secret_token=<TELEGRAM_WEBHOOK_SECRET>
   ```

## ทดสอบ
พิมพ์ใน browser (GET):
```
https://<โดเมน>/buoy/api/save.php?key=buoy-secret-2026&do_val=6.2&do_pct=90&temp=30&ph=8.1&sal=32&cond=50&turb=8
```
ได้ `{"ok":true,"id":1}` = บันทึกสำเร็จ · เปิด `api/latest.php` ต้องเห็นค่าที่เพิ่งส่ง
หรือ flash `esp32-server-test.ino` (ส่งค่าจำลองครบ 7 ค่าผ่าน WiFi ทุก 15 วิ)

## สลับระบบมาใช้ server
- **Dashboard:** `2-dashboard/index.html` ตั้ง `CONFIG.mode = 'server'` และ `CONFIG.apiBase` = URL โฟลเดอร์ `api`
- **Firmware ทุ่น:** เปลี่ยนปลายทางใน `Full-Version.ino` จาก Supabase เป็น `save.php` และเพิ่ม `"key"` ใน JSON (รายละเอียดใน [`../SWITCHING-GUIDE.md`](../SWITCHING-GUIDE.md))

> เกณฑ์ 6 โหมดแก้ได้ที่ `lib/water-modes.json` (ต้องตรงกับอีก 2 สำเนาใน `supabase/functions/_shared/` และ `2-dashboard/`) · cooldown แก้ที่ `TG_COOLDOWN_SEC` ใน `lib/telegram.php`

## ไฟล์
| ไฟล์ | หน้าที่ |
|------|---------|
| `config.php` | ตั้งค่า DB + API key + Telegram token/secret/admins + ฟังก์ชันเชื่อม DB |
| `db.sql` | สร้างฐานข้อมูล (readings 7 ค่า + sensor_ok + GPS, tg_subscribers, alert_state, app_settings) |
| `api/save.php` | รับค่าจาก ESP32 (POST JSON หรือ GET) → บันทึก → ประเมิน → แจ้งเตือน |
| `api/latest.php` | ค่าล่าสุด 1 แถว ให้ Dashboard |
| `api/history.php` | ค่าย้อนหลัง (`?limit=` สูงสุด 500) |
| `api/tg-webhook.php` | รับคำสั่งแชท Telegram |
| `lib/telegram.php` | ตรรกะประเมิน 6 โหมด + จัดรูปข้อความ + ส่ง Telegram + cooldown |
| `lib/water-modes.json` | เกณฑ์ 6 โหมด (config กลาง) |
| `esp32-server-test.ino` | sketch ทดสอบส่งค่าจำลองเข้า `save.php` ผ่าน WiFi |
