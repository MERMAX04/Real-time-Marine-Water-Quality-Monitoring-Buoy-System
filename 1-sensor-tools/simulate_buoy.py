# -*- coding: utf-8 -*-
"""
simulate_buoy.py  —  "ทุ่นจำลอง" ส่งค่าน้ำทะเลปลอมขึ้น Supabase ทุก 15 วิ (เหมือน ESP32 จริง)
------------------------------------------------------------------------------
ใช้ทดสอบทั้งวงจรโดยไม่ต้องมีทุ่น/เซนเซอร์/4G:
    simulate_buoy.py -> Supabase (/rest/v1/readings) -> Dashboard (realtime) + telegram-alert

JSON ที่ส่ง = ชื่อฟิลด์เดียวกับ firmware (3-supabase-primary/Full-Version/Full-Version.ino):
    device, sensor_ok, do_val, do_pct, temp, ph, sal, cond, turb, lat, lon
ค่าเดินแบบ random walk เล็กๆ รอบค่าน้ำทะเลปกติ (DO 6.2, DO% 90, 30°C, pH 8.0, 32 ppt, 50 mS/cm, 5 NTU)

ค่าเริ่มต้น = Supabase "Local" (supabase start) ที่ http://127.0.0.1:54321
    + anon key สาธารณะของ Supabase CLI (demo key เดียวกันทุกเครื่อง ไม่ใช่ความลับ)

วิธีใช้ (stdlib ล้วน ไม่ต้อง pip install):
    py simulate_buoy.py                          # ส่ง local ทุก 15 วิ (Ctrl+C เพื่อหยุด)
    py simulate_buoy.py --once                   # ส่งครั้งเดียวแล้วจบ
    py simulate_buoy.py --interval 5             # ส่งทุก 5 วิ
    py simulate_buoy.py --red                    # ส่งค่า DO ต่ำ (2.0) ทดสอบแจ้งเตือนสถานะแดง
    py simulate_buoy.py --url http://127.0.0.1:54321 --key <anon key จาก supabase status>
    (หรือตั้ง env: SUPABASE_URL / SUPABASE_ANON_KEY)

⚠️ ชี้ไป cloud (production) ได้ แต่ข้อมูลปลอมจะไปปนกับข้อมูลจริงบน Dashboard — ใช้กับ local เป็นหลัก
"""
import os, sys, json, time, random, argparse, urllib.request, urllib.error

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

# ===================== ค่าเริ่มต้น (Local) =====================
LOCAL_URL = "http://127.0.0.1:54321"
# anon key "demo" ของ Supabase CLI (local ทุกเครื่องเหมือนกัน — ถ้าไม่ตรง ดูค่าจริงจาก `supabase status -o env` → ANON_KEY)
LOCAL_DEMO_ANON = ("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
                   "eyJpc3MiOiJzdXBhYmFzZS1kZW1vIiwicm9sZSI6ImFub24iLCJleHAiOjE5ODM4MTI5OTZ9."
                   "CRXP1A7WOeoJeXxjNni43kdQwgnWNReilDMblYTn_I0")
DEVICE = "buoy-01"

# ค่ากลาง (เป้าที่ค่าจะถูกดึงกลับ) / ขนาดก้าวต่อรอบ / ขอบเขต / ทศนิยม — ตรงกับที่ firmware ปัดเศษ
PARAMS = {
    #          กลาง    ก้าว    ต่ำสุด  สูงสุด  ทศนิยม
    "do_val": (6.2,   0.05,   0.0,   20.0,  2),   # ออกซิเจนละลายน้ำ (mg/L)
    "do_pct": (90.0,  0.6,    0.0,   200.0, 1),   # ออกซิเจนอิ่มตัว (%)
    "temp":   (30.0,  0.03,   20.0,  40.0,  2),   # อุณหภูมิ (°C)
    "ph":     (8.0,   0.01,   6.0,   9.5,   2),   # pH
    "sal":    (32.0,  0.03,   0.0,   45.0,  2),   # ความเค็ม (ppt)
    "cond":   (50.0,  0.05,   0.0,   70.0,  2),   # การนำไฟฟ้า (mS/cm)
    "turb":   (5.0,   0.3,    0.0,   100.0, 2),   # ความขุ่น (NTU)
}
HOME_LAT, HOME_LON = 7.1918, 100.6102          # ตำแหน่งทุ่น (GPS แกว่งเล็กน้อย ~2 ม.)
# ===============================================================


