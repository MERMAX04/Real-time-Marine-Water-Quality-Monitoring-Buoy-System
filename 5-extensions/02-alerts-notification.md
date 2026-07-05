# 02 — แจ้งเตือนออกนอกจอ (Telegram / LINE / อีเมล)

เด้งเตือนเข้ามือถือเมื่อค่าน้ำเข้าขั้น **วิกฤต** — ตรงกับเป้าหมาย "ผ่อนแรงคนเฝ้า" ที่สุด
(ไม่ต้องนั่งจ้องจอ ระบบเตือนเองเมื่อมีปัญหา)

## เลือกช่องทางไหนดี?
| ช่องทาง | ข้อดี | ข้อเสีย |
|---------|-------|---------|
| **Telegram Bot** ⭐ แนะนำ | ฟรี, ตั้งค่า 5 นาที, ยิงข้อความง่ายสุด | ต้องมีแอป Telegram |
| **LINE Messaging API** | คนไทยใช้ LINE เยอะ | ตั้งค่ายุ่งกว่า *(หมายเหตุ: LINE Notify ปิดบริการแล้วตั้งแต่ มี.ค. 2025 ใช้ Messaging API แทน)* |
| **อีเมล (SMTP)** | ไม่ต้องลงแอปเพิ่ม | อาจตกไปอยู่ junk, ช้ากว่า |

## จุดที่ควรตรวจ+ส่ง: ฝั่ง server (save.php) ไม่ใช่ Dashboard
เพราะ Dashboard เตือนได้เฉพาะตอนเปิดหน้าเว็บอยู่ แต่ server รับข้อมูลจาก ESP32 ตลอด →
ให้ **save.php ตรวจทุกครั้งที่รับค่าใหม่** ถ้าเกินเกณฑ์ค่อยส่งเตือน

## ตั้งค่า Telegram Bot (ครั้งเดียว)
1. ในแอป Telegram ทักหา **@BotFather** → พิมพ์ `/newbot` → ตั้งชื่อ → ได้ **BOT_TOKEN**
2. ทักบอทที่เพิ่งสร้าง แล้วเปิด `https://api.telegram.org/bot<TOKEN>/getUpdates` เพื่อหา **CHAT_ID** ของคุณ

## โค้ดเพิ่มใน `4-server-backup/api/save.php` (ต่อท้ายก่อน echo ตอบกลับ)
```php
// ---- ตรวจเกณฑ์วิกฤตแล้วเตือน Telegram ----
$TG_TOKEN = 'ใส่_BOT_TOKEN';
$TG_CHAT  = 'ใส่_CHAT_ID';

// เกณฑ์วิกฤต (ปรับตามมาตรฐาน) : [ชื่อ, ค่า, เงื่อนไข, ข้อความ]
$alerts = [];
$do = num($in,'do');  if ($do !== null && $do < 3)   $alerts[] = "⚠️ ออกซิเจนละลายน้ำต่ำมาก: {$do} mg/L";
$oil= num($in,'oil'); if ($oil!== null && $oil > 20) $alerts[] = "🛢️ พบน้ำมันสูง: {$oil} ppb";
$alg= num($in,'algae');if($alg!== null && $alg > 11) $alerts[] = "🦠 สาหร่ายสีเขียวสูง: {$alg} µg/L";

if ($alerts) {
    $msg = "🌊 แจ้งเตือนคุณภาพน้ำ (buoy-01)\n" . implode("\n", $alerts);
    $url = "https://api.telegram.org/bot$TG_TOKEN/sendMessage";
    $ch = curl_init($url);
    curl_setopt_array($ch, [
        CURLOPT_RETURNTRANSFER => true, CURLOPT_POST => true,
        CURLOPT_POSTFIELDS => ['chat_id' => $TG_CHAT, 'text' => $msg],
    ]);
    curl_exec($ch); curl_close($ch);
}
```

## กัน "เตือนซ้ำรัวๆ"
ค่าที่วิกฤตอาจส่งมาทุกนาที → ควรกันไม่ให้เตือนถี่เกิน เช่น เตือนซ้ำได้ทุก 30 นาที
วิธีง่าย: จำเวลาเตือนล่าสุดของแต่ละค่าไว้ (ในไฟล์ หรือคอลัมน์ DB) แล้วเช็คก่อนส่ง

## ทำฝั่ง ESP32 แทนได้ไหม?
ได้ ถ้าไม่อยากพึ่ง server — ให้ ESP32 ยิง Telegram ตรงเมื่อค่าเกินเกณฑ์
(ใช้ HTTPClient ยิง URL เดียวกัน) แต่ทำที่ server จัดการง่ายกว่าและปรับเกณฑ์ได้โดยไม่ต้อง flash ใหม่
