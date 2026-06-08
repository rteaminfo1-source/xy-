<?php
// Сюда Platega присылает данные об оплате
$data = file_get_contents('php://input');
$json = json_decode($data, true);

// Логируем запрос для проверки
file_put_contents('callback_log.txt', print_r($json, true), FILE_APPEND);

if ($json['status'] == 'paid') {
    // Тут код для обновления статуса в БД:
    // UPDATE orders SET status = 'paid' WHERE id = ...
    echo "OK"; // Обязательно ответь им "OK", чтобы они перестали присылать уведомления
}
?>