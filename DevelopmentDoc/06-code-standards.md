# 6. Code Comments & Standards

## 6.1 Coding conventions ที่ใช้

### ทั่วไป
- **คอมเมนต์เป็นภาษาไทย** อธิบาย "ทำไม" · **ชื่อตัวแปร/ฟังก์ชันเป็นภาษาอังกฤษ**
- ไฟล์สำคัญมี **คอมเมนต์หัวไฟล์** บอกหน้าที่ วิธี deploy/ใช้งาน และค่า env ที่ต้องตั้ง (ดู `telegram-bot/index.ts`, `admin/index.ts`, `save.php`, `Full-Version.ino`)
- ชื่อคอลัมน์ฐานข้อมูลและคีย์ JSON ใช้ **snake_case** ชุดเดียวกันทุกส่วน: `do_val, do_pct, temp, ph, sal, cond, turb, sensor_ok, lat, lon`
- ค่าที่ปรับบ่อยรวมไว้จุดเดียว: เกณฑ์ → `water-modes.json` · ค่าเว็บ → `const CONFIG` · ค่า firmware → บล็อก "แก้ค่าตรงนี้" ต้นไฟล์

| ภาษา | รูปแบบชื่อ | ตัวอย่าง |
|---|---|---|
| C++ (firmware) | ค่าคงที่/ขา `UPPER_SNAKE` · ค่าที่อ่านจากเซนเซอร์ขึ้นต้น `s` · ฟังก์ชัน `camelCase` | `PIN_485_RX`, `SENSOR_ADDR`, `sDO`, `sDOpct`, `readSensor()`, `httpPost()` |
| TypeScript (Edge) | `camelCase` · ค่าคงที่ `UPPER_SNAKE` · type `PascalCase` | `evalWater()`, `fmtStatus()`, `COOLDOWN_MS`, `WaterEval` |
| JavaScript (เว็บ) | `camelCase` · ฟังก์ชันที่มิเรอร์ฝั่ง Edge ลงท้าย `JS` | `evalWaterJS()`, `paramStatusMode()`, `computeWQI()` |
| PHP | `snake_case` + prefix ตามโมดูล: `wm_` (water modes), `tg_` (Telegram) | `wm_eval()`, `wm_get_mode()`, `tg_check_and_alert()` |
| SQL | `snake_case` · index `idx_<table>_<cols>` | `readings`, `idx_readings_device_time` |
| Python | `snake_case` (PEP 8) | `build_read()`, `validate_frame()` |

