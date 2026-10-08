<?php
/* =========================================================================
   lib/telegram.php  —  Telegram ฝั่ง server (ใช้ร่วมโดย save.php และ tg-webhook.php)
   ส่งข้อความ + ประเมินคุณภาพน้ำ "ตามโหมด" (6 ประเภท) + กันเตือนซ้ำ + จัดรูป /status
   เกณฑ์ทั้งหมดมาจาก water-modes.json (มิเรอร์ตรรกะเดียวกับฝั่ง Supabase _shared/water-eval.ts)
   ========================================================================= */

const TG_COOLDOWN_SEC = 30 * 60;   // 30 นาที ต่อชุดค่าที่ตก

// บอทถูกตั้งค่าแล้วหรือยัง
function tg_ready() {
    return defined('TELEGRAM_BOT_TOKEN') && TELEGRAM_BOT_TOKEN !== ''
        && strpos(TELEGRAM_BOT_TOKEN, 'ใส่_') !== 0;
}

// ส่งข้อความเข้า Telegram (ใช้ cURL ถ้ามี ไม่งั้น fallback file_get_contents)
function tg_send($chatId, $text) {
    if (!tg_ready()) return false;
    $url = "https://api.telegram.org/bot" . TELEGRAM_BOT_TOKEN . "/sendMessage";
    $payload = ['chat_id' => $chatId, 'text' => $text, 'disable_web_page_preview' => true];
    if (function_exists('curl_init')) {
        $ch = curl_init($url);
        curl_setopt_array($ch, [
            CURLOPT_POST => true,
            CURLOPT_POSTFIELDS => $payload,
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_TIMEOUT => 10,
        ]);
        $ok = curl_exec($ch) !== false;
        curl_close($ch);
        return $ok;
    }
    $ctx = stream_context_create(['http' => [
        'method'  => 'POST',
        'header'  => 'Content-Type: application/x-www-form-urlencoded',
        'content' => http_build_query($payload),
        'timeout' => 10,
    ]]);
    return @file_get_contents($url, false, $ctx) !== false;
}

// cooldown ผ่านตาราง alert_state — คืน true ถ้าพ้น cooldown (แล้วอัปเดตเวลาให้)
function tg_can_fire($key) {
    $st = db()->prepare("SELECT last_sent FROM alert_state WHERE k=?");
    $st->execute([$key]);
    $row = $st->fetch();
    if ($row && $row['last_sent'] && strtotime($row['last_sent']) > time() - TG_COOLDOWN_SEC) return false;
    db()->prepare("REPLACE INTO alert_state (k, last_sent) VALUES (?, NOW())")->execute([$key]);
    return true;
}

/* =========================================================================
   โหมดมาตรฐานคุณภาพน้ำ (6 ประเภท) — อ่าน config + ประเมินตามโหมด
   ========================================================================= */

// โหลด config โหมด (cache ในตัวแปร static)
function wm_config() {
    static $c = null;
    if ($c === null) {
        $raw = @file_get_contents(__DIR__ . '/water-modes.json');
        $c = $raw ? (json_decode($raw, true) ?: []) : [];
    }
    return $c;
}

function wm_mode_valid($k) {
    $c = wm_config();
    return (isset($c['modes'][$k])) ? $k : ($c['defaultMode'] ?? 'recreation');
}

// อ่านโหมดปัจจุบันจากตาราง app_settings
function wm_get_mode() {
    try {
        $st = db()->prepare("SELECT v FROM app_settings WHERE k='mode'");
        $st->execute();
        $row = $st->fetch();
        return wm_mode_valid($row['v'] ?? null);
    } catch (Throwable $e) {
        return wm_config()['defaultMode'] ?? 'recreation';
    }
}

// เขียนโหมดใหม่ (ใช้โดย /mode ฝั่งแอดมิน)
function wm_set_mode($k) {
    $k = wm_mode_valid($k);
    db()->prepare("REPLACE INTO app_settings (k, v, updated_at) VALUES ('mode', ?, NOW())")->execute([$k]);
    return $k;
}

function wm_inband($v, $band) {
    $lo = $band[0]; $hi = $band[1];
    if ($lo !== null && $v < $lo) return false;
    if ($hi !== null && $v > $hi) return false;
    return true;
}

function wm_band_status($v, $green, $orange) {
    if (wm_inband($v, $green))  return 'green';
    if (wm_inband($v, $orange)) return 'orange';
    return 'red';
}


function wm_dot($s) {
    $map = ['green' => '🟢', 'orange' => '🟠', 'red' => '🔴', 'unknown' => '⚪'];
    return $map[$s] ?? '⚪';
}

