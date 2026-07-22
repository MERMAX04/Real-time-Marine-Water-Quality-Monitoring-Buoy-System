<?php
/* =========================================================================
   tg-webhook.php  —  รับ "คำสั่งแชท" จาก Telegram (webhook) แล้วตอบทันที
     /start  = สมัคร (เก็บ chat_id)   /status = ค่าล่าสุดจาก DB
     /stop   = ยกเลิก                 /help   = เมนู
   ตั้ง webhook:
     https://api.telegram.org/bot<TOKEN>/setWebhook?url=https://<โดเมน>/buoy/api/tg-webhook.php&secret_token=buoy-hook-2026
   ========================================================================= */
require __DIR__ . '/../config.php';
require __DIR__ . '/../lib/telegram.php';

// ป้องกันคนอื่นยิง: Telegram แนบ secret_token มาใน header (ตั้งตอน setWebhook)
$hdr = $_SERVER['HTTP_X_TELEGRAM_BOT_API_SECRET_TOKEN'] ?? '';
if (defined('TELEGRAM_WEBHOOK_SECRET') && TELEGRAM_WEBHOOK_SECRET !== '' && $hdr !== TELEGRAM_WEBHOOK_SECRET) {
    http_response_code(401);
    echo 'forbidden';
    exit;
}

$update = json_decode(file_get_contents('php://input'), true) ?: [];
$msg = $update['message'] ?? $update['edited_message'] ?? null;
$chatId = $msg['chat']['id'] ?? null;
$text = trim($msg['text'] ?? '');
if (!$chatId) { echo 'ok'; exit; }

$cmd = strtolower(preg_split('/[\s@]/', $text)[0] ?? '');   // "/status@Bot arg" -> "/status"
$device = defined('DEVICE_ID') ? DEVICE_ID : 'buoy-01';

try {
    if ($cmd === '/start') {
        db()->prepare("REPLACE INTO tg_subscribers (chat_id) VALUES (?)")->execute([(string)$chatId]);
        tg_send($chatId, "✅ สมัครรับแจ้งเตือนคุณภาพน้ำจากทุ่น $device เรียบร้อย!\nพิมพ์ /status ดูค่าล่าสุด · /stop เพื่อยกเลิก");
    } elseif ($cmd === '/status') {
        $st = db()->prepare("SELECT * FROM readings WHERE device=? ORDER BY ts DESC LIMIT 1");
        $st->execute([$device]);
        tg_send($chatId, tg_fmt_status($st->fetch()));
    } elseif ($cmd === '/stop' || $cmd === '/unsubscribe') {
        db()->prepare("DELETE FROM tg_subscribers WHERE chat_id=?")->execute([(string)$chatId]);
        tg_send($chatId, "🛑 ยกเลิกรับแจ้งเตือนแล้ว (พิมพ์ /start เพื่อสมัครใหม่ได้ทุกเมื่อ)");
    } elseif ($cmd === '/help' || $cmd === '/menu') {
        tg_send($chatId, "🤖 คำสั่งทุ่น $device:\n/status – ดูค่าน้ำล่าสุดทุกค่า\n/start – สมัครรับแจ้งเตือน\n/stop – ยกเลิก\n/help – เมนูนี้\n\n(ระบบจะเด้งเตือนเองเมื่อค่าน้ำเข้าขั้นวิกฤต)");
    }
} catch (Throwable $e) {
    error_log('tg-webhook: ' . $e->getMessage());
}
echo 'ok';   // ตอบ 200 เสมอ ไม่งั้น Telegram จะ retry รัวๆ
