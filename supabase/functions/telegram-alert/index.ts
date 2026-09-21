// =========================================================================
// telegram-alert  —  แจ้งเตือนอัตโนมัติ (ทำงานทุกครั้งที่มีแถวใหม่ใน readings)
//   ทริกเกอร์: Supabase Database Webhook (INSERT บน public.readings)
//   ประเมินตาม "โหมดที่เลือกอยู่" -> ถ้าสถานะรวม "แดง" ก็เตือน
//   กันเตือนซ้ำด้วย alert_state (cooldown 30 นาที ต่อชุดค่าที่ตก)
//
// deploy:  supabase functions deploy telegram-alert --no-verify-jwt
// =========================================================================
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
import { DEVICE, tgSend, buildAlert, getMode } from "../_shared/telegram.ts";
import { evalWater } from "../_shared/water-eval.ts";

const sb = createClient(
  Deno.env.get("SUPABASE_URL")!,
  Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!,
);
const ALERT_SECRET = Deno.env.get("ALERT_WEBHOOK_SECRET") ?? "";
const COOLDOWN_MS = 30 * 60 * 1000;   // 30 นาที ต่อชุดค่าที่ตก

Deno.serve(async (req) => {
  if (ALERT_SECRET && req.headers.get("x-alert-secret") !== ALERT_SECRET) {
    return new Response("forbidden", { status: 401 });
  }

  let payload: Record<string, any>;
  try { payload = await req.json(); } catch { return new Response("bad request", { status: 400 }); }
  const row = payload.record ?? payload;
  if (!row || typeof row !== "object") return new Response("no record", { status: 400 });

  const mode = await getMode(sb);
  const ev = evalWater(row, mode);
  const alert = buildAlert(ev);
  if (!alert) return new Response("ok (ปกติ ไม่มีเตือน)");

  // cooldown: key ผูกกับโหมด + ชุดค่าที่ตก (ปัญหาใหม่ = เตือนใหม่ได้)
  const stateKey = `${DEVICE}:${alert.key}`;
  const { data } = await sb.from("alert_state").select("last_sent").eq("key", stateKey).maybeSingle();
  const last = data?.last_sent ? new Date(data.last_sent).getTime() : 0;
  if (Date.now() - last <= COOLDOWN_MS) return new Response("ok (ยังอยู่ใน cooldown)");
  await sb.from("alert_state").upsert({ key: stateKey, last_sent: new Date().toISOString() });

  const message = `🌊 แจ้งเตือนคุณภาพน้ำ (${DEVICE})\n\n${alert.text}`;
  const { data: subs } = await sb.from("tg_subscribers").select("chat_id");
  let sent = 0;
  for (const s of subs ?? []) { if (await tgSend(s.chat_id, message)) sent++; }
  return new Response(`ok (ส่งแล้ว ${sent}/${subs?.length ?? 0} คน)`);
});
