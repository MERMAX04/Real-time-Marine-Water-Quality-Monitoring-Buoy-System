-- =========================================================================
-- schema-telegram.sql  —  ตารางฝั่ง Telegram (รันครั้งเดียวใน Supabase Studio > SQL Editor)
-- ระบบแจ้งเตือน/บอท Telegram ย้ายมาทำ "ฝั่งบก" ด้วย Edge Functions:
--   tg_subscribers = รายชื่อ chat_id คนที่กด /start (แทน NVS บน ESP32)
--   alert_state    = เวลาแจ้งเตือนล่าสุดต่อชนิด (กันเตือนซ้ำ = cooldown แบบไม่ต้องมี state ในโค้ด)
-- =========================================================================

create table if not exists public.tg_subscribers (
  chat_id    text primary key,                 -- Telegram chat id (เก็บเป็น text รองรับ id ยาว/กลุ่มติดลบ)
  created_at timestamptz not null default now()
);

create table if not exists public.alert_state (
  key        text primary key,                 -- เช่น 'buoy-01:do', 'buoy-01:ph_low', 'buoy-01:turb'
  last_sent  timestamptz
);

-- เปิด RLS แต่ "ไม่สร้าง policy" -> anon/public เข้าไม่ได้เลย
-- Edge Functions ใช้ service_role key ซึ่ง bypass RLS อยู่แล้ว = ปลอดภัย (คนอื่นดึง chat_id ไม่ได้)
alter table public.tg_subscribers enable row level security;
alter table public.alert_state    enable row level security;