## 6.2 คำอธิบายส่วนที่ซับซ้อน
| ส่วน | ที่อยู่ | อธิบาย |
|---|---|---|
| **อ่าน Modbus + ถอด float** | `Full-Version.ino` → `readSensor()` | ส่ง `01 03 26 00 00 16 + CRC` รอ 49 ไบต์ (timeout 1 วิ) ตรวจ address/function/byte count/CRC ก่อน แล้ว `memcpy` 4 ไบต์ต่อค่า เพราะเซนเซอร์ส่ง float แบบ little-endian (DCBA) ซึ่งตรงกับ ESP32 · ถ้า pH/DO/EC เป็น 0 ทั้งหมด ถือว่าเซนเซอร์ยังไม่พร้อม |
| **CRC-16 Modbus** | `modbusCRC()` (firmware), `crc()` / `modbus_crc()` (Python) | เริ่ม `0xFFFF` · polynomial `0xA001` · ส่ง low byte ก่อน — ตรวจกับตัวอย่างในคู่มือแล้ว |
| **HTTPS ผ่านโมเด็ม A7670E** | `Full-Version.ino` → `setup()` + `httpPost()` | ใช้คำสั่ง `AT+HTTP...` ของโมเด็มแทน TLS socket (ไม่เสถียร) และต้องตั้ง SSL context `enableSNI=1` เพราะ Supabase อยู่หลัง Cloudflare (ไม่ตั้งจะได้ error 715) |
| **Heartbeat** | `Full-Version.ino` → `loop()` | อ่านเซนเซอร์ไม่ได้ 3 ครั้ง ยังส่งแถว `sensor_ok=false` เพื่อแยก "เซนเซอร์เสีย" ออกจาก "ทุ่น/4G หลุด" |
| **ประเมินตามโหมด (Model A)** | `water-eval.ts` → `evalWater()` (+ มิเรอร์ JS/PHP) | แต่ละค่าเทียบช่วง green/orange ของโหมด → สีรายค่า · ค่าระดับ `critical` เป็นแดง → รวมแดง · ค่า `advisory` แดง/ส้ม → รวมส้ม · ความเค็ม < 1 ppt → "ประเมินไม่ได้" |
| **ดัชนี WQI + เพดานตามสถานะรวม** | `index.html` → `computeWQI()`, `reconcileWQI()` | WQI เป็นค่าเฉลี่ยถ่วงน้ำหนักซึ่งอาจกลบค่าที่อันตราย จึงจำกัดเพดาน: รวมส้ม ≤ 59 ("ปานกลาง") · รวมแดง ≤ 39 ("แย่") · ประเมินไม่ได้ → แสดง "—" |
| **สถานะทุ่น (liveness)** | `index.html` → `updateStatus()` · `telegram.ts` → `freshnessLine()` | คำนวณจากอายุแถวล่าสุด: < 45 วิ ออนไลน์ · 45 วิ–5 นาที ข้อมูลค้าง · > 5 นาที ออฟไลน์ |
| **กันแจ้งเตือนซ้ำ** | `telegram-alert/index.ts` | คีย์ cooldown = ทุ่น + โหมด + ชุดค่าที่ตก → ปัญหาเดิมไม่เตือนซ้ำใน 30 นาที แต่ปัญหาใหม่เตือนได้ทันที |
| **สิทธิ์แอดมิน** | `admin/index.ts` + `index.html` | ตรวจรหัสฝั่งเซิร์ฟเวอร์ (secret) → ได้ token (SHA-256) เก็บใน `sessionStorage` · เปลี่ยนโหมดต้องแนบ token · RLS ปิดสิทธิ์ update ของ anon |

## 6.3 Best practices ของโปรเจค
1. **ทุ่นต้องเบา** — ทุ่นแค่อ่านค่าและส่ง ตรรกะอื่นทำบนคลาวด์ จะได้ไม่ต้อง flash ทุ่นที่อยู่กลางทะเล
2. **เกณฑ์อยู่ใน config ไม่อยู่ในโค้ด** — แก้ `water-modes.json` แล้วคัดลอกให้ครบ 3 ที่ · ตรรกะประเมิน 3 ภาษาต้องแก้พร้อมกันเสมอ
3. **ความลับไม่อยู่ใน git** — token/รหัสอยู่ใน Supabase secrets หรือไฟล์ `*.local.*` ที่ถูก ignore · `service_role` key ใช้เฉพาะใน Edge Function · เว็บและทุ่นใช้ anon key ซึ่งถูกจำกัดด้วย RLS
4. **เก็บเฉพาะค่าที่วัดได้จริง** — ไม่เก็บค่าคำนวณ (TDS) หรือช่องของหัววัดที่ไม่ได้ติดตั้ง
5. **ลำดับการเปลี่ยนโครงสร้างข้อมูล** — แก้ firmware ให้เลิกส่ง field ก่อน แล้วค่อยลบคอลัมน์ (ไม่งั้น HTTP 400 ทั้งแถว)
6. **ข้อผิดพลาดต้องอธิบายได้** — firmware และเครื่องมือ Python พิมพ์สาเหตุที่น่าจะเป็นพร้อมวิธีแก้ (สาย/ไฟ/GND/CRC)
7. **ตรวจโค้ดก่อน deploy**
   | ส่วน | คำสั่งตรวจ |
   |---|---|
   | Firmware | Arduino IDE → Verify (หรือ `arduino-cli compile --fqbn esp32:esp32:esp32 Full-Version`) |
   | Edge Functions | `deno check supabase/functions/<ชื่อ>/index.ts` |
   | PHP | `php -l <ไฟล์>.php` |
   | Python | `py -m py_compile <ไฟล์>.py` |
8. **Commit ภาษาอังกฤษสั้นๆ บอกสิ่งที่เปลี่ยนและเหตุผล** · push ขึ้น `main` = Render deploy เว็บอัตโนมัติ (Edge Functions ต้อง deploy เองด้วย CLI)
