<?php
/* =========================================================================
   latest.php  —  ส่งค่าล่าสุด 1 ชุด ให้ Dashboard (โหมด CONFIG.mode='server')
   เรียก:  latest.php   หรือ   latest.php?device=buoy-01
   ตอบ key ให้ตรงกับ PARAMS ของ dashboard: do, do_pct, temp, ph, sal, cond, turb ...
   ========================================================================= */
require __DIR__ . '/../config.php';

$device = $_GET['device'] ?? 'buoy-01';
try {
    $st = db()->prepare("SELECT * FROM readings WHERE device=? ORDER BY ts DESC LIMIT 1");
    $st->execute([$device]);
    $row = $st->fetch();
    if (!$row) { echo json_encode(['ok' => false, 'error' => 'no data']); exit; }

    $f = function ($v) { return is_null($v) || $v === '' ? null : (float)$v; };
    $doPct = $f($row['do_pct']);
    if ($doPct !== null && $doPct <= 2) $doPct *= 100;   // บางเซนเซอร์ส่งเป็นสัดส่วน (0.90) -> แปลงเป็น % (90)
    echo json_encode([
        'do'     => $f($row['do_val']),   // dashboard ใช้ key 'do'
        'do_pct' => $doPct,
        'temp'   => $f($row['temp']),
        'ph'     => $f($row['ph']),
        'sal'    => $f($row['sal']),
        'cond'   => $f($row['cond']),
        'turb'   => $f($row['turb']),
        'lat'    => $f($row['lat']),
        'lon'    => $f($row['lon']),
        'sensor_ok' => (bool)$row['sensor_ok'],   // heartbeat: false = ESP ไลฟ์ แต่ sensor อ่านไม่ได้
        'ts'     => $row['ts'],
    ], JSON_UNESCAPED_UNICODE);
} catch (Throwable $e) {
    http_response_code(500);
    echo json_encode(['ok' => false, 'error' => $e->getMessage()]);
}
