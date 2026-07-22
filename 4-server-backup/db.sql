-- =========================================================================
-- db.sql  —  สร้างฐานข้อมูลและตารางเก็บค่าจากทุ่น + ตาราง Telegram
-- รันครั้งเดียวใน phpMyAdmin หรือ:  mysql -u root -p < db.sql
-- =========================================================================

CREATE DATABASE IF NOT EXISTS buoy CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE buoy;

-- ค่าที่วัดได้ (คอลัมน์ตรงกับที่ ESP32 ส่ง = เท่ากับฝั่ง Supabase)
CREATE TABLE IF NOT EXISTS readings (
    id        BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    device    VARCHAR(32)  NOT NULL DEFAULT 'buoy-01',   -- เผื่อมีหลายทุ่นในอนาคต
    ts        DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,

    do_val    FLOAT  NULL,   -- ออกซิเจนละลายน้ำ (mg/L)
    do_pct    FLOAT  NULL,   -- ออกซิเจน (อิ่มตัว %)
    temp      FLOAT  NULL,   -- อุณหภูมิ (°C)
    ph        FLOAT  NULL,   -- ความเป็นกรด-ด่าง
    sal       FLOAT  NULL,   -- ความเค็ม (ppt)
    `cond`    FLOAT  NULL,   -- การนำไฟฟ้า (mS/cm)  (backtick กันชนคำสงวน)
    tds       FLOAT  NULL,   -- สารละลายรวม
    turb      FLOAT  NULL,   -- ความขุ่น (NTU)
    chl       FLOAT  NULL,   -- คลอโรฟิลล์ (µg/L)
    orp       FLOAT  NULL,   -- ศักย์ออกซิเดชัน (mV)
    oil       FLOAT  NULL,   -- น้ำมัน
    algae     FLOAT  NULL,   -- สาหร่ายสีเขียว
    lat       DOUBLE NULL,   -- พิกัด GPS
    lon       DOUBLE NULL,

    INDEX idx_ts (ts),
    INDEX idx_device_ts (device, ts)
);

-- รายชื่อ chat_id ผู้รับแจ้งเตือน (คนที่กด /start) — แทน NVS บน ESP32
CREATE TABLE IF NOT EXISTS tg_subscribers (
    chat_id    VARCHAR(32) PRIMARY KEY,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- เวลาแจ้งเตือนล่าสุดต่อชนิด (กันเตือนซ้ำ = cooldown)
CREATE TABLE IF NOT EXISTS alert_state (
    k          VARCHAR(64) PRIMARY KEY,   -- เช่น 'buoy-01:do', 'buoy-01:ph_low'
    last_sent  DATETIME
);

-- ผู้ใช้เฉพาะสำหรับแอป (ปลอดภัยกว่าใช้ root) — แก้รหัสผ่านให้ตรง config.php
-- CREATE USER 'buoy_user'@'localhost' IDENTIFIED BY 'CHANGE_ME';
-- GRANT SELECT, INSERT, UPDATE, DELETE ON buoy.* TO 'buoy_user'@'localhost';
-- FLUSH PRIVILEGES;
