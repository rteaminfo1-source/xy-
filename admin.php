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

$role = $_SESSION["role"] ?? "Гость";
$user = $_SESSION["user"] ?? "Гость";

if ($role !== "Главный разработчик" && $role !== "Администратор"&& $role !== "Кодер"&& $role !== "Тестер"&& $role !== "Разработчик"&& $role !== "Главный кодер"&& $role !== "Руководитель") {
    echo "Доступ запрещён.";
    exit;
}

$tab = $_GET["tab"] ?? "apps";

$applications = load_json("applications.json", []);
$users        = load_json("users.json", []);
$leaks        = load_json("leaks.json", []);
$settings     = load_json("settings.json", [
    "site_name"=>"Rteam — Into the Code",
    "accent"=>"#ff2a2a",
    "neon"=>true,
    "animations"=>true,
    "recruit_open"=>true
]);
$logs         = load_json("logs.json", []);
$messages     = load_json("messages.json", []);
$blog         = load_json("blog.json", []);
$questions    = load_json("questions.json", [
    "team" => [
        "Ник","Email","Возраст","Навыки","Почему хотите в команду","Опыт","Discord"
    ],
    "admin" => [
        "Ник","Email","Возраст","Опыт модерации / управления","Какие проекты модерировали","Почему хотите быть администратором","Готовность быть активным (да/нет)","Discord"
    ]
]);

/* ПОИСК И СОРТИРОВКА ДЛЯ ЗАЯВОК */
$search = trim($_GET["search"] ?? "");
$sort   = $_GET["sort"] ?? "newest";

if ($tab === "apps") {
    if ($search !== "") {
        $s = mb_strtolower($search);
        $applications = array_filter($applications, function($app) use ($s) {
            $email = "";
            $nick  = "";
            foreach ($app["answers"] as $row) {
                if (mb_stripos($row["q"], "email") !== false) $email = $row["a"];
                if (mb_stripos($row["q"], "Ник")   !== false) $nick  = $row["a"];
            }
            $hay = mb_strtolower($email." ".$nick);
            return mb_strpos($hay, $s) !== false;
        });
    }

    usort($applications, function($a, $b) use ($sort) {
        if ($sort === "oldest") {
            return $a["id"] <=> $b["id"];
        }
        if ($sort === "status") {
            $order = [
                "new"              => 0,
                "viewed"           => 1,
                "resolved_accept"  => 2,
                "resolved_decline" => 3
            ];
            $sa = $order[$a["status"] ?? "new"] ?? 99;
            $sb = $order[$b["status"] ?? "new"] ?? 99;
            if ($sa === $sb) return $b["id"] <=> $a["id"];
            return $sa <=> $sb;
        }
        return $b["id"] <=> $a["id"];
    });

    if ($_SERVER["REQUEST_METHOD"] === "POST" && ($_POST["action"] ?? "") === "mark_viewed") {
        $id = $_POST["id"] ?? "";
        $all = load_json("applications.json", []);
        foreach ($all as &$app) {
            if ((string)$app["id"] === (string)$id && ($app["status"] ?? "new") === "new") {
                $app["status"] = "viewed";
                break;
            }
        }
        unset($app);
        save_json("applications.json", $all);
        header("Location: admin.php?tab=apps&search=".urlencode($search)."&sort=".urlencode($sort));
        exit;
    }
}

