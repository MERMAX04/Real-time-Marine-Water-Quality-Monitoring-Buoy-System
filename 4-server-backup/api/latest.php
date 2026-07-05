<?php
/* =========================================================================
   latest.php  —  ส่งค่าล่าสุด 1 ชุด ให้ Dashboard
   เรียก:  latest.php   หรือ   latest.php?device=buoy-01
   ตอบ:   {"do":6.2,"sal":32.1,"turb":8,...,"ts":"2026-07-05 12:00:00"}
   ========================================================================= */
require __DIR__ . '/../config.php';

$device = $_GET['device'] ?? 'buoy-01';
try {
    $st = db()->prepare("SELECT * FROM readings WHERE device=? ORDER BY ts DESC LIMIT 1");
    $st->execute([$device]);
    $row = $st->fetch();
    if (!$row) { echo json_encode(['ok'=>false,'error'=>'no data']); exit; }

    // แปลงชื่อคอลัมน์ให้ตรงกับที่ Dashboard ใช้ (do_val -> do)
    echo json_encode([
        'do'    => is_null($row['do_val']) ? null : (float)$row['do_val'],
        'sal'   => is_null($row['sal'])    ? null : (float)$row['sal'],
        'turb'  => is_null($row['turb'])   ? null : (float)$row['turb'],
        'chl'   => is_null($row['chl'])    ? null : (float)$row['chl'],
        'orp'   => is_null($row['orp'])    ? null : (float)$row['orp'],
        'oil'   => is_null($row['oil'])    ? null : (float)$row['oil'],
        'algae' => is_null($row['algae'])  ? null : (float)$row['algae'],
        'ts'    => $row['ts'],
    ]);
} catch (Throwable $e) {
    http_response_code(500);
    echo json_encode(['ok'=>false,'error'=>$e->getMessage()]);
}
