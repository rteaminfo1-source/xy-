<?php
$file = 'school_data.json';

// Разрешаем запросы (CORS) на случай тестирования
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Headers: Content-Type");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    exit(0);
}

// Если это POST-запрос — сохраняем новые данные в файл
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $input = file_get_contents('php://input');
    if (!empty($input) && json_decode($input) !== null) {
        file_put_contents($file, $input);
        header('Content-Type: application/json; charset=utf-8');
        echo json_encode(["status" => "success"]);
    } else {
        http_response_code(400);
        header('Content-Type: application/json; charset=utf-8');
        echo json_encode(["status" => "error", "message" => "Неверный формат JSON"]);
    }
    exit;
}

// Если это GET-запрос — читаем данные из файла
if ($_SERVER['REQUEST_METHOD'] === 'GET') {
    header('Content-Type: application/json; charset=utf-8');
    if (file_exists($file)) {
        echo file_get_contents($file);
    } else {
        // Если файла еще нет, отдаем базовую пустую структуру дневника
        echo json_encode([
            "teachers" => [],
            "remarks" => (object)[] // Приводится к {} в JSON
        ]);
    }
    exit;
}
?>