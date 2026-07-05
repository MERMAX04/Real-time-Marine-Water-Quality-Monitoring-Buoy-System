# -*- coding: utf-8 -*-
"""
read_sensor.py  —  อ่านค่าจริงจาก Online Multi-parameter Sensor (Modbus RTU)
                   พร้อมโหมด DEBUG: ตรวจทุกจุดที่ทำงานผิด + อธิบายว่าเพราะอะไร
------------------------------------------------------------------------------
ตั้งค่าตามคู่มือ: 9600 8N1, address 0x01, register 0x2600 (22 regs = 44 bytes = 11 float)
ดูรายละเอียด protocol/การต่อสาย ที่ SENSOR-PROTOCOL.md

วิธีใช้:
  py -m pip install pyserial          # ครั้งเดียว
  py read_sensor.py                   # ★ สแกนหา sensor อัตโนมัติ แล้วอ่านเลย
                                      #   (เจอพอร์ตที่ตอบ=ใช้เลย / มีหลายพอร์ต=ขึ้นเมนูให้เลือก)
  py read_sensor.py COM3              # ระบุพอร์ตเอง (ข้ามการสแกน)
  py read_sensor.py --scan            # ถ้าเงียบ ให้ไล่ baud/address ทุกค่าด้วย

การต่อสาย:  แดง=VCC->12-24V(+)  ดำ=GND->ไฟ(-)+GND adapter  เขียว=485_A->A  ขาว=485_B->B
"""
import sys, time, struct

# กัน console Windows พิมพ์ภาษาไทยไม่ออก (UnicodeEncodeError) — บังคับ stdout เป็น UTF-8
try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("[!] ยังไม่ได้ติดตั้ง pyserial -> รัน:  py -m pip install pyserial"); sys.exit(1)

ADDR = 0x01
BAUD = 9600

# ลำดับค่าใน frame 0x2600 + ช่วงค่าที่สมเหตุสมผล (ใช้เตือนถ้าค่าเพี้ยน) ดู SENSOR-PROTOCOL.md
FIELDS = [
    ("DO (ออกซิเจนละลายน้ำ)",  "mg/L",  (0, 20)),
    ("Turbidity (ความขุ่น)",   "NTU",   (0, 1000)),
    ("Conductivity",           "mS/cm", (0, 100)),
    ("pH",                     "",      (0, 14)),
    ("Temperature (อุณหภูมิ)", "C",     (-5, 60)),
    ("ORP (ศักย์ออกซิเดชัน)",  "mV",    (-1999, 1999)),
    ("Chlorophyll (คลอโรฟิลล์)","ug/L", (0, 500)),
    ("OIW/BGA (น้ำมัน/สาหร่าย)","ppm|cells/mL", (0, 300000)),
    ("Salinity (ความเค็ม)",    "ppt",   (0, 80)),
    ("TDS",                    "",      (0, 100000)),
    ("DO (%)",                 "%",     (0, 200)),
]

# ความหมาย Modbus exception code (ใช้ตอน sensor ตอบ error)
EXC = {
    0x01: "Illegal Function — function code ผิด (sensor ไม่รองรับ fn 0x03?) ไม่น่าเกิดกับตัวนี้",
    0x02: "Illegal Data Address — register address ผิด (0x2600 ไม่มีในรุ่นนี้? ลองอ่านทีละค่า)",
    0x03: "Illegal Data Value — จำนวน register ผิด (ขอ 22 มากไป? ลองน้อยลง)",
    0x04: "Slave Device Failure — sensor ภายในมีปัญหา (probe เสีย/ไม่พร้อม) ดู register 0x0800",
    0x05: "Acknowledge — sensor กำลังประมวลผล ลองใหม่",
    0x06: "Slave Busy — sensor ไม่ว่าง ลองใหม่",
}

def dbg(tag, msg): print(f"  [{tag}] {msg}")

def modbus_crc(data: bytes) -> bytes:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if (crc & 1) else (crc >> 1)
    return bytes([crc & 0xFF, (crc >> 8) & 0xFF])   # low, high

def build_read(addr, start, count):
    body = bytes([addr, 0x03, (start >> 8) & 0xFF, start & 0xFF, (count >> 8) & 0xFF, count & 0xFF])
    return body + modbus_crc(body)

