# 🌊 ทุ่นตรวจคุณภาพน้ำทะเลแบบ Realtime (โปรเจคจบ)

ระบบตรวจวัดคุณภาพน้ำทะเลด้วยทุ่นลอย ส่งข้อมูลขึ้น cloud แบบ realtime แล้วแสดงผลบน Dashboard

## เส้นทางข้อมูล (ภาพรวม)
```
   Sensor  --RS485/Modbus-->  ESP32  --โมดูล 4G-->  Cloud  -->  Dashboard
                                                    │
                              แผนหลัก  = Supabase (PostgreSQL) ──┤
                              แผนสำรอง = Server อาจารย์ (PHP+MySQL)
```

## โครงสร้างโฟลเดอร์
| โฟลเดอร์ | คืออะไร | สถานะ |
|----------|---------|-------|
| **1-sensor-tools/** | เครื่องมือ Python แก้ปัญหาอ่านค่า sensor ผ่าน RS485 | 🔧 กำลังแก้ปัญหา |
| **2-dashboard/** | หน้าเว็บแสดงผล (สลับแหล่งข้อมูลได้ 4 โหมด) | ✅ ใช้งานได้ (mock) |
| **3-supabase-primary/** | ★ แผนหลัก ★ Supabase (SQL) — schema.sql + โค้ด ESP32 | ✅ ตั้งค่า+ต่อ Dashboard แล้ว |
| **4-server-backup/** | ทางเลือก: PHP+MySQL API + โค้ด ESP32 | 📦 เตรียมไว้แล้ว |
| **5-extensions/** | roadmap ต่อยอด: AI ผู้ช่วย, แจ้งเตือน, เทรนด์, หลายทุ่น | 💡 แนวทางเตรียมไว้ |

> 🔄 **จะสลับ Supabase ↔ Server?** ดู [SWITCHING-GUIDE.md](SWITCHING-GUIDE.md) — บอกจุดที่ต้องแก้ทั้งฝั่ง ESP32 และ Dashboard
> 💡 **อยากต่อยอด?** ดู [5-extensions/README.md](5-extensions/README.md) — AI ผู้ช่วย (Claude), แจ้งเตือน Telegram, เทรนด์/heatmap, หลายทุ่น

## ตอนนี้ทำอะไรได้เลย
1. **ดู Dashboard:** เปิด `2-dashboard/index.html` — เห็นค่าปลอมขยับ realtime (โหมด mock)
2. **ตั้ง Supabase (แผนหลัก):** ทำตาม `3-supabase-primary/README.md` ทีละขั้น แล้วเปลี่ยน `CONFIG.mode = 'supabase'`
3. **แก้ปัญหา sensor:** ทำตาม `1-sensor-tools/README.md`

## ค่าที่วัด (7 พารามิเตอร์)
ออกซิเจนละลายน้ำ (DO), ความเค็ม, ความขุ่น, คลอโรฟิลล์, ศักย์ออกซิเดชัน (ORP), น้ำมัน, สาหร่ายสีเขียว

## ลำดับงานที่แนะนำ
1. ✅ ทำ Dashboard (เสร็จ - โหมด mock)
2. ✅ ตั้ง Supabase + ต่อ Dashboard (mode='supabase') — ทดสอบ insert แถวใน Table Editor
3. ⏳ ต่อ ESP32 (โมดูล 4G) ส่งค่าปลอมขึ้น Supabase (พิสูจน์ทั้งวงจร โดยไม่ต้องรอ sensor)
4. ⏳ แก้ปัญหา RS485 อ่านค่า sensor จริง
5. ⏳ รวมทุกอย่าง + เปลี่ยนจาก WiFi เป็นโมดูล 4G
