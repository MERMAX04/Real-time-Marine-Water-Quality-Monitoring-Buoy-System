// =========================================================================
// telegram-bot  —  รับ "คำสั่งแชท" จาก Telegram (webhook) แล้วตอบทันที
//   /start  = สมัคร (เก็บ chat_id ลงตาราง tg_subscribers)
//   /status = อ่านค่าล่าสุด แล้วตอบ (แยกสีต่อค่า + สถานะรวม ตามโหมดที่เลือก)
//   /swim   = สถานะรวม "ทำกิจกรรมได้ไหม" ตามโหมด
//   /mode   = ดู/เปลี่ยนโหมดมาตรฐาน (เปลี่ยนได้เฉพาะแอดมิน — ตั้ง chat_id ใน env TG_ADMINS)
//   /stop   = ยกเลิกรับแจ้งเตือน   /help = เมนู
//
// deploy:  supabase functions deploy telegram-bot --no-verify-jwt
// env ที่ต้องตั้ง:  TELEGRAM_BOT_TOKEN, TELEGRAM_WEBHOOK_SECRET, TG_ADMINS (chat_id คั่นด้วย ,)
// =========================================================================
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
import { DEVICE, tgSend, fmtStatus, fmtActivity, getMode, setMode, MODES } from "../_shared/telegram.ts";

const sb = createClient(
  Deno.env.get("SUPABASE_URL")!,
  Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!,
);
const WEBHOOK_SECRET = Deno.env.get("TELEGRAM_WEBHOOK_SECRET") ?? "";
const ADMINS = (Deno.env.get("TG_ADMINS") ?? "").split(",").map((s) => s.trim()).filter(Boolean);
const isAdmin = (id: string | number) => ADMINS.includes(String(id));

const latest = async () => (await sb.from("readings").select("*")
  .eq("device", DEVICE).order("created_at", { ascending: false }).limit(1)).data?.[0] ?? null;

// id (1–6) หรือชื่อคีย์ -> คีย์โหมด
function resolveMode(arg: string): string | null {
  const a = arg.trim().toLowerCase();
  for (const [key, cfg] of Object.entries(MODES)) {
    if (a === key || a === String((cfg as any).id)) return key;
  }
  return null;
}

function modeList(current: string): string {
  let s = "🗂️ โหมดมาตรฐานคุณภาพน้ำ (6 ประเภท):";
  for (const [key, cfg] of Object.entries(MODES)) {
    const mark = key === current ? " ✅ (ใช้อยู่)" : "";
    s += `\n${(cfg as any).id}. ${(cfg as any).name}${mark}`;
  }
  s += `\n\nเปลี่ยนโหมด: /mode <เลข 1–6>  เช่น /mode 4`;
  return s;
}

Deno.serve(async (req) => {
  if (WEBHOOK_SECRET && req.headers.get("x-telegram-bot-api-secret-token") !== WEBHOOK_SECRET) {
    return new Response("forbidden", { status: 401 });
  }

  let update: Record<string, any>;
  try { update = await req.json(); } catch { return new Response("ok"); }

  const m = update.message ?? update.edited_message;
  const chatId = m?.chat?.id;
  const text: string = (m?.text ?? "").trim();
  if (!chatId) return new Response("ok");

  const parts = text.split(/[\s@]+/);
  const cmd = (parts[0] ?? "").toLowerCase().replace(/@.*$/, "");
  const arg = parts.slice(1).join(" ");

  try {
    if (cmd === "/start") {
      await sb.from("tg_subscribers").upsert({ chat_id: String(chatId) });
      await tgSend(chatId, `✅ สมัครรับแจ้งเตือนคุณภาพน้ำจากทุ่น ${DEVICE} เรียบร้อย!\n/swim – ทำกิจกรรมได้ไหม · /status – ค่าล่าสุด · /mode – โหมด · /stop – ยกเลิก`);

    } else if (cmd === "/status") {
      await tgSend(chatId, fmtStatus(await latest(), await getMode(sb)));

    } else if (cmd === "/swim" || cmd === "/activity") {
      await tgSend(chatId, fmtActivity(await latest(), await getMode(sb)));

    } else if (cmd === "/mode") {
      const current = await getMode(sb);
      if (!arg) {                                   // ไม่มีอาร์กิวเมนต์ = แสดงรายการ
        await tgSend(chatId, modeList(current));
      } else if (!isAdmin(chatId)) {
        await tgSend(chatId, "🔒 เปลี่ยนโหมดได้เฉพาะแอดมินเท่านั้น (โหมดกลางกระทบทุกคน)\nดูโหมดปัจจุบันได้ที่ /mode");
      } else {
        const target = resolveMode(arg);
        if (!target) {
          await tgSend(chatId, `❓ ไม่รู้จักโหมด "${arg}"\n` + modeList(current));
        } else if (await setMode(sb, target)) {
          await tgSend(chatId, `✅ เปลี่ยนเป็นโหมด: ${(MODES[target] as any).name}\nมีผลทันทีทั้งเว็บ + แจ้งเตือน\n\n` + fmtActivity(await latest(), target));
        } else {
          await tgSend(chatId, "⚠️ บันทึกโหมดไม่สำเร็จ ลองใหม่อีกครั้ง");
        }
      }

    } else if (cmd === "/stop" || cmd === "/unsubscribe") {
      await sb.from("tg_subscribers").delete().eq("chat_id", String(chatId));
      await tgSend(chatId, "🛑 ยกเลิกรับแจ้งเตือนแล้ว (พิมพ์ /start เพื่อสมัครใหม่ได้ทุกเมื่อ)");

    } else if (cmd === "/help" || cmd === "/menu") {
      await tgSend(chatId, `🤖 คำสั่งทุ่น ${DEVICE}:\n/swim – ทำกิจกรรมได้ไหม (สถานะรวมตามโหมด)\n/status – ค่าน้ำล่าสุดทุกค่า (แยกสี)\n/mode – ดู/เปลี่ยนโหมดมาตรฐาน (6 ประเภท)\n/start – สมัครรับแจ้งเตือน\n/stop – ยกเลิก\n/help – เมนูนี้\n\n(ระบบเด้งเตือนเองเมื่อคุณภาพน้ำตกเกณฑ์ "แดง" ของโหมดที่เลือก)`);
    }
  } catch (e) {
    console.error("handler error", e);
  }
  return new Response("ok");
});
