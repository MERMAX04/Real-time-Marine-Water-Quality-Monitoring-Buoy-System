// =========================================================================
// _shared/telegram.ts  —  โค้ดที่ใช้ร่วมกันระหว่าง telegram-bot กับ telegram-alert
//   ส่งข้อความ + จัดรูป /status (แยกสีต่อค่า) + /swim (สถานะรวมตามโหมด)
//   ตรรกะ/เกณฑ์ทั้งหมดมาจาก water-eval.ts + water-modes.json (แก้ที่นั่นที่เดียว)
// =========================================================================
import { evalWater, STATUS_COLORS, MODES, getMode, setMode, modeKeyValid, DEFAULT_MODE } from "./water-eval.ts";
import type { WaterEval } from "./water-eval.ts";
export { getMode, setMode, MODES, modeKeyValid, DEFAULT_MODE };

export const DEVICE = "buoy-01";

const TOKEN = Deno.env.get("TELEGRAM_BOT_TOKEN") ?? "";

// ส่งข้อความเข้า Telegram (plain text — ไม่ใช้ parse_mode จะได้ไม่ต้อง escape)
export async function tgSend(chatId: string | number, text: string): Promise<boolean> {
  const res = await fetch(`https://api.telegram.org/bot${TOKEN}/sendMessage`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ chat_id: chatId, text, disable_web_page_preview: true }),
  });
  if (!res.ok) console.error("tgSend failed", res.status, await res.text());
  return res.ok;
}

const n = (v: unknown): number => (v === null || v === undefined || v === "") ? NaN : Number(v);
const dot = (s: WaterEval["overall"]): string => s === "unknown" ? "⚪" : STATUS_COLORS[s].emoji;

function tsLine(r: Record<string, unknown>): string {
  if (!r.created_at) return "";
  return `\n🕒 ${new Date(String(r.created_at)).toLocaleString("th-TH", { timeZone: "Asia/Bangkok" })}`;
}

// สถานะความสด/ไลฟ์ของทุ่น (จากอายุของแถวล่าสุด) — ทุ่นส่งทุก ~15 วิ
export function freshnessLine(r: Record<string, unknown> | null): string {
  if (!r || !r.created_at) return "🔴 ไม่มีข้อมูลจากทุ่นเลย";
  const ageSec = (Date.now() - new Date(String(r.created_at)).getTime()) / 1000;
  const ago = ageSec < 60 ? `${Math.round(ageSec)} วิ`
            : ageSec < 3600 ? `${Math.round(ageSec / 60)} นาที`
            : `${(ageSec / 3600).toFixed(1)} ชม.`;
  if (ageSec < 45) {
    if (r.sensor_ok === false) return `🟠 ESP ออนไลน์ แต่ sensor อ่านไม่ได้ (${ago}ที่แล้ว)`;
    return `🟢 ทุ่นออนไลน์ (ข้อมูลสด ${ago}ที่แล้ว)`;
  }
  if (ageSec < 300) return `🟠 ไม่มีข้อมูลใหม่ ${ago} — ทุ่นอาจสะดุด`;
  return `🔴 ทุ่นออฟไลน์ ${ago} — ESP/4G อาจหลุด หรือไฟหมด`;
}

// ---------- /status : ค่าน้ำล่าสุด แยกสีต่อค่า (ตามโหมด) ----------
export function fmtStatus(r: Record<string, unknown> | null, modeKey: string): string {
  if (!r) return "⏳ ยังไม่มีข้อมูลจากทุ่น (ยังไม่เคยส่งค่าขึ้นมา)";
  const ev = evalWater(r, modeKey);
  const mEmoji = [...String(MODES[ev.mode].title)][0] ?? "🌊";
  let m = `📊 ค่าน้ำล่าสุด (${DEVICE})\nโหมด: ${mEmoji} ${MODES[ev.mode].name}`;
  m += `\n\nสถานะทุ่น : ${freshnessLine(r)}`;

  // ค่าล่าสุดเป็น heartbeat (sensor อ่านไม่ได้) — ไม่โชว์สถานะเขียวหลอกๆ
  if (r.sensor_ok === false) {
    m += `\n\n⚠️ ทุ่นส่ง heartbeat (sensor อ่านไม่ได้) — ยังไม่มีค่ารอบล่าสุด ควรตรวจสอบ probe`;
    return m + tsLine(r);
  }

  if (ev.overall === "unknown") {
    m += `\n\n⚪ ${ev.advice}`;
    return m + tsLine(r);
  }

  // สถานะรวม (บนสุด) แล้วค่ารายตัวเยื้องอยู่ใต้
  m += `\n\n${dot(ev.overall)} ${ev.advice}`;
  for (const p of ev.params) m += `\n     ${STATUS_COLORS[p.status].emoji} ${p.label}: ${p.text}`;

  // ค่าที่ไม่ได้อยู่ในเกณฑ์ตัดสิน (แสดงเฉยๆ)
  const tds = n(r.tds); if (!isNaN(tds)) m += `\n     💧 TDS: ${tds.toFixed(0)} mg/L`;
  const orp = n(r.orp); if (!isNaN(orp)) m += `\n     🔬 ORP: ${orp.toFixed(1)} mV`;
  if (n(r.lat) >= -90 && n(r.lat) <= 90 && !isNaN(n(r.lon)))
    m += `\n     📍 พิกัด: ${n(r.lat).toFixed(6)}, ${n(r.lon).toFixed(6)}`;

  // ป้ายหัวข้อโหมดปิดท้าย
  m += `\n\n${ev.title}`;
  return m + tsLine(r);
}

// ---------- /swim : สถานะรวม "ทำกิจกรรมได้ไหม" ตามโหมด ----------
export function fmtActivity(r: Record<string, unknown> | null, modeKey: string): string {
  if (!r) return "⏳ ยังไม่มีข้อมูลจากทุ่น";
  const ev = evalWater(r, modeKey);
  let m = `${ev.title} (${DEVICE})\nโหมด: ${MODES[ev.mode].name}`;
  m += `\n\nสถานะทุ่น : ${freshnessLine(r)}`;
  if (r.sensor_ok === false) {
    m += `\n\n⚠️ ทุ่นส่ง heartbeat (sensor อ่านไม่ได้) — ยังประเมินไม่ได้ ควรตรวจสอบ probe`;
    return m + tsLine(r);
  }
  m += `\n\n${dot(ev.overall)} ${ev.advice}`;
  if (ev.labNote) m += `\n\nหมายเหตุ: ${ev.labNote}`;
  return m + tsLine(r);
}

// ---------- แจ้งเตือนอัตโนมัติ : ยิงเมื่อสถานะรวม "แดง" (ตามโหมด) ----------
// คืนก้อนแจ้งเตือน + key สำหรับ cooldown (null = ไม่ต้องเตือน)
export function buildAlert(ev: WaterEval): { key: string; text: string } | null {
  if (ev.overall !== "red") return null;
  const offend = ev.params.filter((p) => p.status === "red").map((p) => p.key).sort().join(",");
  const text = `${ev.title}\n${STATUS_COLORS.red.emoji} ${ev.advice}`
    + (ev.labNote ? `\n\nหมายเหตุ: ${ev.labNote}` : "");
  return { key: `red:${ev.mode}:${offend}`, text };
}
