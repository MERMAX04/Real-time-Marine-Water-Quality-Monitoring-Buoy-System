<?php
/* =========================================================================
   latest.php  —  ส่งค่าล่าสุด 1 ชุด ให้ Dashboard (โหมด CONFIG.mode='server')
   เรียก:  latest.php   หรือ   latest.php?device=buoy-01
   ตอบ key ให้ตรงกับ PARAMS ของ dashboard: do, do_pct, temp, ph, sal, cond, tds, turb ...
   ========================================================================= */
require __DIR__ . '/../config.php';

$device = $_GET['device'] ?? 'buoy-01';
try {
    $st = db()->prepare("SELECT * FROM readings WHERE device=? ORDER BY ts DESC LIMIT 1");
    $st->execute([$device]);
    $row = $st->fetch();
    if (!$row) { echo json_encode(['ok' => false, 'error' => 'no data']); exit; }

    $f = function ($v) { return is_null($v) || $v === '' ? null : (float)$v; };
    echo json_encode([
        'do'     => $f($row['do_val']),   // dashboard ใช้ key 'do'
        'do_pct' => $f($row['do_pct']),
        'temp'   => $f($row['temp']),
        'ph'     => $f($row['ph']),
        'sal'    => $f($row['sal']),
        'cond'   => $f($row['cond']),
        'tds'    => $f($row['tds']),
        'turb'   => $f($row['turb']),
        'chl'    => $f($row['chl']),
        'orp'    => $f($row['orp']),
        'oil'    => $f($row['oil']),
        'algae'  => $f($row['algae']),
        'lat'    => $f($row['lat']),
        'lon'    => $f($row['lon']),
        'ts'     => $row['ts'],
    ], JSON_UNESCAPED_UNICODE);
} catch (Throwable $e) {
    http_response_code(500);
    echo json_encode(['ok' => false, 'error' => $e->getMessage()]);
}
