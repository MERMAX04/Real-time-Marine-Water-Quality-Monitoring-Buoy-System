# 1-sensor-tools — เครื่องมือแก้ปัญหาอ่านค่า sensor (RS485/Modbus)

ใช้ตอน "ส่งคำสั่งไปแล้ว sensor ไม่ตอบ" รันบน PC ผ่าน RS485-to-USB (ยังไม่ต้องใช้ ESP32)

## ติดตั้งครั้งเดียว
```
py -m pip install pyserial
```

## ไฟล์
| ไฟล์ | ใช้ตอนไหน |
|------|-----------|
| `rs485_scan.py` | ยังไม่รู้ค่า baud/address ที่ถูก — ไล่สแกนหาอัตโนมัติ |
| `rs485_raw.py`  | รู้ค่าแล้ว/มีคู่มือ — ส่ง hex command เฉพาะ |
| `note-troubleshooting.txt` | สรุปวิธีวินิจฉัยฉบับเต็ม |

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