def step(state):
    """random walk 1 ก้าว: ขยับสุ่ม + ดึงกลับหาค่ากลาง 5% (ไม่ลอยหนีไปเรื่อยๆ)"""
    for k, (mid, sd, lo, hi, _) in PARAMS.items():
        v = state[k] + random.gauss(0, sd) + (mid - state[k]) * 0.05
        state[k] = min(hi, max(lo, v))
    return state


def build_payload(state, red=False):
    """สร้าง JSON แบบเดียวกับ ESP32 (sensor_ok ส่งเสมอ = heartbeat)"""
    p = {"device": DEVICE, "sensor_ok": True}
    for k, (_, _, _, _, nd) in PARAMS.items():
        p[k] = round(state[k], nd)
    if red:                                     # ค่าวิกฤต: DO ต่ำ → สถานะรวมแดง (โหมดนันทนาการ)
        p["do_val"], p["do_pct"] = 2.0, 28.0
    p["lat"] = round(HOME_LAT + random.uniform(-2e-5, 2e-5), 6)
    p["lon"] = round(HOME_LON + random.uniform(-2e-5, 2e-5), 6)
    return p


def post_reading(url, key, payload):
    """POST ไป /rest/v1/readings (header เดียวกับ firmware) คืน HTTP status"""
    req = urllib.request.Request(url.rstrip("/") + "/rest/v1/readings",
                                 data=json.dumps(payload).encode(), method="POST")
    req.add_header("apikey", key)
    req.add_header("Authorization", "Bearer " + key)
    req.add_header("Content-Type", "application/json")
    req.add_header("Prefer", "return=minimal")
    with urllib.request.urlopen(req, timeout=10) as r:
        return r.status


def main():
    ap = argparse.ArgumentParser(description="ทุ่นจำลอง: ส่งค่าน้ำทะเลปลอมขึ้น Supabase (ค่าเริ่มต้น = local)")
    ap.add_argument("--url", default=os.environ.get("SUPABASE_URL", LOCAL_URL),
                    help=f"Supabase URL (env SUPABASE_URL, ค่าเริ่มต้น {LOCAL_URL})")
    ap.add_argument("--key", default=os.environ.get("SUPABASE_ANON_KEY", ""),
                    help="anon/publishable key (env SUPABASE_ANON_KEY; local ไม่ใส่ = ใช้ demo key ของ CLI)")
    ap.add_argument("--interval", type=float, default=15.0, help="ส่งทุกกี่วินาที (ค่าเริ่มต้น 15 เหมือนทุ่นจริง)")
    ap.add_argument("--once", action="store_true", help="ส่งครั้งเดียวแล้วจบ")
    ap.add_argument("--red", action="store_true", help="ส่งค่า DO ต่ำ (ทดสอบ telegram-alert สถานะแดง)")
    args = ap.parse_args()

    is_local = "127.0.0.1" in args.url or "localhost" in args.url
    key = args.key or (LOCAL_DEMO_ANON if is_local else "")
    if not key:
        print("[!] ชี้ไป cloud ต้องใส่ anon key: --key <key> หรือ env SUPABASE_ANON_KEY"); sys.exit(1)

    where = "LOCAL" if is_local else "⚠️ CLOUD (ข้อมูลปลอมจะขึ้น Dashboard จริง!)"
    print(f"=== ทุ่นจำลอง {DEVICE} -> {args.url} [{where}]  "
          f"{'ครั้งเดียว' if args.once else f'ทุก {args.interval:.0f} วิ (Ctrl+C เพื่อหยุด)'} ===")

    state = {k: v[0] for k, v in PARAMS.items()}
    while True:
        t0 = time.time()
        payload = build_payload(step(state), red=args.red)
        try:
            code = post_reading(args.url, key, payload)
            print(f"  ✅ HTTP {code} : DO {payload['do_val']} ({payload['do_pct']}%) | Temp {payload['temp']}°C | "
                  f"pH {payload['ph']} | Sal {payload['sal']} | Cond {payload['cond']} | Turb {payload['turb']}")
        except urllib.error.HTTPError as e:
            print(f"  ❌ HTTP {e.code}: {e.read().decode(errors='replace')[:200]}")
            print("     ➜ 401=key ผิด, 403/42501=RLS/สิทธิ์ตาราง (รัน supabase db reset แล้วยัง?), 400/PGRST204=คอลัมน์ผิด")
        except Exception as e:
            print(f"  ❌ ส่งไม่ได้: {e}")
            print("     ➜ local: รัน `supabase start` แล้วหรือยัง? (เช็คด้วย `supabase status`)")

        if args.once:
            break
        time.sleep(max(0, args.interval - (time.time() - t0)))


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nหยุดแล้ว (Ctrl+C)")
