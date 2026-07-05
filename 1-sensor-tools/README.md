# 1-sensor-tools — เครื่องมือแก้ปัญหาอ่านค่า sensor (RS485/Modbus)

ใช้ตอน "ส่งคำสั่งไปแล้ว sensor ไม่ตอบ" รันบน PC ผ่าน RS485-to-USB (ยังไม่ต้องใช้ ESP32)

## ติดตั้งครั้งเดียว
```
py -m pip install pyserial
```

## ไฟล์
| ไฟล์ | ใช้ตอนไหน |
|------|-----------|
| **`read_sensor.py`** ⭐ | **ตัวหลัก** — อ่านค่าจริงทุกตัว + ถอด float ให้เลย (ตั้งค่าตามคู่มือแล้ว) |
| **`SENSOR-PROTOCOL.md`** | สรุป protocol ครบจากคู่มือ (baud/register/wiring/float) |
| `rs485_scan.py` | เผื่อ address/baud เพี้ยน — ไล่สแกนหาอัตโนมัติ |
| `rs485_raw.py`  | ส่ง hex command เฉพาะเจาะจงเอง |
| `note-troubleshooting.txt` | สรุปวิธีวินิจฉัยฉบับแรก |

## ⚡ ค่าที่ได้จากคู่มือ (ดูละเอียดใน SENSOR-PROTOCOL.md)
- Modbus RTU **9600 8N1**, address **0x01**
- อ่านทุกค่า: `01 03 26 00 00 16 CF 4C`
- **ต่อสาย:** 🔴แดง=VCC→ไฟ 12–24V·≥1A  ⚫ดำ=GND→ไฟ−**และ GND adapter**  🟢เขียว=A  ⚪ขาว=B

## วิธีใช้
```
py rs485_scan.py           # ลิสต์พอร์ต COM ที่มี
py rs485_scan.py COM3      # สแกนหาที่ COM3
```

## อ่านผล
- เจอ `[GOT]` → สาย+โปรโตคอลถูก จดค่า baud/addr/parity ไว้
- เจอ `EXCEPTION` → sensor คุยได้แล้ว แค่ register ผิด
- เงียบสนิท → ปัญหาที่สาย: สลับ A↔B, เช็ค GND, เช็คไฟเลี้ยง

รายละเอียดใน `note-troubleshooting.txt`
