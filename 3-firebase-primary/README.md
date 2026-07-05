# 3-firebase-primary — แผนหลัก (Firebase Realtime Database)

เส้นทางข้อมูล:
```
ESP32 (4G/WiFi)  --เขียน-->  Firebase Realtime DB  --realtime-->  Dashboard
```

---

## ขั้นที่ 1 — สร้าง Firebase Project
1. เข้า https://console.firebase.google.com (ล็อกอินด้วย paramet.tx@gmail.com)
2. กด **Add project** ตั้งชื่อ เช่น `buoy-monitor` → กด Continue ไปเรื่อยๆ (ปิด Google Analytics ได้ ไม่จำเป็น)

## ขั้นที่ 2 — สร้าง Realtime Database
1. เมนูซ้าย **Build → Realtime Database → Create Database**
2. เลือก location **Singapore (asia-southeast1)** ← ใกล้ไทยสุด ค่า delay ต่ำ
3. เลือก **Start in test mode** (เปิดให้อ่าน/เขียนได้ก่อน ค่อยล็อกทีหลัง) → Enable
4. จะได้ URL หน้าตาแบบนี้ (จดไว้):
   `https://buoy-monitor-default-rtdb.asia-southeast1.firebasedatabase.app`

## ขั้นที่ 3 — เอาค่า config มาใส่ Dashboard
1. กดรูปเฟือง ⚙️ ข้างบน → **Project settings**
2. เลื่อนลงหัวข้อ **Your apps** → กดไอคอน **</>** (Web) → ตั้งชื่อ app → Register
3. จะเห็นบล็อก `firebaseConfig = { apiKey: "...", ... }`
4. ก๊อปค่าไปใส่ใน `2-dashboard/index.html` ที่ `CONFIG.firebase`:
   ```js
   firebase: {
     apiKey:      "AIza....",
     authDomain:  "buoy-monitor.firebaseapp.com",
     databaseURL: "https://buoy-monitor-default-rtdb.asia-southeast1.firebasedatabase.app",
     projectId:   "buoy-monitor",
   },
   ```
5. เปลี่ยน `CONFIG.mode` จาก `'mock'` เป็น `'firebase'`

## ขั้นที่ 4 — ทดสอบก่อนมี ESP32 (สำคัญ!)
ยังไม่ต้องมีอุปกรณ์เลย ทดสอบว่า Dashboard รับ realtime ได้ไหม:
1. ในหน้า Realtime Database บน Console กด **+** สร้างข้อมูลมือ ตามโครงสร้างในไฟล์ `database-structure.json`
   (path: `buoy-01 / latest / do = 6.2`, `sal = 32`, ...)
2. เปิด `2-dashboard/index.html` → ค่าต้องขึ้นตามที่พิมพ์
3. ลองแก้ตัวเลขบน Console → Dashboard ต้องเปลี่ยน **ทันที** = realtime ทำงาน ✅

## ขั้นที่ 5 — ต่อ ESP32
- ใช้โค้ด `esp32-firebase-test.ino` (ส่งค่าปลอมขึ้น Firebase เพื่อทดสอบทั้งวงจร)
- เริ่มด้วย **WiFi ก่อน** (ง่ายกว่า) พอวงจรครบค่อยเปลี่ยนไปโมดูล 4G ทีหลัง

---

## ⚠️ ก่อนส่งงานจริง — ล็อก Database Rules
test mode เปิด 30 วันแล้วปิดเอง ให้ตั้ง rules แบบนี้ (อ่านได้ทุกคน เขียนได้เฉพาะที่ล็อกอิน):
```json
{ "rules": {
    ".read": true,
    ".write": "auth != null"
} }
```
แล้วให้ ESP32 ใช้ Email/Password auth (มีในโค้ดตัวอย่าง) — กันคนอื่นเขียนข้อมูลมั่ว