// สถานะความสด/ไลฟ์ของทุ่น (จากอายุแถวล่าสุด) — ทุ่นส่งทุก ~15 วิ
function wm_freshness_line($r) {
    if (!$r || empty($r['ts'])) return "🔴 ไม่มีข้อมูลจากทุ่นเลย";
    $ageSec = max(0, time() - strtotime($r['ts']));
    if ($ageSec < 60)        $ago = round($ageSec) . " วิ";
    elseif ($ageSec < 3600)  $ago = round($ageSec / 60) . " นาที";
    else                     $ago = number_format($ageSec / 3600, 1) . " ชม.";
    if ($ageSec < 45) {
        if (isset($r['sensor_ok']) && !$r['sensor_ok'])
            return "🟠 ESP ออนไลน์ แต่ sensor อ่านไม่ได้ (" . $ago . "ที่แล้ว)";
        return "🟢 ทุ่นออนไลน์ (ข้อมูลสด " . $ago . "ที่แล้ว)";
    }
    if ($ageSec < 300) return "🟠 ไม่มีข้อมูลใหม่ " . $ago . " — ทุ่นอาจสะดุด";
    return "🔴 ทุ่นออฟไลน์ " . $ago . " — ESP/4G อาจหลุด หรือไฟหมด";
}

// ประเมิน 1 แถวข้อมูล ตามโหมดที่เลือก (มิเรอร์ evalWater ใน water-eval.ts)
function wm_eval($r, $modeKey) {
    $cfg  = wm_config();
    $mode = wm_mode_valid($modeKey);
    $m    = $cfg['modes'][$mode];
    $crit = $m['criteria'];
    $META = $cfg['params'];
    $DEC  = ['ph'=>2,'do_val'=>2,'do_pct'=>0,'temp'=>1,'sal'=>1,'cond'=>1,'turb'=>1];
    $g = function ($k) use ($r) { return isset($r[$k]) && $r[$k] !== null && $r[$k] !== '' ? (float)$r[$k] : null; };

    // ทุ่นไม่ได้อยู่ในน้ำทะเล (ความเค็มต่ำมาก) -> ไม่ประเมิน
    $sal = $g('sal');
    if ($sal !== null && $sal < 1) {
        return ['mode'=>$mode,'name'=>$m['name'],'title'=>$m['title'],'overall'=>'unknown',
                'advice'=>'ยังประเมินไม่ได้ — ความเค็มต่ำมาก ทุ่นอาจไม่ได้อยู่ในน้ำทะเล',
                'labNote'=>$m['labNote'] ?? '','params'=>[],'reasons'=>[]];
    }

    // เรียงตาม order
    $keys = array_keys($crit);
    usort($keys, function ($a, $b) use ($META) { return ($META[$a]['order'] ?? 99) - ($META[$b]['order'] ?? 99); });

    $anyCritRed = false; $anyRed = false; $anyOrange = false;
    $redR = []; $watchR = []; $params = [];
    foreach ($keys as $k) {
        $v = $g($k);
        if ($v === null) continue;
        if ($k === 'do_pct' && $v <= 2) $v *= 100;   // บางเซนเซอร์ส่งเป็นสัดส่วน (0.9 = 90%)
        $c = $crit[$k];
        $st = wm_band_status($v, $c['green'], $c['orange']);
        $unit = $META[$k]['unit'] ?? '';
        $text = number_format($v, $DEC[$k] ?? 1) . ($unit ? ' ' . $unit : '');
        $params[] = ['key'=>$k,'label'=>$META[$k]['label'],'text'=>$text,'status'=>$st,'tier'=>$c['tier']];
        if ($st === 'red') {
            $anyRed = true;
            if ($c['tier'] === 'critical') $anyCritRed = true;
            $redR[] = $META[$k]['label'] . ' ' . $text;
        } elseif ($st === 'orange') {
            $anyOrange = true;
            $watchR[] = $META[$k]['label'] . ' ' . $text;
        }
    }

    if ($anyCritRed)                 { $overall = 'red';    $reasons = $redR; }
    elseif ($anyRed || $anyOrange)   { $overall = 'orange'; $reasons = array_merge($redR, $watchR); }
    else                             { $overall = 'green';  $reasons = []; }

    $advice = str_replace('{reasons}', implode(', ', $reasons), $m['advice'][$overall]);
    return ['mode'=>$mode,'name'=>$m['name'],'title'=>$m['title'],'overall'=>$overall,
            'advice'=>$advice,'labNote'=>$m['labNote'] ?? '','params'=>$params,'reasons'=>$reasons];
}

// รายการโหมด (สำหรับ /mode)
function wm_mode_list($current) {
    $cfg = wm_config();
    $s = "🗂️ โหมดมาตรฐานคุณภาพน้ำ (6 ประเภท):";
    foreach ($cfg['modes'] as $key => $c) {
        $mark = ($key === $current) ? " ✅ (ใช้อยู่)" : "";
        $s .= "\n" . $c['id'] . ". " . $c['name'] . $mark;
    }
    $s .= "\n\nเปลี่ยนโหมด: /mode <เลข 1–6>  เช่น /mode 4";
    return $s;
}

// id (1–6) หรือชื่อคีย์ -> คีย์โหมด
function wm_resolve_mode($arg) {
    $a = strtolower(trim($arg));
    foreach (wm_config()['modes'] as $key => $c) {
        if ($a === $key || $a === (string)$c['id']) return $key;
    }
    return null;
}

