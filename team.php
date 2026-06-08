<?php
session_start();

function load_json($file, $default) {
    if (!file_exists($file)) file_put_contents($file, json_encode($default, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE));
    $data = json_decode(file_get_contents($file), true);
    return $data ?: $default;
}

$users    = load_json("users.json", []);
$settings = load_json("settings.json", ["site_name"=>"Rteam — Into the Code","accent"=>"#ff2a2a"]);
$accent   = $settings["accent"] ?? "#ff2a2a";

$team = [];
foreach ($users as $login => $u) {
    $role = $u["role"] ?? "Пользователь";
    if ($role !== "Пользователь") {
        $team[] = ["login"=>$login,"role"=>$role];
    }
}
?>
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<title>Команда — <?=htmlspecialchars($settings["site_name"])?></title>
<style>
:root {
    --accent: <?=$accent?>;
    --bg: #050509;
    --card: #101018;
    --text: #e5e5e5;
    --soft: #aaaaaa;
}
* { box-sizing: border-box; }
body {
    margin: 0;
    font-family: "Segoe UI", Arial, sans-serif;
    background: radial-gradient(circle at top, #1a0000 0, #050509 45%, #000 100%);
    color: var(--text);
}
header {
    padding: 14px 40px;
    border-bottom: 1px solid #2a0000;
    background: rgba(5,5,10,0.95);
}
.logo {
    font-size: 24px;
    font-weight: 700;
    color: var(--accent);
    text-shadow: 0 0 12px #ff000066;
}
a { color:#ff7777; text-decoration:none; }
main {
    max-width: 900px;
    margin: 30px auto 50px;
    padding: 0 16px;
}
h1 {
    font-size: 32px;
    color: var(--accent);
    margin-bottom: 10px;
}
p {
    color: var(--soft);
    max-width: 700px;
}
.grid {
    display: grid;
    grid-template-columns: repeat(auto-fit,minmax(220px,1fr));
    gap: 16px;
    margin-top: 20px;
}
.card {
    background: var(--card);
    border: 1px solid #2a0000;
    border-radius: 12px;
    padding: 14px;
    box-shadow: 0 0 18px #ff000022;
}
.card h3 {
    margin: 0 0 6px;
    color: var(--accent);
}
.role {
    font-size: 13px;
    color: #ccc;
}
</style>
</head>
<body>
<header>
    <div class="logo">RTEAM — Команда</div>
</header>
<main>
    <a href="index.php">← На главную</a>
    <h1>Команда</h1>
    <p>Люди, которые двигают проекты вперёд. Здесь только те, кто уже внутри ядра Rteam.</p>

    <?php if (!$team): ?>
        <p>Команда пока не сформирована.</p>
    <?php else: ?>
        <div class="grid">
            <?php foreach ($team as $m): ?>
                <div class="card">
                    <h3><?=htmlspecialchars($m["login"])?></h3>
                    <div class="role"><?=htmlspecialchars($m["role"])?></div>
                </div>
            <?php endforeach; ?>
        </div>
    <?php endif; ?>
</main>
</body>
</html>