def hexs(b): return " ".join(f"{x:02X}" for x in b) if b else "(ว่าง)"

# ---------------------------------------------------------------------------
# STEP 1: เปิดพอร์ต — แยกสาเหตุความผิดพลาดให้ชัด
# ---------------------------------------------------------------------------
def open_port(port, baud):
    print(f"\n[STEP 1] เปิดพอร์ต {port} @ {baud} 8N1")
    try:
        ser = serial.Serial(port, baud, bytesize=8, parity=serial.PARITY_NONE,
                            stopbits=1, timeout=1.2)
        dbg("OK", f"เปิดพอร์ตสำเร็จ (settings: {ser.get_settings()['baudrate']} 8N1, timeout {ser.timeout}s)")
        return ser
    except serial.SerialException as e:
        s = str(e).lower()
        print(f"  [FAIL] เปิดพอร์ตไม่ได้: {e}")
        if "access is denied" in s or "permissionerror" in s or "could not open" in s and "denied" in s:
            print("  ➜ สาเหตุ: พอร์ตถูกโปรแกรมอื่นเปิดค้างอยู่ (Windows ให้ใช้ 1 โปรแกรมต่อ COM)")
            print("     วิธีแก้: ปิด Arduino IDE Serial Monitor / โปรแกรม terminal อื่น / สคริปต์ตัวเก่า แล้วรันใหม่")
        elif "filenotfounderror" in s or "cannot find" in s or "does not exist" in s:
            print("  ➜ สาเหตุ: ไม่มีพอร์ตนี้จริง (เลข COM ผิด หรือ USB หลุด/ไดรเวอร์ยังไม่ลง)")
            print("     วิธีแก้: รัน `py read_sensor.py` เปล่าๆ เพื่อดูรายชื่อพอร์ตที่มีจริง")
        else:
            print("  ➜ สาเหตุ: อื่นๆ — เช็คสาย USB/ไดรเวอร์ของ RS485-to-USB")
        return None

