# -*- coding: utf-8 -*-
"""
sensor_to_supabase.py  —  "สะพาน" อ่านค่าจริงจาก sensor (RS485) แล้วส่งขึ้น Supabase
------------------------------------------------------------------------------
ใช้พิสูจน์ทั้งวงจร "ข้อมูลจริง" บน Dashboard โดยยังไม่ต้องรอ ESP32/4G:
    Sensor -> (USB RS485) -> PC ตัวนี้ -> Supabase -> Dashboard (โหมด supabase)

พอย้ายไป ESP32 ทีหลัง Supabase/Dashboard ไม่ต้องแก้อะไรเลย

วิธีใช้:
    py -m pip install pyserial            # ครั้งเดียว (มีแล้ว)
    py sensor_to_supabase.py              # ใช้ COM9 ส่งทุก 5 วิ
    py sensor_to_supabase.py COM9 3       # ระบุพอร์ต + คาบเวลา(วิ)

จากนั้นเปิด Dashboard แล้วตั้ง CONFIG.mode = 'supabase' -> ค่าจริงจะขึ้นสดๆ
กด Ctrl+C เพื่อหยุด
"""
import sys, time, struct, json, urllib.request, urllib.error

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

try:
    import serial
except ImportError:
    print("[!] ยังไม่ได้ติดตั้ง pyserial -> รัน:  py -m pip install pyserial"); sys.exit(1)

# ===================== ตั้งค่า (ตรงกับโปรเจค) =====================
SB_URL  = "https://jjbrgolulggksxnuicgg.supabase.co"
SB_ANON = ("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
           "eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImpqYnJnb2x1bGdna3N4bnVpY2dnIiwicm9sZSI6ImFub24iLCJpYXQiOjE3ODMyNjc0ODAsImV4cCI6MjA5ODg0MzQ4MH0."
           "0coKRuYMMOwLq9yJFsOff99ya9RBwyLBzqxN2YU1JWg")
DEVICE  = "buoy-01"
PORT    = sys.argv[1] if len(sys.argv) > 1 else "COM9"
PERIOD  = float(sys.argv[2]) if len(sys.argv) > 2 else 5.0    # วินาที
ADDR, BAUD = 0x01, 9600
# =================================================================

def modbus_crc(data: bytes) -> bytes:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if (crc & 1) else (crc >> 1)
    return bytes([crc & 0xFF, (crc >> 8) & 0xFF])

def build_read(addr, start, count):
    body = bytes([addr, 0x03, (start >> 8) & 0xFF, start & 0xFF, (count >> 8) & 0xFF, count & 0xFF])
    return body + modbus_crc(body)

def read_floats(ser, start, count):
    """ส่งคำสั่งอ่าน คืน list ของ float (DCBA/little-endian) หรือ None ถ้าอ่านไม่สำเร็จ"""
    frame = build_read(ADDR, start, count)
    ser.reset_input_buffer(); ser.write(frame); ser.flush()
    time.sleep(0.3)
    resp = ser.read(256)
    if len(resp) < 5 or resp[0] != ADDR or resp[1] != 0x03:
        return None
    n = resp[2]
    data = resp[3:3 + n]
    if len(data) < n or modbus_crc(resp[:3 + n]) != resp[3 + n:3 + n + 2]:
        return None
    return [struct.unpack('<f', data[i:i+4])[0] for i in range(0, len(data) - 3, 4)]

def post_supabase(payload):
    body = json.dumps(payload).encode()
    req = urllib.request.Request(SB_URL + "/rest/v1/readings", data=body, method="POST")
    req.add_header("apikey", SB_ANON)
    req.add_header("Authorization", "Bearer " + SB_ANON)
    req.add_header("Content-Type", "application/json")
    req.add_header("Prefer", "return=minimal")
    with urllib.request.urlopen(req, timeout=10) as r:
        return r.status

def main():
    print(f"=== สะพาน Sensor -> Supabase ===  พอร์ต {PORT}  ทุก {PERIOD:.0f} วิ  (Ctrl+C เพื่อหยุด)")
    try:
        ser = serial.Serial(PORT, BAUD, bytesize=8, parity=serial.PARITY_NONE, stopbits=1, timeout=1.2)
    except serial.SerialException as e:
        print(f"[!] เปิดพอร์ต {PORT} ไม่ได้: {e}")
        print("    ➜ เช็คว่าเสียบ USB อยู่ / ปิดโปรแกรมอื่นที่จับพอร์ตนี้ / เลขพอร์ตถูกไหม (ดู list_ports)")
        return

    with ser:
        while True:
            t0 = time.time()
            f = read_floats(ser, 0x2600, 22)   # 11 ค่าหลัก
            if not f or len(f) < 11:
                print("  [อ่านไม่สำเร็จ] sensor ไม่ตอบ/เฟรมไม่ครบ — เช็คสาย/ไฟ แล้วลองใหม่")
            else:
                # แผนที่ค่าจาก sensor -> คอลัมน์ Supabase (ส่งเฉพาะ probe ที่อ่านได้จริง)
                # ลำดับ sensor: 0 DO,1 Turb,2 Cond,3 pH,4 Temp,5 ORP,6 Chl,7 OIW/BGA,8 Sal,9 TDS,10 DO%
                payload = {
                    "device": DEVICE,
                    "do_val": round(f[0], 2),
                    "turb":   round(f[1], 2),
                    "cond":   round(f[2], 3),
                    "ph":     round(f[3], 2),
                    "temp":   round(f[4], 2),
                    "sal":    round(f[8], 2),
                    "tds":    round(f[9], 2),
                    "do_pct": round(f[10], 2),
                    # ORP/Chl/OIW-BGA ตอนนี้ = 0 (ไม่มี probe/เซนเซอร์แสง) — ไม่ส่ง
                }
                try:
                    code = post_supabase(payload)
                    print(f"  ✅ ส่งขึ้น Supabase ({code}) : DO {payload['do_val']} | Temp {payload['temp']}°C | "
                          f"pH {payload['ph']} | Sal {payload['sal']} | Cond {payload['cond']} | Turb {payload['turb']}")
                except urllib.error.HTTPError as e:
                    print(f"  ❌ Supabase ปฏิเสธ HTTP {e.code}: {e.read().decode(errors='replace')[:200]}")
                    print("     ➜ 401/403=RLS/anon key, 400=คอลัมน์ผิด (รัน schema.sql/ALTER ph แล้วยัง?)")
                except Exception as e:
                    print(f"  ❌ ส่งไม่ได้ (เน็ต?): {e}")

            time.sleep(max(0, PERIOD - (time.time() - t0)))

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nหยุดแล้ว (Ctrl+C)")
