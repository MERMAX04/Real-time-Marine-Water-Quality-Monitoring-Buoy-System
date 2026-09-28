// =========================================================================
// admin  —  ล็อกอินแอดมิน (เว็บ) + เปลี่ยนโหมดแบบ verified
//   POST { action:"login",   user, pass }   -> { ok, token }   (เช็ครหัสจาก env)
//   POST { action:"setmode", token, mode }  -> { ok, mode }    (verify token ก่อนเขียน)
//   เรียกจากเบราว์เซอร์ (dashboard บน onrender.com) -> ต้องมี CORS
//
// deploy:  supabase functions deploy admin --no-verify-jwt
// env ที่ต้องตั้ง:  ADMIN_USER, ADMIN_PASS   (เก็บใน Supabase secret ไม่อยู่ในโค้ด)
// =========================================================================
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
import { setMode } from "../_shared/water-eval.ts";

const sb = createClient(
  Deno.env.get("SUPABASE_URL")!,
  Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!,
);
const ADMIN_USER = Deno.env.get("ADMIN_USER") ?? "";
const ADMIN_PASS = Deno.env.get("ADMIN_PASS") ?? "";

const CORS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Headers": "content-type",
  "Access-Control-Allow-Methods": "POST, OPTIONS",
};

// token = hash ของรหัสผ่าน (ให้ client เก็บไว้ verify ตอน setmode โดยไม่ต้องเก็บรหัสจริง)
async function sessionToken(): Promise<string> {
  const buf = new TextEncoder().encode("buoy-admin:" + ADMIN_PASS);
  const hash = await crypto.subtle.digest("SHA-256", buf);
  return btoa(String.fromCharCode(...new Uint8Array(hash)));
}

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: CORS });

  const json = (o: unknown, status = 200) =>
    new Response(JSON.stringify(o), { status, headers: { ...CORS, "content-type": "application/json" } });

  let body: Record<string, any>;
  try { body = await req.json(); } catch { return json({ ok: false, error: "bad json" }, 400); }

  try {
    if (body.action === "login") {
      const ok = !!ADMIN_USER && !!ADMIN_PASS && body.user === ADMIN_USER && body.pass === ADMIN_PASS;
      return ok ? json({ ok: true, token: await sessionToken() }) : json({ ok: false, error: "invalid credentials" }, 401);
    }

    if (body.action === "setmode") {
      if (!ADMIN_PASS || body.token !== await sessionToken()) return json({ ok: false, error: "unauthorized" }, 403);
      const saved = await setMode(sb, String(body.mode ?? ""));
      return saved ? json({ ok: true, mode: body.mode }) : json({ ok: false, error: "save failed" }, 500);
    }

    return json({ ok: false, error: "unknown action" }, 400);
  } catch (e) {
    console.error("admin error", e);
    return json({ ok: false, error: "server error" }, 500);
  }
});
