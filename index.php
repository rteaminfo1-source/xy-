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

$users      = load_json("users.json", []);
$settings   = load_json("settings.json", [
    "site_name"    => "Rteam — Into the Code",
    "accent"       => "#ff2a2a",
    "neon"         => true,
    "animations"   => true,
    "recruit_open" => true
]);
$leaks      = load_json("leaks.json", []);
$blog       = load_json("blog.json", []);
$messages   = load_json("messages.json", []);
$questions  = load_json("questions.json", []);
$applications = load_json("applications.json", []);

if (empty($questions["team"]) || empty($questions["admin"])) {
    $questions = [
        "team" => [
            "Ник","Email","Возраст","Навыки","Почему хотите в команду","Опыт","Discord"
        ],
        "admin" => [
            "Ник","Email","Возраст","Опыт модерации / управления","Какие проекты модерировали","Почему хотите быть администратором","Готовность быть активным (да/нет)","Discord"
        ]
    ];
}

$error = "";

/* РЕГИСТРАЦИЯ */
if (isset($_POST["action"]) && $_POST["action"] === "register") {
    $login = trim($_POST["login"]);
    $pass  = trim($_POST["password"]);

    if ($login === "" || $pass === "") {
        $error = "Заполните все поля.";
    } elseif (isset($users[$login])) {
        $error = "Такой логин уже существует.";
    } else {
        $users[$login] = [
            "password" => $pass,
            "role"     => "Пользователь"
        ];
        save_json("users.json", $users);
        $error = "Регистрация успешна. Теперь войдите.";
    }
}

/* ВХОД */
if (isset($_POST["action"]) && $_POST["action"] === "login") {
    $login = trim($_POST["login"]);
    $pass  = trim($_POST["password"]);

    if (isset($users[$login]) && $users[$login]["password"] === $pass) {
        $_SESSION["user"] = $login;
        $_SESSION["role"] = $users[$login]["role"];

        $logs = load_json("logs.json", []);
        $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"login","msg"=>"Вход: $login"];
        save_json("logs.json", $logs);

        header("Location: index.php");
        exit;
    } else {
        $error = "Неверный логин или пароль.";
    }
}

/* ВЫХОД */
if (isset($_GET["logout"])) {
    session_destroy();
    header("Location: index.php");
    exit;
}

$user = $_SESSION["user"] ?? null;
$role = $_SESSION["role"] ?? "Гость";

$accent = $settings["accent"] ?? "#ff2a2a";

$teamQ  = $questions["team"];
$adminQ = $questions["admin"];
?>
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<title>RTeam — Команда программирования и IT-разработки | rteam.info</title>
<meta name="description" content="RTeam — профессиональная команда программирования. Разрабатываем сайты, приложения и сложные IT-решения под ключ">
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
    position: fixed;
    top: 0;
    width: 100%;
    background: rgba(5,5,10,0.95);
    border-bottom: 1px solid #2a0000;
    padding: 12px 40px;
    display: flex;
    justify-content: space-between;
    align-items: center;
    z-index: 1000;
    backdrop-filter: blur(10px);
}
.logo {
    font-size: 24px;
    font-weight: 700;
    color: var(--accent);
    text-shadow: 0 0 12px #ff000066;
}
nav a {
    margin-left: 20px;
    text-decoration: none;
    color: #ccc;
    font-size: 14px;
}
nav a:hover { color: var(--accent); }
.user-info {
    font-size: 13px;
    color: var(--soft);
}
.user-info b { color: var(--accent); }

section {
    padding: 90px 60px 70px;
    min-height: 60vh;
}
h1 {
    font-size: 32px;
    color: var(--accent);
    margin-bottom: 10px;
}
p {
    max-width: 700px;
    color: var(--soft);
    line-height: 1.6;
}

/* БЛОГ */
.blog-post {
    background: var(--card);
    border: 1px solid #2a0000;
    padding: 16px;
    border-radius: 10px;
    margin-top: 15px;
}
.blog-date {
    font-size: 12px;
    color: #888;
    margin-bottom: 4px;
}
.blog-title {
    font-size: 18px;
    color: var(--accent);
    margin-bottom: 6px;
}

/* СЛИВЫ / КАРТОЧКИ */
.card {
    background: var(--card);
    border: 1px solid #2a0000;
    padding: 18px;
    border-radius: 12px;
    margin-top: 15px;
}

/* ФОРМЫ */
.box {
    width: 420px;
    max-width: 100%;
    background: var(--card);
    border: 1px solid #2a0000;
    padding: 20px;
    border-radius: 12px;
    box-shadow: 0 0 20px #ff000022;
}
input, textarea, select {
    width: 100%;
    padding: 10px;
    margin-top: 8px;
    border-radius: 8px;
    border: 1px solid #333;
    background: #050509;
    color: #fff;
    resize: none;
    font-size: 14px;
}
textarea { height: 110px; }

