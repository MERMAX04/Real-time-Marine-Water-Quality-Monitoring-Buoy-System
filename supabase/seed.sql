-- =========================================================================
-- seed.sql  —  ข้อมูลตั้งต้น "เฉพาะ Local dev" (รันอัตโนมัติหลัง migrations ตอน supabase start ครั้งแรก
--              และทุกครั้งที่ `supabase db reset`)
--
-- ⚠️ ห้ามส่งไฟล์นี้ขึ้น cloud (production): อย่าใช้ `supabase db push --include-seed`
--    ในไฟล์มีข้อมูลจำลอง + trigger ที่ยิงไปหา Edge Function ของเครื่อง local
-- =========================================================================


-- ===== 1) ค่าตั้งต้นของระบบ (เหมือน cloud) =====
insert into public.app_settings (key, value)
values ('mode', 'recreation')            -- โหมดนันทนาการ (defaultMode ใน water-modes.json)
on conflict (key) do nothing;


-- ===== 2) ค่าจำลองย้อนหลัง 3 ชม. ทุก 15 วิ (~721 แถว) ให้กราฟบน Dashboard มีข้อมูลทันที =====
-- ค่าทะเลปกติ (สถานะเขียวในโหมดนันทนาการ): คลื่น sin ช้าๆ + noise เล็กน้อย
-- DO% ขยับตาม DO, การนำไฟฟ้าขยับตามความเค็ม (สมจริงกว่าสุ่มแยกกัน)
insert into public.readings
  (created_at, device, do_val, do_pct, temp, ph, sal, cond, turb, lat, lon, sensor_ok)
select
  ts,
  'buoy-01',
  round((6.2  + 0.25 * sin(i / 90.0)  + (random() - 0.5) * 0.10)::numeric, 2)::real,  -- DO (mg/L)
  round((90.0 + 3.5  * sin(i / 90.0)  + (random() - 0.5) * 1.0 )::numeric, 1)::real,  -- DO (%)
  round((30.0 + 0.30 * sin(i / 120.0) + (random() - 0.5) * 0.10)::numeric, 2)::real,  -- อุณหภูมิ (°C)
  round((8.0  + 0.05 * sin(i / 150.0) + (random() - 0.5) * 0.02)::numeric, 2)::real,  -- pH
  round((32.0 + 0.20 * sin(i / 200.0) + (random() - 0.5) * 0.05)::numeric, 2)::real,  -- ความเค็ม (ppt)
  round((50.0 + 0.30 * sin(i / 200.0) + (random() - 0.5) * 0.10)::numeric, 2)::real,  -- EC (mS/cm)
  round(greatest(0, 5.0 + 1.0 * sin(i / 60.0) + (random() - 0.5) * 0.6)::numeric, 2)::real, -- ความขุ่น (NTU)
  round((7.1918   + (random() - 0.5) * 0.00004)::numeric, 6)::double precision,       -- GPS แกว่ง ~2 ม.
  round((100.6102 + (random() - 0.5) * 0.00004)::numeric, 6)::double precision,
  true
from (
  select ts, row_number() over (order by ts) as i
  from generate_series(now() - interval '3 hours', now(), interval '15 seconds') as ts
) s;


-- ===== 3) Database Webhook แบบ local: INSERT readings → Edge Function telegram-alert =====
-- บน cloud ใช้ Studio → Database → Webhooks (ไม่อยู่ใน SQL) ; ที่นี่ทำเทียบเท่าด้วย pg_net
-- payload เหมือน Webhook ของ Supabase: { type, table, schema, record, old_record }
--   (telegram-alert อ่าน payload.record)
--
-- URL ที่ใช้: http://kong:8000/functions/v1/telegram-alert
--   - DB อยู่ใน container → 127.0.0.1 คือตัว container เอง ใช้ไม่ได้
--   - "kong" = API gateway ในเครือข่าย docker ของ Supabase CLI (Edge Functions local ก็ได้
--     SUPABASE_URL=http://kong:8000 แบบเดียวกัน) → ไม่ขึ้นกับพอร์ตฝั่ง host/firewall
--   - ถ้าใช้ไม่ได้ เปลี่ยนเป็น 'http://host.docker.internal:54321/functions/v1/telegram-alert'
--     (Docker Desktop บน Windows/Mac) แล้ว `supabase db reset`
-- header x-alert-secret ต้องตรงกับ ALERT_WEBHOOK_SECRET ใน supabase/.env.local
--   (ถ้าไม่ตั้ง ALERT_WEBHOOK_SECRET เลย ฟังก์ชันจะไม่ตรวจ header)
-- ต้องรัน `supabase functions serve --env-file supabase/.env.local` ด้วย ไม่งั้น request ตกหล่น
--   (ดูผลได้ที่: select * from net._http_response order by created desc limit 5;)
create extension if not exists pg_net;

create or replace function public.local_dev_telegram_alert_webhook()
returns trigger
language plpgsql
security definer                 -- anon (ESP32/simulate_buoy) insert ได้ แต่ไม่มีสิทธิ์ schema net
set search_path = ''
as $$
begin
  perform net.http_post(
    url     := 'http://kong:8000/functions/v1/telegram-alert',
    body    := jsonb_build_object(
                 'type',       'INSERT',
                 'table',      TG_TABLE_NAME,
                 'schema',     TG_TABLE_SCHEMA,
                 'record',     to_jsonb(NEW),
                 'old_record', null),
    headers := jsonb_build_object(
                 'Content-Type',   'application/json',
                 'x-alert-secret', 'local-alert-secret'),
    timeout_milliseconds := 5000
  );
  return NEW;
end;
$$;

revoke all on function public.local_dev_telegram_alert_webhook() from public, anon, authenticated;

drop trigger if exists local_dev_telegram_alert on public.readings;
create trigger local_dev_telegram_alert
  after insert on public.readings
  for each row execute function public.local_dev_telegram_alert_webhook();
