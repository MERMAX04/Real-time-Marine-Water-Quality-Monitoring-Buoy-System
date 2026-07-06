# -*- coding: utf-8 -*-
"""
rs485_console.py  —  คอนโซลโต้ตอบ ยิงคำสั่ง Modbus เองแล้วดู response ดิบๆ
------------------------------------------------------------------------------
เอาไว้ "ลองเล่น" กับ sensor: พิมพ์คำสั่ง -> เห็น TX (ส่ง) / RX (รับ) เป็น hex + ถอดค่าให้

วิธีใช้:
    py rs485_console.py            # ใช้ COM9
    py rs485_console.py COM9       # ระบุพอร์ต

พิมพ์อะไรได้บ้าง (ที่ prompt >>> ):
    all              อ่านทุกค่ารวดเดียว (0x2600, 22 regs = 11 float)
    do  ph  temp  sal  turb  orp  chl  cond  tds  do%   อ่านทีละค่า
    oil   algae      อ่านน้ำมัน(0x260D) / สาหร่าย(0x260E)
    status           อ่าน sensor status (0x0800) — probe ไหนต่ออยู่
    ver              อ่านเวอร์ชัน (0x0700)
    r 2601 2         อ่านเอง: register 0x2601 จำนวน 2 regs (ใส่เลขฐาน16)
    01 03 26 01 00 02        พิมพ์ hex เอง (จะเติม CRC ให้อัตโนมัติ)
    raw 01 03 26 01 00 02 9E 83   ส่ง hex ตรงๆ ทั้งหมด (ไม่เติม CRC — ไว้ทดสอบ CRC ผิด)
    list             โชว์คำสั่งสำเร็จรูปทั้งหมด
    q                ออก
"""
import sys, time, struct

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

try:
    import serial
except ImportError:
    print("[!] ยังไม่ได้ติดตั้ง pyserial -> รัน:  py -m pip install pyserial"); sys.exit(1)

ADDR, BAUD = 0x01, 9600
PORT = sys.argv[1] if len(sys.argv) > 1 else "COM9"

def crc(data: bytes) -> bytes:
    c = 0xFFFF
    for b in data:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ 0xA001 if (c & 1) else (c >> 1)
    return bytes([c & 0xFF, (c >> 8) & 0xFF])

def build(start, count):
    body = bytes([ADDR, 0x03, (start >> 8) & 0xFF, start & 0xFF, (count >> 8) & 0xFF, count & 0xFF])
    return body + crc(body)

def hexs(b): return " ".join(f"{x:02X}" for x in b) if b else "(ว่าง)"

# คำสั่งสำเร็จรูป: ชื่อ -> (register, จำนวน regs, ชื่อค่า)
PRESETS = {
    "all":   (0x2600, 22, "ทุกค่า (11 float)"),
    "do":    (0x2601, 2,  "DO (mg/L)"),
    "turb":  (0x2602, 2,  "Turbidity (NTU)"),
    "cond":  (0x2603, 2,  "Conductivity (mS/cm)"),
    "ph":    (0x2604, 2,  "pH"),
    "temp":  (0x2606, 2,  "Temperature (C)"),
    "sal":   (0x2608, 2,  "Salinity (ppt)"),
    "do%":   (0x260A, 2,  "DO (%)"),
    "orp":   (0x260B, 2,  "ORP (mV)"),
    "chl":   (0x260C, 2,  "Chlorophyll (ug/L)"),
    "oil":   (0x260D, 2,  "OIW น้ำมัน (ppm)"),
    "algae": (0x260E, 2,  "BGA สาหร่าย (cells/mL)"),
    "tds":   (0x260F, 2,  "TDS"),
    "status":(0x0800, 1,  "Sensor status (probe flags)"),
    "ver":   (0x0700, 2,  "เวอร์ชัน hw/sw"),
}

def decode_floats(payload):
    """ถอด float DCBA/little-endian ทีละ 4 ไบต์"""
    return [struct.unpack('<f', payload[i:i+4])[0] for i in range(0, len(payload) - 3, 4)]

