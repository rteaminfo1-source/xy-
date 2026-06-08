<?php
header('Content-Type: application/json');

// Твои данные
$shop_id = 'd81f649a-ae48-4856-a335-57eb06eafa75'; 
$secret_key = 'DTUX35QPUZV3RGjhGdVpEAaJIzvP69MFF3OTzpT9n5l8FYBnL75JP4iHKhI9yGJfeQFg0iVx72CRwQM7Ynj035MPrXaDZtDusffX'; 

/**
 * ВНИМАНИЕ: Если здесь 404, значит URL нужно взять ТОЧНО ТАКОЙ, 
 * как указан в разделе "API Docs" твоего личного кабинета Platega.
 */
$api_url = 'https://app.platega.io/v2/transaction/process'; 

// Данные заказа
$amount = 300.00;
$order_id = 'ord_' . time();

$post_data = [
    'merchant_id' => $shop_id,
    'amount'      => $amount,
    'currency'    => 'RUB',
    'order_id'    => $order_id,
    'description' => 'Подписка PLUS'
];

$json_payload = json_encode($post_data);

$ch = curl_init($api_url);
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
curl_setopt($ch, CURLOPT_POST, true);
curl_setopt($ch, CURLOPT_POSTFIELDS, $json_payload);
curl_setopt($ch, CURLOPT_HTTPHEADER, [
    'Content-Type: application/json',
    'X-MerchantId: ' . $shop_id,
    'X-Secret: ' . $secret_key
]);
curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false);
curl_setopt($ch, CURLOPT_HEADER, true); // Включаем заголовки для отладки

$response = curl_exec($ch);
$header_size = curl_getinfo($ch, CURLINFO_HEADER_SIZE);
$http_code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
$header = substr($response, 0, $header_size);
$body = substr($response, $header_size);
$error = curl_error($ch);
curl_close($ch);

// Вывод результата для тебя
echo json_encode([
    'debug_url' => $api_url,
    'http_code' => $http_code,
    'response_body' => $body,
    'curl_error' => $error
]);
?>