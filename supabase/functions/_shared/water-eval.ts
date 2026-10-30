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

// ประเภทน้ำ (แยกจากความเค็ม) — ถ้าไม่ใช่น้ำทะเล จะไม่ตัดสินค่าใน seaOnly (เกณฑ์ของน้ำทะเล)
export type WaterType = "fresh" | "brackish" | "sea";
export const WATER_TYPE = modesConfig.waterType as {
  freshMaxSal: number; seaMinSal: number; seaOnly: string[]; labels: Record<WaterType, string>;
};

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
  waterType: WaterType | null;      // null = ไม่มีค่าความเค็ม (ประเมินทุกค่าที่มี)
  waterLabel: string;               // "น้ำจืด" / "น้ำกร่อย" / "น้ำทะเล" ("" เมื่อ null)
  skipped: SkippedParam[];          // ค่าใน seaOnly ที่ไม่ได้ตัดสิน (น้ำจืด/น้ำกร่อย) — แสดงเป็น "อ้างอิง"
};

export type SkippedParam = { key: string; label: string; unit: string; value: number; text: string };

// แยกประเภทน้ำจากความเค็ม (ppt) — NaN/ว่าง -> null
export function waterTypeOf(sal: unknown): WaterType | null {
  const s = num(sal);
  if (isNaN(s)) return null;
  if (s < WATER_TYPE.freshMaxSal) return "fresh";
  if (s < WATER_TYPE.seaMinSal) return "brackish";
  return "sea";
}

// ประเมิน 1 แถวข้อมูล ตามโหมดที่เลือก
export function evalWater(r: Record<string, unknown>, modeKey: string): WaterEval {
  const mode = modeKeyValid(modeKey);
  const cfg = MODES[mode];
  const crit = cfg.criteria as Record<string, { green: Band; orange: Band; tier: Tier }>;

  // เฟรมเสีย / เซนเซอร์อ่านไม่ได้ (pH และอุณหภูมิเป็น 0/ว่าง พร้อมกัน) -> ไม่ประเมิน (กันแดงหลอกจาก pH = 0)
  //   ค่าความเค็มในเฟรมเสียเชื่อไม่ได้ จึงไม่ระบุประเภทน้ำ (waterType = null)
  const dead = (v: number) => isNaN(v) || v === 0;
  if (dead(num(r.ph)) && dead(num(r.temp))) {
    return {
      mode, modeName: cfg.name, title: cfg.title, overall: "unknown",
      advice: "ยังประเมินไม่ได้ — เซนเซอร์อ่านค่าไม่ได้หรือเฟรมข้อมูลเสีย",
      labNote: cfg.labNote ?? "", params: [], reasons: [],
      waterType: null, waterLabel: "", skipped: [],
    };
  }

  // ประเภทน้ำ (จากความเค็ม)
  const waterType = waterTypeOf(r.sal);
  const waterLabel = waterType ? WATER_TYPE.labels[waterType] : "";

  // ไม่ใช่น้ำทะเล -> ค่าใน seaOnly ไม่นำมาตัดสิน (แสดงเป็นค่าอ้างอิงแทน)
  const skipSea = waterType === "fresh" || waterType === "brackish";
  const params: ParamEval[] = [];
  const skipped: SkippedParam[] = [];
  let anyCriticalRed = false, anyRed = false, anyOrange = false;
  const redReasons: string[] = [], watchReasons: string[] = [];

  // เรียงตาม order ใน params meta
  const keys = Object.keys(crit).sort((a, b) => (PARAMS_META[a]?.order ?? 99) - (PARAMS_META[b]?.order ?? 99));

  for (const k of keys) {
    let v = num((r as any)[k]);
    if (isNaN(v)) continue;
    if (k === "do_pct" && v <= 2) v = v * 100;      // บางเซนเซอร์ส่งเป็นสัดส่วน (0.9 = 90%)

    const c = crit[k];
    const meta = PARAMS_META[k] ?? { label: k, unit: "", order: 99 };
    const dec = DEC[k] ?? 1;
    const text = `${v.toFixed(dec)}${meta.unit ? " " + meta.unit : ""}`;

    if (skipSea && WATER_TYPE.seaOnly.includes(k)) {
      // ค่าน้อยในน้ำจืด (เช่น 0.07 ppt) ปัด 1 ตำแหน่งจะกลายเป็น 0.1 -> ใช้ 2 ตำแหน่งเมื่อ < 1
      const t2 = Math.abs(v) < 1 ? `${v.toFixed(2)}${meta.unit ? " " + meta.unit : ""}` : text;
      skipped.push({ key: k, label: meta.label, unit: meta.unit, value: v, text: t2 });
      continue;
    }

    const st = bandStatus(v, c.green, c.orange);
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

  return {
    mode, modeName: cfg.name, title: cfg.title, overall, advice, labNote: cfg.labNote ?? "", params, reasons,
    waterType, waterLabel, skipped,
  };
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
