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

$parts = preg_split('/[\s@]+/', $text);
$cmd = strtolower($parts[0] ?? '');   // "/status@Bot arg" -> "/status"
$arg = trim(implode(' ', array_slice($parts, 1)));
$device = defined('DEVICE_ID') ? DEVICE_ID : 'buoy-01';

// อ่านค่าล่าสุด 1 แถว
$latest = function () use ($device) {
    $st = db()->prepare("SELECT * FROM readings WHERE device=? ORDER BY ts DESC LIMIT 1");
    $st->execute([$device]);
    return $st->fetch();
};

try {
    if ($cmd === '/start') {
        db()->prepare("REPLACE INTO tg_subscribers (chat_id) VALUES (?)")->execute([(string)$chatId]);
        tg_send($chatId, "✅ สมัครรับแจ้งเตือนคุณภาพน้ำจากทุ่น $device เรียบร้อย!\n/swim – ทำกิจกรรมได้ไหม · /status – ค่าล่าสุด · /mode – โหมด · /stop – ยกเลิก");
    } elseif ($cmd === '/status') {
        tg_send($chatId, tg_fmt_status($latest()));
    } elseif ($cmd === '/swim' || $cmd === '/activity') {
        tg_send($chatId, tg_fmt_activity($latest()));
    } elseif ($cmd === '/mode') {
        $current = wm_get_mode();
        $admins = defined('TG_ADMINS') ? array_filter(array_map('trim', explode(',', TG_ADMINS))) : [];
        $isAdmin = in_array((string)$chatId, $admins, true);
        if ($arg === '') {
            tg_send($chatId, wm_mode_list($current));
        } elseif (!$isAdmin) {
            tg_send($chatId, "🔒 เปลี่ยนโหมดได้เฉพาะแอดมินเท่านั้น (โหมดกลางกระทบทุกคน)\nดูโหมดปัจจุบันได้ที่ /mode");
        } else {
            $target = wm_resolve_mode($arg);
            if (!$target) {
                tg_send($chatId, "❓ ไม่รู้จักโหมด \"$arg\"\n" . wm_mode_list($current));
            } else {
                $name = wm_config()['modes'][$target]['name'];
                wm_set_mode($target);
                tg_send($chatId, "✅ เปลี่ยนเป็นโหมด: $name\nมีผลทันทีทั้งเว็บ + แจ้งเตือน\n\n" . tg_fmt_activity($latest()));
            }
        }
    } elseif ($cmd === '/stop' || $cmd === '/unsubscribe') {
        db()->prepare("DELETE FROM tg_subscribers WHERE chat_id=?")->execute([(string)$chatId]);
        tg_send($chatId, "🛑 ยกเลิกรับแจ้งเตือนแล้ว (พิมพ์ /start เพื่อสมัครใหม่ได้ทุกเมื่อ)");
    } elseif ($cmd === '/help' || $cmd === '/menu') {
        tg_send($chatId, "🤖 คำสั่งทุ่น $device:\n/swim – ทำกิจกรรมได้ไหม (สถานะรวมตามโหมด)\n/status – ค่าน้ำล่าสุดทุกค่า (แยกสี)\n/mode – ดู/เปลี่ยนโหมดมาตรฐาน (6 ประเภท)\n/start – สมัครรับแจ้งเตือน\n/stop – ยกเลิก\n/help – เมนูนี้\n\n(ระบบเด้งเตือนเองเมื่อคุณภาพน้ำตกเกณฑ์ \"แดง\" ของโหมดที่เลือก)");
    }
} catch (Throwable $e) {
    error_log('tg-webhook: ' . $e->getMessage());
}
echo 'ok';   // ตอบ 200 เสมอ ไม่งั้น Telegram จะ retry รัวๆ
