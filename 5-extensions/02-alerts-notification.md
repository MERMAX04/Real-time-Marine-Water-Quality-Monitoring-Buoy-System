# 02 — แจ้งเตือน + บอท Telegram (ทำ "ฝั่งบก" อ่านค่าจาก DB) — ✅ ใช้งานจริง

เด้งเตือนเข้า **Telegram** เมื่อสถานะรวมของน้ำเป็น **"แดง" ตามโหมดมาตรฐานที่เลือก** และสั่งดูค่าล่าสุดได้ทุกเมื่อ
> เลือก Telegram เพราะ Bot API ฟรี เปิดใช้ง่าย ไม่ต้องมี Official Account

---

## สถาปัตยกรรม: Telegram อยู่ "ฝั่งบก" ไม่ใช่บนทุ่น
```
🌊 ทุ่น ESP32                  🏝️ ฝั่งบก (ทำงานเอง 24/7)
อ่าน sensor → POST ─────► [DB: readings]
                              │  └─(INSERT แถวใหม่)─► telegram-alert: ประเมินตามโหมด → สถานะรวมแดง → เตือน
   ผู้ใช้ /status /swim /mode ──► telegram-bot (webhook) อ่านค่าล่าสุดจาก DB → ตอบทันที
```

| | ให้ ESP32 ส่ง Telegram เอง | ทำฝั่งบก (ที่ใช้) |
|---|---|---|
| ความเร็วตอบคำสั่ง | ช้า (ต้องรอรอบ poll) | **ทันที** (webhook) |
| ภาระทุ่น | ยิง HTTP หลายครั้งต่อรอบ | **POST ครั้งเดียว** |
| แก้เกณฑ์เตือน | ต้อง flash ทุ่นใหม่ | **แก้ config บนคลาวด์** |

---

## ทำ 2 ช่องทาง (ตาม backend ที่โปรเจกต์รองรับ)
| | แผนหลัก Supabase | แผนสำรอง PHP |
|---|---|---|
| แจ้งเตือนอัตโนมัติ | Edge Function `telegram-alert` (Database Webhook ตอน INSERT) | `api/save.php` → `tg_check_and_alert()` |
| คำสั่งแชท | Edge Function `telegram-bot` | `api/tg-webhook.php` |
| ตรรกะประเมิน | `_shared/water-eval.ts` | `lib/telegram.php` → `wm_eval()` |
| ขั้นตอนติดตั้ง | [`supabase/README.md`](../supabase/README.md) | [`4-server-backup/README.md`](../4-server-backup/README.md) |

ทั้งสองใช้ **เกณฑ์ไฟล์เดียวกัน** (`water-modes.json`) ข้อความและ cooldown เหมือนกัน

---

## ตั้งค่า Bot (ครั้งเดียว)
1. Telegram หา **@BotFather** → `/newbot` → ได้ **token** แบบ `123456789:ABCdef...`
   - Supabase: `supabase secrets set TELEGRAM_BOT_TOKEN="..."`
   - PHP: `define('TELEGRAM_BOT_TOKEN', '...')` ใน `config.php`
2. (แนะนำ) ตั้งเมนูคำสั่งที่ @BotFather → Edit Commands
   ```
   status - ค่าน้ำล่าสุดทุกค่า (แยกสี)
   swim - ทำกิจกรรมทางน้ำได้ไหม
   mode - ดู/เปลี่ยนโหมดมาตรฐาน
   start - สมัครรับแจ้งเตือน
   stop - ยกเลิกรับแจ้งเตือน
   help - แสดงคำสั่ง
   ```
3. **ผู้รับแจ้งเตือน** = ใครก็ได้ที่ทัก `/start` (เก็บ chat_id ลงตาราง `tg_subscribers` อัตโนมัติ)
4. **แอดมิน** (เปลี่ยนโหมดได้) = chat_id ที่ใส่ใน `TG_ADMINS` (หา chat_id ได้จาก @userinfobot)

## คำสั่งแชท
| คำสั่ง | ผล |
|--------|----|
| `/status` | ค่าน้ำล่าสุด 7 ค่า แยกสี 🟢🟠🔴 + สถานะทุ่น + สถานะรวมตามโหมด + พิกัด |
| `/swim` | สรุปว่า "ทำกิจกรรมได้ไหม" ตามโหมดที่เลือก + เหตุผล |
| `/mode` | ดูโหมดปัจจุบัน · `/mode <เลข/ชื่อ>` เปลี่ยนโหมด (เฉพาะแอดมิน) |
| `/start` / `/stop` | สมัคร / ยกเลิกรับแจ้งเตือน |
| `/help` | เมนูคำสั่ง |

## เงื่อนไขการแจ้งเตือน
- เตือนเมื่อ **สถานะรวม = แดง** ตามโหมดปัจจุบัน (มีค่าระดับ "วิกฤต" ตกเกณฑ์แดง — Model A)
- เกณฑ์แต่ละค่าและระดับวิกฤต/เฝ้าระวังอยู่ใน `water-modes.json` (ดู [`05-blueflag-swim-safety.md`](05-blueflag-swim-safety.md))
- **กันเตือนซ้ำ:** cooldown 30 นาที ต่อชุด "โหมด + ค่าที่ตก" (เก็บใน `alert_state`)
- แถว heartbeat (`sensor_ok=false`) ไม่มีค่าน้ำ จึงไม่ประเมิน/ไม่เตือน

## แก้เกณฑ์ได้โดยไม่ต้อง flash ทุ่น
แก้ `water-modes.json` (3 สำเนาให้ตรงกัน) แล้ว deploy Edge Functions ใหม่ / อัปโหลดไฟล์ PHP ใหม่
