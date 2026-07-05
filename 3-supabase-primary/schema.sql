-- =========================================================================
-- schema.sql — สร้างตารางเก็บค่าจากทุ่น + เปิดสิทธิ์ + เปิด realtime
-- วิธีใช้: Supabase Dashboard -> SQL Editor -> วางทั้งหมดนี้ -> Run (ครั้งเดียว)
-- =========================================================================

-- ตารางค่าที่วัดได้ (1 แถว = 1 ครั้งที่ทุ่นส่งข้อมูล) — เหมือน Excel
create table if not exists public.readings (
  id         bigint generated always as identity primary key,
  device     text        not null default 'buoy-01',     -- เผื่อมีหลายทุ่น
  created_at timestamptz not null default now(),          -- เวลาอัตโนมัติ (ESP32 ไม่ต้องส่ง)
  do_val     real,   -- ออกซิเจนละลายน้ำ (mg/L)   (ใช้ do_val เพราะ do เป็นคำสงวนของ SQL)
  sal        real,   -- ความเค็ม (ppt)
  turb       real,   -- ความขุ่น (NTU)
  chl        real,   -- คลอโรฟิลล์ (µg/L)
  orp        real,   -- ศักย์ออกซิเดชัน (mV)
  oil        real,   -- น้ำมัน (ppm)
  algae      real    -- สาหร่ายสีเขียว (cells/mL)
);

-- index: ดึง "ล่าสุด/ย้อนหลัง" ของแต่ละทุ่นให้เร็ว
create index if not exists idx_readings_device_time
  on public.readings (device, created_at desc);

-- ===== สิทธิ์ (Row Level Security) — จุดที่คนพลาดบ่อยสุด! =====
-- เปิด RLS แล้วอนุญาตให้ key แบบ anon: insert ได้ (ESP32) + read ได้ (Dashboard)
alter table public.readings enable row level security;

create policy "anon can insert readings"
  on public.readings for insert to anon with check (true);

create policy "anon can read readings"
  on public.readings for select to anon using (true);

-- ===== เปิด realtime ให้ตารางนี้ (Dashboard จะได้อัปเดตสดเมื่อมีแถวใหม่) =====
alter publication supabase_realtime add table public.readings;

-- ---------------------------------------------------------------------------
-- โบนัส: view ค่าเฉลี่ยรายวัน (ทำกราฟรายวันด้วย SQL ง่ายๆ — ข้อดีของ Supabase)
create or replace view public.readings_daily as
select device,
       date_trunc('day', created_at) as day,
       avg(do_val) do_avg, avg(sal) sal_avg, avg(turb) turb_avg,
       avg(chl) chl_avg, avg(orp) orp_avg, avg(oil) oil_avg, avg(algae) algae_avg,
       count(*) n
from public.readings
group by device, date_trunc('day', created_at)
order by day desc;
