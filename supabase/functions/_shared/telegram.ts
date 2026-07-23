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

// =====================================================================
// ธงสถานะการลงเล่นน้ำ (Blue Flag / bathing water) — ดู 5-extensions/05-blueflag-swim-safety.md
// เกณฑ์: คพ. ไทย (นันทนาการ) pH 7.0-8.5, DO >= 6 mg/L  +  Blue Flag ข้อ 3 (ห้ามน้ำทิ้งลงหาด)
// ⚠️ ตัดสินขาดไม่ได้ — E. coli / Enterococci ต้องตรวจในห้องแลป
// =====================================================================
export const SWIM_NOTE = "หมายเหตุ: ผลชี้ขาดต้องตรวจ E. coli / Enterococci ในห้องปฏิบัติการ (ทุ่นวัดแบคทีเรียไม่ได้)";

export type SwimResult = { flag: "green" | "yellow" | "red" | "unknown"; label: string; reasons: string[] };

// baseline ความเค็มจากแถวย้อนหลัง (ไม่รวมแถวล่าสุด) — ใช้มัธยฐาน กันค่าแกว่ง
export function baselineSal(rows: Record<string, unknown>[]): number | null {
  const vals = rows.map((r) => n(r.sal)).filter((v) => !isNaN(v) && v > 1).sort((a, b) => a - b);
  return vals.length < 3 ? null : vals[Math.floor(vals.length / 2)];
}

export function evalSwim(r: Record<string, unknown>, baseSal: number | null): SwimResult {
  const dov = n(r.do_val), ph = n(r.ph), temp = n(r.temp), turb = n(r.turb), sal = n(r.sal);
  const red: string[] = [], yellow: string[] = [];

  // ความเค็มต่ำมาก = ทุ่นน่าจะไม่ได้อยู่ในน้ำทะเล (เช่น ทดสอบในอากาศ) -> ไม่ตัดสินธง
  if (!isNaN(sal) && sal < 1) {
    return { flag: "unknown", label: "⚪ ยังประเมินไม่ได้", reasons: ["ความเค็มต่ำมาก — ทุ่นอาจไม่ได้อยู่ในน้ำทะเล"] };
  }

  if (!isNaN(dov)) {
    if (dov < 4)      red.push(`ออกซิเจนละลายน้ำต่ำมาก ${dov.toFixed(2)} mg/L (มาตรฐานนันทนาการ ≥ 6)`);
    else if (dov < 6) yellow.push(`ออกซิเจนละลายน้ำ ${dov.toFixed(2)} mg/L ต่ำกว่ามาตรฐาน (≥ 6)`);
  }
  // DO% — เซนเซอร์บางตัวส่งเป็นสัดส่วน (0.90 = 90%) จึง normalize ก่อน
  let dp = n(r.do_pct); if (!isNaN(dp) && dp <= 2) dp = dp * 100;
  if (!isNaN(dp)) {
    if (dp < 60 || dp > 130)      red.push(`ออกซิเจนอิ่มตัว ${dp.toFixed(0)}% ผิดปกติมาก (ปกติ 80–120%)`);
    else if (dp < 80 || dp > 120) yellow.push(`ออกซิเจนอิ่มตัว ${dp.toFixed(0)}% นอกช่วงปกติ (80–120%)`);
  }
  const cond = n(r.cond);   // การนำไฟฟ้าต่ำ = น้ำจืดเจือ (สอดคล้องกับความเค็ม)
  if (!isNaN(cond)) {
    if (cond < 35)      red.push(`การนำไฟฟ้าต่ำ ${cond.toFixed(1)} mS/cm (น้ำทะเลปกติ 45–55)`);
    else if (cond < 45) yellow.push(`การนำไฟฟ้า ${cond.toFixed(1)} mS/cm ต่ำกว่าปกติ (45–55)`);
  }
  if (!isNaN(ph)) {
    if (ph < 6.5 || ph > 9.0)      red.push(`pH ${ph.toFixed(2)} นอกช่วงปลอดภัย (6.5–9.0)`);
    else if (ph < 7.0 || ph > 8.5) yellow.push(`pH ${ph.toFixed(2)} นอกมาตรฐานนันทนาการ (7.0–8.5)`);
  }
  if (!isNaN(turb)) {
    if (turb > 40)      red.push(`น้ำขุ่นมาก ${turb.toFixed(1)} NTU — มองไม่เห็นใต้น้ำ`);
    else if (turb > 15) yellow.push(`น้ำขุ่น ${turb.toFixed(1)} NTU — ทัศนวิสัยใต้น้ำแย่`);
  }
  if (!isNaN(temp) && temp > 33) yellow.push(`อุณหภูมิน้ำสูง ${temp.toFixed(1)} °C — แบคทีเรียโตเร็ว`);

  // ความเค็มตกฮวบ = สัญญาณน้ำจืด/น้ำทิ้งไหลลง (Blue Flag ข้อ 3)
  if (!isNaN(sal) && baseSal && baseSal > 1) {
    const drop = ((baseSal - sal) / baseSal) * 100;
    if (drop > 30)      red.push(`ความเค็มลดฮวบ ${drop.toFixed(0)}% — อาจมีน้ำจืด/น้ำทิ้งไหลลง`);
    else if (drop > 15) yellow.push(`ความเค็มลดลง ${drop.toFixed(0)}% — เฝ้าระวังน้ำจืดเจือ`);
  }

  if (red.length)    return { flag: "red",    label: "🔴 ธงแดง — ไม่ควรลงเล่นน้ำ",           reasons: red.concat(yellow) };
  if (yellow.length) return { flag: "yellow", label: "🟡 ธงเหลือง — ลงเล่นได้ แต่ต้องระวัง", reasons: yellow };
  return { flag: "green", label: "🟢 ธงเขียว — น้ำอยู่ในเกณฑ์ ลงเล่นได้", reasons: [] };
}

// ข้อความตอบคำสั่ง /swim
export function fmtSwim(r: Record<string, unknown> | null, baseSal: number | null): string {
  if (!r) return "⏳ ยังไม่มีข้อมูลจากทุ่น";
  const s = evalSwim(r, baseSal);
  let m = `🏖️ สถานะการลงเล่นน้ำ (${DEVICE})\n${s.label}`;
  if (s.reasons.length) m += `\n\nเหตุผล:\n• ${s.reasons.join("\n• ")}`;
  m += `\n\n${SWIM_NOTE}`;
  if (r.created_at) {
    m += `\n🕒 ${new Date(String(r.created_at)).toLocaleString("th-TH", { timeZone: "Asia/Bangkok" })}`;
  }
  return m;
}