def show_response(resp):
    print(f"  RX  : {hexs(resp)}   ({len(resp)} ไบต์)")
    if len(resp) == 0:
        print("  ➜ เงียบสนิท: เช็คไฟ/GND ร่วม/สลับ A↔B"); return
    if len(resp) < 5:
        print("  ➜ สั้นเกินไป (ไม่ครบเฟรม)"); return
    # exception?
    if resp[1] & 0x80:
        print(f"  ➜ ⚠️ EXCEPTION fn=0x{resp[1]:02X} code=0x{resp[2]:02X} (คุยได้แล้ว แต่คำขอไม่ถูกใจ)")
        return
    if resp[1] != 0x03:
        print(f"  ➜ function code = 0x{resp[1]:02X} (ไม่ใช่ 0x03)"); return
    n = resp[2]
    payload = resp[3:3 + n]
    ok = len(payload) >= n and crc(resp[:3 + n]) == resp[3 + n:3 + n + 2]
    print(f"  ➜ byte count = {n}, CRC {'ผ่าน ✓' if ok else 'ไม่ผ่าน ✗ (ข้อมูลอาจเพี้ยน)'}")
    if len(payload) >= 4:
        vals = decode_floats(payload)
        print(f"  ค่า float (DCBA): {', '.join(f'{v:.3f}' for v in vals)}")

def send(ser, frame):
    print(f"  TX  : {hexs(frame)}")
    ser.reset_input_buffer(); ser.write(frame); ser.flush()
    time.sleep(0.3)
    show_response(ser.read(256))

def parse_line(line):
    """คืน bytes ที่จะส่ง หรือ None ถ้าเป็นคำสั่งควบคุม"""
    s = line.strip()
    low = s.lower()
    if low in PRESETS:
        reg, cnt, name = PRESETS[low]
        print(f"  [{low}] {name}  (register 0x{reg:04X}, {cnt} regs)")
        return build(reg, cnt)
    if low.startswith("r "):                    # r 2601 2
        parts = s.split()
        reg = int(parts[1], 16); cnt = int(parts[2]) if len(parts) > 2 else 2
        return build(reg, cnt)
    if low.startswith("raw "):                  # ส่ง hex ตรงๆ ไม่เติม CRC
        return bytes(int(x, 16) for x in s[4:].split())
    # เป็น hex ล้วน -> เติม CRC ให้
    try:
        body = bytes(int(x, 16) for x in s.split())
        return body + crc(body)
    except ValueError:
        print("  ไม่เข้าใจคำสั่งนี้ — พิมพ์ 'list' ดูตัวอย่าง"); return None

def main():
    print(f"=== RS485 Console ===  พอร์ต {PORT} @ {BAUD} 8N1   (พิมพ์ 'list' ดูคำสั่ง, 'q' ออก)")
    try:
        ser = serial.Serial(PORT, BAUD, bytesize=8, parity=serial.PARITY_NONE, stopbits=1, timeout=1.2)
    except serial.SerialException as e:
        print(f"[!] เปิดพอร์ต {PORT} ไม่ได้: {e}"); return
    with ser:
        while True:
            try:
                line = input(">>> ").strip()
            except (EOFError, KeyboardInterrupt):
                print(); break
            if not line: continue
            if line.lower() in ("q", "quit", "exit"): break
            if line.lower() == "list":
                print("  คำสั่งสำเร็จรูป:")
                for k, (reg, cnt, name) in PRESETS.items():
                    print(f"    {k:<8} 0x{reg:04X} x{cnt}  - {name}")
                print("    r <hexReg> <count> | <hex...> (เติม CRC ให้) | raw <hex...> (ส่งตรงๆ)")
                continue
            frame = parse_line(line)
            if frame:
                send(ser, frame)
    print("ปิดคอนโซลแล้ว")

if __name__ == "__main__":
    main()
