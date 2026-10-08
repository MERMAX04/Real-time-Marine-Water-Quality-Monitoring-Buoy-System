<?php
/* =========================================================================
   history.php  —  ส่งค่าย้อนหลังหลายจุด ไว้วาดกราฟ
   เรียก:  history.php?limit=60   (ค่าเริ่มต้น 60 จุดล่าสุด)
   ตอบ:   [{"ts":"...","do":6.2,...}, ...]  (เรียงเก่า -> ใหม่)
   ========================================================================= */
require __DIR__ . '/../config.php';

$device = $_GET['device'] ?? 'buoy-01';
$limit  = max(1, min(500, intval($_GET['limit'] ?? 60)));  // กัน limit เกิน

try {
    $st = db()->prepare("SELECT * FROM readings WHERE device=? ORDER BY ts DESC LIMIT $limit");
    $st->execute([$device]);
    $rows = array_reverse($st->fetchAll());   // ให้เก่าไปใหม่

    $out = array_map(function($r){
        return [
            'ts'    => $r['ts'],
            'do'    => is_null($r['do_val'])?null:(float)$r['do_val'],
            'sal'   => is_null($r['sal'])?null:(float)$r['sal'],
            'turb'  => is_null($r['turb'])?null:(float)$r['turb'],
            'ph'    => is_null($r['ph'])?null:(float)$r['ph'],
            'cond'  => is_null($r['cond'])?null:(float)$r['cond'],
            'do_pct'=> is_null($r['do_pct'])?null:(float)$r['do_pct'],
            'temp'  => is_null($r['temp'])?null:(float)$r['temp'],
        ];
    }, $rows);
    echo json_encode($out);
} catch (Throwable $e) {
    http_response_code(500);
    echo json_encode(['ok'=>false,'error'=>$e->getMessage()]);
}