/* =========================================================================
   ข้อความตอบ + แจ้งเตือน
   ========================================================================= */

// /status = ค่าน้ำล่าสุด แยกสีต่อค่า (ตามโหมด)
function tg_fmt_status($r) {
    if (!$r) return "⏳ ยังไม่มีข้อมูลจากทุ่น (ยังไม่เคยส่งค่าขึ้นมา)";
    $ev = wm_eval($r, wm_get_mode());
    $cfg = wm_config();
    $emoji = explode(' ', $cfg['modes'][$ev['mode']]['title'])[0];
    $m = "📊 ค่าน้ำล่าสุด (" . ($r['device'] ?? DEVICE_ID) . ")\nโหมด: " . $emoji . " " . $ev['name'];
    $m .= "\n\nสถานะทุ่น : " . wm_freshness_line($r);

    // ค่าล่าสุดเป็น heartbeat (sensor อ่านไม่ได้) — ไม่โชว์สถานะเขียวหลอกๆ
    if (isset($r['sensor_ok']) && !$r['sensor_ok']) {
        $m .= "\n\n⚠️ ทุ่นส่ง heartbeat (sensor อ่านไม่ได้) — ยังไม่มีค่ารอบล่าสุด ควรตรวจสอบ probe";
        if ($r['ts'] ?? null) $m .= "\n🕒 " . $r['ts'];
        return $m;
    }

    if ($ev['overall'] === 'unknown') {
        $m .= "\n\n⚪ " . $ev['advice'];
        if ($r['ts'] ?? null) $m .= "\n🕒 " . $r['ts'];
        return $m;
    }

    // สถานะรวม (บนสุด) แล้วค่ารายตัวเยื้องอยู่ใต้
    $m .= "\n\n" . wm_dot($ev['overall']) . " " . $ev['advice'];
    foreach ($ev['params'] as $p) $m .= "\n     " . wm_dot($p['status']) . " " . $p['label'] . ": " . $p['text'];

    $f = function ($v, $d = 2) { return ($v === null || $v === '') ? '–' : number_format((float)$v, $d); };
    if (($r['lat'] ?? null) !== null && ($r['lon'] ?? null) !== null)
        $m .= "\n     📍 พิกัด: " . $f($r['lat'], 6) . ", " . $f($r['lon'], 6);

    // ป้ายหัวข้อโหมดปิดท้าย
    $m .= "\n\n" . $ev['title'];
    if ($r['ts'] ?? null) $m .= "\n🕒 " . $r['ts'];
    return $m;
}

// /swim = สถานะรวม "ทำกิจกรรมได้ไหม" ตามโหมด
function tg_fmt_activity($r) {
    if (!$r) return "⏳ ยังไม่มีข้อมูลจากทุ่น";
    $ev = wm_eval($r, wm_get_mode());
    $m = $ev['title'] . " (" . ($r['device'] ?? DEVICE_ID) . ")\nโหมด: " . $ev['name'];
    $m .= "\n\nสถานะทุ่น : " . wm_freshness_line($r);
    if (isset($r['sensor_ok']) && !$r['sensor_ok']) {
        $m .= "\n\n⚠️ ทุ่นส่ง heartbeat (sensor อ่านไม่ได้) — ยังประเมินไม่ได้ ควรตรวจสอบ probe";
        if ($r['ts'] ?? null) $m .= "\n🕒 " . $r['ts'];
        return $m;
    }
    $m .= "\n\n" . wm_dot($ev['overall']) . " " . $ev['advice'];
    if ($ev['labNote']) $m .= "\n\nหมายเหตุ: " . $ev['labNote'];
    if ($r['ts'] ?? null) $m .= "\n🕒 " . $r['ts'];
    return $m;
}
// alias เดิม (เผื่อโค้ดอื่นเรียก tg_fmt_swim อยู่)
function tg_fmt_swim($r, $baseSal = null) { return tg_fmt_activity($r); }

// เรียกหลัง insert: ถ้าสถานะรวม "แดง" (ตามโหมด) แล้วส่งหาทุก subscriber
function tg_check_and_alert($device, $r) {
    if (!tg_ready()) return;
    $ev = wm_eval($r, wm_get_mode());
    if ($ev['overall'] !== 'red') return;

    $offend = array_map(function ($p) { return $p['key']; },
                        array_filter($ev['params'], function ($p) { return $p['status'] === 'red'; }));
    sort($offend);
    $key = $device . ':red:' . $ev['mode'] . ':' . implode(',', $offend);
    if (!tg_can_fire($key)) return;

    $text = $ev['title'] . "\n🔴 " . $ev['advice'];
    if ($ev['labNote']) $text .= "\n\nหมายเหตุ: " . $ev['labNote'];
    $msg = "🌊 แจ้งเตือนคุณภาพน้ำ (" . $device . ")\n\n" . $text;

    $subs = db()->query("SELECT chat_id FROM tg_subscribers")->fetchAll();
    foreach ($subs as $s) tg_send($s['chat_id'], $msg);
}
