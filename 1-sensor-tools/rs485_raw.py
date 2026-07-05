# -*- coding: utf-8 -*-
"""
rs485_raw.py  —  ส่ง "คำสั่ง hex ดิบ" ตามที่คู่มือ sensor บอก แล้วดู byte ที่ตอบกลับ
-----------------------------------------------------------------
ใช้ตอนที่ "รู้ค่าที่ถูกต้องแล้ว" (เจอจาก rs485_scan.py หรืออ่านจากคู่มือ)
เหมาะกับการทดสอบ command เฉพาะที่คู่มือให้มา เช่น อ่านค่า pH, DO, salinity

วิธีใช้ (แก้ 3 ค่าด้านล่างให้ตรงกับ sensor ของคุณ):
  PORT, BAUD, และ CMD_HEX

  - ถ้าคู่มือให้ command มาพร้อม CRC แล้ว  -> ตั้ง APPEND_CRC = False
  - ถ้าคู่มือให้แค่ address+function+register (ไม่มี CRC) -> ตั้ง APPEND_CRC = True

รัน:  py rs485_raw.py
"""

import time
import sys

try:
    import serial
except ImportError:
    print("[!] ติดตั้งก่อน:  py -m pip install pyserial")
    sys.exit(1)

# ================== แก้ตรงนี้ ==================
PORT = "COM3"          # เปลี่ยนเป็นพอร์ตของคุณ
BAUD = 9600            # baud rate ที่ถูกต้อง
PARITY = "N"           # "N" / "E" / "O"

# คำสั่ง hex จากคู่มือ เช่น "01 04 00 00 00 01"
# (address=01, function=04 อ่าน input reg, เริ่มที่ reg 0, จำนวน 1 ตัว)
CMD_HEX = "01 04 00 00 00 01"

APPEND_CRC = True      # True = ให้โปรแกรมเติม CRC ให้, False = ใน CMD_HEX มี CRC ครบแล้ว
# ==============================================

_PARITY = {"N": serial.PARITY_NONE, "E": serial.PARITY_EVEN, "O": serial.PARITY_ODD}


def modbus_crc(data: bytes) -> bytes:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if (crc & 1) else (crc >> 1)
    return bytes([crc & 0xFF, (crc >> 8) & 0xFF])


def parse_hex(s: str) -> bytes:
    return bytes(int(x, 16) for x in s.replace(",", " ").split())


def decode_registers(resp: bytes):
    """ถ้าเป็น response Modbus อ่าน register ปกติ จะลองแปลงเป็นตัวเลขให้ดู"""
    if len(resp) >= 5 and resp[1] in (0x03, 0x04):
        nbytes = resp[2]
        data = resp[3:3 + nbytes]
        vals = []
        for i in range(0, len(data) - 1, 2):
            vals.append((data[i] << 8) | data[i + 1])   # 16-bit unsigned, big-endian
        print(f"    -> register (16-bit): {vals}")
        # ค่า sensor หลายตัวเก็บเป็น float 32-bit (2 register ต่อกัน) หรือหารสเกล เช่น /10, /100
        print(f"    (ถ้าค่าดูใหญ่ไป ลองหาร 10 หรือ 100 ตามคู่มือ; ถ้าเป็น float ต้องรวม 2 reg)")


def main():
    body = parse_hex(CMD_HEX)
    frame = body + modbus_crc(body) if APPEND_CRC else body

    print(f"พอร์ต {PORT} @ {BAUD} {PARITY}81")
    print(f"ส่งไป : {' '.join(f'{b:02X}' for b in frame)}")

    try:
        ser = serial.Serial(PORT, BAUD, parity=_PARITY[PARITY],
                            bytesize=8, stopbits=1, timeout=1.0)
    except serial.SerialException as e:
        print(f"[!] เปิดพอร์ตไม่ได้: {e}")
        sys.exit(1)

    with ser:
        ser.reset_input_buffer()
        ser.write(frame)
        ser.flush()
        time.sleep(0.2)
        resp = ser.read(128)

    if resp:
        print(f"ตอบกลับ: {' '.join(f'{b:02X}' for b in resp)}")
        if len(resp) >= 2 and (resp[1] & 0x80):
            print(f"    -> นี่คือ EXCEPTION code 0x{resp[2]:02X} (คุยได้ แต่ register/คำสั่งผิด)")
        else:
            decode_registers(resp)
    else:
        print("ตอบกลับ: (ไม่มี) — ลองใช้ rs485_scan.py หาค่าที่ถูกก่อน หรือเช็คสาย A/B, GND")


if __name__ == "__main__":
    main()
