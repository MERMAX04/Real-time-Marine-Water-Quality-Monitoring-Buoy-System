-- =========================================================================
-- schema-settings.sql  —  ตั้งค่ากลางของระบบ (รันครั้งเดียวใน Supabase Studio > SQL Editor)
--   เก็บ "โหมดมาตรฐานคุณภาพน้ำ" ที่เลือกอยู่ (1 ค่า ใช้ร่วมทั้งเว็บ + Telegram + แจ้งเตือน)
--   - เว็บ (anon key)  : อ่าน + เปลี่ยนโหมดผ่าน dropdown ได้
--   - Telegram /mode   : เปลี่ยนได้เฉพาะแอดมิน (เช็คจาก env TG_ADMINS ในโค้ด ไม่เกี่ยวกับ RLS)
--   - Edge Functions   : ใช้ service_role อ่านค่า (bypass RLS)
-- =========================================================================

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

-- เปลี่ยนโหมดจากเว็บได้ (อัปเดตค่าเดิม) — จำกัดเฉพาะ key 'mode'
drop policy if exists "app_settings update mode" on public.app_settings;
create policy "app_settings update mode" on public.app_settings
  for update using (key = 'mode') with check (key = 'mode');

-- สิทธิ์ระดับตาราง (เผื่อ default ไม่ได้ให้ไว้) — RLS ยังคุมเงื่อนไขอยู่
grant select, update on public.app_settings to anon, authenticated;

-- เปิด realtime: เปลี่ยนโหมดจาก Telegram/อีกจอ แล้วหน้าเว็บที่เปิดอยู่อัปเดตตามทันที
do $$
begin
  alter publication supabase_realtime add table public.app_settings;
exception when duplicate_object then null;
end $$;