/* POST ДЛЯ ОСТАЛЬНОГО (СЛИВЫ, ПОЛЬЗОВАТЕЛИ, НАСТРОЙКИ, КОМАНДА, БЛОГ, СООБЩЕНИЯ, НАБОР) */
if ($_SERVER["REQUEST_METHOD"] === "POST" && $tab !== "apps") {

    if ($_POST["action"] === "save_recruit") {
        $settings["recruit_open"] = isset($_POST["recruit_open"]);
        save_json("settings.json", $settings);

        $team_raw  = trim($_POST["team_questions"] ?? "");
        $admin_raw = trim($_POST["admin_questions"] ?? "");

        $team_lines  = array_values(array_filter(array_map("trim", explode("\n", $team_raw))));
        $admin_lines = array_values(array_filter(array_map("trim", explode("\n", $admin_raw))));

        if (!$team_lines) {
            $team_lines = [
                "Ник",
                "Email",
                "Возраст",
                "Навыки",
                "Почему хотите в команду",
                "Опыт",
                "Discord"
            ];
        }

        if (!$admin_lines) {
            $admin_lines = [
                "Ник",
                "Email",
                "Возраст",
                "Опыт модерации / управления",
                "Какие проекты модерировали",
                "Почему хотите быть администратором",
                "Готовность быть активным (да/нет)",
                "Discord"
            ];
        }

        $questions = [
            "team"  => $team_lines,
            "admin" => $admin_lines
        ];
        save_json("questions.json", $questions);

        $logs[] = [
            "time" => date("Y-m-d H:i:s"),
            "type" => "recruit",
            "msg"  => "Обновлены вопросы и статус набора"
        ];
        save_json("logs.json", $logs);

        header("Location: admin.php?tab=recruit");
        exit;
    }

    if ($_POST["action"] === "add_leak") {
        $title = trim($_POST["title"] ?? "");
        $content = trim($_POST["content"] ?? "");
        if ($title !== "" && $content !== "") {
            $leaks[] = [
                "id" => time(),
                "title" => $title,
                "content" => $content,
                "time" => date("Y-m-d H:i:s"),
                "hidden" => false
            ];
            save_json("leaks.json", $leaks);
            $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"leak_add","msg"=>"Добавлен слив: $title"];
            save_json("logs.json", $logs);
        }
        header("Location: admin.php?tab=leaks");
        exit;
    }
    if ($_POST["action"] === "edit_leak") {
        $id = $_POST["id"];
        $title = trim($_POST["title"] ?? "");
        $content = trim($_POST["content"] ?? "");
        foreach ($leaks as &$l) {
            if ((string)$l["id"] === (string)$id) {
                $l["title"] = $title;
                $l["content"] = $content;
                break;
            }
        }
        unset($l);
        save_json("leaks.json", $leaks);
        $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"leak_edit","msg"=>"Изменён слив ID $id"];
        save_json("logs.json", $logs);
        header("Location: admin.php?tab=leaks");
        exit;
    }
    if ($_POST["action"] === "toggle_leak") {
        $id = $_POST["id"];
        foreach ($leaks as &$l) {
            if ((string)$l["id"] === (string)$id) {
                $l["hidden"] = !empty($l["hidden"]) ? false : true;
                break;
            }
        }
        unset($l);
        save_json("leaks.json", $leaks);
        $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"leak_toggle","msg"=>"Переключен слив ID $id"];
        save_json("logs.json", $logs);
        header("Location: admin.php?tab=leaks");
        exit;
    }
    if ($_POST["action"] === "del_leak") {
        $id = $_POST["id"];
        $new = [];
        foreach ($leaks as $l) {
            if ((string)$l["id"] !== (string)$id) $new[] = $l;
        }
        $leaks = $new;
        save_json("leaks.json", $leaks);
        $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"leak_del","msg"=>"Удалён слив ID $id"];
        save_json("logs.json", $logs);
        header("Location: admin.php?tab=leaks");
        exit;
    }

    if ($_POST["action"] === "add_user") {
        $login = trim($_POST["login"] ?? "");
        $pass  = trim($_POST["password"] ?? "");
        $roleN = $_POST["role"] ?? "Пользователь";
        if ($login !== "" && $pass !== "" && !isset($users[$login])) {
            $users[$login] = ["password"=>$pass,"role"=>$roleN];
            save_json("users.json", $users);
            $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"user_add","msg"=>"Добавлен пользователь $login ($roleN)"];
            save_json("logs.json", $logs);
        }
        header("Location: admin.php?tab=users");
        exit;
    }
    if ($_POST["action"] === "set_role") {
        $login = $_POST["login"];
        $roleN = $_POST["role"];
        if (isset($users[$login])) {
            $users[$login]["role"] = $roleN;
            save_json("users.json", $users);
            $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"user_role","msg"=>"Роль $login → $roleN"];
            save_json("logs.json", $logs);
        }
        header("Location: admin.php?tab=users");
        exit;
    }
    if ($_POST["action"] === "set_pass") {
        $login = $_POST["login"];
        $pass  = trim($_POST["password"] ?? "");
        if ($pass !== "" && isset($users[$login])) {
            $users[$login]["password"] = $pass;
            save_json("users.json", $users);
            $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"user_pass","msg"=>"Смена пароля для $login"];
            save_json("logs.json", $logs);
        }
        header("Location: admin.php?tab=users");
        exit;
    }
    if ($_POST["action"] === "del_user") {
        $login = $_POST["login"];
        if (isset($users[$login]) && $login !== "Roma_07b" && $login !== "Petryha") {
            unset($users[$login]);
            save_json("users.json", $users);
            $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"user_del","msg"=>"Удалён пользователь $login"];
            save_json("logs.json", $logs);
        }
        header("Location: admin.php?tab=users");
        exit;
    }

    if ($_POST["action"] === "save_settings") {
        $settings["site_name"]  = trim($_POST["site_name"] ?? $settings["site_name"]);
        $settings["accent"]     = trim($_POST["accent"] ?? $settings["accent"]);
        $settings["neon"]       = isset($_POST["neon"]);
        $settings["animations"] = isset($_POST["animations"]);
        save_json("settings.json", $settings);
        $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"settings","msg"=>"Изменены настройки сайта"];
        save_json("logs.json", $logs);
        header("Location: admin.php?tab=settings");
        exit;
    }

    if ($_POST["action"] === "add_to_team") {
        $login = $_POST["login"];
        $roleN = $_POST["role"];
        if (isset($users[$login])) {
            $users[$login]["role"] = $roleN;
            save_json("users.json", $users);
            $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"team_add","msg"=>"$login добавлен в команду как $roleN"];
            save_json("logs.json", $logs);
        }
        header("Location: admin.php?tab=team");
        exit;
    }
    if ($_POST["action"] === "change_team_role") {
        $login = $_POST["login"];
        $roleN = $_POST["role"];
        if (isset($users[$login])) {
            $users[$login]["role"] = $roleN;
            save_json("users.json", $users);
            $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"team_role","msg"=>"Роль $login изменена на $roleN"];
            save_json("logs.json", $logs);
        }
        header("Location: admin.php?tab=team");
        exit;
    }
    if ($_POST["action"] === "remove_from_team") {
        $login = $_POST["login"];
        if (isset($users[$login])) {
            $users[$login]["role"] = "Пользователь";
            save_json("users.json", $users);
            $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"team_remove","msg"=>"$login удалён из команды"];
            save_json("logs.json", $logs);
        }
        header("Location: admin.php?tab=team");
        exit;
    }

    if ($_POST["action"] === "del_msg") {
        $id = $_POST["id"];
        $new = [];
        foreach ($messages as $m) {
            if ((string)$m["id"] !== (string)$id) $new[] = $m;
        }
        $messages = $new;
        save_json("messages.json", $messages);
        $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"msg_del","msg"=>"Удалено сообщение ID $id"];
        save_json("logs.json", $logs);
        header("Location: admin.php?tab=messages");
        exit;
    }

    if ($_POST["action"] === "add_post") {
        $title = trim($_POST["title"] ?? "");
        $content = trim($_POST["content"] ?? "");
        if ($title !== "" && $content !== "") {
            $blog[] = [
                "id" => time(),
                "title" => $title,
                "content" => $content,
                "date" => date("Y-m-d H:i:s"),
                "hidden" => false
            ];
            save_json("blog.json", $blog);
            $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"blog_add","msg"=>"Добавлен пост: $title"];
            save_json("logs.json", $logs);
        }
        header("Location: admin.php?tab=blog");
        exit;
    }
    if ($_POST["action"] === "edit_post") {
        $id = $_POST["id"];
        $title = trim($_POST["title"] ?? "");
        $content = trim($_POST["content"] ?? "");
        foreach ($blog as &$p) {
            if ((string)$p["id"] === (string)$id) {
                $p["title"] = $title;
                $p["content"] = $content;
                break;
            }
        }
        unset($p);
        save_json("blog.json", $blog);
        $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"blog_edit","msg"=>"Изменён пост ID $id"];
        save_json("logs.json", $logs);
        header("Location: admin.php?tab=blog");
        exit;
    }
    if ($_POST["action"] === "toggle_post") {
        $id = $_POST["id"];
        foreach ($blog as &$p) {
            if ((string)$p["id"] === (string)$id) {
                $p["hidden"] = !empty($p["hidden"]) ? false : true;
                break;
            }
        }
        unset($p);
        save_json("blog.json", $blog);
        $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"blog_toggle","msg"=>"Переключен пост ID $id"];
        save_json("logs.json", $logs);
        header("Location: admin.php?tab=blog");
        exit;
    }
    if ($_POST["action"] === "del_post") {
        $id = $_POST["id"];
        $new = [];
        foreach ($blog as $p) {
            if ((string)$p["id"] !== (string)$id) $new[] = $p;
        }
        $blog = $new;
        save_json("blog.json", $blog);
        $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"blog_del","msg"=>"Удалён пост ID $id"];
        save_json("logs.json", $logs);
        header("Location: admin.php?tab=blog");
        exit;
    }
}

