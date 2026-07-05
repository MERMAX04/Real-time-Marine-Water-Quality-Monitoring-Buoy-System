# 04 — หลายทุ่นบนแผนที่จริง

ขยายจากทุ่นเดียวเป็นหลายสถานี แสดงบนแผนที่จริง เลือกดูทีละทุ่นได้

## ข่าวดี: โครงข้อมูลรองรับอยู่แล้ว
เราออกแบบให้มี `device` ตั้งแต่แรก:
- PHP: ตาราง `readings` มีคอลัมน์ `device` (ดู `db.sql`)
- Firebase: ข้อมูลอยู่ใต้ `buoy-01/...` → เพิ่ม `buoy-02/...` ได้เลย
- Dashboard: มี `CONFIG.device` อยู่แล้ว

## สิ่งที่ต้องเพิ่ม

### 1) ตัวเลือกสถานี (station selector)
Dropdown ให้เลือกทุ่น → เปลี่ยน `CONFIG.device` แล้ว re-subscribe/ดึงใหม่
```js
// ตัวอย่าง: เปลี่ยนทุ่นแล้วโหลดข้อมูลของทุ่นนั้น
function switchDevice(id){ CONFIG.device = id; history/*ล้าง*/; initSource(); }
```

### 2) ตารางทะเบียนทุ่น (พิกัดแต่ละตัว)
เก็บ lat/lon ของแต่ละทุ่น เช่นตาราง `devices`:
```sql
CREATE TABLE devices (
  device VARCHAR(32) PRIMARY KEY,
  name   VARCHAR(64),
  lat    DOUBLE,
  lon    DOUBLE
);
```

### 3) แผนที่จริง
ตอนนี้ Dashboard ใช้เรดาร์จำลอง (offline) — ถ้าต้องการแผนที่จริง:
- **Leaflet + OpenStreetMap** (ฟรี ไม่ต้อง API key) วางหมุดแต่ละทุ่น
- สีหมุดตามสถานะ (ปกติ/เฝ้าระวัง/วิกฤต) ของทุ่นนั้น
- คลิกหมุด → เปิด Dashboard ของทุ่นนั้น
- หมายเหตุ: Leaflet ต้องโหลดจาก CDN (ต้องต่อเน็ต) — ต่างจากหน้าปัจจุบันที่ offline ได้

## ปรับ API ให้รับหลายทุ่น
`latest.php` / `history.php` รับ `?device=buoy-02` อยู่แล้ว — เพิ่มแค่ `list-devices.php`
สำหรับดึงรายชื่อทุ่นทั้งหมดมาทำ dropdown + หมุดบนแผนที่

## ระวัง
- ESP32 แต่ละตัวต้องตั้ง `DEVICE_ID` ไม่ซ้ำกัน (buoy-01, buoy-02, ...)
- ค่า `device` ต้องตรงกันทุกที่เสมอ (ESP32 / cloud / Dashboard) — เหมือนที่เตือนใน SWITCHING-GUIDE.md
