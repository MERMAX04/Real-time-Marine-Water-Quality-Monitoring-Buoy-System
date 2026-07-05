<?php
/* =========================================================================
   config.php  —  ตั้งค่าการเชื่อมต่อฐานข้อมูล + คีย์ความปลอดภัย
   *** แก้ค่าตรงนี้ให้ตรงกับ server ของอาจารย์ ***
   ========================================================================= */

// --- ข้อมูลฐานข้อมูล MySQL (ขอจากอาจารย์/ผู้ดูแล server) ---
define('DB_HOST', 'localhost');       // ปกติ localhost ถ้ารันบน server เดียวกัน
define('DB_NAME', 'buoy');            // ชื่อฐานข้อมูล (สร้างตาม db.sql)
define('DB_USER', 'buoy_user');       // ชื่อผู้ใช้ DB
define('DB_PASS', 'CHANGE_ME');       // รหัสผ่าน DB

// --- API key กันคนอื่นยิงข้อมูลมั่ว (ESP32 ต้องส่งค่านี้มาด้วย) ---
define('API_KEY', 'buoy-secret-2026'); // เปลี่ยนเป็นค่าลับของคุณเอง

// --- เชื่อมต่อ DB (ไม่ต้องแก้) ---
function db() {
    static $pdo = null;
    if ($pdo === null) {
        $dsn = "mysql:host=" . DB_HOST . ";dbname=" . DB_NAME . ";charset=utf8mb4";
        $pdo = new PDO($dsn, DB_USER, DB_PASS, [
            PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
        ]);
    }
    return $pdo;
}

// อนุญาตให้ dashboard (คนละที่อยู่) เรียก API ได้
header('Access-Control-Allow-Origin: *');
header('Content-Type: application/json; charset=utf-8');
