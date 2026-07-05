<?php
/* =========================================================================
   save.php  —  รับค่าจาก ESP32 แล้วบันทึกลงฐานข้อมูล
   ---------------------------------------------------------------------------
   ESP32 เรียกได้ 2 แบบ:
   (A) ง่ายสุด — GET query string (ดีตอนทดสอบ พิมพ์ใน browser ได้เลย):
       save.php?key=buoy-secret-2026&do=6.2&sal=32.1&turb=8&chl=3.4&orp=280&oil=5&algae=2.1
   (B) POST JSON:
       {"key":"buoy-secret-2026","do":6.2,"sal":32.1, ...}
   ---------------------------------------------------------------------------
   ตอบกลับ: {"ok":true,"id":123}  หรือ  {"ok":false,"error":"..."}
   ========================================================================= */
require __DIR__ . '/../config.php';

// รับข้อมูลได้ทั้ง GET และ POST JSON
$in = $_GET;
$raw = file_get_contents('php://input');
if ($raw) {
    $json = json_decode($raw, true);
    if (is_array($json)) $in = array_merge($in, $json);
}

// ตรวจ API key
if (($in['key'] ?? '') !== API_KEY) {
    http_response_code(401);
    echo json_encode(['ok' => false, 'error' => 'invalid api key']);
    exit;
}

// ดึงค่าตัวเลข (ไม่ส่งมา = NULL)
function num($in, $k) {
    return isset($in[$k]) && $in[$k] !== '' ? floatval($in[$k]) : null;
}

$device = $in['device'] ?? 'buoy-01';
$fields = ['do'=>'do_val','sal'=>'sal','turb'=>'turb','chl'=>'chl',
           'orp'=>'orp','oil'=>'oil','algae'=>'algae'];

try {
    $cols = ['device']; $vals = [':device']; $args = [':device'=>$device];
    foreach ($fields as $in_key => $col) {
        $cols[] = $col; $vals[] = ":$col"; $args[":$col"] = num($in, $in_key);
    }
    $sql = "INSERT INTO readings (" . implode(',', $cols) . ") VALUES (" . implode(',', $vals) . ")";
    $st = db()->prepare($sql);
    $st->execute($args);
    echo json_encode(['ok' => true, 'id' => (int)db()->lastInsertId()]);
} catch (Throwable $e) {
    http_response_code(500);
    echo json_encode(['ok' => false, 'error' => $e->getMessage()]);
}
