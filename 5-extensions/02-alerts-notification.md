# 02 — แจ้งเตือนเข้า LINE เมื่อค่าน้ำวิกฤต (ESP32 ยิงตรง)

เด้งเตือนเข้า **LINE** มือถือเมื่อค่าน้ำเข้าขั้น **วิกฤต** — ตรงเป้าหมาย "ผ่อนแรงคนเฝ้า"
(ไม่ต้องนั่งจ้อง Dashboard ระบบเตือนเองเมื่อมีปัญหา)

> ✅ **เลือกแล้ว:** ESP32 เป็นคนเช็คเกณฑ์แล้ว **push เข้า LINE เอง** (ภาคสนาม ลอยทะเลเตือนได้แม้ไม่เปิดคอม)
> โค้ดพร้อมใช้อยู่ใน [`3-supabase-primary/esp32-supabase-4g.ino`](../3-supabase-primary/esp32-supabase-4g.ino) แล้ว — แค่ใส่ token กับปลายทาง

---

## ⚠️ สำคัญ: LINE Notify ปิดบริการแล้ว
LINE Notify (ตัวที่ตั้งง่ายสุด) **ปิดถาวรตั้งแต่ มี.ค. 2025** → ต้องใช้ **LINE Messaging API** แทน
ยุ่งกว่านิดหน่อยแต่ทำครั้งเดียวจบ ตามขั้นตอนล่างนี้

---

## ขั้นตอนตั้งค่า LINE Messaging API (ครั้งเดียว)

### 1) สร้าง Channel
1. เข้า **https://developers.line.biz/console/** (ล็อกอินด้วยบัญชี LINE)
2. สร้าง **Provider** (ชื่ออะไรก็ได้ เช่น "Buoy Project")
3. ในนั้นสร้าง **Channel → Messaging API**
4. จะได้ **LINE Official Account** มาอัตโนมัติ 1 ตัว (คือบอทที่จะส่งข้อความให้เรา)

### 2) เอา Channel Access Token
- ในหน้า Channel → แท็บ **Messaging API** → เลื่อนหา **Channel access token (long-lived)** → กด **Issue**
- คัดลอกไปใส่ `LINE_TOKEN` ในโค้ด ESP32

### 3) เอาปลายทางที่จะส่งหา (`LINE_TO`)
**push** ต้องรู้ว่าจะส่งหา "ใคร" (userId หรือ groupId):

**แบบส่งหาตัวเอง (userId):**
1. เพิ่มบอท (LINE Official Account) เป็นเพื่อนก่อน — สแกน QR ในแท็บ Messaging API
2. หา userId ของตัวเอง: ในหน้า Channel → แท็บ **Basic settings** → เลื่อนล่างสุดมี **Your user ID**
   *(หรือเปิด webhook รับ event `userId` ก็ได้)*

**แบบส่งเข้ากลุ่ม (groupId):**
1. เชิญบอทเข้ากลุ่ม LINE
2. ต้องเปิด webhook รับ event เพื่ออ่าน `groupId` (ขั้นสูงกว่า — ถ้าเอาง่ายใช้ userId ตัวเองก่อน)

### 4) ใส่ค่าในโค้ด ESP32
ในไฟล์ `esp32-supabase-4g.ino` แก้ 2 บรรทัด:
```cpp
#define LINE_TOKEN "eyJhbGci...(Channel access token)"
#define LINE_TO    "Uxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"   // userId หรือ groupId
```

---

## โค้ดทำงานยังไง (อยู่ในสเก็ตช์แล้ว)

หลัง ESP32 ส่งค่าขึ้น Supabase สำเร็จ → เรียก `checkAndAlert()` เช็คเกณฑ์ ถ้าเกินก็ `pushLine()`:

