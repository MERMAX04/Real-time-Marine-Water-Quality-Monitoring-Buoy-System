# 01 — AI ผู้ช่วยภาษาธรรมชาติ (Claude API)

อัปเกรดจาก "สรุปด้วยกฎ" ใน Dashboard v2 ไปเป็นให้ **โมเดล AI จริง (Claude)** เขียนบรีฟและตอบคำถาม
เช่น "ตอนนี้คุณภาพน้ำเป็นยังไง มีอะไรต้องระวัง ควรทำอะไร"

## หลักการ (สำคัญเรื่องความปลอดภัย)
```
Dashboard  --ส่งค่าล่าสุด-->  ai-summary.php (บน server)  --เรียก Claude-->  Anthropic API
           <--ข้อความสรุป----                            <--คำตอบ--------
```
**API key ต้องอยู่ฝั่ง server เท่านั้น** ห้ามใส่ใน Dashboard (โค้ดหน้าเว็บใครก็เปิดดูได้ = key หลุด)
ฉะนั้นต้องมี server (ใช้ `4-server-backup` ที่ทำไว้ได้เลย)

## ขั้นตอน
1. สมัคร Anthropic API key ที่ https://console.anthropic.com (ใส่ค่าใน env ของ server)
2. วางไฟล์ `ai-summary.php` (ด้านล่าง) ไว้ในโฟลเดอร์ `api/` บน server
3. ใน Dashboard เพิ่มปุ่ม "ถาม AI" ที่ยิงมาที่ไฟล์นี้ แล้วเอาคำตอบมาแสดง

## โค้ดเริ่มต้น server: `api/ai-summary.php`
ใช้ cURL เรียกตรง (ไม่ต้องลง SDK) — model = `claude-opus-4-8`
```php
<?php
header('Access-Control-Allow-Origin: *');
header('Content-Type: application/json; charset=utf-8');

$API_KEY = getenv('ANTHROPIC_API_KEY');      // ตั้ง env บน server อย่าฝังในโค้ด
if (!$API_KEY) { http_response_code(500); echo json_encode(['error'=>'no api key']); exit; }

// รับค่าล่าสุดจาก Dashboard (POST JSON) เช่น {"do":6.2,"sal":32,...}
$in = json_decode(file_get_contents('php://input'), true) ?: [];
$readings = json_encode($in, JSON_UNESCAPED_UNICODE);

$prompt = "คุณเป็นผู้ช่วยตรวจสอบคุณภาพน้ำทะเลจากทุ่นตรวจวัด. ".
          "นี่คือค่าล่าสุด (หน่วย: DO=mg/L, ความเค็ม=ppt, ความขุ่น=NTU, คลอโรฟิลล์=µg/L, ".
          "ORP=mV, น้ำมัน=ppb, สาหร่าย=µg/L): $readings. ".
          "สรุปสถานการณ์เป็นภาษาไทยสั้นๆ 2-3 ประโยค บอกว่าค่าไหนน่าห่วง ".
          "และแนะนำสิ่งที่ควรทำ. ตอบเฉพาะข้อความสรุป ไม่ต้องขึ้นต้นด้วยคำทักทาย.";

$payload = json_encode([
  'model' => 'claude-opus-4-8',
  'max_tokens' => 500,
  'messages' => [['role' => 'user', 'content' => $prompt]],
]);

$ch = curl_init('https://api.anthropic.com/v1/messages');
curl_setopt_array($ch, [
  CURLOPT_RETURNTRANSFER => true,
  CURLOPT_POST => true,
  CURLOPT_HTTPHEADER => [
    'content-type: application/json',
    'x-api-key: ' . $API_KEY,
    'anthropic-version: 2023-06-01',
  ],
  CURLOPT_POSTFIELDS => $payload,
]);
$res = curl_exec($ch);
$http = curl_getinfo($ch, CURLINFO_HTTP_CODE);
curl_close($ch);

if ($http !== 200) { http_response_code(502); echo json_encode(['error'=>'api error','detail'=>$res]); exit; }

$data = json_decode($res, true);
$text = $data['content'][0]['text'] ?? '(ไม่มีคำตอบ)';
echo json_encode(['summary' => $text], JSON_UNESCAPED_UNICODE);
```

## ต่อใน Dashboard (ตัวอย่าง)
```js
async function askAI(){
  const latest = {}; PARAMS.forEach(p => latest[p.key] = history[p.key].at(-1));
  const r = await fetch('https://server-อาจารย์.ac.th/buoy/api/ai-summary.php', {
    method:'POST', headers:{'content-type':'application/json'}, body: JSON.stringify(latest)
  });
  const j = await r.json();
  document.getElementById('ai-text').textContent = j.summary;   // เอาไปแสดงในแถบ AI
}
```

## ข้อควรรู้
- Claude API มีค่าใช้จ่ายตามจำนวน token (ถูกมากสำหรับงานเล็กแบบนี้) — เรียกเฉพาะตอนกดปุ่ม ไม่ต้องเรียกทุก 2 วิ
- ถ้าใช้ Firebase (ไม่มี server ของตัวเอง) ต้องใช้ Cloud Functions แทน `ai-summary.php` เพื่อซ่อน key
