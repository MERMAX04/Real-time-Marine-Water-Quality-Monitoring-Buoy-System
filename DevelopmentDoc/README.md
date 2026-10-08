# DevelopmentDoc — เอกสารประกอบการพัฒนาระบบ

ระบบทุ่นตรวจวัดคุณภาพน้ำทะเลแบบเรียลไทม์ (Real-time Marine Water Quality Monitoring Buoy System)
ข้อมูลในเอกสารชุดนี้ตรงกับซอร์สโค้ดและฐานข้อมูล ณ วันที่ 8 ตุลาคม 2569

| # | เอกสาร | เนื้อหา |
|---|--------|---------|
| 1 | [README.md (หน้าแรกของโปรเจค)](../README.md) | โปรเจคคืออะไร · ทำงานอย่างไร · วิธีติดตั้งและรัน · ตัวอย่างการใช้งานเบื้องต้น |
| 2 | [02-architecture.md](02-architecture.md) | ภาพรวมสถาปัตยกรรม (diagram) · เทคโนโลยีที่ใช้และเหตุผลที่เลือก · โครงสร้าง folder/module |
| 3 | [03-api-documentation.md](03-api-documentation.md) | รายการ API endpoint ทั้งหมด · รูปแบบ request/response · ตัวอย่างการเรียกใช้ |
| 4 | [04-database-schema.md](04-database-schema.md) | ER diagram · คำอธิบายตารางและความสัมพันธ์ · index · สิทธิ์ (RLS) |
| 5 | [05-setup-configuration.md](05-setup-configuration.md) | ค่าที่ต้องตั้ง (environment variables / config) · dependencies · ขั้นตอน deployment |
| 6 | [06-code-standards.md](06-code-standards.md) | coding conventions · คำอธิบายส่วนที่ซับซ้อน · best practices ของโปรเจค |
| 7 | [07-known-issues-future-plans.md](07-known-issues-future-plans.md) | ข้อจำกัด/ปัญหาที่ทราบ · ฟีเจอร์ที่วางแผน · แนวทางพัฒนาต่อ |

## ภาพรวมระบบโดยย่อ
- **ทุ่น:** ESP32 (LilyGO T-Call-A7670) อ่านเซนเซอร์คุณภาพน้ำผ่าน RS485/Modbus ทุก 15 วินาที ได้ **7 ค่า** (DO, DO%, อุณหภูมิ, pH, ความเค็ม, การนำไฟฟ้า, ความขุ่น) + พิกัด GPS แล้วส่งขึ้นคลาวด์ผ่าน 4G
- **คลาวด์:** Supabase (PostgreSQL + Realtime + Edge Functions) เก็บข้อมูลและประเมินตาม **มาตรฐานคุณภาพน้ำทะเลของไทย 6 ประเภท**
- **ผู้ใช้:** เว็บแดชบอร์ด (Render) และ Telegram Bot
- **แผนสำรอง:** PHP + MySQL ที่ใช้ตรรกะและเกณฑ์ชุดเดียวกัน
