# -*- coding: utf-8 -*-
"""
probe_check.py  —  แผน B: ตรวจว่าโพรบเสริม ORP / CHL / OIW / BGA "อ่านได้จริง" ไหม
------------------------------------------------------------------------------
ต่อ sensor ผ่าน USB-to-RS485 (เหมือน read_sensor.py) แล้วรัน:
    py probe_check.py              # ใช้ COM9, อ่าน 5 รอบ
    py probe_check.py COM9 10      # ระบุพอร์ต + จำนวนรอบ

สิ่งที่ตรวจ:
  1) Sensor status (0x0800)  — bit flags ว่าโพรบไหนต่ออยู่ (แสดงเป็นเลขฐาน 2 ให้เทียบคู่มือ)
  2) เวอร์ชัน (0x0700)
  3) เฟรมรวม 0x2600 — ดูช่อง ORP(f5) / CHL(f6) / OIW-BGA(f7)
  4) อ่านแยกทีละ register: 0x260B ORP, 0x260C CHL, 0x260D OIW, 0x260E BGA
  แล้วสรุปผลรายโพรบ: อ่านได้ / เป็น 0 ตลอด (ไม่มีโพรบ) / EXCEPTION (ไม่รองรับ) / ไม่ตอบ
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
PORT   = sys.argv[1] if len(sys.argv) > 1 else "COM9"
ROUNDS = int(sys.argv[2]) if len(sys.argv) > 2 else 5

PROBES = [  # key, register, ชื่อ, หน่วย, ช่องในเฟรมรวม
    ("orp",   0x260B, "ORP (ศักย์รีดอกซ์)",    "mV",       5),
    ("chl",   0x260C, "CHL (คลอโรฟิลล์)",      "µg/L",     6),
    ("oil",   0x260D, "OIW (น้ำมัน)",           "ppm",      7),
    ("algae", 0x260E, "BGA (สาหร่าย)",          "cells/mL", 7),
]

def crc(data):
    c = 0xFFFF
    for b in data:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ 0xA001 if (c & 1) else (c >> 1)
    return bytes([c & 0xFF, (c >> 8) & 0xFF])

def hexs(b): return " ".join(f"{x:02X}" for x in b) if b else "(ว่าง)"

def query(ser, reg, cnt):
    """คืน (สถานะ, payload) — สถานะ: ok / exception / noreply / badframe / badcrc"""
    body = bytes([ADDR, 0x03, reg >> 8, reg & 0xFF, 0x00, cnt])
    ser.reset_input_buffer(); ser.write(body + crc(body)); ser.flush()
    time.sleep(0.3)
    r = ser.read(256)
    if not r:                      return "noreply", r
    if len(r) >= 3 and r[1] & 0x80: return "exception", r
    if len(r) < 5 or r[1] != 0x03: return "badframe", r
    n = r[2]
    if len(r) < 3 + n + 2:         return "badframe", r
    if crc(r[:3 + n]) != r[3 + n:5 + n]: return "badcrc", r
    return "ok", r[3:3 + n]

def f32(p, i=0): return struct.unpack('<f', p[i:i + 4])[0]

def main():
    print(f"=== แผน B: ตรวจโพรบเสริม ===  พอร์ต {PORT} @ {BAUD} 8N1 · {ROUNDS} รอบ\n")
    try:
        ser = serial.Serial(PORT, BAUD, bytesize=8, parity=serial.PARITY_NONE, stopbits=1, timeout=1.2)
    except serial.SerialException as e:
        print(f"[!] เปิดพอร์ต {PORT} ไม่ได้: {e}"); return
    with ser:
        # 1) status — คู่มือ: 0x0800, 2 regs = 4 ไบต์
        #    byte0 = Error flag (bit0 แปรง, bit1 ไฟ, bit2 โพรบ : 0 ปกติ / 1 ผิดปกติ)
        #    byte1 = Power supply flag (bit0-5 ไฟแต่ละสาย : 0 ผิดปกติ / 1 ปกติ)
        #    byte2-3 = Probe flag (1 = เสียบอยู่+สื่อสารปกติ, 0 = ไม่ได้เสียบ/สื่อสารผิดปกติ)
        print("[1] Sensor status 0x0800  (TX 01 03 08 00 00 02 C6 6B)")
        probe_flag = {}
        st, p = query(ser, 0x0800, 2)
        if st == "ok" and len(p) >= 4:
            print(f"    RX ข้อมูล: {hexs(p)}")
            e = p[0]
            print(f"    Error flag   0x{e:02X}: แปรง={'ผิดปกติ' if e & 1 else 'ปกติ'} · "
                  f"ไฟ={'ผิดปกติ' if e & 2 else 'ปกติ'} · โพรบ={'ผิดปกติ/เสียบไม่สุด' if e & 4 else 'ปกติ'}")
            pw = p[1]
            print(f"    Power flag   0x{pw:02X}: " + " ".join(f"สาย{b+1}={'✓' if pw >> b & 1 else '✗'}" for b in range(6)))
            BITS = {0: "DO", 1: "Conductivity", 2: "Turbidity", 3: "CHL", 4: "pH", 5: "OIW/BGA", 8: "ORP", 9: "Salinity"}
            # ลำดับไบต์ของ word นี้คู่มือไม่ระบุ -> เลือกแบบที่โพรบพื้นฐาน (DO/EC/ขุ่น/pH/เค็ม ที่ใช้งานได้จริง) เป็น 1
            base = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 4) | (1 << 9)
            w_le, w_be = p[2] | (p[3] << 8), (p[2] << 8) | p[3]
            w, order = (w_le, "low-byte-first") if bin(w_le & base).count("1") >= bin(w_be & base).count("1") else (w_be, "high-byte-first")
            print(f"    Probe flag   0x{w:04X} = {w:016b}b  (ตีความแบบ {order})")
            for b, name in BITS.items():
                ok = bool(w >> b & 1); probe_flag[name] = ok
                print(f"      bit{b:<2} {name:<13}: {'✅ เสียบอยู่ สื่อสารปกติ' if ok else '⛔ ไม่ได้เสียบ / สื่อสารผิดปกติ'}")
        else:
            print(f"    {st}  {hexs(p)}")

        # 2) version — 4 ไบต์: 00-01 hardware, 02-03 software (0101 = 1.1)
        st, p = query(ser, 0x0700, 2)
        if st == "ok" and len(p) >= 4:
            print(f"\n[2] เวอร์ชัน: hardware {p[0]}.{p[1]} · software {p[2]}.{p[3]}")
        else:
            print(f"\n[2] เวอร์ชัน 0x0700: {st}  {hexs(p)}")

        # 2b) ค่าคาลิเบรต K/B (0x1100 + offset, 4 regs = K float + B float, DCBA)
        #     ถ้า K = 0 เซนเซอร์จะส่ง 0 เสมอแม้มีโพรบ -> ต้องเช็ค
        print("\n[2b] ค่าคาลิเบรต K/B (ถ้า K = 0 จะได้ค่า 0 ตลอด แม้มีโพรบ)")
        for off, name in ((0x05, "ORP"), (0x06, "CHL"), (0x07, "OIW/BGA")):
            st, p = query(ser, 0x1100 + off, 4)
            if st == "ok" and len(p) >= 8:
                k, b = f32(p, 0), f32(p, 4)
                warn = "  ⚠️ K = 0 → ค่าที่อ่านจะเป็น 0 เสมอ!" if k == 0 else ""
                print(f"    {name:<8} 0x{0x1100+off:04X}: K = {k:.4f} · B = {b:.4f}{warn}")
            else:
                print(f"    {name:<8} 0x{0x1100+off:04X}: {st}  {hexs(p)}")

        # 3+4) sampling
        print(f"\n[3] อ่านซ้ำ {ROUNDS} รอบ (ห่าง 2 วิ) — เฟรมรวม + register แยก")
        log = {k: [] for k, *_ in PROBES}
        bulk = {5: [], 6: [], 7: []}
        stat = {k: set() for k, *_ in PROBES}
        for rnd in range(1, ROUNDS + 1):
            st, p = query(ser, 0x2600, 22)
            line = f"  รอบ {rnd}: เฟรมรวม {st}"
            if st == "ok" and len(p) >= 44:
                for s in bulk: bulk[s].append(f32(p, s * 4))
                line += f" (f5={f32(p,20):.3f} f6={f32(p,24):.3f} f7={f32(p,28):.3f})"
            for k, reg, name, unit, _ in PROBES:
                st, p = query(ser, reg, 2)
                stat[k].add(st)
                if st == "ok" and len(p) >= 4:
                    v = f32(p); log[k].append((v, p[:4]))
                    line += f" | {k}={v:.3f}"
                else:
                    line += f" | {k}={st}"
            print(line)
            time.sleep(2)

    # 5) verdict
    print("\n================ สรุปผลรายโพรบ ================")
    works = []
    for k, reg, name, unit, slot in PROBES:
        vals = log[k]
        if not vals:
            s = stat[k]
            if "exception" in s: v = "❌ EXCEPTION — sensor ไม่รองรับ register นี้ (ไม่มีโพรบ/รุ่นนี้ไม่มี)"
            elif "noreply"  in s: v = "⚠️ ไม่ตอบ — เช็คสาย/ไฟ แล้วลองใหม่"
            else:                 v = f"⚠️ อ่านไม่สำเร็จ ({', '.join(sorted(s))})"
        else:
            nums = [x for x, _ in vals]
            allzero = all(raw == b"\x00\x00\x00\x00" for _, raw in vals)
            if allzero:
                v = "⛔ ได้ 0.000 เป๊ะทุกรอบ — น่าจะ 'ไม่มีโพรบติดตั้ง' หรือโพรบไม่ทำงาน"
            else:
                spread = max(nums) - min(nums)
                v = (f"✅ อ่านได้จริง  ช่วง {min(nums):.3f}–{max(nums):.3f} {unit}"
                     + ("  (ค่าขยับ = โพรบตอบสนอง)" if spread > 0 else "  (ค่านิ่ง — ลองกระตุ้นโพรบตามเอกสารแผน B)"))
                works.append(k)
        flag_name = {"orp": "ORP", "chl": "CHL", "oil": "OIW/BGA", "algae": "OIW/BGA"}[k]
        if flag_name in probe_flag:
            v += f"\n  {'':<22}        status 0x0800 บอกว่า: " + ("เสียบอยู่ ✅" if probe_flag[flag_name] else "ไม่ได้เสียบ ⛔")
        print(f"  {name:<22} 0x{reg:04X} : {v}")
    print()
    if not works:
        print("➡️  ไม่มีโพรบเสริมตัวไหนอ่านได้ → ใช้ 'แผน A' (ตัด 4 ค่าออกจากระบบ)")
    else:
        print(f"➡️  โพรบที่อ่านได้: {', '.join(works)} → เก็บค่าพวกนี้ไว้ / ตัดเฉพาะตัวที่อ่านไม่ได้")
    print("    (บันทึกผลหน้าจอนี้ไว้ใส่เล่ม: ภาคผนวก/ผลการตรวจสอบเซนเซอร์)")

if __name__ == "__main__":
    main()
