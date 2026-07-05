# 🔄 คู่มือสลับแหล่งข้อมูล (Firebase ↔ Server อาจารย์)

เอกสารนี้บอก **ทุกจุดที่ต้องแก้** เวลาสลับระหว่างแผนหลัก (Firebase) กับแผนสำรอง (Server อาจารย์)

## แนวคิดสำคัญ: มีแค่ 2 ฝั่งที่ต้องแก้
```
   [ฝั่ง ESP32]  ─ ส่งข้อมูลขึ้น ─→  [Cloud]  ─ ดึงมาแสดง ─→  [ฝั่ง Dashboard]
    แก้ 1 จุด                                                    แก้ 1 จุด
```
เปลี่ยน cloud = แก้แค่ **2 ไฟล์** เท่านั้น: โค้ด ESP32 (1 ไฟล์) + Dashboard (1 ไฟล์)

---

## ตารางสรุป: แต่ละโหมดใช้ไฟล์ไหน / แก้อะไร

| | 🟢 Firebase (แผนหลัก) | 🟡 Server อาจารย์ (แผนสำรอง) | ⚪ Mock (ทดสอบจอ) |
|---|---|---|---|
| **โค้ด ESP32** | `3-firebase-primary/esp32-firebase-test.ino` | `4-server-backup/esp32-server-test.ino` | — (ไม่ต้องมีอุปกรณ์) |
| **Dashboard `CONFIG.mode`** | `'firebase'` | `'server'` | `'mock'` |
| **ต้องกรอกใน Dashboard** | `CONFIG.firebase = {...}` | `CONFIG.apiBase = '...'` | — |
| **ฝั่ง cloud ต้องเตรียม** | สร้าง Firebase project | ติดตั้ง PHP+MySQL, รัน db.sql | — |

---

## 🟢 สลับไปใช้ FIREBASE (แผนหลัก)

### จุดที่ 1 — Dashboard: `2-dashboard/index.html`
หา `const CONFIG` (ราวบรรทัดต้นๆ ของ `<script>`) แล้วแก้ **2 ที่**:
```js
const CONFIG = {
  mode: 'firebase',                    // ← (1) เปลี่ยนเป็น 'firebase'
  device: 'buoy-01',
  firebase: {                          // ← (2) กรอกค่าจาก Firebase Console
    apiKey:      "AIza....",
    authDomain:  "buoy-monitor.firebaseapp.com",
    databaseURL: "https://buoy-monitor-default-rtdb.asia-southeast1.firebasedatabase.app",
    projectId:   "buoy-monitor",
  },
  ...
};
```
> ค่าใน `firebase{}` เอามาจากไหน → ดู `3-firebase-primary/README.md` ขั้นที่ 3

### จุดที่ 2 — ESP32: ใช้ไฟล์ `3-firebase-primary/esp32-firebase-test.ino`
แก้ 4 บรรทัดบนสุด:
```cpp
#define WIFI_SSID     "ชื่อ_WiFi"
#define WIFI_PASS     "รหัส_WiFi"
#define API_KEY       "AIza..."          // ← ตัวเดียวกับ apiKey ใน Dashboard
#define DATABASE_URL  "https://....firebasedatabase.app"   // ← ตัวเดียวกับ databaseURL
```

### ✅ Checklist Firebase
- [ ] สร้าง Firebase project + Realtime Database แล้ว (ดู README ขั้น 1-2)
- [ ] Dashboard: `mode = 'firebase'` และกรอก `firebase{}` ครบ
- [ ] ESP32: กรอก WiFi + API_KEY + DATABASE_URL
- [ ] `API_KEY` และ `DATABASE_URL` ใน ESP32 = ตรงกับ Dashboard

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
แก้ 3 บรรทัดบนสุด:
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
```

### ✅ Checklist Server
- [ ] รัน `db.sql` สร้างฐานข้อมูลแล้ว
- [ ] `config.php`: กรอก DB + ตั้ง API_KEY
- [ ] อัปโหลดโฟลเดอร์ขึ้น server แล้ว (เช่น `public_html/buoy/`)
- [ ] Dashboard: `mode = 'server'` และ `apiBase` ถูกต้อง
- [ ] ESP32: `SERVER_URL` ชี้ที่ save.php, `API_KEY` ตรงกับ config.php
- [ ] server มี **public IP/โดเมน** ที่ ESP32 เข้าถึงได้ (สำคัญสุด!)

---

## ⚠️ กับดักที่เจอบ่อยเวลาสลับ

| อาการ | สาเหตุ | แก้ |
|-------|--------|-----|
| Dashboard ค่าไม่ขึ้น (firebase) | ค่า `firebase{}` ผิด หรือ `mode` ยังเป็น mock | เช็ค Console (F12) หา error, ตรวจ databaseURL |
| Dashboard ค่าไม่ขึ้น (server) | CORS หรือ apiBase ผิด | เปิด `apiBase/latest.php` ตรงๆใน browser ดูว่าได้ JSON ไหม |
| ESP32 ส่งไม่ขึ้น (server) | API_KEY ไม่ตรง / server เข้าไม่ถึง | ดู Serial Monitor, ลองเปิด save.php ใน browser |
| ESP32 ส่งไม่ขึ้น (firebase) | API_KEY/DATABASE_URL ผิด | ดู Serial Monitor หา sign-in error |
| ต่อผ่าน 4G ไม่ได้ แต่ WiFi ได้ | server เป็น LAN ภายใน (server อาจารย์) | ต้องมี public IP — Firebase ไม่มีปัญหานี้ |

> 💡 **ข้อสังเกต:** `device` (เช่น `'buoy-01'`) ต้องเหมือนกันทั้ง 3 ที่ (ESP32, Dashboard, และ path ใน cloud) เสมอ ไม่ว่าโหมดไหน
