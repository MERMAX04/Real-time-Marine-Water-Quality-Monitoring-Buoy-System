// =========================================================================
// _shared/telegram.ts  —  โค้ดที่ใช้ร่วมกันระหว่าง telegram-bot กับ telegram-alert
// (ส่งข้อความ, ประเมินเกณฑ์วิกฤต, จัดรูปข้อความ /status)
// =========================================================================

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

// ประเมินเกณฑ์วิกฤต (ตรงกับที่เคยอยู่บน ESP32) -> คืน list ของ alert ที่เข้าเกณฑ์
export function evalAlerts(r: Record<string, unknown>): { key: string; text: string }[] {
  const a: { key: string; text: string }[] = [];
  const dov = n(r.do_val), ph = n(r.ph), temp = n(r.temp), turb = n(r.turb);
  if (!isNaN(dov)  && dov  < 3.0)  a.push({ key: "do",      text: `🔴 ออกซิเจนละลายน้ำต่ำวิกฤต: ${dov.toFixed(2)} mg/L (สัตว์น้ำเสี่ยงตาย)` });
  if (!isNaN(ph)   && ph   < 7.0)  a.push({ key: "ph_low",  text: `🔴 น้ำเป็นกรดผิดปกติ: pH ${ph.toFixed(2)}` });
  if (!isNaN(ph)   && ph   > 9.0)  a.push({ key: "ph_high", text: `🔴 น้ำเป็นด่างผิดปกติ: pH ${ph.toFixed(2)}` });
  if (!isNaN(temp) && temp > 33.0) a.push({ key: "temp",    text: `🟠 อุณหภูมิน้ำสูง: ${temp.toFixed(1)} °C` });
  if (!isNaN(turb) && turb > 40.0) a.push({ key: "turb",    text: `🟠 ความขุ่นสูงผิดปกติ: ${turb.toFixed(1)} NTU` });
  return a;
}

// จัดรูปข้อความ /status = ค่าน้ำล่าสุดครบทุกตัว
export function fmtStatus(r: Record<string, unknown> | null): string {
  if (!r) return "⏳ ยังไม่มีข้อมูลจากทุ่น (ยังไม่เคยส่งค่าขึ้นมา)";
  const f = (v: unknown, d = 2) => isNaN(n(v)) ? "–" : n(v).toFixed(d);
  let m = `📊 ค่าน้ำล่าสุด (${DEVICE})`;
  m += `\n🫧 DO: ${f(r.do_val)} mg/L (${f(r.do_pct, 0)}%)`;
  m += `\n🌡 อุณหภูมิ: ${f(r.temp)} °C`;
  m += `\n⚗️ pH: ${f(r.ph)}`;
  m += `\n🧂 ความเค็ม: ${f(r.sal)} ppt`;
  m += `\n⚡ การนำไฟฟ้า: ${f(r.cond)} mS/cm`;
  m += `\n💧 TDS: ${f(r.tds)}`;
  m += `\n🌫 ความขุ่น: ${f(r.turb)} NTU`;
  if (r.chl != null) m += `\n🌿 คลอโรฟิลล์: ${f(r.chl)} µg/L`;
  if (r.orp != null) m += `\n🔬 ORP: ${f(r.orp, 1)} mV`;
  if (r.lat != null && r.lon != null) m += `\n📍 พิกัด: ${f(r.lat, 6)}, ${f(r.lon, 6)}`;
  if (r.created_at) {
    const t = new Date(String(r.created_at)).toLocaleString("th-TH", { timeZone: "Asia/Bangkok" });
    m += `\n🕒 ${t}`;
  }
  return m;
}