# ---------------------------------------------------------------------------
# STEP 2-4: ส่ง 1 คำสั่ง แล้ววิเคราะห์ผลตอบกลับอย่างละเอียด
# ---------------------------------------------------------------------------
def transact(ser, start, count, label):
    frame = build_read(ADDR, start, count)
    body, crc = frame[:-2], frame[-2:]
    print(f"\n[STEP 2] ส่งคำสั่งอ่าน {label}  (register 0x{start:04X}, {count} regs)")
    dbg("TX", f"{hexs(frame)}")
    dbg("=", f"addr=01 fn=03 start={hexs(body[2:4])} count={hexs(body[4:6])} CRC={hexs(crc)} (คำนวณอัตโนมัติ)")

    ser.reset_input_buffer()
    t0 = time.time()
    ser.write(frame); ser.flush()
    time.sleep(0.3)
    resp = ser.read(256)
    dt = (time.time() - t0) * 1000

    print(f"\n[STEP 3] รอผลตอบกลับ (ใช้เวลา {dt:.0f} ms)")
    dbg("RX", f"{hexs(resp)}  ({len(resp)} bytes)")

    # ---- กรณี A: ไม่มี byte เลย = ปัญหา hardware ----
    if len(resp) == 0:
        print("\n[STEP 4] ❌ ไม่ได้รับ byte ใดๆ เลย — ปัญหาอยู่ที่ 'สาย/ไฟ' ไม่ใช่โค้ด")
        print("  เรียงลำดับที่ต้องเช็ค (พร้อมเหตุผล):")
        print("   1) ไฟเลี้ยง: วัดที่ขั้ว แดง–ดำ ของ sensor ต้องได้ 12–24V")
        print("      ➜ เพราะ sensor กินไฟ ≥1A ที่ 12–24V; USB จ่ายแค่ 5V/0.5A ไม่พอ sensor จะไม่ทำงานเลย")
        print("   2) GND ร่วม: สายดำ(GND) ของ sensor ต้องต่อถึง GND ของ RS485 adapter ด้วย")
        print("      ➜ เพราะ RS485 วัดแรงดันต่าง A-B เทียบกับ GND ร่วม ถ้าไม่มี ตัวรับอ่านสัญญาณไม่ออก")
        print("   3) สลับ A↔B: ลองสลับ เขียว(485_A) กับ ขาว(485_B)")
        print("      ➜ เพราะถ้าต่อกลับขั้ว สัญญาณจะกลับด้าน ตัวรับ decode ไม่ได้ = เงียบสนิท")
        print("   4) รอ warm-up: sensor บางตัวหลังจ่ายไฟต้องรอ 2–10 วิ ก่อนตอบ")
        print("   5) แปรงทำความสะอาดหมุนตอนเปิดไฟ = ไฟเข้าแล้ว (สังเกตได้ว่าต่อไฟถูก)")
        return None

    # ---- กรณี B: byte ที่รับ = สิ่งที่เราส่ง (echo) = sensor ไม่ได้ตอบ ----
    if resp[:len(frame)] == frame:
        print("\n[STEP 4] ⚠️ ได้ byte กลับมา = 'สิ่งที่เราส่งออกไปเอง' (echo/loopback)")
        print("  ➜ สาเหตุ: A กับ B ช็อตถึงกัน หรือ adapter half-duplex สะท้อนกลับ — sensor ยังไม่ตอบจริง")
        print("     วิธีแก้: เช็คว่า A/B ไม่แตะกัน, ต่อ sensor ครบ (ไฟ+GND+A+B), ลองสลับ A↔B")
        return None

    # ---- หา frame ที่ CRC ถูกต้อง (เผื่อมี noise byte นำหน้า) ----
    valid, offset = validate_frame(resp)

    if valid is None:
        print("\n[STEP 4] ⚠️ ได้ byte มาบ้าง แต่ประกอบเป็น frame ที่ถูกต้องไม่ได้ (CRC ไม่ผ่านทุกตำแหน่ง)")
        print("  เป็นไปได้ 3 อย่าง:")
        print("   1) Baud rate ไม่ตรง (ควรเป็น 9600) — byte เลยเพี้ยนเป็นขยะ")
        print("      ➜ ลอง `--scan` เพื่อไล่ baud อื่น")
        print("   2) สัญญาณรบกวน/GND ไม่ดี — บาง byte ผิด CRC เลยไม่ผ่าน")
        print("      ➜ เช็ค GND ร่วม, สายไม่ยาว/ไม่พันกับสายไฟแรงสูง, ลองใส่ตัวต้านทาน 120Ω ปลายสาย")
        print("   3) A↔B สลับบางส่วน / สายหลวม")
        return None

    if offset > 0:
        print(f"\n[STEP 4a] ⓘ มี noise {offset} byte นำหน้า frame (line noise/echo) — ข้ามไปใช้ frame ที่ถูกต้อง")
    resp = valid

    # ---- กรณี C: sensor ตอบ EXCEPTION ----
    if resp[1] & 0x80:
        code = resp[2]
        print(f"\n[STEP 4] ⚠️ sensor ตอบ EXCEPTION (fn=0x{resp[1]:02X}, code=0x{code:02X})")
        print("  ✅ ข่าวดี: สาย+โปรโตคอลถูกแล้ว sensor 'คุยได้' — แค่คำขอไม่ถูกใจ")
        print(f"  ➜ ความหมาย: {EXC.get(code, 'ไม่ทราบ code นี้ ดูตาราง Modbus exception')}")
        return None

    # ---- กรณี D: ตอบปกติ แต่ตรวจโครงสร้างให้ครบ ----
    print("\n[STEP 4] ✅ ได้ frame ตอบกลับที่ CRC ถูกต้อง")
    if resp[0] != ADDR:
        print(f"  ⚠️ แต่ address ตอบกลับ = 0x{resp[0]:02X} ไม่ใช่ 0x{ADDR:02X} ที่เราถาม")
        print("     ➜ อาจมี sensor หลายตัวบนสาย หรือ address ถูกเปลี่ยน — ลอง `--scan`")
    if resp[1] != 0x03:
        print(f"  ⚠️ function code ตอบกลับ = 0x{resp[1]:02X} ไม่ใช่ 0x03"); return None

    nbytes = resp[2]
    data = resp[3:3 + nbytes]
    dbg("=", f"byte count = {nbytes}, data = {len(data)} bytes, CRC ผ่าน ✓")
    if len(data) < nbytes:
        print(f"  ⚠️ ข้อมูลมาไม่ครบ (คาด {nbytes} ได้ {len(data)}) — timeout สั้นไป/สายหลวม")
    return data

