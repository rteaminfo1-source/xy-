<?php
session_start();

function load_json($file, $default) {
    if (!file_exists($file)) file_put_contents($file, json_encode($default, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE));
    $data = json_decode(file_get_contents($file), true);
    return $data ?: $default;
}
function save_json($file, $data) {
    file_put_contents($file, json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE));
}

$type = $_POST["type"] ?? "";
if ($type === "") {
    echo "<script>alert('Тип заявки не указан.');history.back();</script>";
    exit;
}

$questions = load_json("questions.json", []);
$list = $type === "Администратор" ? ($questions["admin"] ?? []) : ($questions["team"] ?? []);

if (!$list) {
    echo "<script>alert('Вопросы не настроены.');history.back();</script>";
    exit;
}

$answers = [];
foreach ($list as $i => $q) {
    $val = trim($_POST["field".$i] ?? "");
    if ($val === "") {
        echo "<script>alert('Заполните все поля.');history.back();</script>";
        exit;
    }
    $answers[] = ["q"=>$q,"a"=>$val];
}

$email = "";
$name  = "";
foreach ($answers as $row) {
    if (mb_stripos($row["q"], "email") !== false) $email = $row["a"];
    if (mb_stripos($row["q"], "Ник")   !== false) $name  = $row["a"];
}

$applications = load_json("applications.json", []);
$applications[] = [
    "id"      => time(),
    "type"    => $type,
    "answers" => $answers,
    "time"    => date("Y-m-d H:i:s"),
    "status"  => "new"
];
save_json("applications.json", $applications);

$logs = load_json("logs.json", []);
$logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"apply","msg"=>"Новая заявка ($type) от $name ($email)"];
save_json("logs.json", $logs);

echo "<script>alert('Заявка отправлена! Решение придёт на email.');location.href='index.php#join';</script>";
