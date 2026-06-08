<?php
session_start();

function load_json($file,$default){
    if(!file_exists($file)) file_put_contents($file,json_encode($default,JSON_PRETTY_PRINT|JSON_UNESCAPED_UNICODE));
    $data = json_decode(file_get_contents($file),true);
    return $data ?: $default;
}
function save_json($file,$data){
    file_put_contents($file,json_encode($data,JSON_PRETTY_PRINT|JSON_UNESCAPED_UNICODE));
}

$name  = trim($_POST["name"]  ?? "");
$email = trim($_POST["email"] ?? "");
$text  = trim($_POST["text"]  ?? "");

if($name==="" || $email==="" || $text===""){
    echo "<script>alert('Заполните все поля.');history.back();</script>";
    exit;
}

$messages = load_json("messages.json",[]);
$messages[] = [
    "id"     => time(),
    "name"   => $name,
    "email"  => $email,
    "text"   => $text,
    "time"   => date("Y-m-d H:i:s"),
    "status" => "new"   // new / read
];
save_json("messages.json",$messages);

$logs = load_json("logs.json",[]);
$logs[] = [
    "time" => date("Y-m-d H:i:s"),
    "type" => "message_new",
    "msg"  => "Новое сообщение от $name ($email)"
];
save_json("logs.json",$logs);

echo "<script>alert('Сообщение отправлено! Ответ придёт на email.');location.href='contact.php';</script>";
