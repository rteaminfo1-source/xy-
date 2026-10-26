<?php
/* ==========================================================
   УВЕДОМЛЕНИЯ PLATEGA ОБ ОПЛАТЕ СЧЕТОВ ЗА ЗАКАЗЫ САЙТОВ.
   Этот адрес указывается в кабинете Platega как callback (его же
   показывает админка: «Заказы сайтов» → «Настройки» → Platega).

   Телу запроса мы не верим на слово: по присланному ID платежа сами
   спрашиваем Platega (своими ключами), оплачен ли он, — подделать
   оплату, отправив сюда запрос, не получится.
   ========================================================== */
require_once __DIR__ . '/_orders_lib.php';

header('Content-Type: text/plain; charset=utf-8');

$raw = (string)file_get_contents('php://input');
$data = json_decode($raw, true);
if (!is_array($data)) $data = $_POST;

$cfg = so_config();
if (!so_platega_ready($cfg)) { http_response_code(503); echo 'not configured'; exit; }

// Если Platega прислала свои заголовки — они должны совпадать с нашими ключами.
$hdr_id  = (string)($_SERVER['HTTP_X_MERCHANTID'] ?? '');
$hdr_sec = (string)($_SERVER['HTTP_X_SECRET'] ?? '');
if ($hdr_id !== '' && !hash_equals(trim((string)$cfg['platega_id']), $hdr_id)) { http_response_code(403); echo 'bad merchant'; exit; }
if ($hdr_sec !== '' && !hash_equals(trim((string)$cfg['platega_secret']), $hdr_sec)) { http_response_code(403); echo 'bad secret'; exit; }

$tx = (string)($data['id'] ?? $data['transactionId'] ?? '');
$payload = (string)($data['payload'] ?? '');

// payload мы сами передали при создании счёта: «ord:<номер заказа>:<номер счёта>»
$order_id = 0; $inv_id = '';
if (preg_match('/^ord:(\d+):([A-Za-z0-9]+)$/', $payload, $m)) { $order_id = (int)$m[1]; $inv_id = $m[2]; }

// Запасной путь: ищем счёт по ID платежа
if (!$order_id && $tx !== '') {
    foreach (so_order_ids() as $id) {
        $o = so_order_load($id);
        foreach ($o['invoices'] ?? [] as $inv) if (($inv['tx'] ?? '') === $tx) { $order_id = $id; $inv_id = (string)$inv['id']; break 2; }
    }
}

$log = date('Y-m-d H:i:s') . ' tx=' . $tx . ' payload=' . $payload . ' status=' . (string)($data['status'] ?? '') . "\n";
so_tx('live/callback_log', [], function (&$d) use ($log) { $d[] = $log; if (count($d) > 200) $d = array_slice($d, -200); return true; });

if (!$order_id || $inv_id === '') { echo 'OK'; exit; }  // не наш платёж — просто подтверждаем получение

$o = so_order_load($order_id);
$i = $o ? so_find_invoice($o, $inv_id) : null;
if ($i === null || ($tx !== '' && ($o['invoices'][$i]['tx'] ?? '') !== $tx)) { echo 'OK'; exit; }

so_invoice_check($order_id, $inv_id, 'Platega');
echo 'OK';
