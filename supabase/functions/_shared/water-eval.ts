// =========================================================================
// _shared/water-eval.ts  —  ตรรกะประเมินคุณภาพน้ำ "ตามโหมด" (single source of truth)
//   อ่านเกณฑ์จาก water-modes.json (6 ประเภทมาตรฐานน้ำทะเลไทย)
//   ใช้ร่วมกันโดย telegram-bot / telegram-alert  (Dashboard + PHP มิเรอร์ตรรกะเดียวกันนี้)
// =========================================================================
import modesConfig from "./water-modes.json" with { type: "json" };

export type Status = "green" | "orange" | "red";
export type Tier = "critical" | "advisory";
type Band = [number | null, number | null];

export const MODES = modesConfig.modes as Record<string, any>;
export const PARAMS_META = modesConfig.params as Record<string, { label: string; unit: string; order: number }>;
export const STATUS_COLORS = modesConfig.statusColors as Record<Status, { emoji: string; label: string }>;
export const DEFAULT_MODE = modesConfig.defaultMode as string;

// ทศนิยมที่ใช้แสดงต่อค่า
const DEC: Record<string, number> = { ph: 2, do_val: 2, do_pct: 0, temp: 1, sal: 1, cond: 1, turb: 1 };

const num = (v: unknown): number => (v === null || v === undefined || v === "") ? NaN : Number(v);

export function modeKeyValid(k: string | null | undefined): string {
  return (k && Object.prototype.hasOwnProperty.call(MODES, k)) ? k : DEFAULT_MODE;
}

function inBand(v: number, band: Band): boolean {
  const [lo, hi] = band;
  if (lo !== null && v < lo) return false;
  if (hi !== null && v > hi) return false;
  return true;
}

function bandStatus(v: number, green: Band, orange: Band): Status {
  if (inBand(v, green)) return "green";
  if (inBand(v, orange)) return "orange";
  return "red";
}

export type ParamEval = {
  key: string; label: string; unit: string;
  value: number; text: string;      // "3.50 mg/L"
  status: Status; tier: Tier;
};

export type WaterEval = {
  mode: string; modeName: string; title: string;
  overall: Status | "unknown";
  advice: string;                   // ข้อความตามโหมด (เติม {reasons} แล้ว)
  labNote: string;
  params: ParamEval[];              // เรียงตาม order
  reasons: string[];                // ค่าที่ทำให้ตกเกณฑ์ (สำหรับ overall != green)
};

// ประเมิน 1 แถวข้อมูล ตามโหมดที่เลือก
export function evalWater(r: Record<string, unknown>, modeKey: string): WaterEval {
  const mode = modeKeyValid(modeKey);
  const cfg = MODES[mode];
  const crit = cfg.criteria as Record<string, { green: Band; orange: Band; tier: Tier }>;

  // ทุ่นไม่ได้อยู่ในน้ำทะเล (ความเค็มต่ำมาก) -> ไม่ประเมิน
  const salRaw = num(r.sal);
  if (!isNaN(salRaw) && salRaw < 1) {
    return {
      mode, modeName: cfg.name, title: cfg.title, overall: "unknown",
      advice: "ยังประเมินไม่ได้ — ความเค็มต่ำมาก ทุ่นอาจไม่ได้อยู่ในน้ำทะเล",
      labNote: cfg.labNote ?? "", params: [], reasons: [],
    };
  }

  const params: ParamEval[] = [];
  let anyCriticalRed = false, anyRed = false, anyOrange = false;
  const redReasons: string[] = [], watchReasons: string[] = [];

  // เรียงตาม order ใน params meta
  const keys = Object.keys(crit).sort((a, b) => (PARAMS_META[a]?.order ?? 99) - (PARAMS_META[b]?.order ?? 99));

  for (const k of keys) {
    let v = num((r as any)[k]);
    if (isNaN(v)) continue;
    if (k === "do_pct" && v <= 2) v = v * 100;      // บางเซนเซอร์ส่งเป็นสัดส่วน (0.9 = 90%)

    const c = crit[k];
    const st = bandStatus(v, c.green, c.orange);
    const meta = PARAMS_META[k] ?? { label: k, unit: "", order: 99 };
    const dec = DEC[k] ?? 1;
    const text = `${v.toFixed(dec)}${meta.unit ? " " + meta.unit : ""}`;

    params.push({ key: k, label: meta.label, unit: meta.unit, value: v, text, status: st, tier: c.tier });

    if (st === "red") {
      anyRed = true;
      if (c.tier === "critical") anyCriticalRed = true;
      redReasons.push(`${meta.label} ${text}`);
    } else if (st === "orange") {
      anyOrange = true;
      watchReasons.push(`${meta.label} ${text}`);
    }
  }

  let overall: Status;
  let reasons: string[];
  if (anyCriticalRed) { overall = "red"; reasons = redReasons; }
  else if (anyRed || anyOrange) { overall = "orange"; reasons = redReasons.concat(watchReasons); }
  else { overall = "green"; reasons = []; }

  const tmpl = cfg.advice[overall] as string;
  const advice = tmpl.replace("{reasons}", reasons.join(", "));

  return { mode, modeName: cfg.name, title: cfg.title, overall, advice, labNote: cfg.labNote ?? "", params, reasons };
}

// อ่านโหมดปัจจุบันจากตาราง app_settings (คืน default ถ้าไม่มี)
export async function getMode(sb: any): Promise<string> {
  try {
    const { data } = await sb.from("app_settings").select("value").eq("key", "mode").maybeSingle();
    return modeKeyValid(data?.value);
  } catch { return DEFAULT_MODE; }
}

// เขียนโหมดใหม่ (ใช้โดย /mode ฝั่งแอดมิน)
export async function setMode(sb: any, modeKey: string): Promise<boolean> {
  const m = modeKeyValid(modeKey);
  const { error } = await sb.from("app_settings")
    .upsert({ key: "mode", value: m, updated_at: new Date().toISOString() });
  return !error;
}
