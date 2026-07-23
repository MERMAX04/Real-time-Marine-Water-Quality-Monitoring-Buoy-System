// =========================================================================
// telegram-alert  —  แจ้งเตือนวิกฤตอัตโนมัติ (ทำงานทุกครั้งที่มีแถวใหม่ใน readings)
//   ทริกเกอร์: Supabase Database Webhook (INSERT บน public.readings) ยิงเข้ามาที่นี่
//   เช็คเกณฑ์ -> กันเตือนซ้ำด้วย alert_state (cooldown 30 นาที/ชนิด) -> ส่งหาทุก subscriber
//
// deploy:  supabase functions deploy telegram-alert --no-verify-jwt
//   ป้องกันด้วย header 'x-alert-secret' (ตั้งใน Database Webhook ให้ตรงกับ ALERT_WEBHOOK_SECRET)
// =========================================================================
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
import { DEVICE, tgSend, evalAlerts, evalSwim, baselineSal, SWIM_NOTE } from "../_shared/telegram.ts";

const sb = createClient(
  Deno.env.get("SUPABASE_URL")!,
  Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!,
);
const ALERT_SECRET = Deno.env.get("ALERT_WEBHOOK_SECRET") ?? "";
const COOLDOWN_MS = 30 * 60 * 1000;   // 30 นาที/ชนิด (เท่ากับ ALERT_COOLDOWN เดิมบน ESP32)

Deno.serve(async (req) => {
  if (ALERT_SECRET && req.headers.get("x-alert-secret") !== ALERT_SECRET) {
    return new Response("forbidden", { status: 401 });
  }

  let payload: Record<string, any>;
  try { payload = await req.json(); } catch { return new Response("bad request", { status: 400 }); }
  const row = payload.record ?? payload;          // Database Webhook -> { type, record, ... }
  if (!row || typeof row !== "object") return new Response("no record", { status: 400 });

  const alerts = evalAlerts(row);

  // ธงแดง (ไม่ควรลงเล่นน้ำ) ก็เตือนด้วย — baseline ความเค็มจากแถวย้อนหลัง เพื่อจับน้ำจืด/น้ำทิ้งไหลลง
  const { data: recent } = await sb.from("readings").select("sal")
    .eq("device", DEVICE).order("created_at", { ascending: false }).limit(20);
  const swim = evalSwim(row, baselineSal((recent ?? []).slice(1)));
  if (swim.flag === "red") {
    alerts.push({ key: "swim_red", text: `🏖️ ${swim.label}\n   • ${swim.reasons.join("\n   • ")}` });
  }

  if (alerts.length === 0) return new Response("ok (ปกติ ไม่มีเตือน)");

  // cooldown ต่อชนิด: อ่าน/อัปเดต alert_state
  const now = Date.now();
  const fire: { key: string; text: string }[] = [];
  for (const a of alerts) {
    const key = `${DEVICE}:${a.key}`;
    const { data } = await sb.from("alert_state").select("last_sent").eq("key", key).maybeSingle();
    const last = data?.last_sent ? new Date(data.last_sent).getTime() : 0;
    if (now - last > COOLDOWN_MS) {
      fire.push(a);
      await sb.from("alert_state").upsert({ key, last_sent: new Date(now).toISOString() });
    }
  }
  if (fire.length === 0) return new Response("ok (ยังอยู่ใน cooldown)");

  let message = `🌊 แจ้งเตือนคุณภาพน้ำ (${DEVICE})\n` + fire.map((a) => a.text).join("\n");
  if (fire.some((a) => a.key === "swim_red")) message += `\n\n${SWIM_NOTE}`;
  const { data: subs } = await sb.from("tg_subscribers").select("chat_id");
  let sent = 0;
  for (const s of subs ?? []) { if (await tgSend(s.chat_id, message)) sent++; }
  return new Response(`ok (ส่งแล้ว ${sent}/${subs?.length ?? 0} คน)`);
});