```cpp
// ตรวจเกณฑ์วิกฤต (โฟกัสค่าที่อันตรายจริง)
if(doVal < 3.0 ) ... "🔴 ออกซิเจนละลายน้ำต่ำวิกฤต"
if(ph    < 7.0 ) ... "🔴 น้ำเป็นกรดผิดปกติ"
if(ph    > 9.0 ) ... "🔴 น้ำเป็นด่างผิดปกติ"
if(temp  > 33.0) ... "🟠 อุณหภูมิน้ำสูง"
if(turb  > 40.0) ... "🟠 ความขุ่นสูงผิดปกติ"
```

**push เข้า LINE** = HTTPS POST ไป `api.line.me/v2/bot/message/push`:
```
Authorization: Bearer <LINE_TOKEN>
{"to":"<LINE_TO>","messages":[{"type":"text","text":"..."}]}
```
ตอบ **200** = ส่งสำเร็จ

## เกณฑ์แจ้งเตือน (ปรับได้)
เลือกเฉพาะค่าที่ **"อันตรายจริง"** ไม่ให้เตือนพร่ำเพรื่อ:
| ค่า | เกณฑ์เตือน | เหตุผล |
|-----|-----------|--------|
| DO | < 3 mg/L | ออกซิเจนต่ำ สัตว์น้ำเสี่ยงตาย (ตัวชี้วัดสำคัญสุด) |
| pH | < 7 หรือ > 9 | น้ำกรด/ด่างผิดปกติ |
| อุณหภูมิ | > 33 °C | น้ำร้อน เสี่ยงปะการังฟอกขาว/ออกซิเจนต่ำ |
| ความขุ่น | > 40 NTU | ตะกอน/ปนเปื้อนสูง |

> ⚠️ ไม่ตั้งเกณฑ์กับ **ความเค็ม/Conductivity** เพราะถ้าทดสอบในอากาศ (ค่า=0) จะเตือนรัวๆ — เปิดได้เมื่อทุ่นอยู่ในน้ำจริงตลอด

## กัน "เตือนซ้ำรัวๆ"
โค้ดมี **cooldown 30 นาที/รายการ** อยู่แล้ว (`ALERT_COOLDOWN`) — ค่าวิกฤตที่ส่งทุก 15 วิ จะไม่สแปม LINE
ปรับเวลาได้ที่:
```cpp
const unsigned long ALERT_COOLDOWN = 30UL*60UL*1000UL;  // 30 นาที
```

---

## ทดสอบว่า LINE ใช้ได้ก่อน (ไม่ต้องรอค่าวิกฤต)
ลองบังคับส่งครั้งเดียวใน `setup()`:
```cpp
pushLine("✅ ทดสอบ: ทุ่น buoy-01 ออนไลน์แล้ว");
```
ได้ข้อความเข้า LINE = token/userId ถูก พร้อมใช้งาน

## ปิดชั่วคราว
ตั้ง `#define LINE_ENABLE 0` (เช่น ตอน bench test จะได้ไม่เตือน)

---

## ทางเลือกอื่น (ถ้าไม่อยากตั้ง LINE Official Account)
| ช่องทาง | ข้อดี | ข้อเสีย |
|---------|-------|---------|
| **Telegram Bot** | ตั้ง 5 นาที (@BotFather), ฟรี, ยิงง่ายสุด | ต้องมีแอป Telegram (คนไทยใช้น้อยกว่า) |
| **อีเมล (SMTP)** | ไม่ต้องลงแอปเพิ่ม | อาจตกไป junk, ช้ากว่า |

Telegram push (ถ้าเลือกแทน): `POST api.telegram.org/bot<TOKEN>/sendMessage` body `chat_id` + `text` — โครงเหมือน LINE แค่เปลี่ยน URL/header

## จะให้ Server อาจารย์ (PHP) เป็นคนเตือนแทน ESP32 ก็ได้
ถ้าใช้แผนสำรอง (mode=server): ให้ `4-server-backup/api/save.php` เช็คเกณฑ์ทุกครั้งที่รับค่า แล้ว curl ยิง LINE เดียวกัน
(ข้อดี: ปรับเกณฑ์ได้โดยไม่ต้อง flash ESP32 ใหม่ / ข้อเสีย: ต้องมี server ออนไลน์)
