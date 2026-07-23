// =========================================================================
// telegram-bot  —  รับ "คำสั่งแชท" จาก Telegram (webhook) แล้วตอบทันที
//   /start  = สมัคร (เก็บ chat_id ลงตาราง tg_subscribers)
//   /status = อ่านค่าล่าสุดจากตาราง readings แล้วตอบ (เร็ว! ไม่ต้องรบกวนทุ่น)
//   /stop   = ยกเลิกรับแจ้งเตือน   /help = เมนู
//
// deploy:  supabase functions deploy telegram-bot --no-verify-jwt
//   (--no-verify-jwt เพราะ Telegram ยิงเข้ามาโดยไม่มี JWT — ป้องกันด้วย secret_token แทน)
// =========================================================================
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
import { DEVICE, tgSend, fmtStatus, fmtSwim, baselineSal } from "../_shared/telegram.ts";

// SUPABASE_URL / SUPABASE_SERVICE_ROLE_KEY = Supabase ใส่ให้อัตโนมัติใน Edge Functions
const sb = createClient(
  Deno.env.get("SUPABASE_URL")!,
  Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!,
);
const WEBHOOK_SECRET = Deno.env.get("TELEGRAM_WEBHOOK_SECRET") ?? "";

Deno.serve(async (req) => {
  // ป้องกันคนอื่นยิง: ตอน setWebhook เราตั้ง secret_token ไว้ Telegram จะแนบ header นี้มาทุกครั้ง
  if (WEBHOOK_SECRET && req.headers.get("x-telegram-bot-api-secret-token") !== WEBHOOK_SECRET) {
    return new Response("forbidden", { status: 401 });
  }

  let update: Record<string, any>;
  try { update = await req.json(); } catch { return new Response("ok"); }

  const m = update.message ?? update.edited_message;
  const chatId = m?.chat?.id;
  const text: string = (m?.text ?? "").trim();
  if (!chatId) return new Response("ok");

  const cmd = text.split(/[\s@]/)[0].toLowerCase();   // "/status@MyBot arg" -> "/status"

  try {
    if (cmd === "/start") {
      await sb.from("tg_subscribers").upsert({ chat_id: String(chatId) });
      await tgSend(chatId, `✅ สมัครรับแจ้งเตือนคุณภาพน้ำจากทุ่น ${DEVICE} เรียบร้อย!\n/swim – ลงเล่นน้ำได้ไหม · /status – ค่าล่าสุด · /stop – ยกเลิก`);
    } else if (cmd === "/status") {
      const { data } = await sb.from("readings").select("*")
        .eq("device", DEVICE).order("created_at", { ascending: false }).limit(1);
      await tgSend(chatId, fmtStatus(data?.[0] ?? null));
    } else if (cmd === "/swim") {
      // ดึงย้อนหลังไว้ทำ baseline ความเค็ม (จับ "ความเค็มตกฮวบ" = น้ำจืด/น้ำทิ้งไหลลง)
      const { data } = await sb.from("readings").select("*")
        .eq("device", DEVICE).order("created_at", { ascending: false }).limit(20);
      await tgSend(chatId, fmtSwim(data?.[0] ?? null, baselineSal((data ?? []).slice(1))));
    } else if (cmd === "/stop" || cmd === "/unsubscribe") {
      await sb.from("tg_subscribers").delete().eq("chat_id", String(chatId));
      await tgSend(chatId, "🛑 ยกเลิกรับแจ้งเตือนแล้ว (พิมพ์ /start เพื่อสมัครใหม่ได้ทุกเมื่อ)");
    } else if (cmd === "/help" || cmd === "/menu") {
      await tgSend(chatId, `🤖 คำสั่งทุ่น ${DEVICE}:\n/swim – ลงเล่นน้ำได้ไหม (ธงเขียว/เหลือง/แดง)\n/status – ดูค่าน้ำล่าสุดทุกค่า\n/start – สมัครรับแจ้งเตือน\n/stop – ยกเลิก\n/help – เมนูนี้\n\n(ระบบจะเด้งเตือนเองเมื่อค่าน้ำวิกฤต หรือขึ้นธงแดง)`);
    }
  } catch (e) {
    console.error("handler error", e);
  }
  return new Response("ok");   // ตอบ 200 เสมอ ไม่งั้น Telegram จะ retry รัวๆ
});
