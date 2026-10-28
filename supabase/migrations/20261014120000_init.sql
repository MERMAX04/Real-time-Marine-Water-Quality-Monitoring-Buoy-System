-- =========================================================================
-- 20261014120000_init.sql  —  schema ตั้งต้นของ "Local dev" (supabase start / db reset)
--
-- ⚠️ ไฟล์นี้ "สำเนา" ของ 3 ไฟล์ที่รันบน cloud (production) ตามลำดับนี้:
--      1) 3-supabase-primary/schema.sql      (readings + index + RLS + realtime + view readings_daily)
--      2) supabase/schema-settings.sql       (app_settings + โหมดเริ่มต้น + ถอนสิทธิ์ anon update)
--      3) supabase/schema-telegram.sql       (tg_subscribers, alert_state — RLS ไม่มี policy)
--    ถ้าแก้ไฟล์ต้นฉบับ ต้องแก้ที่นี่ให้ตรงกันด้วย (หรือเพิ่ม migration ใหม่) — ไม่งั้น local ≠ cloud
--
-- ต่างจากต้นฉบับ (ตั้งใจ):
--   - ใส่ GRANT ชัดเจน: Supabase รุ่นใหม่ (local CLI + โปรเจกต์ใหม่) "ไม่ให้สิทธิ์ตารางใหม่ใน public
--     กับ anon/authenticated/service_role อัตโนมัติ" แล้ว (config.toml: api.auto_expose_new_tables)
--     cloud เดิมได้สิทธิ์อัตโนมัติจึงไม่ต้องมี GRANT — ที่นี่ต้องใส่ ไม่งั้น ESP32/เว็บ/Edge Function
--     โดน "permission denied" ทั้งที่ RLS ถูก  (ผลลัพธ์ที่ผู้ใช้เห็นเหมือน cloud ทุกอย่าง)
--   - เขียนแบบรันซ้ำได้ (drop policy if exists / exception duplicate_object)
--   - ไม่มี Database Webhook → telegram-alert (cloud สร้างผ่าน Studio) — local ใส่ไว้ใน seed.sql แทน
-- =========================================================================


-- ===================== 1) schema.sql — ตาราง readings =====================
create table if not exists public.readings (
  id         bigint generated always as identity primary key,
  device     text        not null default 'buoy-01',     -- เผื่อมีหลายทุ่น
  created_at timestamptz not null default now(),          -- เวลาอัตโนมัติ (ESP32 ไม่ต้องส่ง)
  do_val     real,   -- ออกซิเจนละลายน้ำ (mg/L)
  do_pct     real,   -- ออกซิเจน % อิ่มตัว (%)
  temp       real,   -- อุณหภูมิ (°C)
  ph         real,   -- ความเป็นกรด-ด่าง (pH)
  sal        real,   -- ความเค็ม (ppt)
  cond       real,   -- การนำไฟฟ้า (mS/cm)
  turb       real,   -- ความขุ่น (NTU)
  sensor_ok  boolean not null default true,   -- heartbeat: false = ESP ยังไลฟ์ แต่ sensor อ่านไม่ได้
  lat        double precision,   -- พิกัด GPS ละติจูด
  lon        double precision    -- พิกัด GPS ลองจิจูด
);

create index if not exists idx_readings_device_time
  on public.readings (device, created_at desc);

-- RLS: anon insert ได้ (ESP32) + read ได้ (Dashboard)
alter table public.readings enable row level security;

drop policy if exists "anon can insert readings" on public.readings;
create policy "anon can insert readings"
  on public.readings for insert to anon with check (true);

drop policy if exists "anon can read readings" on public.readings;
create policy "anon can read readings"
  on public.readings for select to anon using (true);

-- realtime (Dashboard อัปเดตสดเมื่อมีแถวใหม่)
do $$
begin
  alter publication supabase_realtime add table public.readings;
exception when duplicate_object then null;
end $$;

-- view ค่าเฉลี่ยรายวัน
create or replace view public.readings_daily as
select device,
       date_trunc('day', created_at) as day,
       avg(do_val) do_avg, avg(do_pct) do_pct_avg, avg(temp) temp_avg, avg(ph) ph_avg,
       avg(sal) sal_avg, avg(cond) cond_avg, avg(turb) turb_avg,
       count(*) n
from public.readings
group by device, date_trunc('day', created_at)
order by day desc;


-- ===================== 2) schema-settings.sql — ตาราง app_settings =====================
create table if not exists public.app_settings (
  key        text primary key,
  value      text not null,
  updated_at timestamptz not null default now()
);

-- ค่าเริ่มต้น = โหมดนันทนาการ (recreation) — ถ้ามีอยู่แล้วไม่ทับ
insert into public.app_settings (key, value)
values ('mode', 'recreation')
on conflict (key) do nothing;

alter table public.app_settings enable row level security;

-- อ่านได้ทุกคน (เว็บต้องรู้โหมดปัจจุบัน)
drop policy if exists "app_settings read" on public.app_settings;
create policy "app_settings read" on public.app_settings
  for select using (true);

-- เปลี่ยนโหมด: เฉพาะผ่าน Edge Function 'admin' / telegram-bot (service_role bypass RLS) เท่านั้น
drop policy if exists "app_settings update mode" on public.app_settings;
grant select on public.app_settings to anon, authenticated;
revoke update on public.app_settings from anon, authenticated;

do $$
begin
  alter publication supabase_realtime add table public.app_settings;
exception when duplicate_object then null;
end $$;


-- ===================== 3) schema-telegram.sql — ตารางฝั่ง Telegram =====================
create table if not exists public.tg_subscribers (
  chat_id    text primary key,                 -- Telegram chat id (text รองรับ id ยาว/กลุ่มติดลบ)
  created_at timestamptz not null default now()
);

create table if not exists public.alert_state (
  key        text primary key,                 -- เช่น 'buoy-01:red:recreation:do_val'
  last_sent  timestamptz
);

-- RLS เปิด แต่ไม่มี policy → anon/authenticated เข้าไม่ได้ ; Edge Functions ใช้ service_role
alter table public.tg_subscribers enable row level security;
alter table public.alert_state    enable row level security;


-- ===================== 4) GRANT (เฉพาะ local — ดูหมายเหตุต้นไฟล์) =====================
-- ให้สิทธิ์ระดับตาราง "เท่าที่ RLS อนุญาตจริงบน cloud" (least privilege):
--   anon/authenticated : readings (select, insert) · readings_daily (select) · app_settings (select ให้ไว้ข้างบนแล้ว)
--   service_role       : ทุกตาราง (Edge Functions อ่าน/เขียน — bypass RLS แต่ยังต้องมีสิทธิ์ตาราง)
-- tg_subscribers / alert_state ไม่ให้ anon เลย (บน cloud ก็เข้าไม่ได้เพราะ RLS ไม่มี policy)
grant usage on schema public to anon, authenticated, service_role;

grant select, insert on public.readings       to anon, authenticated;
grant select         on public.readings_daily to anon, authenticated;

grant select, insert, update, delete on
  public.readings, public.app_settings, public.tg_subscribers, public.alert_state
  to service_role;
grant select on public.readings_daily to service_role;

-- sequence ของ identity (ปกติ identity ไม่เช็คสิทธิ์ sequence แต่ใส่ไว้กันพลาด)
grant usage, select on sequence public.readings_id_seq to anon, authenticated, service_role;