def validate_frame(resp):
    """หา frame ที่ CRC ถูกต้องใน buffer (เผื่อมี byte ขยะนำหน้า). คืน (frame, offset) หรือ (None, -1)"""
    for off in range(0, min(len(resp), 8)):
        b = resp[off:]
        if len(b) < 5:
            break
        # exception frame ยาว 5 byte
        if b[1] & 0x80 and len(b) >= 5:
            if modbus_crc(b[:3]) == b[3:5]:
                return b[:5], off
        # read response ปกติ: addr fn count data crc
        if len(b) >= 3 and b[1] == 0x03:
            n = b[2]
            total = 3 + n + 2
            if len(b) >= total and modbus_crc(b[:3 + n]) == b[3 + n:total]:
                return b[:total], off
    return None, -1

# ---------------------------------------------------------------------------
# ถอด float + ตรวจ byte order + ตรวจช่วงค่า
# ---------------------------------------------------------------------------
def decode_and_report(data):
    if not data or len(data) < 4:
        print("\n[STEP 5] ❌ ไม่มีข้อมูลพอให้ถอด"); return

    print("\n[STEP 5] ถอดค่า float (คู่มือใช้ DCBA = little-endian)")
    # โชว์ค่าแรก (DO) ในทุก byte order เพื่อวินิจฉัยถ้าค่าเพี้ยน
    b0, b1, b2, b3 = data[0], data[1], data[2], data[3]
    orders = {
        "DCBA (little, ที่คู่มือใช้) <-- ใช้ตัวนี้": struct.unpack('<f', bytes([b0,b1,b2,b3]))[0],
        "ABCD (big-endian)":                        struct.unpack('>f', bytes([b0,b1,b2,b3]))[0],
        "BADC (byte swap)":                         struct.unpack('>f', bytes([b1,b0,b3,b2]))[0],
        "CDAB (word swap)":                         struct.unpack('<f', bytes([b2,b3,b0,b1]))[0],
    }
    dbg("byte order test (ค่าแรก DO)", "")
    for name, v in orders.items():
        print(f"      {name:<40} = {v:.4f}")
    print("      ➜ ถ้า DCBA ให้ค่าที่สมเหตุสมผล (DO ~0-20) แปลว่า byte order ถูก; ถ้าตัวอื่นดูถูกกว่า แจ้งผม")

    vals = [struct.unpack('<f', data[i:i+4])[0] for i in range(0, len(data) - 3, 4)]

    print("\n[STEP 6] ✅ ผลการอ่าน (ตรวจช่วงค่าให้ด้วย)\n" + "-"*58)
    warned = False
    for (name, unit, (lo, hi)), v in zip(FIELDS, vals):
        flag = ""
        if not (lo <= v <= hi):
            flag = f"  ⚠️ นอกช่วงคาด ({lo}..{hi}) — probe อาจไม่ได้ต่อ/byte order เพี้ยน"
            warned = True
        print(f"  {name:<28} {v:12.3f} {unit:<12}{flag}")
    print("-"*58)
    if warned:
        print("  หมายเหตุ: ค่าที่นอกช่วงมาก มักเพราะ (1) probe นั้นไม่ได้เสียบ")
        print("           (2) byte order เพี้ยน (ดู byte order test ข้างบน)")
        print("           อ่าน register 0x0800 เพื่อดูว่า probe ตัวไหนต่ออยู่จริง")
    else:
        print("  ทุกค่าอยู่ในช่วงสมเหตุสมผล ✓ อ่านสำเร็จสมบูรณ์!")

