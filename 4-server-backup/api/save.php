<?php
/* =========================================================================
   save.php  —  รับค่าจาก ESP32 บันทึกลง DB + เช็คเกณฑ์แจ้งเตือน Telegram
   ---------------------------------------------------------------------------
   ESP32 ส่ง POST JSON (คีย์ตรงกับฝั่ง Supabase):
     {"key":"buoy-secret-2026","device":"buoy-01",
      "do_val":6.2,"do_pct":95,"temp":28.5,"ph":8.1,
      "sal":32,"cond":48,"tds":34,"turb":8,"lat":7.19,"lon":100.6}
   ทดสอบผ่าน browser (GET) ก็ได้:  save.php?key=...&do=6.2&ph=8.1&temp=28
   ตอบกลับ: {"ok":true,"id":123}
   ========================================================================= */
require __DIR__ . '/../config.php';
require __DIR__ . '/../lib/telegram.php';

// รับข้อมูลได้ทั้ง GET และ POST JSON
$in = $_GET;
$raw = file_get_contents('php://input');
if ($raw) { $json = json_decode($raw, true); if (is_array($json)) $in = array_merge($in, $json); }

// ตรวจ API key
if (($in['key'] ?? '') !== API_KEY) {
    http_response_code(401);
    echo json_encode(['ok' => false, 'error' => 'invalid api key']);
    exit;
}

function num($in, $k) { return isset($in[$k]) && $in[$k] !== '' ? floatval($in[$k]) : null; }

$device = $in['device'] ?? DEVICE_ID;
$cols = ['do_val','do_pct','temp','ph','sal','cond','tds','turb','chl','orp','oil','algae','lat','lon'];

// รับค่าตามชื่อคอลัมน์ + รองรับ alias เก่า (do -> do_val) เผื่อทดสอบด้วย query แบบเดิม
$vals = [];
foreach ($cols as $c) $vals[$c] = num($in, $c);
if ($vals['do_val'] === null) $vals['do_val'] = num($in, 'do');

try {
    $colList = array_merge(['device'], $cols);
    $colSql  = implode(',', array_map(fn($c) => "`$c`", $colList));
    $phSql   = implode(',', array_map(fn($c) => ":$c", $colList));
    $args = [':device' => $device];
    foreach ($cols as $c) $args[":$c"] = $vals[$c];

    db()->prepare("INSERT INTO readings ($colSql) VALUES ($phSql)")->execute($args);
    echo json_encode(['ok' => true, 'id' => (int)db()->lastInsertId()]);

    // เช็คเกณฑ์วิกฤต + แจ้งเตือน Telegram (server ทำแทน ESP32)
    tg_check_and_alert($device, $vals);
} catch (Throwable $e) {
    http_response_code(500);
    echo json_encode(['ok' => false, 'error' => $e->getMessage()]);
}