/* СТАТЫ */
$stats = [
    "apps_total"  => count(load_json("applications.json", [])),
    "users_total" => count($users),
    "leaks_total" => count($leaks),
    "logs_total"  => count($logs),
    "msgs_total"  => count($messages),
    "blog_total"  => count($blog)
];
?>
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<title>Админ — Rteam</title>
<style>
body {
    margin: 0;
    font-family: "Segoe UI", Arial, sans-serif;
    background: #050509;
    color: #eee;
}
.wrap {
    max-width: 1100px;
    margin: 30px auto;
    padding: 0 16px 40px;
}
h1 {
    color: #ff2a2a;
    text-shadow: 0 0 12px #ff000066;
    margin-bottom: 10px;
}
a { color:#ff7777; text-decoration:none; }
.tabs {
    display: flex;
    gap: 10px;
    margin: 10px 0 20px;
    flex-wrap: wrap;
}
.tab {
    padding: 8px 14px;
    border-radius: 8px;
    background: #101018;
    border: 1px solid #2a0000;
    font-size: 14px;
}
.tab.active {
    background: #ff2a2a;
    color: #fff;
    box-shadow: 0 0 14px #ff000066;
}
.card {
    background: #101018;
    border: 1px solid #2a0000;
    padding: 14px;
    border-radius: 10px;
    margin-top: 10px;
}
.card h3 {
    margin: 0 0 6px;
    color: #ff2a2a;
}
.meta {
    font-size: 13px;
    color: #aaa;
    margin-bottom: 6px;
}
textarea, input, select {
    width: 100%;
    padding: 8px;
    border-radius: 8px;
    border: 1px solid #333;
    background: #050509;
    color: #fff;
    resize: none;
    font-size: 13px;
    margin-top: 6px;
}
textarea { height: 80px; }
.btn {
    display: inline-block;
    margin-top: 8px;
    padding: 7px 14px;
    border-radius: 8px;
    border: none;
    cursor: pointer;
    font-size: 13px;
}
.ok { background:#1f9d55; color:#fff; }
.no { background:#c53030; color:#fff; }
.gray { background:#2d3748; color:#fff; }
.stat-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit,minmax(160px,1fr));
    gap: 10px;
    margin-top: 10px;
}
.stat {
    background:#101018;
    border:1px solid #2a0000;
    border-radius:10px;
    padding:10px;
    text-align:center;
    font-size:13px;
}
.stat b { font-size:18px; color:#ff2a2a; }

.badge {
    display:inline-block;
    padding:2px 8px;
    border-radius:999px;
    font-size:11px;
    margin-left:6px;
}
.badge-new { background:#2b6cb0; color:#fff; }
.badge-viewed { background:#b7791f; color:#fff; }
.badge-acc { background:#2f855a; color:#fff; }
.badge-dec { background:#c53030; color:#fff; }

.search-bar {
    display:flex;
    gap:8px;
    flex-wrap:wrap;
    margin:10px 0 16px;
}
.search-bar input, .search-bar select {
    width:auto;
}
</style>
</head>
<body>
<div class="wrap">
    <h1>Админ‑панель Rteam</h1>
    <p>Вы вошли как <b><?=htmlspecialchars($user)?></b> (<?=htmlspecialchars($role)?>) — <a href="index.php">← На сайт</a></p>

    <div class="stat-grid">
        <div class="stat">Заявок<br><b><?=$stats["apps_total"]?></b></div>
        <div class="stat">Пользователей<br><b><?=$stats["users_total"]?></b></div>
        <div class="stat">Сливов<br><b><?=$stats["leaks_total"]?></b></div>
        <div class="stat">Постов блога<br><b><?=$stats["blog_total"]?></b></div>
        <div class="stat">Сообщений<br><b><?=$stats["msgs_total"]?></b></div>
        <div class="stat">Логов<br><b><?=$stats["logs_total"]?></b></div>
    </div>

    <div class="tabs">
        <a class="tab <?=$tab==='apps'?'active':''?>" href="?tab=apps">Заявки</a>
        <a class="tab <?=$tab==='leaks'?'active':''?>" href="?tab=leaks">Сливы</a>
        <a class="tab <?=$tab==='blog'?'active':''?>" href="?tab=blog">Блог</a>
        <a class="tab <?=$tab==='messages'?'active':''?>" href="?tab=messages">Сообщения</a>
        <a class="tab <?=$tab==='users'?'active':''?>" href="?tab=users">Пользователи</a>
        <a class="tab <?=$tab==='team'?'active':''?>" href="?tab=team">Команда</a>
        <a class="tab <?=$tab==='recruit'?'active':''?>" href="?tab=recruit">Набор</a>
        <a class="tab <?=$tab==='settings'?'active':''?>" href="?tab=settings">Настройки</a>
        <a class="tab <?=$tab==='logs'?'active':''?>" href="?tab=logs">Логи</a>
    </div>

    <?php if ($tab === "apps"): ?>
        <form class="search-bar" method="GET">
            <input type="hidden" name="tab" value="apps">
            <div>
                <label>Поиск по нику / email</label><br>
                <input type="text" name="search" value="<?=htmlspecialchars($search)?>" placeholder="Ник или email">
            </div>
            <div>
                <label>Сортировка</label><br>
                <select name="sort">
                    <option value="newest" <?=$sort==="newest"?"selected":""?>>Сначала новые</option>
                    <option value="oldest" <?=$sort==="oldest"?"selected":""?>>Сначала старые</option>
                    <option value="status" <?=$sort==="status"?"selected":""?>>По статусу</option>
                </select>
            </div>
            <div style="align-self:flex-end;">
                <button class="btn gray" type="submit">Применить</button>
            </div>
        </form>

        <?php if (!$applications): ?>
            <p>Заявок пока нет.</p>
        <?php else: ?>
            <?php foreach ($applications as $app): ?>
                <?php
                $status = $app["status"] ?? "new";
                $badgeText = "Новая";
                $badgeClass = "badge-new";
                if ($status === "viewed") {
                    $badgeText = "Просмотрена";
                    $badgeClass = "badge-viewed";
                } elseif ($status === "resolved_accept") {
                    $badgeText = "Решена (принята)";
                    $badgeClass = "badge-acc";
                } elseif ($status === "resolved_decline") {
                    $badgeText = "Решена (отказ)";
                    $badgeClass = "badge-dec";
                }

                $email = "";
                $nick  = "";
                foreach ($app["answers"] as $row) {
                    if (mb_stripos($row["q"], "email") !== false) $email = $row["a"];
                    if (mb_stripos($row["q"], "Ник")   !== false) $nick  = $row["a"];
                }
                ?>
                <div class="card">
                    <div class="meta">
                        ID: <?=htmlspecialchars($app["id"])?> |
                        <?=htmlspecialchars($app["type"])?> |
                        <?=htmlspecialchars($app["time"])?>
                        <span class="badge <?=$badgeClass?>"><?=$badgeText?></span>
                    </div>
                    <div class="meta">
                        Ник: <?=htmlspecialchars($nick)?> |
                        Email: <?=htmlspecialchars($email)?>
                    </div>

                    <?php foreach ($app["answers"] as $row): ?>
                        <div class="meta">
                            <b><?=htmlspecialchars($row["q"])?></b><br>
                            <?=nl2br(htmlspecialchars($row["a"]))?>
                        </div>
                    <?php endforeach; ?>

                    <form action="decision.php" method="POST" style="margin-top:8px;">
                        <input type="hidden" name="id" value="<?=htmlspecialchars($app["id"])?>">
                        <textarea name="comment" placeholder="Комментарий кандидату (необязательно)"></textarea>
                        <div style="margin-top:6px;">
                            <button class="btn ok" name="decision" value="accept">Принять</button>
                            <button class="btn no" name="decision" value="decline">Отказать</button>
                        </div>
                    </form>

                    <?php if ($status === "new"): ?>
                        <form method="POST" style="margin-top:6px;">
                            <input type="hidden" name="action" value="mark_viewed">
                            <input type="hidden" name="id" value="<?=htmlspecialchars($app["id"])?>">
                            <button class="btn gray" type="submit">Отметить как просмотренную</button>
                        </form>
                    <?php endif; ?>
                </div>
            <?php endforeach; ?>
        <?php endif; ?>

    <?php elseif ($tab === "leaks"): ?>
        <div class="card">
            <h3>Добавить слив</h3>
            <form method="POST">
                <input type="hidden" name="action" value="add_leak">
                <input type="text" name="title" placeholder="Заголовок" required>
                <textarea name="content" placeholder="Текст / описание" required></textarea>
                <button class="btn gray" type="submit">Сохранить</button>
            </form>
        </div>

        <?php if ($leaks): ?>
            <?php foreach (array_reverse($leaks) as $l): ?>
                <div class="card">
                    <h3><?=htmlspecialchars($l["title"])?></h3>
                    <div class="meta">
                        ID: <?=htmlspecialchars($l["id"])?> | <?=htmlspecialchars($l["time"] ?? "")?> |
                        <?=!empty($l["hidden"]) ? "Скрыт" : "Показан"?>
                    </div>
                    <form method="POST" style="margin-top:6px;">
                        <input type="hidden" name="action" value="edit_leak">
                        <input type="hidden" name="id" value="<?=htmlspecialchars($l["id"])?>">
                        <input type="text" name="title" value="<?=htmlspecialchars($l["title"])?>">
                        <textarea name="content"><?=htmlspecialchars($l["content"])?></textarea>
                        <button class="btn gray" type="submit">Сохранить изменения</button>
                    </form>
                    <form method="POST" style="margin-top:6px;display:inline-block;">
                        <input type="hidden" name="action" value="toggle_leak">
                        <input type="hidden" name="id" value="<?=htmlspecialchars($l["id"])?>">
                        <button class="btn ok" type="submit"><?=!empty($l["hidden"]) ? "Показать" : "Скрыть"?></button>
                    </form>
                    <form method="POST" style="margin-top:6px;display:inline-block;">
                        <input type="hidden" name="action" value="del_leak">
                        <input type="hidden" name="id" value="<?=htmlspecialchars($l["id"])?>">
                        <button class="btn no" type="submit">Удалить</button>
                    </form>
                </div>
            <?php endforeach; ?>
        <?php else: ?>
            <p>Сливов пока нет.</p>
        <?php endif; ?>

    <?php elseif ($tab === "blog"): ?>
        <div class="card">
            <h3>Добавить пост</h3>
            <form method="POST">
                <input type="hidden" name="action" value="add_post">
                <input type="text" name="title" placeholder="Заголовок" required>
                <textarea name="content" placeholder="Текст поста" required></textarea>
                <button class="btn gray" type="submit">Сохранить</button>
            </form>
        </div>

        <?php if ($blog): ?>
            <?php foreach (array_reverse($blog) as $p): ?>
                <div class="card">
                    <h3><?=htmlspecialchars($p["title"])?></h3>
                    <div class="meta">
                        ID: <?=htmlspecialchars($p["id"])?> | <?=htmlspecialchars($p["date"] ?? "")?> |
                        <?=!empty($p["hidden"]) ? "Скрыт" : "Показан"?>
                    </div>
                    <form method="POST" style="margin-top:6px;">
                        <input type="hidden" name="action" value="edit_post">
                        <input type="hidden" name="id" value="<?=htmlspecialchars($p["id"])?>">
                        <input type="text" name="title" value="<?=htmlspecialchars($p["title"])?>">
                        <textarea name="content"><?=htmlspecialchars($p["content"])?></textarea>
                        <button class="btn gray" type="submit">Сохранить изменения</button>
                    </form>
                    <form method="POST" style="margin-top:6px;display:inline-block;">
                        <input type="hidden" name="action" value="toggle_post">
                        <input type="hidden" name="id" value="<?=htmlspecialchars($p["id"])?>">
                        <button class="btn ok" type="submit"><?=!empty($p["hidden"]) ? "Показать" : "Скрыть"?></button>
                    </form>
                    <form method="POST" style="margin-top:6px;display:inline-block;">
                        <input type="hidden" name="action" value="del_post">
                        <input type="hidden" name="id" value="<?=htmlspecialchars($p["id"])?>">
                        <button class="btn no" type="submit">Удалить</button>
                    </form>
                </div>
            <?php endforeach; ?>
        <?php else: ?>
            <p>Постов пока нет.</p>
        <?php endif; ?>

    <?php elseif ($tab === "messages"): ?>
        <?php if (!$messages): ?>
            <p>Сообщений пока нет.</p>
        <?php else: ?>
            <?php foreach (array_reverse($messages) as $m): ?>
                <div class="card">
                    <h3><?=htmlspecialchars($m["name"])?></h3>
                    <div class="meta">
                        Email: <?=htmlspecialchars($m["email"])?> | <?=htmlspecialchars($m["time"] ?? "")?>
                    </div>
                    <div><?=nl2br(htmlspecialchars($m["text"]))?></div>
                    <form method="POST" style="margin-top:6px;">
                        <input type="hidden" name="action" value="del_msg">
                        <input type="hidden" name="id" value="<?=htmlspecialchars($m["id"])?>">
                        <button class="btn no" type="submit">Удалить</button>
                    </form>
                </div>
            <?php endforeach; ?>
        <?php endif; ?>

    <?php elseif ($tab === "users"): ?>
        <div class="card">
            <h3>Добавить пользователя</h3>
            <form method="POST">
                <input type="hidden" name="action" value="add_user">
                <input type="text" name="login" placeholder="Логин" required>
                <input type="password" name="password" placeholder="Пароль" required>
                <select name="role">
                    <option value="Пользователь">Пользователь</option>
                    <option value="Администратор">Администратор</option>
                    <option value="Главный разработчик">Главный разработчик</option>
                </select>
                <button class="btn gray" type="submit">Создать</button>
            </form>
        </div>

        <?php if (!$users): ?>
            <p>Пользователей пока нет.</p>
        <?php else: ?>
            <?php foreach ($users as $login => $u): ?>
                <div class="card">
                    <h3><?=htmlspecialchars($login)?></h3>
                    <div class="meta">Роль: <?=htmlspecialchars($u["role"] ?? "Пользователь")?></div>

                    <form method="POST" style="margin-top:6px;">
                        <input type="hidden" name="action" value="set_role">
                        <input type="hidden" name="login" value="<?=htmlspecialchars($login)?>">
                        <select name="role">
                            <option value="Пользователь" <?=($u["role"]??"")==="Пользователь"?"selected":""?>>Пользователь</option>
                            <option value="Администратор" <?=($u["role"]??"")==="Администратор"?"selected":""?>>Администратор</option>
                            <option value="Главный разработчик" <?=($u["role"]??"")==="Главный разработчик"?"selected":""?>>Главный разработчик</option>
                        </select>
                        <button class="btn gray" type="submit">Сохранить роль</button>
                    </form>

                    <form method="POST" style="margin-top:6px;">
                        <input type="hidden" name="action" value="set_pass">
                        <input type="hidden" name="login" value="<?=htmlspecialchars($login)?>">
                        <input type="password" name="password" placeholder="Новый пароль">
                        <button class="btn gray" type="submit">Сменить пароль</button>
                    </form>

                    <?php if ($login !== "Roma_07b" && $login !== "Petryha"): ?>
                    <form method="POST" style="margin-top:6px;">
                        <input type="hidden" name="action" value="del_user">
                        <input type="hidden" name="login" value="<?=htmlspecialchars($login)?>">
                        <button class="btn no" type="submit">Удалить пользователя</button>
                    </form>
                    <?php endif; ?>
                </div>
            <?php endforeach; ?>
        <?php endif; ?>

    <?php elseif ($tab === "team"): ?>
        <div class="card">
            <h3>Добавить пользователя в команду</h3>
            <form method="POST">
                <input type="hidden" name="action" value="add_to_team">
                <select name="login" required>
                    <option value="">Выберите пользователя</option>
                    <?php foreach ($users as $login => $u): ?>
                        <?php if (($u["role"] ?? "Пользователь") === "Пользователь"): ?>
                            <option value="<?=$login?>"><?=$login?></option>
                        <?php endif; ?>
                    <?php endforeach; ?>
                </select>
                <select name="role" required>
                    <option value="Администратор">Администратор</option>
                    <option value="Главный разработчик">Главный разработчик</option>
                </select>
                <button class="btn gray" type="submit">Добавить в команду</button>
            </form>
        </div>

        <h3 style="margin-top:20px;">Состав команды</h3>
        <?php
        $team = [];
        foreach ($users as $login => $u) {
            if (($u["role"] ?? "Пользователь") !== "Пользователь") {
                $team[] = [$login, $u["role"]];
            }
        }
        ?>
        <?php if (!$team): ?>
            <p>Команда пока пуста.</p>
        <?php else: ?>
            <?php foreach ($team as $member): ?>
                <div class="card">
                    <h3><?=$member[0]?></h3>
                    <div class="meta">Роль: <?=$member[1]?></div>

                    <form method="POST" style="margin-top:6px;">
                        <input type="hidden" name="action" value="change_team_role">
                        <input type="hidden" name="login" value="<?=$member[0]?>">
                        <select name="role">
                            <option value="Администратор" <?=$member[1]==="Администратор"?"selected":""?>>Администратор</option>
                            <option value="Главный разработчик" <?=$member[1]==="Главный разработчик"?"selected":""?>>Главный разработчик</option>
                        </select>
                        <button class="btn gray" type="submit">Сохранить</button>
                    </form>

                    <?php if ($member[0] !== "Roma_07b" && $member[0] !== "Petryha"): ?>
                    <form method="POST" style="margin-top:6px;">
                        <input type="hidden" name="action" value="remove_from_team">
                        <input type="hidden" name="login" value="<?=$member[0]?>">
                        <button class="btn no" type="submit">Удалить из команды</button>
                    </form>
                    <?php endif; ?>
                </div>
            <?php endforeach; ?>
        <?php endif; ?>

    <?php elseif ($tab === "recruit"): ?>
        <div class="card">
            <h3>Набор</h3>
            <form method="POST">
                <input type="hidden" name="action" value="save_recruit">
                <label>
                    <input type="checkbox" name="recruit_open" <?=!empty($settings["recruit_open"])?"checked":""?>>
                    Набор открыт
                </label>

                <h4 style="margin-top:12px;">Вопросы для заявки «Команда» (по одному на строку)</h4>
                <textarea name="team_questions"><?=
                    htmlspecialchars(implode("\n", $questions["team"] ?? []))
                ?></textarea>

                <h4 style="margin-top:12px;">Вопросы для заявки «Администратор» (по одному на строку)</h4>
                <textarea name="admin_questions"><?=
                    htmlspecialchars(implode("\n", $questions["admin"] ?? []))
                ?></textarea>

                <button class="btn gray" type="submit" style="margin-top:10px;">Сохранить</button>
            </form>
        </div>

    <?php elseif ($tab === "settings"): ?>
        <div class="card">
            <h3>Настройки сайта</h3>
            <form method="POST">
                <input type="hidden" name="action" value="save_settings">
                <label>Название сайта</label>
                <input type="text" name="site_name" value="<?=htmlspecialchars($settings["site_name"])?>">
                <label>Цвет акцента (hex)</label>
                <input type="text" name="accent" value="<?=htmlspecialchars($settings["accent"])?>">
                <label><input type="checkbox" name="neon" <?=!empty($settings["neon"])?"checked":""?>> Неон‑эффекты</label>
                <label><input type="checkbox" name="animations" <?=!empty($settings["animations"])?"checked":""?>> Анимации</label>
                <button class="btn gray" type="submit">Сохранить</button>
            </form>
        </div>

    <?php elseif ($tab === "logs"): ?>
        <?php if (!$logs): ?>
            <p>Логов пока нет.</p>
        <?php else: ?>
            <?php foreach (array_reverse($logs) as $log): ?>
                <div class="card">
                    <div class="meta"><?=htmlspecialchars($log["time"] ?? "")?> — <?=htmlspecialchars($log["type"] ?? "")?></div>
                    <div><?=nl2br(htmlspecialchars($log["msg"] ?? ""))?></div>
                </div>
            <?php endforeach; ?>
        <?php endif; ?>

    <?php endif; ?>
</div>
</body>
</html>
