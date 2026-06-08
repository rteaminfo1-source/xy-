<?php
session_start();

function load($f,$d){return file_exists($f)?json_decode(file_get_contents($f),true):$d;}
function save($f,$d){file_put_contents($f,json_encode($d,JSON_PRETTY_PRINT|JSON_UNESCAPED_UNICODE));}

$id       = $_POST["id"] ?? "";
$decision = $_POST["decision"] ?? "";
$comment  = trim($_POST["comment"] ?? "");

if ($id==="" || !in_array($decision,["accept","decline"])) {
    echo "<script>alert('Ошибка данных');history.back();</script>";
    exit;
}

$apps = load("applications.json", []);
$new  = [];
$found = null;

foreach ($apps as $app) {
    if ((string)$app["id"] === (string)$id) {
        $found = $app;
    } else {
        $new[] = $app; // ← заявка удаляется
    }
}

if (!$found) {
    echo "<script>alert('Заявка не найдена');history.back();</script>";
    exit;
}

$email = "";
$name  = "";

foreach ($found["answers"] as $row) {
    if (stripos($row["q"], "email") !== false) $email = $row["a"];
    if (stripos($row["q"], "Ник")   !== false) $name  = $row["a"];
}

$subject = "Решение по вашей заявке в Rteam";
$body = "Здравствуйте, $name!\n\n";

if ($decision === "accept") $body .= "Ваша заявка одобрена.\n\n";
else                        $body .= "Ваша заявка отклонена.\n\n";

if ($comment) $body .= "Комментарий:\n$comment\n\n";

$body .= "С уважением,\nRteam\n";

@mail($email, $subject, $body, "From: noreply@rteam.local\r\n");

save("applications.json", $new);

echo "<script>alert('Решение отправлено. Заявка удалена.');location.href='admin.php?tab=apps';</script>";
