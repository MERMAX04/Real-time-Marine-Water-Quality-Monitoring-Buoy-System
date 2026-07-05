# 4-server-backup — แผนสำรอง (Self-hosted PHP + MySQL)

ใช้เมื่ออาจารย์ให้ใช้ server ของท่าน (แทน Firebase) — เส้นทางเหมือนกัน แค่เปลี่ยน cloud

```
ESP32 --POST--> save.php --> MySQL --> latest.php/history.php --> Dashboard
```

## ⚠️ ต้องถามอาจารย์ก่อน
1. Server มี **PHP + MySQL** ไหม (ถ้ามี ชุดนี้ใช้ได้เลย)
2. มี **public IP / โดเมน** ไหม ← สำคัญสุด ESP32 ฝั่ง 4G ต้องเข้าถึงจากภายนอกได้
3. ขอ user/pass ของ MySQL และที่วางไฟล์ (cPanel/FTP/SSH)
4. เปิด HTTPS ได้ไหม

## ติดตั้ง
1. รัน `db.sql` ใน phpMyAdmin (สร้าง database + ตาราง)
2. แก้ `config.php` ใส่ข้อมูล DB + เปลี่ยน API_KEY
3. อัปโหลดทั้งโฟลเดอร์ขึ้น server (เช่น `public_html/buoy/`)

## ทดสอบ (พิมพ์ใน browser)
```
https://server.ac.th/buoy/api/save.php?key=buoy-secret-2026&do=6.2&sal=32&turb=8&chl=3&orp=280&oil=5&algae=2
```
ได้ `{"ok":true,"id":1}` = สำเร็จ

## สลับ Dashboard มาใช้ server
ใน `2-dashboard/index.html`: ตั้ง `CONFIG.mode = 'server'` และ `apiBase` เป็น URL จริง

## ไฟล์
| ไฟล์ | หน้าที่ |
|------|---------|
| `config.php` | ตั้งค่า DB + API key |
| `db.sql` | สร้างฐานข้อมูล |
| `api/save.php` | ESP32 ส่งข้อมูลมาที่นี่ |
| `api/latest.php` | Dashboard ดึงค่าล่าสุด |
| `api/history.php` | ดึงค่าย้อนหลัง |
