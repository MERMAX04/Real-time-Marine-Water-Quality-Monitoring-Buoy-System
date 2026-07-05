-- =========================================================================
-- db.sql  —  สร้างฐานข้อมูลและตารางเก็บค่าจากทุ่น
-- รันครั้งเดียวใน phpMyAdmin หรือ:  mysql -u root -p < db.sql
-- =========================================================================

CREATE DATABASE IF NOT EXISTS buoy CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE buoy;

CREATE TABLE IF NOT EXISTS readings (
    id        BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    device    VARCHAR(32)  NOT NULL DEFAULT 'buoy-01',  -- เผื่อมีหลายทุ่นในอนาคต
    ts        DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,

    -- ค่าที่วัดได้ทั้ง 7 ตัว (NULL ได้ เผื่อ sensor บางตัวยังไม่พร้อม)
    do_val    FLOAT NULL,   -- ออกซิเจนละลายน้ำ (mg/L)
    sal       FLOAT NULL,   -- ความเค็ม (ppt)
    turb      FLOAT NULL,   -- ความขุ่น (NTU)
    chl       FLOAT NULL,   -- คลอโรฟิลล์ (µg/L)
    orp       FLOAT NULL,   -- ศักย์ออกซิเดชัน (mV)
    oil       FLOAT NULL,   -- น้ำมัน (ppb)
    algae     FLOAT NULL,   -- สาหร่ายสีเขียว (µg/L)

    INDEX idx_ts (ts),
    INDEX idx_device_ts (device, ts)
);

-- ผู้ใช้เฉพาะสำหรับแอป (ปลอดภัยกว่าใช้ root) — แก้รหัสผ่านให้ตรง config.php
-- CREATE USER 'buoy_user'@'localhost' IDENTIFIED BY 'CHANGE_ME';
-- GRANT SELECT, INSERT ON buoy.* TO 'buoy_user'@'localhost';
-- FLUSH PRIVILEGES;