.btn {
    display: inline-block;
    margin-top: 10px;
    padding: 9px 16px;
    background: var(--accent);
    color: #fff;
    border-radius: 8px;
    text-decoration: none;
    border: none;
    cursor: pointer;
    box-shadow: 0 0 12px #ff000055;
}
.btn:hover {
    background: #ff4444;
    box-shadow: 0 0 18px #ff000088;
}

.auth-wrap {
    display: flex;
    gap: 20px;
    flex-wrap: wrap;
    margin-top: 18px;
}
.error {
    color: #ff7777;
    margin-top: 8px;
    font-size: 13px;
}

footer {
    text-align: center;
    padding: 18px;
    background: #050509;
    border-top: 1px solid #2a0000;
    color: #777;
    font-size: 12px;
}

@media (max-width: 768px) {
    header { padding: 10px 16px; }
    section { padding: 80px 16px 60px; }
    h1 { font-size: 26px; }
}

/* Бейджи статусов (для вывода на главной, если нужно) */
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
</style>
</head>
<body>

<header>
    <div class="logo">RTEAM</div>
    <nav>
        <a href="#home">Главная</a>
        <a href="team.php">Команда</a>
        <a href="#blog">Блог</a>
        <a href="#leaks">Сливы</a>
        <a href="#join">Заявка</a>
        <a href="#contact">Контакты</a>
		<a href="forum.html">Форум</a>
		<a href="projects.php">Проекты</a>
		<a href="Тупая игра.html">Игра</a>
		<a href="index2.html">RMain</a>
        <?php if ($role === "Главный разработчик" || $role === "Администратор"|| $role === "Главный Администратор"|| $role === "Тестер"|| $role === "Главный Тестер"|| $role === "Кодер"|| $role === "Главный Кодер"|| $role === "Руководитель"): ?>
            <a href="admin.php">Админ</a>
        <?php endif; ?>
    </nav>
    <div class="user-info">
        <?php if ($user): ?>
            Вход: <b><?=htmlspecialchars($user)?></b> (<?=htmlspecialchars($role)?>) —
            <a href="?logout=1" style="color:#ff7777;">Выйти</a>
        <?php else: ?>
            Гость
        <?php endif; ?>
    </div>
</header>

<!-- ГЛАВНЫЙ ЭКРАН -->
<section id="home">
    <div style="text-align:center; margin-top:40px;">

        <h1 id="dynamicText"
            style="
                font-size:48px;
                font-weight:700;
                color:#ff2a2a;
                text-shadow:0 0 20px #ff0000aa;
                transition:0.6s ease;
            ">
            Загрузка...
        </h1>

        <p style="margin:10px auto 0; max-width:600px;">
            Мы собираем людей,Которые не боятся писать код.
        </p>

        <div style="
            margin:40px auto 0;
            max-width:700px;
            background:#000;
            border:1px solid #ff000033;
            padding:20px;
            border-radius:10px;
            font-family:Consolas, monospace;
            color:#ff4444;
            box-shadow:0 0 25px #ff000022;
            text-align:left;
        ">
            <div style="color:#888;">C:\RTEAM\root></div>
            <div id="cmdLine" style="
                white-space:nowrap;
                overflow:hidden;
                border-right:2px solid #ff4444;
                font-size:16px;
            "></div>
        </div>

    </div>
</section>

<section id="blog">
    <h1>Блог</h1>
    <p>Лента записей от команды.</p>

    <?php if (!$blog): ?>
        <p>Пока нет постов.</p>
    <?php else: ?>
        <?php foreach (array_reverse($blog) as $post): ?>
            <?php if (!empty($post["hidden"])) continue; ?>
            <div class="blog-post">
                <div class="blog-date"><?=htmlspecialchars($post["date"] ?? "")?></div>
                <div class="blog-title"><?=htmlspecialchars($post["title"] ?? "")?></div>
                <div><?=nl2br(htmlspecialchars($post["content"] ?? ""))?></div>
            </div>
        <?php endforeach; ?>
    <?php endif; ?>
</section>

<section id="leaks">
    <h1>Сливы</h1>
    <p>Материалы, опубликованные командой.</p>

    <?php if (!$leaks): ?>
        <p>Пока нет сливов.</p>
    <?php else: ?>
        <?php foreach (array_reverse($leaks) as $l): ?>
            <?php if (!empty($l["hidden"])) continue; ?>
            <div class="card">
                <h3><?=htmlspecialchars($l["title"])?></h3>
                <div style="font-size:12px;color:#888;"><?=htmlspecialchars($l["time"] ?? "")?></div>
                <p><?=nl2br(htmlspecialchars($l["content"] ?? ""))?></p>
            </div>
        <?php endforeach; ?>
    <?php endif; ?>
</section>

<section id="join">
    <h1>Подача заявки</h1>

    <?php if (empty($settings["recruit_open"])): ?>
        <p style="color:#ff5555;font-size:18px;">Набор временно закрыт.</p>
    <?php else: ?>
        <p style="color:#ff4444;font-size:14px;margin-bottom:10px;">
            Решение придёт на email, указанный в заявке.
        </p>

        <div class="box">
            <form action="apply.php" method="POST">
                <select name="type" id="typeSelect" required>
                    <option value="Команда">Вступление в команду</option>
                    <option value="Администратор">Заявка на администратора</option>
                </select>

                <div id="formFields"></div>

                <button class="btn" style="width:100%;margin-top:12px;">Отправить заявку</button>
            </form>
        </div>
    <?php endif; ?>
