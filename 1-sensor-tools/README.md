# 1-sensor-tools — เครื่องมืออ่านค่า sensor (RS485/Modbus) บน PC

รันบน PC ผ่านสาย **USB-to-RS485** (ไม่ต้องใช้ ESP32) — ใช้ทดสอบ/อ่านค่า sensor จริง และพิสูจน์ทั้ง pipeline
> ทดสอบแล้วอ่านค่าได้จริง (พอร์ต COM9 ชิป CH343) — รายละเอียด protocol ดู [SENSOR-PROTOCOL.md](SENSOR-PROTOCOL.md)

**ค่าที่อ่าน (7 ค่า จาก 4 หัววัด):** DO · DO% · อุณหภูมิ · pH · ความเค็ม · การนำไฟฟ้า · ความขุ่น

## ติดตั้งครั้งเดียว
```
py -m pip install pyserial
```

## ไฟล์
| ไฟล์ | ใช้ทำอะไร |
|------|-----------|
| **`read_sensor.py`** ⭐ | **ตัวหลัก** — สแกน COM port อัตโนมัติ + อ่าน 7 ค่า + ถอด float + debug ทุกขั้น (บอกสาเหตุเมื่ออ่านไม่ได้) |
| **`rs485_console.py`** | คอนโซลโต้ตอบ — พิมพ์คำสั่ง Modbus เอง ดู TX/RX ดิบ (เจาะทีละ register) |
| **`sensor_to_supabase.py`** | "สะพาน" — PC อ่าน sensor แล้วส่งขึ้น Supabase (พิสูจน์ทั้ง pipeline โดยไม่ต้องมี ESP32) |
| `SENSOR-PROTOCOL.md` | สรุป protocol จากคู่มือ (การต่อสาย / register / float order / CRC) |

## การต่อสาย (จุดที่ทำให้ "ไม่ตอบ")
🔴 แดง=VCC→ไฟ **12–24V ≥1A** · ⚫ ดำ=GND→ไฟ− **และ GND ของ adapter (กราวด์ร่วม!)** · 🟢 เขียว=485_A→A · ⚪ ขาว=485_B→B

## วิธีใช้
```
py read_sensor.py                 # สแกนหา sensor อัตโนมัติแล้วอ่านเลย
py read_sensor.py COM9            # ระบุพอร์ตตรงๆ
py read_sensor.py COM9 --scan     # ถ้าเงียบ ให้ไล่ baud/address อื่นด้วย
py rs485_console.py COM9          # คอนโซลพิมพ์คำสั่งเอง (all / do / do% / temp / ph / sal / cond / turb / status)
py sensor_to_supabase.py COM9 5   # อ่านค่าจริงส่งขึ้น Supabase ทุก 5 วิ
```

## อ่านผล (read_sensor.py บอกทีละขั้น)
- ได้ 7 ค่าในช่วงสมเหตุสมผล → อ่านสำเร็จ ✅
- `EXCEPTION` → สายและโปรโตคอลถูกแล้ว แค่ register ที่ขอไม่ถูก
- เงียบสนิท → ปัญหาที่สาย/ไฟ: สลับ A↔B, เช็ค GND ร่วม, เช็คไฟ 12V

## โปรโตคอล (ย่อ)
- Modbus RTU **9600 8N1**, address **0x01**
- อ่านรวดเดียว: `01 03 26 00 00 16 CF 4C` (register 0x2600 → ใช้ช่อง 0,1,2,3,4,8,10)
- float = **IEEE754 little-endian (DCBA)**
