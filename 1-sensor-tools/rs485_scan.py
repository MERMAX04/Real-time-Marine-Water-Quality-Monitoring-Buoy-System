# -*- coding: utf-8 -*-
"""
rs485_scan.py  —  เครื่องมือหา sensor Modbus RTU บนสาย RS485
-----------------------------------------------------------------
ใช้ตอนที่ "ส่งไปแล้ว sensor ไม่ตอบ" และยังไม่รู้ค่า baud/address ที่ถูกต้อง
โปรแกรมจะไล่ลอง baud rate x address x parity ทุกคู่ แล้วบอกว่าคู่ไหนได้คำตอบ

วิธีใช้:
  1) เสียบ RS485-to-USB เข้าคอม แล้วต่อสาย A->A, B->B, GND->GND เข้ากับ sensor
  2) เปิด Device Manager ดูว่าเป็น COM อะไร (เช่น COM3)
  3) รัน:  py rs485_scan.py COM3
     ถ้าไม่ใส่ COM มันจะลิสต์พอร์ตที่มีให้เลือก

ต้องติดตั้ง pyserial ก่อน 1 ครั้ง:  py -m pip install pyserial
"""

import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("[!] ยังไม่ได้ติดตั้ง pyserial")
    print("    รันคำสั่งนี้ก่อน:  py -m pip install pyserial")
    sys.exit(1)


# ---------- ค่าที่จะไล่สแกน (แก้ได้ตามคู่มือถ้ารู้ค่าแล้ว) ----------
BAUDS   = [9600, 4800, 19200, 38400, 115200, 2400]
ADDRS   = list(range(1, 11)) + [0]      # address 1..10 (ที่พบบ่อย) + 0
PARITY  = [serial.PARITY_NONE, serial.PARITY_EVEN, serial.PARITY_ODD]
PARITY_NAME = {serial.PARITY_NONE: "N", serial.PARITY_EVEN: "E", serial.PARITY_ODD: "O"}

# ฟังก์ชัน Modbus ที่จะลองอ่าน: 0x03=holding reg, 0x04=input reg
# sensor ส่วนใหญ่เก็บค่าวัดไว้ใน input register (0x04)
FUNCS   = [0x04, 0x03]


def modbus_crc(data: bytes) -> bytes:
    """คำนวณ CRC16 แบบ Modbus (little-endian) — ต้องต่อท้ายทุก frame"""
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return bytes([crc & 0xFF, (crc >> 8) & 0xFF])


def build_read(addr: int, func: int, start_reg: int = 0, count: int = 1) -> bytes:
    """สร้าง frame คำสั่งอ่าน register"""
    body = bytes([addr, func,
                  (start_reg >> 8) & 0xFF, start_reg & 0xFF,
                  (count >> 8) & 0xFF, count & 0xFF])
    return body + modbus_crc(body)


def hexs(b: bytes) -> str:
    return " ".join(f"{x:02X}" for x in b)


def try_one(port, baud, parity, addr, func):
    """เปิดพอร์ตด้วยค่าที่กำหนด ส่งคำสั่งอ่าน แล้วคืนค่า response (bytes) หรือ b''"""
    try:
        ser = serial.Serial(port=port, baudrate=baud, parity=parity,
                            bytesize=8, stopbits=1, timeout=0.4)
    except serial.SerialException as e:
        print(f"[!] เปิดพอร์ต {port} ไม่ได้: {e}")
        sys.exit(1)

    with ser:
        ser.reset_input_buffer()
        frame = build_read(addr, func, start_reg=0, count=1)
        ser.write(frame)
        ser.flush()
        time.sleep(0.15)
        resp = ser.read(64)
    return frame, resp


def choose_port():
    ports = list(list_ports.comports())
    if not ports:
        print("[!] ไม่เจอพอร์ต COM เลย — เช็คว่าเสียบ USB แล้วหรือยัง")
        sys.exit(1)
    print("พอร์ตที่เจอ:")
    for p in ports:
        print(f"   {p.device}  ({p.description})")
    print("\nรันใหม่โดยระบุพอร์ต เช่น:  py rs485_scan.py " + ports[0].device)
    sys.exit(0)


def main():
    if len(sys.argv) < 2:
        choose_port()
    port = sys.argv[1]

    print(f"=== เริ่มสแกนที่ {port} ===")
    print("(ถ้าเจอ frame ที่ตอบกลับ = สายและโปรโตคอลถูกแล้ว!)\n")

    hits = []
    for parity in PARITY:
        for baud in BAUDS:
            for func in FUNCS:
                for addr in ADDRS:
                    frame, resp = try_one(port, baud, parity, addr, func)
                    tag = f"baud={baud:<6} {PARITY_NAME[parity]}81 addr={addr:<2} fn=0x{func:02X}"
                    if resp:
                        # ได้ byte อะไรกลับมาบ้าง = มีสัญญาณ!
                        ok_addr = resp[0] == addr
                        is_exc  = len(resp) >= 2 and (resp[1] & 0x80)
                        note = ""
                        if is_exc:
                            note = " <- sensor ตอบ EXCEPTION (แปลว่าคุยได้! แค่ register ผิด)"
                        elif ok_addr:
                            note = " <- ตอบถูก address! นี่แหละค่าที่ใช่"
                        print(f"[GOT] {tag}  resp: {hexs(resp)}{note}")
                        hits.append((tag, resp))
    print("\n=== สรุป ===")
    if hits:
        print(f"เจอการตอบกลับ {len(hits)} ครั้ง — ใช้ค่า baud/address/parity ตามบรรทัด [GOT] ด้านบน")
        print("จด baud + address + parity ที่ใช่ไว้ แล้วเอาไปตั้งใน ESP32 ได้เลย")
    else:
        print("ไม่มีการตอบกลับเลยแม้แต่ครั้งเดียว — ปัญหาน่าจะอยู่ที่ 'สาย' ไม่ใช่ 'ค่า':")
        print("  1) ลองสลับสาย A <-> B  (สลับสองเส้นนี้คือปัญหา RS485 ที่พบบ่อยที่สุด)")
        print("  2) เช็คว่า GND ของ RS485 ต่อถึง GND ของ sensor จริง")
        print("  3) เช็คไฟเลี้ยง sensor ว่าครบ (วัดแรงดันที่ขั้ว sensor ด้วยมัลติมิเตอร์)")
        print("  4) sensor บางตัวต้องรอ warm-up 2-10 วิ หลังจ่ายไฟ ค่อยตอบ")


if __name__ == "__main__":
    main()
