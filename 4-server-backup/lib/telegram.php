<?php
/* =========================================================================
   lib/telegram.php  —  Telegram ฝั่ง server (ใช้ร่วมโดย save.php และ tg-webhook.php)
   ส่งข้อความ + ประเมินเกณฑ์วิกฤต + กันเตือนซ้ำ (cooldown) + จัดรูป /status
   ========================================================================= */

const TG_COOLDOWN_SEC = 30 * 60;   // 30 นาที/ชนิด (เท่ากับ ALERT_COOLDOWN เดิมบน ESP32)

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

// ประเมินเกณฑ์วิกฤต (ตรงกับ ESP32/Supabase) -> คืน [[key,text], ...]
function tg_eval_alerts($r) {
    $a = [];
    $do = $r['do_val'] ?? null; $ph = $r['ph'] ?? null;
    $temp = $r['temp'] ?? null; $turb = $r['turb'] ?? null;
    if ($do   !== null && $do   < 3.0)  $a[] = ['do',      "🔴 ออกซิเจนละลายน้ำต่ำวิกฤต: " . number_format($do, 2) . " mg/L (สัตว์น้ำเสี่ยงตาย)"];
    if ($ph   !== null && $ph   < 7.0)  $a[] = ['ph_low',  "🔴 น้ำเป็นกรดผิดปกติ: pH " . number_format($ph, 2)];
    if ($ph   !== null && $ph   > 9.0)  $a[] = ['ph_high', "🔴 น้ำเป็นด่างผิดปกติ: pH " . number_format($ph, 2)];
    if ($temp !== null && $temp > 33.0) $a[] = ['temp',    "🟠 อุณหภูมิน้ำสูง: " . number_format($temp, 1) . " °C"];
    if ($turb !== null && $turb > 40.0) $a[] = ['turb',    "🟠 ความขุ่นสูงผิดปกติ: " . number_format($turb, 1) . " NTU"];
    return $a;
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
   ธงสถานะการลงเล่นน้ำ (Blue Flag / bathing water)
   เกณฑ์: คพ.ไทย (นันทนาการ) pH 7.0–8.5, DO ≥ 6 mg/L + Blue Flag ข้อ 3 (ห้ามน้ำทิ้งลงหาด)
   ⚠️ ตัดสินขาดไม่ได้ — E. coli / Enterococci ต้องตรวจแลป
   ที่มา/รายละเอียด: 5-extensions/05-blueflag-swim-safety.md
   ========================================================================= */
const TG_SWIM_NOTE = "หมายเหตุ: ผลชี้ขาดต้องตรวจ E. coli / Enterococci ในห้องปฏิบัติการ (ทุ่นวัดแบคทีเรียไม่ได้)";

// baseline ความเค็ม = มัธยฐานย้อนหลัง 20 แถว (ตัดแถวล่าสุดออก) เพื่อจับการเปลี่ยนแปลงเฉียบพลัน
function tg_baseline_sal($device, $skipLatest = true) {
    $st = db()->prepare("SELECT sal FROM readings WHERE device=? ORDER BY ts DESC LIMIT 20");
    $st->execute([$device]);
    $rows = $st->fetchAll();
    if ($skipLatest) array_shift($rows);
    $v = [];
    foreach ($rows as $r) if ($r['sal'] !== null && (float)$r['sal'] > 1) $v[] = (float)$r['sal'];
    if (count($v) < 3) return null;
    sort($v);
    return $v[intdiv(count($v), 2)];
}

function tg_eval_swim($r, $baseSal) {
    $red = []; $yellow = [];
    $g = function ($k) use ($r) { return isset($r[$k]) && $r[$k] !== null && $r[$k] !== '' ? (float)$r[$k] : null; };
    $do = $g('do_val'); $ph = $g('ph'); $temp = $g('temp'); $turb = $g('turb'); $sal = $g('sal');

    if ($sal !== null && $sal < 1)   // ทุ่นน่าจะไม่ได้อยู่ในน้ำทะเล -> ไม่ตัดสินธง
        return ['flag'=>'unknown','label'=>'⚪ ยังประเมินไม่ได้','reasons'=>['ความเค็มต่ำมาก — ทุ่นอาจไม่ได้อยู่ในน้ำทะเล']];

    if ($do !== null) {
        if ($do < 4)     $red[]    = "ออกซิเจนละลายน้ำต่ำมาก " . number_format($do,2) . " mg/L (มาตรฐานนันทนาการ ≥ 6)";
        elseif ($do < 6) $yellow[] = "ออกซิเจนละลายน้ำ " . number_format($do,2) . " mg/L ต่ำกว่ามาตรฐาน (≥ 6)";
    }
    // DO% — เซนเซอร์บางตัวส่งเป็นสัดส่วน (0.90 = 90%) จึง normalize ก่อน
    $dp = $g('do_pct'); if ($dp !== null && $dp <= 2) $dp = $dp * 100;
    if ($dp !== null) {
        if ($dp < 60 || $dp > 130)      $red[]    = "ออกซิเจนอิ่มตัว " . number_format($dp,0) . "% ผิดปกติมาก (ปกติ 80–120%)";
        elseif ($dp < 80 || $dp > 120)  $yellow[] = "ออกซิเจนอิ่มตัว " . number_format($dp,0) . "% นอกช่วงปกติ (80–120%)";
    }
    $cond = $g('cond');   // การนำไฟฟ้าต่ำ = น้ำจืดเจือ (สอดคล้องกับความเค็ม)
    if ($cond !== null) {
        if ($cond < 35)     $red[]    = "การนำไฟฟ้าต่ำ " . number_format($cond,1) . " mS/cm (น้ำทะเลปกติ 45–55)";
        elseif ($cond < 45) $yellow[] = "การนำไฟฟ้า " . number_format($cond,1) . " mS/cm ต่ำกว่าปกติ (45–55)";
    }
    if ($ph !== null) {
        if ($ph < 6.5 || $ph > 9.0)     $red[]    = "pH " . number_format($ph,2) . " นอกช่วงปลอดภัย (6.5–9.0)";
        elseif ($ph < 7.0 || $ph > 8.5) $yellow[] = "pH " . number_format($ph,2) . " นอกมาตรฐานนันทนาการ (7.0–8.5)";
    }
    if ($turb !== null) {
        if ($turb > 40)     $red[]    = "น้ำขุ่นมาก " . number_format($turb,1) . " NTU — มองไม่เห็นใต้น้ำ";
        elseif ($turb > 15) $yellow[] = "น้ำขุ่น " . number_format($turb,1) . " NTU — ทัศนวิสัยใต้น้ำแย่";
    }
    if ($temp !== null && $temp > 33) $yellow[] = "อุณหภูมิน้ำสูง " . number_format($temp,1) . " °C — แบคทีเรียโตเร็ว";
    if ($sal !== null && $baseSal && $baseSal > 1) {
        $drop = ($baseSal - $sal) / $baseSal * 100;
        if ($drop > 30)     $red[]    = "ความเค็มลดฮวบ " . number_format($drop,0) . "% — อาจมีน้ำจืด/น้ำทิ้งไหลลง";
        elseif ($drop > 15) $yellow[] = "ความเค็มลดลง " . number_format($drop,0) . "% — เฝ้าระวังน้ำจืดเจือ";
    }
    if ($red)    return ['flag'=>'red',   'label'=>'🔴 ธงแดง — ไม่ควรลงเล่นน้ำ',           'reasons'=>array_merge($red,$yellow)];
    if ($yellow) return ['flag'=>'yellow','label'=>'🟡 ธงเหลือง — ลงเล่นได้ แต่ต้องระวัง', 'reasons'=>$yellow];
    return ['flag'=>'green','label'=>'🟢 ธงเขียว — น้ำอยู่ในเกณฑ์ ลงเล่นได้','reasons'=>[]];
}

// ข้อความตอบคำสั่ง /swim
function tg_fmt_swim($r, $baseSal) {
    if (!$r) return "⏳ ยังไม่มีข้อมูลจากทุ่น";
    $s = tg_eval_swim($r, $baseSal);
    $m = "🏖️ สถานะการลงเล่นน้ำ (" . ($r['device'] ?? DEVICE_ID) . ")\n" . $s['label'];
    if ($s['reasons']) $m .= "\n\nเหตุผล:\n• " . implode("\n• ", $s['reasons']);
    $m .= "\n\n" . TG_SWIM_NOTE;
    if ($r['ts'] ?? null) $m .= "\n🕒 " . $r['ts'];
    return $m;
}

// เรียกหลัง insert: เช็คเกณฑ์ + ธงแดง แล้วส่งหาทุก subscriber (เงียบถ้ายังไม่ตั้ง token)
function tg_check_and_alert($device, $r) {
    if (!tg_ready()) return;
    $fire = [];
    foreach (tg_eval_alerts($r) as $al) {
        if (tg_can_fire($device . ':' . $al[0])) $fire[] = $al[1];
    }
    // ธงแดง (ไม่ควรลงเล่นน้ำ) ก็เตือนด้วย — คนละ cooldown key
    $swim = tg_eval_swim($r, tg_baseline_sal($device));
    $swimRed = false;
    if ($swim['flag'] === 'red' && tg_can_fire($device . ':swim_red')) {
        $fire[] = "🏖️ " . $swim['label'] . "\n   • " . implode("\n   • ", $swim['reasons']);
        $swimRed = true;
    }
    if (!$fire) return;
    $msg = "🌊 แจ้งเตือนคุณภาพน้ำ (" . $device . ")\n" . implode("\n", $fire);
    if ($swimRed) $msg .= "\n\n" . TG_SWIM_NOTE;
    $subs = db()->query("SELECT chat_id FROM tg_subscribers")->fetchAll();
    foreach ($subs as $s) tg_send($s['chat_id'], $msg);
}

// จัดรูปข้อความ /status = ค่าน้ำล่าสุดครบทุกตัว
function tg_fmt_status($r) {
    if (!$r) return "⏳ ยังไม่มีข้อมูลจากทุ่น (ยังไม่เคยส่งค่าขึ้นมา)";
    $f = function ($v, $d = 2) { return ($v === null || $v === '') ? '–' : number_format((float)$v, $d); };
    $m  = "📊 ค่าน้ำล่าสุด (" . ($r['device'] ?? DEVICE_ID) . ")";
    $m .= "\n🫧 DO: " . $f($r['do_val']) . " mg/L (" . $f($r['do_pct'], 0) . "%)";
    $m .= "\n🌡 อุณหภูมิ: " . $f($r['temp']) . " °C";
    $m .= "\n⚗️ pH: " . $f($r['ph']);
    $m .= "\n🧂 ความเค็ม: " . $f($r['sal']) . " ppt";
    $m .= "\n⚡ การนำไฟฟ้า: " . $f($r['cond']) . " mS/cm";
    $m .= "\n💧 TDS: " . $f($r['tds']);
    $m .= "\n🌫 ความขุ่น: " . $f($r['turb']) . " NTU";
    if (($r['orp'] ?? null) !== null) $m .= "\n🔬 ORP: " . $f($r['orp'], 1) . " mV";
    if (($r['lat'] ?? null) !== null && ($r['lon'] ?? null) !== null)
        $m .= "\n📍 พิกัด: " . $f($r['lat'], 6) . ", " . $f($r['lon'], 6);
    if ($r['ts'] ?? null) $m .= "\n🕒 " . $r['ts'];
    return $m;
}
