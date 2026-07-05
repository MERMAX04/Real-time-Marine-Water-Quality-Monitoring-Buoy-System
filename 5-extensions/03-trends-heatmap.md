# 03 — เทรนด์รายวัน/สัปดาห์ + Heatmap

Dashboard ตอนนี้เก็บประวัติแค่ในหน่วยความจำเบราว์เซอร์ (หายเมื่อปิดหน้า)
โมดูลนี้ทำให้ดูย้อนหลัง **หลายวัน/สัปดาห์** จากข้อมูลจริงที่สะสมใน cloud + เห็นรูปแบบรายชั่วโมง

## ต้องมีก่อน: ข้อมูลจริงถูกเก็บลง DB/Firebase
- ฝั่ง PHP: ตาราง `readings` มีคอลัมน์ `ts` อยู่แล้ว (ดู `4-server-backup/db.sql`) → พร้อมทำ query ย้อนหลังได้เลย
- ฝั่ง Firebase: ESP32 push ลง `buoy-01/history` อยู่แล้ว (ดู `3-firebase-primary/esp32-firebase-test.ino`)

## แนวทางทำ

### A) กราฟเทรนด์รายวัน (ค่าเฉลี่ยต่อวัน)
เพิ่ม API `api/daily.php` ที่ query ค่าเฉลี่ยรายวัน:
```php
<?php require __DIR__.'/../config.php';
$st = db()->query(
  "SELECT DATE(ts) d, AVG(do_val) do_avg, AVG(sal) sal_avg, AVG(turb) turb_avg
   FROM readings WHERE ts >= DATE_SUB(NOW(), INTERVAL 7 DAY)
   GROUP BY DATE(ts) ORDER BY d");
echo json_encode($st->fetchAll());
```
แล้วใน Dashboard วาดกราฟแท่ง/เส้น 7 วันล่าสุด (ใช้โครงวาด SVG เดิมได้)

### B) Heatmap รายชั่วโมง (ดูว่าน้ำแย่ช่วงเวลาไหนของวัน)
ตาราง 24 ชั่วโมง × 7 วัน ระบายสีตามค่า — เห็นแพทเทิร์น เช่น "DO ต่ำช่วงเช้ามืด"
- query: `GROUP BY HOUR(ts), DAYOFWEEK(ts)`
- ระบายสี: ใช้ **sequential ramp สีเดียว อ่อน→เข้ม** (เช่น ฟ้า) ตามค่า — อย่าใช้สีรุ้ง
  (แนวทางสีตาม dataviz: ค่าน้อย=จางเข้าใกล้พื้น, ค่ามาก=เข้ม)

## หมายเหตุการออกแบบสี (จาก dataviz skill)
- **Heatmap = sequential 1 สี** ไล่อ่อน→เข้ม ห้ามรุ้ง
- มี legend เสมอ + มี "ตารางดูข้อมูลดิบ" ให้กดสลับ (เข้าถึงง่าย)
- ระวัง contrast กับพื้นหลังธีมทะเล

## เก็บข้อมูลนานแค่ไหน?
- ทุ่นตัวเดียวส่งทุก 5 นาที = ~288 แถว/วัน ~ แสนแถว/ปี → MySQL/Firebase สบายๆ
- ถ้าอยากประหยัด: เก็บ raw 30 วัน + สรุปเป็นค่าเฉลี่ยรายวันเก็บยาว