# ---------------------------------------------------------------------------
# โหมด --scan : ไล่ baud/address ตอนเงียบ
# ---------------------------------------------------------------------------
def scan(port):
    print("\n[SCAN] ไล่หา baud/address (อ่าน DO 0x2601)...")
    found = False
    for baud in [9600, 4800, 19200, 38400]:
        try:
            ser = serial.Serial(port, baud, bytesize=8, parity=serial.PARITY_NONE, stopbits=1, timeout=0.4)
        except serial.SerialException as e:
            print(f"  เปิดพอร์ตไม่ได้: {e}"); return
        with ser:
            for addr in range(1, 11):
                body = bytes([addr, 0x03, 0x26, 0x01, 0x00, 0x02])
                ser.reset_input_buffer(); ser.write(body + modbus_crc(body)); ser.flush()
                time.sleep(0.12); r = ser.read(32)
                if r and r[0] == addr:
                    print(f"  ✅ เจอตอบที่ baud={baud} addr=0x{addr:02X}  resp: {hexs(r)}")
                    found = True
    if not found:
        print("  ไม่เจอที่ baud/address ใดเลย -> ยืนยันว่าปัญหาอยู่ที่สาย/ไฟ (ดู checklist STEP 4)")

# ---------------------------------------------------------------------------
# เลือกพอร์ตอัตโนมัติ: สแกนหา sensor ในทุกพอร์ตก่อน ถ้าเจอใช้เลย / ถ้าไม่เจอค่อยให้เลือก
# ---------------------------------------------------------------------------
def auto_detect(ports):
    print("[AUTO] สแกนหา sensor ในแต่ละพอร์ต (9600 8N1, addr 0x01)...")
    frame = build_read(ADDR, 0x2600, 22)
    for p in ports:
        try:
            ser = serial.Serial(p.device, BAUD, bytesize=8, parity=serial.PARITY_NONE,
                                stopbits=1, timeout=0.5)
        except serial.SerialException:
            print(f"   {p.device:<7} เปิดไม่ได้ (อาจถูกโปรแกรมอื่นใช้อยู่) — ข้าม"); continue
        with ser:
            ser.reset_input_buffer(); ser.write(frame); ser.flush()
            time.sleep(0.3); r = ser.read(64)
        valid, _ = validate_frame(r)
        if valid is not None and not (valid[1] & 0x80) and valid[0] == ADDR:
            print(f"   {p.device:<7} ✅ พบ sensor ตอบกลับ!"); return p.device
        elif r:
            print(f"   {p.device:<7} มี byte แต่ยังไม่ใช่ frame ถูก ({hexs(r)})")
        else:
            print(f"   {p.device:<7} เงียบ")
    return None

def select_port():
    ports = list(list_ports.comports())
    if not ports:
        print("[!] ไม่เจอพอร์ต COM เลย — เสียบ RS485-to-USB แล้วหรือยัง?"); return None

    hit = auto_detect(ports)
    if hit:
        return hit

    # ไม่เจอ sensor อัตโนมัติ
    if len(ports) == 1:
        print(f"\n[AUTO] ไม่พบการตอบจาก sensor แต่มีพอร์ตเดียว → ใช้ {ports[0].device} แล้ว debug ต่อ")
        return ports[0].device

    # มีหลายพอร์ต → ขึ้นเมนูให้เลือก
    print("\n[เลือกพอร์ต] ไม่พบ sensor อัตโนมัติ และมีหลายพอร์ต — เลือกที่ต่อ RS485-to-USB:")
    for i, p in enumerate(ports):
        print(f"   [{i+1}] {p.device:<7} - {p.description}")
    while True:
        try:
            sel = input("   พิมพ์หมายเลข (Enter=ตัวแรก, q=ออก): ").strip()
        except EOFError:
            return None
        if sel.lower() == "q":
            return None
        if sel == "":
            return ports[0].device
        if sel.isdigit() and 1 <= int(sel) <= len(ports):
            return ports[int(sel) - 1].device
        print("   เลือกไม่ถูก ลองใหม่")

# ---------------------------------------------------------------------------
def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    do_scan = "--scan" in sys.argv

    # มี arg = ระบุพอร์ตเอง / ไม่มี arg = สแกนเลือกอัตโนมัติ
    port = args[0] if args else select_port()
    if not port:
        return
    print(f"\n================ อ่าน SENSOR ที่ {port} (โหมด DEBUG) ================")
    ser = open_port(port, BAUD)
    if not ser:
        return

    data = None
    try:
        data = transact(ser, 0x2600, 22, "ทุกค่า (frame รวม)")
        if data:
            decode_and_report(data)
    finally:
        ser.close()   # ปิดพอร์ตก่อน เผื่อจะ scan ต่อ

    if not data and do_scan:
        scan(port)

if __name__ == "__main__":
    main()
