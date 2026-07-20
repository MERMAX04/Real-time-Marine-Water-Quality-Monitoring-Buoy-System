# 1-sensor-tools — เครื่องมืออ่านค่า sensor (RS485/Modbus) บน PC

รันบน PC ผ่าน **USB-to-RS485** (ยังไม่ต้องใช้ ESP32) — ใช้ทดสอบ/อ่านค่า sensor จริง
> ✅ ทดสอบแล้วอ่านค่าได้จริง (พอร์ต COM9 ชิป CH343) — ดูบันทึกใน [[SENSOR-PROTOCOL.md]]

## ติดตั้งครั้งเดียว
```
py -m pip install pyserial
```

## ไฟล์ (3 ตัว)
| ไฟล์ | ใช้ทำอะไร |
|------|-----------|
| **`read_sensor.py`** ⭐ | **ตัวหลัก** — สแกน COM port อัตโนมัติ + อ่านค่าทุกตัว + ถอด float + debug ทุกจุด |
| **`rs485_console.py`** | คอนโซลโต้ตอบ — พิมพ์คำสั่ง Modbus เอง ดู TX/RX ดิบ (เล่น/เจาะทีละ register) |
| **`sensor_to_supabase.py`** | "สะพาน" — PC อ่าน sensor แล้วส่งขึ้น Supabase (พิสูจน์ทั้ง pipeline โดยไม่ต้องมี ESP32) |
| `SENSOR-PROTOCOL.md` | สรุป protocol ครบจากคู่มือ (baud/register/wiring/float order) |

## การต่อสาย (จุดที่ทำให้ "ไม่ตอบ")
🔴 แดง=VCC→ไฟ **12–24V ≥1A**  ⚫ ดำ=GND→ไฟ− **และ GND ของ adapter (กราวด์ร่วม!)**  🟢 เขียว=485_A→A  ⚪ ขาว=485_B→B

## วิธีใช้
```
py read_sensor.py                 # สแกนหา sensor อัตโนมัติแล้วอ่านเลย
py read_sensor.py COM9            # ระบุพอร์ตตรงๆ (ถ้าสแกนช้าเพราะพอร์ต Bluetooth)
py rs485_console.py COM9          # เปิดคอนโซลพิมพ์คำสั่งเอง (all/do/ph/temp/... )
py sensor_to_supabase.py COM9 5   # อ่านค่าจริงส่งขึ้น Supabase ทุก 5 วิ
```

## อ่านผล (read_sensor.py จะบอก step-by-step)
- ได้ค่าครบในช่วงสมเหตุสมผล → อ่านสำเร็จ ✅
- `EXCEPTION` → สาย+โปรโตคอลถูก แค่ register ผิด
- เงียบสนิท → ปัญหาที่สาย: สลับ A↔B, เช็ค GND ร่วม, เช็คไฟ 12V

## โปรโตคอล (ย่อ — ละเอียดใน SENSOR-PROTOCOL.md)
- Modbus RTU **9600 8N1**, address **0x01**
- อ่านทุกค่า: `01 03 26 00 00 16 CF 4C` (register 0x2600, 44 ไบต์ = 11 float)
- float = **IEEE754 little-endian (DCBA)**