</section>

<section id="contact">
    <h1>Контакты и вход</h1>

    <div class="auth-wrap">
        <div class="box">
            <h3 style="margin:0 0 8px;color:var(--accent);">Написать нам</h3>
            <form action="send.php" method="POST">
                <input type="text" name="name" placeholder="Ваше имя" required>
                <input type="email" name="email" placeholder="Ваш email" required>
                <textarea name="text" placeholder="Ваш текст..." required></textarea>
                <button class="btn" style="width:100%;margin-top:12px;">Отправить</button>
            </form>
        </div>

        <div class="box">
            <h3 style="margin:0 0 8px;color:var(--accent);">Вход / Регистрация</h3>
            <?php if ($error): ?>
                <div class="error"><?=htmlspecialchars($error)?></div>
            <?php endif; ?>

            <?php if (!$user): ?>
                <form action="" method="POST" style="margin-top:10px;">
                    <input type="hidden" name="action" value="login">
                    <input type="text" name="login" placeholder="Логин" required>
                    <input type="password" name="password" placeholder="Пароль" required>
                    <button class="btn" style="width:100%;margin-top:10px;">Войти</button>
                </form>

                <form action="" method="POST" style="margin-top:12px;">
                    <input type="hidden" name="action" value="register">
                    <input type="text" name="login" placeholder="Новый логин" required>
                    <input type="password" name="password" placeholder="Новый пароль" required>
                    <button class="btn" style="width:100%;margin-top:10px;">Зарегистрироваться</button>
                </form>
            <?php else: ?>
                <p>Вы уже вошли как <b><?=htmlspecialchars($user)?></b>.</p>
            <?php endif; ?>
        </div>
    </div>
</section>

<footer>
    © 2026 Rteam. Все права защищены.
</footer>

<script>
// ДИНАМИЧЕСКИЙ ТЕКСТ
const phrases = [
    "НОВЫЙ ВЗГЛЯД НА РАЗРАБОТКУ",
    "КОМАНДА, КОТОРАЯ ДЕЛАЕТ БОЛЬШЕ",
    "ТЕХНОЛОГИИ, КОТОРЫЕ МЕНЯЮТ ИГРУ",
    "КОД, КОТОРЫЙ ГОВОРИТ ГРОМЧЕ",
    "БУДУЩЕЕ СОЗДАЁТСЯ ЗДЕСЬ",
    "RTEAM — СИЛА В ИДЕЯХ"
];
let index = 0;
const textEl = document.getElementById("dynamicText");

function changePhrase() {
    textEl.style.opacity = 0;
    setTimeout(() => {
        textEl.innerText = phrases[index];
        const colors = ["#ff2a2a","#ff6b00","#ff00c8","#00eaff","#00ff6a"];
        const c = colors[index % colors.length];
        textEl.style.color = c;
        textEl.style.textShadow = `0 0 20px ${c}aa`;
        textEl.style.opacity = 1;
        index = (index + 1) % phrases.length;
    }, 600);
}
setInterval(changePhrase, 3000);
changePhrase();

// CMD ТЕРМИНАЛ
const cmdPhrases = [
    "sudo rm -rf /bugs",
    "loading Rteam modules...",
    "initializing core systems...",
    "checking security layer...",
    "compiling ideas...",
    "system online."
];
let cmdIndex = 0;
let charIndex = 0;
const cmdEl = document.getElementById("cmdLine");

function typeCMD() {
    let text = cmdPhrases[cmdIndex];
    cmdEl.innerText = text.substring(0, charIndex);

    if (charIndex < text.length) {
        charIndex++;
        setTimeout(typeCMD, 60);
    } else {
        setTimeout(() => {
            charIndex = 0;
            cmdIndex = (cmdIndex + 1) % cmdPhrases.length;
            typeCMD();
        }, 1200);
    }
}
typeCMD();

// ДИНАМИЧЕСКАЯ ФОРМА ЗАЯВОК
const teamQ  = <?=json_encode($teamQ,  JSON_UNESCAPED_UNICODE)?>;
const adminQ = <?=json_encode($adminQ, JSON_UNESCAPED_UNICODE)?>;

const typeSelect = document.getElementById("typeSelect");
const formFields = document.getElementById("formFields");

function render(type) {
    const list = type === "Команда" ? teamQ : adminQ;
    formFields.innerHTML = "";
    list.forEach((q, i) => {
        const name = "field" + i;
        formFields.innerHTML += `
            <label>${q}</label>
            <input type="text" name="${name}" required>
        `;
    });
}

if (typeSelect) {
    typeSelect.addEventListener("change", () => render(typeSelect.value));
    render("Команда");
}
</script>

</body>
</html>