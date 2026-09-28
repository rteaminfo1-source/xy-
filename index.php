<?php
require __DIR__ . '/config.php';

/* ==========================================
   ГЕО-БЛОКИРОВКА ПО СТРАНАМ
   Список стран настраивается в admin.php (вкладка «Баны»),
   хранится в geoblock.json. По умолчанию: Украина, Польша,
   Литва, Латвия, Эстония.
   ========================================== */
if (!function_exists('rteam_client_ip')) {
    function rteam_client_ip() {
        // Cloudflare / прокси заголовки имеют приоритет, но проверяем валидность IP
        foreach (["HTTP_CF_CONNECTING_IP", "HTTP_X_FORWARDED_FOR", "HTTP_X_REAL_IP", "REMOTE_ADDR"] as $key) {
            if (!empty($_SERVER[$key])) {
                $candidate = trim(explode(",", $_SERVER[$key])[0]);
                if (filter_var($candidate, FILTER_VALIDATE_IP)) return $candidate;
            }
        }
        return $_SERVER["REMOTE_ADDR"] ?? "0.0.0.0";
    }
}
if (!function_exists('rteam_country_by_ip')) {
    function rteam_country_by_ip($ip) {
        // Локальные/приватные адреса не определяем — иначе тесты с localhost будут ломаться
        if (!filter_var($ip, FILTER_VALIDATE_IP, FILTER_FLAG_NO_PRIV_RANGE | FILTER_FLAG_NO_RES_RANGE)) {
            return null;
        }
        $cache = load_json("geo_cache.json", []);
        if (isset($cache[$ip]) && ($cache[$ip]["time"] ?? 0) > time() - 43200) {
            return $cache[$ip]["country"] ?: null;
        }
        $country = null;
        $ctx = stream_context_create(["http" => ["timeout" => 2]]);
        $resp = @file_get_contents("http://ip-api.com/json/" . urlencode($ip) . "?fields=status,countryCode", false, $ctx);
        if ($resp) {
            $data = json_decode($resp, true);
            if (($data["status"] ?? "") === "success" && !empty($data["countryCode"])) {
                $country = $data["countryCode"];
            }
        }
        $cache[$ip] = ["country" => $country, "time" => time()];
        // Не даём файлу с кэшем разрастаться бесконечно
        if (count($cache) > 5000) $cache = array_slice($cache, -3000, null, true);
        save_json("geo_cache.json", $cache);
        return $country;
    }
}

$geoblock_settings = load_json("geoblock.json", [
    "enabled" => true,
    "countries" => ["UA", "PL", "LT", "LV", "EE"], // Украина, Польша, Литва, Латвия, Эстония
]);

$rteam_visitor_ip = rteam_client_ip();

/* ==========================================
   РЕАЛЬНАЯ БЛОКИРОВКА ПО IP (Баны / Чёрный список)
   Список банов настраивается в admin.php → «Баны» и
   «Чёрный список», хранится в bans.json. Если IP
   посетителя забанен и бан ещё не истёк — показываем
   страницу блокировки вместо входа/логина.
   ========================================== */
if (!function_exists('rteam_active_ban')) {
    function rteam_active_ban($ip) {
        $bans = load_json("bans.json", []);
        foreach ($bans as $b) {
            if (($b["ip"] ?? "") === $ip) {
                $expires = (int)($b["expires"] ?? 0);
                if ($expires === 0 || $expires > time()) {
                    return $b;
                }
            }
        }
        return null;
    }
}

$rteam_active_ban = rteam_active_ban($rteam_visitor_ip);
if ($rteam_active_ban) {
    http_response_code(403);
    $ban_reason = $rteam_active_ban["reason"] ?? "Нарушение правил";
    $ban_expires = (int)($rteam_active_ban["expires"] ?? 0);
    $ban_until = $ban_expires === 0 ? "Навсегда" : date("d.m.Y H:i", $ban_expires);
    ?><!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<title>Вы заблокированы</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
    body { margin:0; min-height:100vh; display:flex; align-items:center; justify-content:center;
        background:#07040f; color:#ece7fb; font-family: Arial, sans-serif; text-align:center; padding:20px; }
    .box { max-width:480px; padding:32px; border:1px solid rgba(255,42,42,.35); border-radius:14px; background:#14101f; }
    h1 { color:#ff2a2a; font-size:22px; margin-bottom:12px; }
    p { color:#ada2cc; line-height:1.5; margin:6px 0; }
    .reason { margin-top:16px; padding:12px; border-left:2px solid #ff2a2a; background:rgba(255,42,42,.08); text-align:left; border-radius:4px; }
    .ipline { color:#6f6689; font-size:12px; margin-top:18px; }
</style>
</head>
<body>
    <div class="box">
        <h1>🚫 Вы заблокированы</h1>
        <p>Доступ к сайту для вашего IP-адреса ограничен администрацией.</p>
        <div class="reason"><b>Причина:</b> <?=htmlspecialchars($ban_reason)?><br><b>Срок:</b> <?=htmlspecialchars($ban_until)?></div>
        <div class="ipline">IP: <?=htmlspecialchars($rteam_visitor_ip)?></div>
    </div>
</body>
</html><?php
    exit;
}

if (!empty($geoblock_settings["enabled"]) && !empty($geoblock_settings["countries"])) {
    $rteam_visitor_country = rteam_country_by_ip($rteam_visitor_ip);
    if ($rteam_visitor_country && in_array($rteam_visitor_country, $geoblock_settings["countries"], true)) {
        // Логируем попытку захода — видно в admin.php → Баны → «Гео-блокировка»
        $blocked_log = load_json("blocked_attempts.json", []);
        $blocked_log[] = [
            "time"    => date("Y-m-d H:i:s"),
            "ip"      => $rteam_visitor_ip,
            "country" => $rteam_visitor_country,
            "ua"      => substr($_SERVER["HTTP_USER_AGENT"] ?? "", 0, 200),
        ];
        if (count($blocked_log) > 500) $blocked_log = array_slice($blocked_log, -500);
        save_json("blocked_attempts.json", $blocked_log);

        http_response_code(403);
        ?><!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<title>Доступ ограничен</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
    body { margin:0; min-height:100vh; display:flex; align-items:center; justify-content:center;
        background:#07040f; color:#ece7fb; font-family: Arial, sans-serif; text-align:center; padding:20px; }
    .box { max-width:480px; padding:32px; border:1px solid rgba(155,92,255,.28); border-radius:14px; background:#14101f; }
    h1 { color:#ff2a2a; font-size:22px; margin-bottom:12px; }
    p { color:#ada2cc; line-height:1.5; }
</style>
</head>
<body>
    <div class="box">
        <h1>🚫 Доступ ограничен</h1>
        <p>Доступ к сайту с территории вашей страны в данный момент недоступен.</p>
    </div>
</body>
</html><?php
        exit;
    }
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

$THEME_CATALOG = rteam_theme_catalog();
$theme_settings = load_json("theme.json", ["active" => "tennis", "enabled" => false, "text" => ""]);
$site_theme_active = !empty($theme_settings["enabled"]) && isset($THEME_CATALOG[$theme_settings["active"]]);
if ($site_theme_active) {
    $site_theme = $THEME_CATALOG[$theme_settings["active"]];
    $site_theme_text = trim($theme_settings["text"] ?? "");
}

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
// Вход, регистрация и 2FA теперь живут на отдельных страницах: login.php и register.php.
// Здесь остаётся только выход из аккаунта.

/* ВЫХОД */
if (isset($_GET["logout"])) {
    session_destroy();
    header("Location: index.php");
    exit;
}

/* Страница открыта для всех: гости просто видят кнопки «Войти» / «Регистрация» */

$user = $_SESSION["user"] ?? null;
$role = $_SESSION["role"] ?? "Гость";
$is_golden = $user && !empty($users[$user]["golden"] ?? false);

/* Сохраняем IP и время последнего захода — видно в admin.php → Пользователи */
if ($user && isset($users[$user])) {
    $users[$user]["ip"] = $rteam_visitor_ip;
    $users[$user]["last_seen"] = date("Y-m-d H:i:s");
    save_json("users.json", $users);
}

/* ==========================================
   ИГРА В КАЛЬМАРА: состояние и AJAX-эндпоинт
   ========================================== */
$squid_game = load_json("squid_game.json", ["paused" => false, "progress" => []]);

if (isset($_POST["squid_action"])) {
    header("Content-Type: application/json; charset=utf-8");

    if (!$user) { echo json_encode(["ok" => false, "error" => "not_logged_in"]); exit; }

    if (!isset($squid_game["progress"][$user])) {
        $squid_game["progress"][$user] = ["season" => 0, "completed" => false];
    }

    if ($_POST["squid_action"] === "get_status") {
        echo json_encode([
            "ok" => true,
            "paused" => !empty($squid_game["paused"]),
            "season" => $squid_game["progress"][$user]["season"] ?? 0,
            "completed" => !empty($squid_game["progress"][$user]["completed"]),
        ]);
        exit;
    }

    if ($_POST["squid_action"] === "complete_season") {
        if (!empty($squid_game["paused"])) {
            echo json_encode(["ok" => false, "error" => "paused"]);
            exit;
        }
        $season = (int)($_POST["season"] ?? 0);
        if ($season >= 1 && $season <= 3 && $season > ($squid_game["progress"][$user]["season"] ?? 0)) {
            $squid_game["progress"][$user]["season"] = $season;
            if ($season === 3) {
                $squid_game["progress"][$user]["completed"] = true;
                $squid_game["progress"][$user]["completed_at"] = date("Y-m-d H:i:s");
                $logs = load_json("logs.json", []);
                $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "squid_game", "msg" => "$user прошёл все 3 сезона «Игры в кальмара» и добавлен в список претендентов на Золотой билет."];
                save_json("logs.json", $logs);
            }
            save_json("squid_game.json", $squid_game);
        }
        echo json_encode([
            "ok" => true,
            "season" => $squid_game["progress"][$user]["season"],
            "completed" => !empty($squid_game["progress"][$user]["completed"]),
        ]);
        exit;
    }

    echo json_encode(["ok" => false, "error" => "unknown_action"]);
    exit;
}

$squid_winners = [];
foreach ($squid_game["progress"] as $sg_login => $sg_p) {
    if (!empty($sg_p["completed"])) $squid_winners[$sg_login] = $sg_p;
}
uasort($squid_winners, fn($a, $b) => strcmp($a["completed_at"] ?? "", $b["completed_at"] ?? ""));

$accent = $settings["accent"] ?? "#9b5cff";

$teamQ  = $questions["team"];
$adminQ = $questions["admin"];

/* ==========================================
   НОВОГОДНИЙ РОЗЫГРЫШ — сезон, снег, авто-подведение итогов
========================================== */
function ny_current_season() {
    $m = (int)date("n"); $y = (int)date("Y");
    if ($m === 12) return $y;        // декабрь текущего года
    if ($m === 1)  return $y - 1;    // январь — всё ещё сезон прошлого декабря
    return null;                     // не сезон
}
$ny_season = ny_current_season();
$ny_active = $ny_season !== null;

if ($ny_active) {
    $ny_giveaway = load_json("ny_giveaway.json", ["season" => null, "participants" => [], "drawn" => false, "winner" => null, "drawn_at" => null]);
    // Новый сезон начался (наступило 1 декабря нового года) — сбрасываем розыгрыш
    if ((int)date("n") === 12 && (int)($ny_giveaway["season"] ?? 0) !== $ny_season) {
        $ny_giveaway = ["season" => $ny_season, "participants" => [], "drawn" => false, "winner" => null, "drawn_at" => null];
        save_json("ny_giveaway.json", $ny_giveaway);
    }
    // Автоматическое подведение итогов 1 января 00:00
    $ny_jan1 = new DateTime(($ny_season + 1) . "-01-01 00:00:00");
    if (!$ny_giveaway["drawn"] && new DateTime() >= $ny_jan1) {
        if (!empty($ny_giveaway["participants"])) {
            $ny_winner = $ny_giveaway["participants"][array_rand($ny_giveaway["participants"])];
            $ny_giveaway["winner"] = $ny_winner;
            if (isset($users[$ny_winner])) {
                $users[$ny_winner]["golden"] = true;
                if (empty($users[$ny_winner]["golden_title"])) $users[$ny_winner]["golden_title"] = "🎄 Золотой билет RTeam — Победитель новогоднего розыгрыша";
                save_json("users.json", $users);
            }
        }
        $ny_giveaway["drawn"] = true;
        $ny_giveaway["drawn_at"] = date("Y-m-d H:i:s");
        save_json("ny_giveaway.json", $ny_giveaway);
        $ny_logs = load_json("logs.json", []);
        $ny_logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "ny_giveaway", "msg" => "Автоматически подведены итоги новогоднего розыгрыша. " . ($ny_giveaway["winner"] ? "Победитель: {$ny_giveaway["winner"]}, выдан золотой билет." : "Участников не было.")];
        save_json("logs.json", $ny_logs);
    }
}
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
    --accent-2: #d946ef;
    --accent-3: #6d28d9;
    --accent-glow: rgba(155, 92, 255, .55);
    --bg: #07040f;
    --card: #14101f;
    --card-2: #1b1430;
    --border: rgba(155, 92, 255, .28);
    --text: #ece7fb;
    --soft: #ada2cc;
    --grad: linear-gradient(135deg, var(--accent-3), var(--accent) 55%, var(--accent-2));
}
* { box-sizing: border-box; }

/* Плавная прокрутка при клике на меню */
html { scroll-behavior: smooth; }

/* Кастомный скроллбар в фиолетовой гамме */
::-webkit-scrollbar { width: 10px; height: 10px; }
::-webkit-scrollbar-track { background: #08050f; }
::-webkit-scrollbar-thumb { background: linear-gradient(180deg, var(--accent-2), var(--accent-3)); border-radius: 6px; border: 2px solid #08050f; }
::-webkit-scrollbar-thumb:hover { background: var(--accent); }
* { scrollbar-width: thin; scrollbar-color: var(--accent) #08050f; }

/* Фоновые парящие частицы — общая атмосфера сайта */
#ember-bg { position: fixed; inset: 0; z-index: 0; pointer-events: none; overflow: hidden; }
.ember { position: absolute; bottom: -10px; width: 3px; height: 3px; border-radius: 50%; background: var(--accent-2); box-shadow: 0 0 8px 3px var(--accent-glow); animation-name: ember-rise; animation-timing-function: ease-in; animation-iteration-count: infinite; opacity: 0; }
@keyframes ember-rise {
    0%   { transform: translateY(0) translateX(0); opacity: 0; }
    10%  { opacity: .9; }
    90%  { opacity: .35; }
    100% { transform: translateY(-100vh) translateX(20px); opacity: 0; }
}

/* Кнопка "наверх" */
#back-to-top {
    position: fixed; top: 90px; right: 18px; z-index: 700;
    width: 42px; height: 42px; border-radius: 50%;
    background: linear-gradient(145deg, var(--card-2), var(--card));
    border: 1px solid var(--border); color: var(--accent-2);
    font-size: 18px; cursor: pointer; display: none;
    box-shadow: 0 0 16px var(--accent-glow), inset 0 0 0 1px rgba(255,255,255,.03);
    transition: transform .25s ease, box-shadow .25s ease;
}
#back-to-top:hover { transform: translateY(-3px); box-shadow: 0 4px 22px var(--accent-glow); }
#back-to-top.show { display: block; }
@media (max-width: 768px) {
    #back-to-top { top: auto; bottom: 22px; right: 14px; }
}

body {
    margin: 0;
    font-family: "Segoe UI", Arial, sans-serif;
    background:
        radial-gradient(circle at 15% -10%, rgba(155,92,255,.20) 0, transparent 40%),
        radial-gradient(circle at 90% 10%, rgba(217,70,239,.14) 0, transparent 45%),
        radial-gradient(circle at top, #1a0838 0, #0a0616 45%, #030109 100%);
    color: var(--text);
    overflow-x: hidden;
}

/* ------------------------------------------- */
/* ГЛОБАЛЬНАЯ АНИМАЦИЯ И ОВЕРЛЕЙ               */
/* ------------------------------------------- */
body.no-scroll { overflow: hidden; }

/* КЛАССЫ ДЛЯ ПРОПУСКА АНИМАЦИИ */
body.skip-intro #intro-overlay {
    display: none !important;
}
body.skip-intro #main-content {
    opacity: 1 !important;
    visibility: visible !important;
}

#intro-overlay {
    position: fixed;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background: #000000;
    z-index: 99999;
    display: flex;
    justify-content: center;
    align-items: center;
    transition: background-color 1s ease;
}
.intro-logo-container {
    position: absolute;
    display: flex;
    justify-content: center;
    align-items: center;
    transition: opacity 0.5s ease;
}
.intro-logo {
    font-size: 7vw;
    font-weight: 900;
    font-family: "Segoe UI", Arial, sans-serif;
    color: transparent;
    -webkit-text-stroke: 1px rgba(155, 92, 255, 0.35);
    letter-spacing: 20px;
    text-transform: uppercase;
    transform: scale(0.9);
    opacity: 0;
    animation: revealLogo 2s cubic-bezier(0.19, 1, 0.22, 1) forwards;
}
.intro-logo::before {
    content: "RTEAM";
    position: absolute;
    left: 0;
    top: 0;
    color: var(--accent);
    -webkit-text-stroke: 0px;
    opacity: 0;
    filter: drop-shadow(0 0 20px var(--accent)) drop-shadow(0 0 40px var(--accent));
    animation: fillLogo 1.5s ease-in-out 1s forwards;
}

@keyframes revealLogo {
    0% { opacity: 0; transform: scale(0.85); letter-spacing: 40px; filter: blur(15px); }
    100% { opacity: 1; transform: scale(1); letter-spacing: 20px; filter: blur(0px); }
}
@keyframes fillLogo {
    0% { opacity: 0; }
    50% { opacity: 0.8; }
    100% { opacity: 1; text-shadow: 0 0 30px var(--accent); }
}

/* ------------------------------------------- */
/* CMD БОКС И ЕГО СОСТОЯНИЯ                    */
/* ------------------------------------------- */
#cmd-box {
    background: #05030c;
    border: 1px solid rgba(155,92,255,.25);
    padding: 20px;
    border-radius: 12px;
    font-family: Consolas, monospace;
    color: #c9a6ff;
    box-shadow: 0 0 30px rgba(155,92,255,.18), inset 0 0 30px rgba(155,92,255,.04);
    text-align: left;
    box-sizing: border-box;
    z-index: 100000;
}

.cmd-intro-mode {
    position: fixed !important;
    top: 50% !important;
    left: 50% !important;
    width: 80% !important;
    max-width: 800px !important;
    transform: translate(-50%, -50%) !important;
    opacity: 0;
    transition: opacity 0.5s ease;
}
.cmd-intro-mode.visible { opacity: 1; }

#cmd-content {
    white-space: pre-wrap;
    font-size: 16px;
    line-height: 1.5;
}
.cmd-cursor {
    display: inline-block;
    width: 10px;
    height: 18px;
    background-color: var(--accent-2);
    box-shadow: 0 0 8px var(--accent-glow);
    vertical-align: middle;
    animation: blink 1s step-end infinite;
    margin-left: 4px;
}
@keyframes blink { 50% { opacity: 0; } }

/* ------------------------------------------- */
/* ОСНОВНОЙ КОНТЕНТ И АНИМАЦИЯ СКРОЛЛА         */
/* ------------------------------------------- */
#main-content {
    opacity: 0;
    visibility: hidden;
    transition: opacity 1s ease, visibility 1s;
}
#main-content.visible {
    opacity: 1;
    visibility: visible;
}

/* Классы для анимации секций при прокрутке */
.reveal {
    opacity: 0;
    transform: translateY(60px) scale(0.98);
    transition: all 0.9s cubic-bezier(0.25, 1, 0.3, 1);
}
.reveal.active {
    opacity: 1;
    transform: translateY(0) scale(1);
}

header {
    position: fixed;
    top: 0;
    width: 100%;
    background: rgba(9,6,18,0.85);
    border-bottom: 1px solid var(--border);
    box-shadow: 0 1px 24px rgba(155,92,255,.08);
    padding: 12px 40px;
    display: flex;
    justify-content: space-between;
    align-items: center;
    z-index: 1000;
    backdrop-filter: blur(14px);
}
.logo {
    font-size: 24px;
    font-weight: 800;
    letter-spacing: .5px;
    background: var(--grad);
    -webkit-background-clip: text;
    background-clip: text;
    color: transparent;
    filter: drop-shadow(0 0 14px var(--accent-glow));
}
nav a {
    position: relative;
    margin-left: 20px;
    text-decoration: none;
    color: #ccc;
    font-size: 14px;
}
nav a::after {
    content: "";
    position: absolute; left: 0; bottom: -4px;
    width: 0; height: 2px;
    background: var(--grad);
    border-radius: 2px;
    transition: width .25s ease;
}
nav a:hover { color: #fff; }
nav a:hover::after { width: 100%; }
.user-info { font-size: 13px; color: var(--soft); }
.user-info b { color: var(--accent-2); }

section {
    padding: 90px 60px 70px;
    min-height: 60vh;
}
h1 {
    font-size: 32px;
    font-weight: 800;
    background: var(--grad);
    -webkit-background-clip: text;
    background-clip: text;
    color: transparent;
    margin-bottom: 10px;
    filter: drop-shadow(0 0 10px var(--accent-glow));
    display: inline-block;
}
p {
    max-width: 700px;
    color: var(--soft);
    line-height: 1.6;
}

/* БЛОГ */
.blog-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(280px, 1fr)); gap: 16px; margin-top: 20px; }
.blog-post {
    position: relative;
    background: linear-gradient(160deg, var(--card-2), var(--card));
    border: 1px solid var(--border);
    padding: 18px 18px 18px 20px;
    border-radius: 14px;
    margin-top: 0;
    transition: transform .25s ease, box-shadow .25s ease, border-color .25s ease;
    overflow: hidden;
}
.blog-post::before {
    content: "";
    position: absolute; left: 0; top: 0; bottom: 0; width: 4px;
    background: var(--grad);
}
.blog-post:hover {
    transform: translateY(-4px);
    border-color: rgba(217,70,239,.5);
    box-shadow: 0 12px 32px rgba(109,40,217,.28);
}
.blog-date { font-size: 11.5px; color: #8a7fae; margin-bottom: 6px; text-transform: uppercase; letter-spacing: .5px; }
.blog-title { font-size: 18px; color: #fff; margin-bottom: 8px; font-weight: 800; }
.blog-post > div:last-child { color: var(--soft); font-size: 14px; line-height: 1.6; }

/* ЛЕНТА ЗОЛОТЫХ БИЛЕТОВ */
.gold-strip {
    overflow: hidden;
    background: linear-gradient(90deg, #0d0a02, #14100a, #0d0a02);
    border-top: 1px solid #3a2c00;
    border-bottom: 1px solid #3a2c00;
    padding: 16px 0;
}
.gold-strip-title {
    text-align: center;
    font-size: 12px;
    letter-spacing: 2px;
    color: #ffd76a;
    text-transform: uppercase;
    margin-bottom: 12px;
}
.gold-strip-viewport {
    overflow: hidden;
}
.gold-strip-track {
    display: flex;
    width: max-content;
    gap: 34px;
}
.gold-strip-track.scrolling {
    animation: goldStripScroll linear infinite;
}
.gold-strip-track.centered {
    width: 100%;
    justify-content: center;
}
.gold-strip:hover .gold-strip-track { animation-play-state: paused; }
@keyframes goldStripScroll {
    from { transform: translateX(0); }
    to { transform: translateX(-50%); }
}
.gold-strip-item {
    display: flex;
    flex-direction: column;
    align-items: center;
    text-decoration: none;
    flex: 0 0 auto;
    width: 92px;
}
.gold-strip-avatar {
    width: 54px; height: 54px; border-radius: 50%;
    border: 2px solid #d4a017;
    box-shadow: 0 0 10px rgba(255,215,0,.4);
    overflow: hidden;
    display:flex; align-items:center; justify-content:center;
    background: linear-gradient(135deg,#ffe066,#d4a017);
    font-size: 20px;
}
.gold-strip-avatar img { width:100%; height:100%; object-fit:cover; }
.gold-strip-name {
    color: #ffd76a; font-size: 12px; font-weight: bold; margin-top: 6px;
    max-width: 92px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
}
.gold-strip-status {
    color: #999; font-size: 10px; margin-top: 2px;
    max-width: 92px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
}

/* СЛИВЫ / КАРТОЧКИ */
.card {
    position: relative;
    background: linear-gradient(160deg, var(--card-2), var(--card));
    border: 1px solid var(--border);
    padding: 18px;
    border-radius: 14px;
    margin-top: 15px;
    box-shadow: 0 8px 24px rgba(5,2,15,.4);
    transition: transform .25s ease, box-shadow .25s ease, border-color .25s ease;
    overflow: hidden;
}
.card::before {
    content: "";
    position: absolute; inset: 0;
    background: linear-gradient(120deg, rgba(155,92,255,.08), transparent 40%);
    pointer-events: none;
}
.card:hover {
    transform: translateY(-4px);
    border-color: rgba(155,92,255,.55);
    box-shadow: 0 14px 34px rgba(109,40,217,.28), 0 0 0 1px rgba(217,70,239,.12);
}

/* ФОРМЫ */
.box {
    width: 420px;
    max-width: 100%;
    background: linear-gradient(160deg, var(--card-2), var(--card));
    border: 1px solid var(--border);
    padding: 22px;
    border-radius: 14px;
    box-shadow: 0 0 30px rgba(109,40,217,.18), inset 0 0 0 1px rgba(255,255,255,.02);
}
input, textarea, select {
    width: 100%;
    padding: 11px 12px;
    margin-top: 8px;
    border-radius: 9px;
    border: 1px solid rgba(155,92,255,.22);
    background: #0b0716;
    color: #fff;
    resize: none;
    font-size: 14px;
    transition: border-color .2s ease, box-shadow .2s ease;
}
input:focus, textarea:focus, select:focus {
    outline: none;
    border-color: var(--accent-2);
    box-shadow: 0 0 0 3px rgba(217,70,239,.15);
}
textarea { height: 110px; }

.btn {
    display: inline-block;
    margin-top: 10px;
    padding: 10px 18px;
    background: var(--grad);
    background-size: 160% 160%;
    color: #fff;
    font-weight: 600;
    border-radius: 9px;
    text-decoration: none;
    border: none;
    cursor: pointer;
    box-shadow: 0 4px 16px rgba(155,92,255,.4);
    transition: transform .2s ease, box-shadow .2s ease, background-position .4s ease;
}
.btn:hover {
    transform: translateY(-2px);
    background-position: 100% 0;
    box-shadow: 0 8px 24px rgba(217,70,239,.5);
}
.btn:active { transform: translateY(0); }

.auth-wrap {
    display: flex;
    gap: 20px;
    flex-wrap: wrap;
    margin-top: 18px;
    align-items: stretch;
}
.auth-wrap .box { flex: 1 1 360px; }
.error { color: #ff8fd6; margin-top: 8px; font-size: 13px; }

/* ===== ОБНОВЛЁННЫЕ ЭЛЕМЕНТЫ ФОРМ / АККАУНТА (новый дизайн) ===== */
.box-kicker {
    display: inline-block;
    font-size: 11px;
    letter-spacing: 1.5px;
    text-transform: uppercase;
    color: var(--accent-2);
    background: rgba(217,70,239,.12);
    border: 1px solid rgba(217,70,239,.3);
    padding: 3px 10px;
    border-radius: 999px;
    margin-bottom: 10px;
}
.box-title { font-size: 20px; font-weight: 800; color: #fff; margin: 0 0 6px; }
.muted { color: var(--soft); font-size: 13.5px; line-height: 1.55; margin: 0 0 14px; }
.field-label { display: block; font-size: 12px; color: var(--soft); margin-top: 12px; margin-bottom: -2px; letter-spacing: .3px; }
.styled-form input, .styled-form textarea, .styled-form select { margin-top: 6px; }
.btn-block { display: block; width: 100%; text-align: center; margin-top: 10px; }
.btn-ghost {
    background: transparent;
    box-shadow: none;
    border: 1px solid var(--border);
    color: var(--text);
}
.btn-ghost:hover { border-color: var(--accent-2); box-shadow: 0 6px 18px rgba(217,70,239,.18); }
.btn-golden {
    background: linear-gradient(135deg,#ffe066,#d4a017);
    color: #3a2a00;
    box-shadow: 0 6px 18px rgba(212,160,23,.35);
}
.btn-google {
    background: #fff;
    color: #1f1f1f;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 10px;
    box-shadow: 0 4px 14px rgba(0,0,0,.25);
}
.btn-google:hover { transform: translateY(-2px); box-shadow: 0 8px 20px rgba(0,0,0,.3); }
.divider { display: flex; align-items: center; gap: 10px; margin: 18px 0 4px; color: #6d6390; font-size: 12px; }
.divider::before, .divider::after { content: ""; flex: 1; height: 1px; background: var(--border); }

/* Кнопки в шапке (Войти / Регистрация / Выйти) */
.nav-btn {
    margin-left: 10px;
    padding: 7px 16px;
    border-radius: 999px;
    text-decoration: none;
    font-size: 13px;
    font-weight: 600;
    transition: transform .2s ease, box-shadow .2s ease, background .2s ease;
}
.nav-btn-ghost { color: var(--text); border: 1px solid var(--border); background: rgba(255,255,255,.02); }
.nav-btn-ghost:hover { border-color: var(--accent-2); box-shadow: 0 4px 14px rgba(217,70,239,.2); }
.nav-btn-solid { color: #fff; background: var(--grad); box-shadow: 0 4px 14px rgba(155,92,255,.35); }
.nav-btn-solid:hover { transform: translateY(-1px); box-shadow: 0 8px 20px rgba(217,70,239,.4); }
.user-chip { display: inline-flex; align-items: center; gap: 8px; font-size: 13px; color: var(--soft); }
.user-chip-avatar {
    width: 26px; height: 26px; border-radius: 50%;
    background: var(--grad); color: #fff; font-weight: 800; font-size: 12px;
    display: flex; align-items: center; justify-content: center;
    box-shadow: 0 0 10px var(--accent-glow);
}
.user-chip b { color: #fff; display: block; font-size: 13px; line-height: 1.1; }
.user-chip small { color: var(--soft); font-size: 10.5px; }
.badge-golden {
    background: linear-gradient(135deg,#ffe066,#d4a017); color: #3a2a00; font-weight: bold;
    padding: 3px 10px; border-radius: 999px; font-size: 11px; margin-left: 8px;
    box-shadow: 0 0 10px rgba(255,215,0,.4);
}
@media (max-width: 768px) {
    .nav-btn { margin-left: 6px; padding: 6px 12px; font-size: 12px; }
    .user-chip { display: none; }
}

footer {
    text-align: center;
    padding: 18px;
    background: #06040d;
    border-top: 1px solid var(--border);
    color: #8a7fae;
    font-size: 12px;
}

@media (max-width: 768px) {
    header { padding: 10px 16px; }
    section { padding: 80px 16px 60px; }
    h1 { font-size: 26px; }
    .intro-logo { font-size: 12vw; letter-spacing: 10px; }
    .cmd-intro-mode { width: 95% !important; }
}

/* ===== НОВОГОДНИЙ РОЗЫГРЫШ: СНЕГ ===== */
#snowfall {
    position: fixed; inset: 0; width: 100%; height: 100%;
    pointer-events: none; z-index: 500; overflow: hidden;
}
.snowflake {
    position: absolute; top: -10px; color: #fff;
    text-shadow: 0 0 6px rgba(255,255,255,.6);
    animation-name: snowfall-drop;
    animation-timing-function: linear;
    animation-iteration-count: infinite;
    opacity: .85;
}
@keyframes snowfall-drop {
    0%   { transform: translateY(-10vh) translateX(0); }
    100% { transform: translateY(110vh) translateX(20px); }
}

#ny-button {
    position: fixed; right: 22px; bottom: 22px; z-index: 600;
    background: linear-gradient(135deg,#2e7d32,#1b5e20);
    color: #fff; text-decoration: none; font-weight: bold;
    padding: 12px 20px; border-radius: 999px;
    box-shadow: 0 0 18px rgba(46,125,50,.6), 0 0 0 2px #ffd76a inset;
    font-size: 14px;
    animation: ny-pulse 2.4s ease-in-out infinite;
}
@keyframes ny-pulse {
    0%, 100% { transform: scale(1); }
    50% { transform: scale(1.06); }
}

#ny-intro-overlay {
    position: fixed; inset: 0; z-index: 9998;
    background: radial-gradient(circle at center, rgba(10,20,10,.92), rgba(0,0,0,.97));
    display: flex; align-items: center; justify-content: center; text-align: center;
    flex-direction: column; opacity: 0; pointer-events: none;
    transition: opacity 1s ease;
}
#ny-intro-overlay.show { opacity: 1; pointer-events: all; }
#ny-intro-overlay h2 {
    font-size: 42px; color: #ffd76a; text-shadow: 0 0 24px rgba(255,215,102,.6);
    margin: 0 0 10px;
}
#ny-intro-overlay p { color: #cde; font-size: 16px; max-width: 480px; }
@media (max-width: 768px) {
    #ny-intro-overlay h2 { font-size: 28px; }
    #ny-button { right: 14px; bottom: 14px; padding: 10px 16px; font-size: 13px; }
}

/* ===== ТЕМЫ САЙТА (раздел "Темы сайта" в админ-панели) ===== */
#theme-banner {
    position: fixed; top: 64px; left: 0; width: 100%;
    z-index: 999;
    background: linear-gradient(135deg, rgba(255,42,42,.16), rgba(255,42,42,.05));
    border-bottom: 1px solid rgba(255,42,42,.35);
    backdrop-filter: blur(6px);
    color: #f2f2f2; text-align: center;
    padding: 10px 16px; font-size: 14px;
}
#theme-banner b { color: var(--accent); }
#theme-decor {
    position: fixed; inset: 0; width: 100%; height: 100%;
    pointer-events: none; z-index: 498; overflow: hidden;
}
.theme-icon {
    position: absolute; top: -10vh;
    animation-name: theme-icon-fall;
    animation-timing-function: linear;
    animation-iteration-count: infinite;
    opacity: .8;
    filter: drop-shadow(0 0 6px rgba(0,0,0,.5));
}
@keyframes theme-icon-fall {
    0%   { transform: translateY(-10vh) translateX(0) rotate(0deg); }
    100% { transform: translateY(110vh) translateX(20px) rotate(25deg); }
}
@media (max-width: 768px) {
    #theme-banner { top: 56px; font-size: 13px; padding: 8px 12px; }
}

/* ===== ИГРА В КАЛЬМАРА ===== */
#squid-game-btn {
    position: fixed; left: 22px; bottom: 22px; z-index: 600;
    background: #111; color: #ff2fa0; border: 1px solid #ff2fa0;
    padding: 12px 18px; border-radius: 30px; font-weight: 700; font-size: 14px;
    cursor: pointer; box-shadow: 0 0 18px rgba(255,47,160,.4);
}
#squid-game-modal {
    display: none; position: fixed; inset: 0; z-index: 9997;
    background: rgba(0,0,0,.85); align-items: center; justify-content: center;
}
#squid-game-modal.show { display: flex; }
#squid-game-box {
    position: relative; background: #0c0c12; border: 1px solid #ff2fa055;
    border-radius: 14px; padding: 26px; width: 90%; max-width: 460px; text-align: center;
    box-shadow: 0 0 40px rgba(255,47,160,.25);
}
#squid-game-box h3 { color: #ff2fa0; margin-bottom: 10px; }
#squid-game-box p { color: #ccc; font-size: 13px; margin-bottom: 14px; }
#squid-game-box { max-height: 88vh; overflow-y: auto; }
#squid-hall-of-fame { margin-top: 18px; padding-top: 14px; border-top: 1px solid #262633; text-align: left; }
#squid-hall-of-fame h4 { color: #ff2fa0; font-size: 13px; margin-bottom: 8px; }
#squid-hall-of-fame ul { list-style: none; padding: 0; margin: 0; font-size: 13px; color: #ddd; }
#squid-hall-of-fame li { padding: 4px 0; border-bottom: 1px solid #1c1c26; }
#squid-game-close {
    position: absolute; top: 10px; right: 14px; background: none; border: none;
    color: #999; font-size: 18px; cursor: pointer;
}
#squid-game-field {
    position: relative; height: 140px; background: #15151d; border-radius: 10px;
    margin-bottom: 14px; overflow: hidden; border: 1px solid #262633;
}
#squid-game-light { position: absolute; top: 8px; right: 12px; font-size: 22px; }
#squid-game-finish { position: absolute; right: 10px; top: 0; bottom: 0; width: 8px; background: #ff2fa0; }
#squid-game-player {
    position: absolute; left: 10px; bottom: 14px; font-size: 26px;
    transition: left .15s linear;
}
#squid-game-move, #squid-game-restart {
    background: #ff2fa0; color: #fff; border: none; border-radius: 8px;
    padding: 10px 18px; font-weight: 700; cursor: pointer; font-size: 14px;
}
#squid-game-lives { color: #ff4444; font-size: 15px; margin-bottom: 8px; letter-spacing: 4px; }
.squid-season-title { color: #ff2fa0; font-size: 13px; letter-spacing: 2px; text-transform: uppercase; margin-bottom: 4px; }
.squid-hidden { display: none !important; }

/* Дальгона (сезон 2) */
#squid-dalgona-track { position: relative; height: 28px; background: #15151d; border-radius: 6px; border: 1px solid #262633; margin-bottom: 10px; overflow: hidden; }
#squid-dalgona-zone { position: absolute; top: 0; bottom: 0; background: rgba(57,255,106,.25); border-left: 2px solid #39ff6a; border-right: 2px solid #39ff6a; }
#squid-dalgona-marker { position: absolute; top: 0; bottom: 0; width: 4px; background: #fff; box-shadow: 0 0 8px #fff; }
#squid-dalgona-progress { height: 8px; background: #262633; border-radius: 4px; overflow: hidden; margin-bottom: 14px; }
#squid-dalgona-progress-fill { height: 100%; width: 0%; background: #ff2fa0; transition: width .2s ease; }

/* Стеклянный мост (сезон 3) */
#squid-bridge-steps { display: flex; flex-direction: column; gap: 6px; margin-bottom: 14px; max-height: 160px; overflow-y: auto; }
.squid-bridge-step { display: flex; gap: 8px; justify-content: center; }
.squid-bridge-tile {
    flex: 1; max-width: 90px; padding: 10px 0; text-align: center; border-radius: 8px;
    background: #15151d; border: 1px solid #333; cursor: pointer; font-size: 20px;
}
.squid-bridge-tile.chosen-safe { background: rgba(57,255,106,.25); border-color: #39ff6a; }
.squid-bridge-tile.chosen-broken { background: rgba(255,68,68,.3); border-color: #ff4444; }
.squid-bridge-tile.disabled { pointer-events: none; opacity: .5; }

/* ===== КРОВЬ ВНИЗУ ЭКРАНА (тема «Игра в кальмара») ===== */
#squid-blood-bar {
    position: fixed; left: 0; right: 0; bottom: 0; z-index: 610;
    text-align: center; padding: 14px 16px 60px;
    background: linear-gradient(to top, rgba(60,0,0,.65), transparent);
    pointer-events: none;
}
#squid-blood-drips { position: absolute; left: 0; right: 0; bottom: 100%; height: 34px; display: flex; justify-content: space-around; }
.squid-drip {
    width: 10px; background: #8b0000; border-radius: 0 0 8px 8px;
    animation: squid-drip-fall 3.5s ease-in infinite;
}
@keyframes squid-drip-fall {
    0%   { height: 6px; opacity: .9; }
    70%  { height: 34px; opacity: .9; }
    100% { height: 34px; opacity: 0; }
}
#squid-invite-text {
    pointer-events: all; display: inline-block; color: #ff2fa0;
    font-weight: 800; font-size: 16px; letter-spacing: 1px;
    text-shadow: 0 0 14px rgba(255,47,160,.7);
    background: rgba(10,10,14,.75); border: 1px solid #ff2fa055;
    padding: 10px 18px; border-radius: 30px; cursor: pointer;
}
@media (max-width: 768px) {
    #squid-invite-text { font-size: 13px; padding: 8px 14px; }
}

/* Декорации на КАЖДОМ блоке сайта (карточки, формы) при активной теме "Игра в кальмара" */
body.squid-theme-active .card,
body.squid-theme-active .box {
    position: relative;
    overflow: visible;
}
body.squid-theme-active .card::after,
body.squid-theme-active .box::after {
    content: "🦑";
    position: absolute;
    top: -10px;
    right: -8px;
    font-size: 18px;
    opacity: .55;
    filter: drop-shadow(0 0 4px rgba(255,47,160,.6));
    transform: rotate(8deg);
    pointer-events: none;
}
body.squid-theme-active .card:nth-of-type(3n)::after { content: "▲"; color: #ff2fa0; }
body.squid-theme-active .card:nth-of-type(3n+1)::after { content: "●"; color: #ff2fa0; }

/* Декорации вокруг блока входа (лого RTEAM / cmd-терминал) */
#squid-intro-decor {
    position: absolute; inset: 0; pointer-events: none; z-index: 100001;
    display: flex; align-items: center; justify-content: center;
}
#squid-intro-decor span {
    position: absolute;
    font-size: 22px;
    opacity: .7;
    filter: drop-shadow(0 0 6px rgba(255,47,160,.6));
    animation: squid-orbit 6s linear infinite;
}
@keyframes squid-orbit {
    from { transform: rotate(0deg) translateX(220px) rotate(0deg); }
    to   { transform: rotate(360deg) translateX(220px) rotate(-360deg); }
}
@media (max-width: 768px) {
    #squid-intro-decor span { font-size: 16px; }
    @keyframes squid-orbit {
        from { transform: rotate(0deg) translateX(120px) rotate(0deg); }
        to   { transform: rotate(360deg) translateX(120px) rotate(-360deg); }
    }
}

/* Выбор игры сезона */
#squid-picker-list { display: flex; flex-direction: column; gap: 8px; text-align: left; }
.squid-pick-card {
    display: flex; align-items: center; gap: 10px;
    background: #15151d; border: 1px solid #262633; border-radius: 10px;
    padding: 10px 12px; cursor: pointer;
}
.squid-pick-card:hover { border-color: #ff2fa0; }
.squid-pick-card .squid-pick-icon { font-size: 22px; }
.squid-pick-card b { color: #fff; display: block; font-size: 14px; }
.squid-pick-card span { color: #999; font-size: 12px; }

/* Перетягивание каната */
#squid-tug-track { position: relative; height: 30px; background: #15151d; border-radius: 6px; border: 1px solid #262633; margin-bottom: 14px; overflow: hidden; }
#squid-tug-fill { position: absolute; top:0; bottom:0; left:0; width:50%; background: linear-gradient(90deg, #ff2fa0, #ff8fce); transition: width .15s ease; }
#squid-tug-marker { position: absolute; top: 2px; left: 50%; transform: translateX(-50%); font-size: 20px; transition: left .15s ease; }

/* Скрипучие половицы */
#squid-memory-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; margin-bottom: 14px; }
.squid-memory-tile { aspect-ratio: 1; background: #15151d; border: 1px solid #262633; border-radius: 8px; cursor: pointer; }
.squid-memory-tile.lit { background: rgba(255,47,160,.5); border-color: #ff2fa0; }
.squid-memory-tile.correct { background: rgba(57,255,106,.4); border-color: #39ff6a; }
.squid-memory-tile.wrong { background: rgba(255,68,68,.4); border-color: #ff4444; }

/* Точный укол */
#squid-gauge-track { position: relative; height: 28px; background: #15151d; border-radius: 6px; border: 1px solid #262633; margin-bottom: 14px; overflow: hidden; }
#squid-gauge-zone { position: absolute; top:0; bottom:0; background: rgba(57,255,106,.25); border-left: 2px solid #39ff6a; border-right: 2px solid #39ff6a; }
#squid-gauge-marker { position: absolute; top:0; bottom:0; width: 4px; background:#fff; box-shadow: 0 0 8px #fff; }

/* Simon */
#squid-simon-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; margin-bottom: 14px; }
.squid-simon-btn { height: 70px; border-radius: 10px; opacity: .55; cursor: pointer; transition: opacity .15s ease; }
.squid-simon-btn.active { opacity: 1; box-shadow: 0 0 20px rgba(255,255,255,.6); }

/* Напёрстки */
#squid-shell-cups { display: flex; justify-content: center; gap: 18px; font-size: 40px; margin-bottom: 14px; }
.squid-cup { cursor: pointer; transition: transform .15s ease; }
.squid-cup:hover { transform: translateY(-4px); }
.squid-cup.disabled { pointer-events: none; opacity: .6; }

/* Дуэль */
#squid-duel-score { color: #ccc; font-size: 13px; margin-bottom: 10px; }
#squid-duel-btn { background: #ff2fa0; color:#fff; border:none; border-radius:8px; padding: 16px 26px; font-weight:700; font-size:15px; cursor:pointer; width: 100%; }
</style>
</head>
<body class="no-scroll<?php echo ($site_theme_active && $theme_settings["active"] === "squid_game") ? " squid-theme-active" : ""; ?>">

<div id="ember-bg">
    <?php for ($i = 0; $i < 22; $i++): ?>
    <div class="ember" style="left:<?=rand(0,100)?>%; animation-duration:<?=rand(6,14)?>s; animation-delay:-<?=rand(0,14)?>s;"></div>
    <?php endfor; ?>
</div>
<button id="back-to-top" title="Наверх">↑</button>

<script>
    // СИНХРОННАЯ ПРОВЕРКА (Убирает белые экраны и мерцания)
    // Если пользователь уже был на сайте, мгновенно выключаем анимацию
    if (localStorage.getItem('rteam_intro_played') === '1') {
        document.body.classList.remove('no-scroll');
        document.body.classList.add('skip-intro');
    }
</script>

<!-- СЛОЙ АНИМАЦИИ ПРИ ВХОДЕ (ЛОГОТИП И CMD) -->
<div id="intro-overlay">
    <?php if ($site_theme_active && $theme_settings["active"] === "squid_game"): ?>
    <div id="squid-intro-decor">
        <span style="animation-delay:0s;">🦑</span>
        <span style="animation-delay:-1.5s;">▲</span>
        <span style="animation-delay:-3s;">●</span>
        <span style="animation-delay:-4.5s;">■</span>
    </div>
    <?php endif; ?>
    <div class="intro-logo-container" id="intro-logo-wrap">
        <div class="intro-logo">RTEAM</div>
    </div>
    <div id="cmd-box" class="cmd-intro-mode">
        <span id="cmd-content"></span><span class="cmd-cursor"></span>
    </div>
</div>

<!-- ДЕКОРАЦИИ ТЕМЫ САЙТА (не зависят от анимации входа — видны сразу) -->
<?php if ($site_theme_active): ?>
<div id="theme-banner">
    <?=$site_theme["icon"]?> <b><?=htmlspecialchars($site_theme["name"])?></b><?php if ($site_theme_text !== ""): ?> — <?=htmlspecialchars($site_theme_text)?><?php endif; ?>
</div>
<div id="theme-decor">
    <?php
    // Для темы "Игра в кальмара" используем набор символов охраны + кальмара,
    // для остальных тем — иконку самой темы.
    $decor_icons = ($theme_settings["active"] === "squid_game")
        ? ["▲", "●", "■", "🦑"]
        : [$site_theme["icon"]];
    for ($i = 0; $i < 26; $i++):
        $left = rand(0, 100);
        $duration = rand(9, 20);
        $delay = rand(0, 16);
        $size = rand(16, 30);
        $icon = $decor_icons[array_rand($decor_icons)];
    ?>
    <div class="theme-icon" style="left:<?=$left?>%; font-size:<?=$size?>px; animation-duration:<?=$duration?>s; animation-delay:-<?=$delay?>s;"><?=$icon?></div>
    <?php endfor; ?>
</div>
<?php if ($theme_settings["active"] === "squid_game"): ?>
<!-- КРОВАВЫЙ БАННЕР-ПРИГЛАШЕНИЕ ВНИЗУ ЭКРАНА -->
<div id="squid-blood-bar">
    <div id="squid-blood-drips">
        <?php for ($i = 0; $i < 14; $i++): $bd = rand(0, 35) / 10; ?>
        <div class="squid-drip" style="animation-delay:-<?=$bd?>s; animation-duration:<?=rand(28,45)/10?>s;"></div>
        <?php endfor; ?>
    </div>
    <div id="squid-invite-text">Хотите сыграть в игру? 🦑</div>
</div>

<div id="squid-game-modal">
    <div id="squid-game-box">
        <button id="squid-game-close">✕</button>

        <!-- ЭКРАН: НЕ АВТОРИЗОВАН -->
        <div id="squid-screen-guest" class="squid-hidden">
            <h3>🦑 Игра в кальмара</h3>
            <p>Чтобы сыграть и попасть в розыгрыш «Золотого билета RTeam», нужно войти или зарегистрироваться на сайте.</p>
        </div>

        <!-- ЭКРАН: ИГРЫ ОСТАНОВЛЕНЫ -->
        <div id="squid-screen-paused" class="squid-hidden">
            <h3>🦑 Игра в кальмара</h3>
            <p>Организаторы временно приостановили игры. Загляните позже.</p>
        </div>

        <!-- ЭКРАН: ПРИГЛАШЕНИЕ -->
        <div id="squid-screen-invite" class="squid-hidden">
            <h3>🦑 Приглашение получено</h3>
            <p>456 игроков в долгах ставят на кон всё. Три сезона испытаний, в каждом — выбор из трёх игр. Пройдёшь все сезоны — попадёшь в розыгрыш «Золотого билета RTeam».</p>
            <button id="squid-start-btn">Сыграть в игру</button>
        </div>

        <!-- ЭКРАН: ВЫБОР ИГРЫ СЕЗОНА -->
        <div id="squid-screen-picker" class="squid-hidden">
            <div class="squid-season-title" id="squid-picker-season-label">Сезон 1 из 3</div>
            <h3>Выбери игру</h3>
            <div id="squid-picker-list"></div>
        </div>

        <!-- ЭКРАН: ПЕРЕХОД МЕЖДУ СЕЗОНАМИ -->
        <div id="squid-screen-transition" class="squid-hidden">
            <h3 id="squid-transition-title">Сезон пройден</h3>
            <p id="squid-transition-text"></p>
            <button id="squid-transition-next">Продолжить</button>
        </div>

        <!-- СЕЗОН 1, ИГРА 1 — КРАСНЫЙ СВЕТ, ЗЕЛЁНЫЙ СВЕТ -->
        <div id="squid-screen-s1" class="squid-hidden">
            <div class="squid-season-title">Сезон 1 из 3</div>
            <h3>Красный свет, зелёный свет</h3>
            <p id="squid-game-status">Жми и держи «Идти», пока горит зелёный. На красном — замри, иначе проигрыш.</p>
            <div id="squid-game-field">
                <div id="squid-game-light">🟢</div>
                <div id="squid-game-finish"></div>
                <div id="squid-game-player">🏃</div>
            </div>
            <button id="squid-game-move">Идти</button>
            <button id="squid-game-restart" style="display:none;">Начать заново</button>
        </div>

        <!-- СЕЗОН 1, ИГРА 2 — ПЕРЕТЯГИВАНИЕ КАНАТА -->
        <div id="squid-screen-tug" class="squid-hidden">
            <div class="squid-season-title">Сезон 1 из 3</div>
            <h3>Перетягивание каната</h3>
            <p id="squid-tug-status">Жми «Тяни!» как можно быстрее — соперник тянет канат на свою сторону.</p>
            <div id="squid-tug-track"><div id="squid-tug-fill"></div><div id="squid-tug-marker">🪢</div></div>
            <button id="squid-tug-pull">Тяни!</button>
            <button id="squid-tug-restart" style="display:none;">Начать заново</button>
        </div>

        <!-- СЕЗОН 1, ИГРА 3 — СКРИПУЧИЕ ПОЛОВИЦЫ -->
        <div id="squid-screen-memory" class="squid-hidden">
            <div class="squid-season-title">Сезон 1 из 3</div>
            <h3>Скрипучие половицы</h3>
            <p id="squid-memory-status">Запомни подсвеченный путь, затем повтори его, нажимая на плитки в том же порядке.</p>
            <div id="squid-memory-grid"></div>
            <button id="squid-memory-restart" style="display:none;">Начать заново</button>
        </div>

        <!-- СЕЗОН 2, ИГРА 1 — ДАЛЬГОНА -->
        <div id="squid-screen-s2" class="squid-hidden">
            <div class="squid-season-title">Сезон 2 из 3</div>
            <h3>Дальгона: вырежи фигуру</h3>
            <p id="squid-dalgona-status">Жми «Вырезать» ровно тогда, когда белая полоска внутри зелёной зоны. 3 ошибки — и печенье треснет.</p>
            <div id="squid-dalgona-lives">❤️❤️❤️</div>
            <div id="squid-dalgona-track">
                <div id="squid-dalgona-zone"></div>
                <div id="squid-dalgona-marker"></div>
            </div>
            <div id="squid-dalgona-progress"><div id="squid-dalgona-progress-fill"></div></div>
            <button id="squid-dalgona-cut">Вырезать</button>
            <button id="squid-dalgona-restart" style="display:none;">Начать заново</button>
        </div>

        <!-- СЕЗОН 2, ИГРА 2 — ТОЧНЫЙ УКОЛ -->
        <div id="squid-screen-gauge" class="squid-hidden">
            <div class="squid-season-title">Сезон 2 из 3</div>
            <h3>Точный укол</h3>
            <p id="squid-gauge-status">Жми «Удар», когда стрелка в зелёной зоне. Нужно 3 попадания подряд, промахов разрешено 2.</p>
            <div id="squid-gauge-lives">❤️❤️</div>
            <div id="squid-gauge-track">
                <div id="squid-gauge-zone"></div>
                <div id="squid-gauge-marker"></div>
            </div>
            <button id="squid-gauge-hit">Удар</button>
            <button id="squid-gauge-restart" style="display:none;">Начать заново</button>
        </div>

        <!-- СЕЗОН 2, ИГРА 3 — ПОВТОРИ ПОСЛЕДОВАТЕЛЬНОСТЬ -->
        <div id="squid-screen-simon" class="squid-hidden">
            <div class="squid-season-title">Сезон 2 из 3</div>
            <h3>Повтори последовательность</h3>
            <p id="squid-simon-status">Смотри на цепочку цветов, затем повтори её в том же порядке. Дойди до цепочки из 6.</p>
            <div id="squid-simon-grid">
                <div class="squid-simon-btn" data-c="0" style="background:#ff2fa0;"></div>
                <div class="squid-simon-btn" data-c="1" style="background:#39ff6a;"></div>
                <div class="squid-simon-btn" data-c="2" style="background:#3aa0ff;"></div>
                <div class="squid-simon-btn" data-c="3" style="background:#ffd23a;"></div>
            </div>
            <button id="squid-simon-restart" style="display:none;">Начать заново</button>
        </div>

        <!-- СЕЗОН 3, ИГРА 1 — СТЕКЛЯННЫЙ МОСТ -->
        <div id="squid-screen-s3" class="squid-hidden">
            <div class="squid-season-title">Сезон 3 из 3</div>
            <h3>Стеклянный мост</h3>
            <p id="squid-bridge-status">Выбирай левую или правую панель. Закалённое стекло выдержит, обычное — разобьётся. У тебя 2 жизни.</p>
            <div id="squid-bridge-lives">❤️❤️</div>
            <div id="squid-bridge-steps"></div>
            <button id="squid-bridge-restart" style="display:none;">Начать заново</button>
        </div>

        <!-- СЕЗОН 3, ИГРА 2 — НАПЁРСТКИ -->
        <div id="squid-screen-shell" class="squid-hidden">
            <div class="squid-season-title">Сезон 3 из 3</div>
            <h3>Напёрстки</h3>
            <p id="squid-shell-status">Угадай, под каким стаканом шарик. Нужно 3 верных ответа из 5 раундов.</p>
            <div id="squid-shell-cups">
                <div class="squid-cup" data-i="0">🥤</div>
                <div class="squid-cup" data-i="1">🥤</div>
                <div class="squid-cup" data-i="2">🥤</div>
            </div>
            <button id="squid-shell-restart" style="display:none;">Начать заново</button>
        </div>

        <!-- СЕЗОН 3, ИГРА 3 — ФИНАЛЬНАЯ ДУЭЛЬ -->
        <div id="squid-screen-duel" class="squid-hidden">
            <div class="squid-season-title">Сезон 3 из 3</div>
            <h3>Финальная дуэль</h3>
            <p id="squid-duel-status">Жди сигнал «ДАВАЙ!» и жми кнопку как можно быстрее. Побеждает тот, кто выиграл 2 раунда из 3.</p>
            <div id="squid-duel-score">Ты: 0 — Соперник: 0</div>
            <button id="squid-duel-btn">Жди...</button>
            <button id="squid-duel-restart" style="display:none;">Начать заново</button>
        </div>

        <!-- ЭКРАН: ПОБЕДА -->
        <div id="squid-screen-win" class="squid-hidden">
            <h3>🎫 Все три сезона пройдены</h3>
            <p>Твоё имя внесено в список претендентов на «Золотой билет RTeam». Итоги розыгрыша объявят организаторы.</p>
        </div>

        <!-- ЗАЛ СЛАВЫ -->
        <div id="squid-hall-of-fame">
            <h4>🏆 Прошли все три сезона</h4>
            <?php if (!$squid_winners): ?>
                <p style="color:#888; font-size:13px;">Пока никто не прошёл все игры.</p>
            <?php else: ?>
                <ul>
                    <?php foreach (array_reverse($squid_winners, true) as $sw_login => $sw_data): ?>
                        <li><b><?=htmlspecialchars($sw_login)?></b><?php if (!empty($users[$sw_login]["golden"])): ?> <span class="badge badge-gold" style="font-size:10px;">🎫 золотой билет</span><?php endif; ?></li>
                    <?php endforeach; ?>
                </ul>
            <?php endif; ?>
        </div>
    </div>
</div>
<?php endif; ?>
<?php endif; ?>

<!-- ОСНОВНОЙ КОНТЕНТ САЙТА -->
<div id="main-content">
    <?php if ($ny_active): ?>
    <div id="snowfall">
        <?php for ($i = 0; $i < 45; $i++):
            $left = rand(0, 100);
            $duration = rand(8, 18);
            $delay = rand(0, 15);
            $size = rand(10, 22);
            $drift = rand(-40, 40);
        ?>
        <div class="snowflake" style="left:<?=$left?>%; font-size:<?=$size?>px; animation-duration:<?=$duration?>s; animation-delay:-<?=$delay?>s;">❄</div>
        <?php endfor; ?>
    </div>
    <a href="giveaway.php" id="ny-button">🎁 Новогодний розыгрыш</a>
    <div id="ny-intro-overlay">
        <h2>❄️ С наступающим Новым годом!</h2>
        <p>RTeam запускает новогодний розыгрыш — один счастливчик получит «Золотой билет RTeam». Загляните на страницу розыгрыша, чтобы поучаствовать!</p>
    </div>
    <script>
    (function() {
        var key = 'ny_intro_played_<?=$ny_season?>';
        if (!localStorage.getItem(key)) {
            window.addEventListener('load', function() {
                setTimeout(function() {
                    var el = document.getElementById('ny-intro-overlay');
                    el.classList.add('show');
                    setTimeout(function() { el.classList.remove('show'); }, 4000);
                    localStorage.setItem(key, '1');
                }, 1500);
            });
        }
    })();
    </script>
    <?php endif; ?>
    <header>
        <div class="logo">RTEAM</div>
        <nav>
            <a href="#home">Главная</a>
            <a href="team.php">Команда</a>
            <a href="#blog">Блог</a>
            <a href="#leaks">Сливы</a>
            <a href="#rating">Рейтинг</a>
            <a href="#join">Заявка</a>
            <a href="#contact">Контакты</a>
            <a href="forum.html">Форум</a>
            <a href="projects.php">Проекты</a>
            <a href="play.html">Игра</a>
            <a href="index2.html">RMain</a>
            <a href="pay.html">Поддержать</a>
			<a href="support.php">Поддержка</a>
            
            <?php if (in_array($role, ["Главный разработчик", "Администратор", "Главный Администратор", "Тестер", "Главный Тестер", "Кодер", "Главный Кодер", "Руководитель"])): ?>
                <a href="admin.php" style="color: var(--accent); font-weight: bold; border: 1px solid var(--accent); padding: 4px 10px; border-radius: 6px;">Админ-Панель</a>
                <a href="oauth/apps.php" style="color: var(--accent-2); font-weight: bold; border: 1px solid var(--accent-2); padding: 4px 10px; border-radius: 6px;">Вход через RTeam</a>
            <?php endif; ?>
            <?php if ($is_golden): ?>
                <a href="https://rteam.info/profile.php" style="color:#3a2a00; font-weight: bold; background: linear-gradient(135deg,#ffe066,#d4a017); padding: 4px 10px; border-radius: 6px; box-shadow: 0 0 10px rgba(255,215,0,.5);">🎫 Золотой профиль</a>
            <?php endif; ?>
            <?php if ($user): ?>
                <a href="https://rteam.info/cabinet.php" style="color: var(--accent-2); font-weight: bold; border: 1px solid var(--accent-2); padding: 4px 10px; border-radius: 6px;">Профиль</a>
            <?php endif; ?>
        </nav>
        <div class="user-info">
            <?php if ($user): ?>
                <span class="user-chip">
                    <span class="user-chip-avatar"><?=strtoupper(mb_substr($user,0,1))?></span>
                    <span><b><?=htmlspecialchars($user)?></b><small><?=htmlspecialchars($role)?></small></span>
                </span>
                <?php if ($is_golden): ?><span class="badge-golden">🎫 <?=htmlspecialchars($users[$user]["golden_title"] ?? "Золотой билет RTeam")?></span><?php endif; ?>
                <a href="?logout=1" class="nav-btn nav-btn-ghost">Выйти</a>
            <?php else: ?>
                <a href="login.php" class="nav-btn nav-btn-ghost">Войти</a>
                <a href="register.php" class="nav-btn nav-btn-solid">Регистрация</a>
            <?php endif; ?>
        </div>
    </header>

    <section id="home">
        <div style="text-align:center; margin-top:40px;">
            <h1 id="dynamicText" style="font-size:48px; font-weight:700; color:#ff2a2a; text-shadow:0 0 20px #ff0000aa; transition:0.6s ease;">
                Загрузка...
            </h1>
            <p style="margin:10px auto 0; max-width:600px;">
                Мы собираем людей, которые не боятся писать большие коды.
            </p>

            <!-- Место для приземления терминала -->
            <div id="cmd-placeholder" style="max-width:700px; margin:40px auto 0; min-height: 80px;"></div>
        </div>
    </section>

    <!-- Добавляем класс 'reveal' ко всем остальным секциям -->
    <section id="blog" class="reveal">
        <h1>Блог</h1>
        <p>Лента записей от команды.</p>
        <?php if (!$blog): ?>
            <p>Пока нет постов.</p>
        <?php else: ?>
            <div class="blog-grid">
                <?php foreach (array_reverse($blog) as $post): ?>
                    <?php if (!empty($post["hidden"])) continue; ?>
                    <div class="blog-post">
                        <div class="blog-date"><?=htmlspecialchars($post["date"] ?? "")?></div>
                        <div class="blog-title"><?=htmlspecialchars($post["title"] ?? "")?></div>
                        <div><?=nl2br(htmlspecialchars($post["content"] ?? ""))?></div>
                    </div>
                <?php endforeach; ?>
            </div>
        <?php endif; ?>
    </section>

    <?php
    $golden_list = [];
    foreach ($users as $u_login => $u_data) if (!empty($u_data["golden"])) $golden_list[$u_login] = $u_data;
    ?>
    <?php if ($golden_list): ?>
    <div class="gold-strip">
        <div class="gold-strip-title">🎫 Обладатели золотого билета RTeam</div>
        <div class="gold-strip-viewport">
            <div class="gold-strip-track" id="goldStripTrack">
                <?php
                $render_gold_strip_item = function($g_login, $g_data) {
                    $g_avatar = $g_data["avatar"] ?? "";
                    $g_status = trim($g_data["golden_status"] ?? "");
                    echo '<a href="cabinet.php?u=' . urlencode($g_login) . '" class="gold-strip-item">';
                    echo '<div class="gold-strip-avatar">';
                    echo $g_avatar ? '<img src="' . htmlspecialchars($g_avatar) . '" alt="">' : '🎫';
                    echo '</div>';
                    echo '<div class="gold-strip-name">' . htmlspecialchars($g_login) . '</div>';
                    if ($g_status !== "") echo '<div class="gold-strip-status">' . htmlspecialchars($g_status) . '</div>';
                    echo '</a>';
                };
                foreach ($golden_list as $g_login => $g_data) $render_gold_strip_item($g_login, $g_data);
                ?>
            </div>
        </div>
    </div>
    <script>
    (function() {
        const track = document.getElementById('goldStripTrack');
        if (!track) return;
        const viewport = track.parentElement;
        window.addEventListener('load', function() {
            if (track.scrollWidth > viewport.clientWidth + 4) {
                // Не помещается целиком — клонируем один раз для бесшовной прокрутки по кругу
                track.innerHTML += track.innerHTML;
                const fullWidth = track.scrollWidth / 2;
                const duration = Math.max(12, Math.round(fullWidth / 40));
                track.style.animationDuration = duration + 's';
                track.classList.add('scrolling');
            } else {
                // Все аватарки помещаются на экран — просто показываем их по центру без прокрутки
                track.classList.add('centered');
            }
        });
    })();
    </script>
    <?php endif; ?>

    <section id="leaks" class="reveal">
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

    <section id="rating" class="reveal">
        <h1>🏆 Рейтинг</h1>
        <p>Обладатели «Золотого билета RTeam» и игроки, прошедшие все испытания.</p>

        <div class="card">
            <h3>🎫 Золотой билет</h3>
            <?php
            $gold_holders = [];
            foreach ($users as $g_login => $g_data) { if (!empty($g_data["golden"])) $gold_holders[$g_login] = $g_data; }
            ?>
            <?php if (!$gold_holders): ?>
                <p style="color:#888;">Пока никто не получил золотой билет.</p>
            <?php else: ?>
                <ul style="list-style:none; padding:0; margin:0;">
                    <?php foreach ($gold_holders as $g_login => $g_data): ?>
                        <li style="padding:8px 0; border-bottom:1px solid #2a0000; display:flex; justify-content:space-between; align-items:center;">
                            <span><b><?=htmlspecialchars($g_login)?></b></span>
                            <span style="background:linear-gradient(135deg,#ffe066,#d4a017); color:#3a2a00; font-weight:bold; padding:2px 10px; border-radius:999px; font-size:11px;">🎫 <?=htmlspecialchars($g_data["golden_title"] ?? "Золотой билет RTeam")?></span>
                        </li>
                    <?php endforeach; ?>
                </ul>
            <?php endif; ?>
        </div>

        <?php if (!empty($squid_winners)): ?>
        <div class="card">
            <h3>🦑 Прошли «Игру в кальмара»</h3>
            <ul style="list-style:none; padding:0; margin:0;">
                <?php $sw_place = 0; foreach (array_reverse($squid_winners, true) as $sw_login => $sw_data): $sw_place++; ?>
                    <li style="padding:8px 0; border-bottom:1px solid #2a0000; display:flex; justify-content:space-between; align-items:center;">
                        <span>#<?=$sw_place?> <b><?=htmlspecialchars($sw_login)?></b></span>
                        <span style="color:#888; font-size:12px;"><?=htmlspecialchars($sw_data["completed_at"] ?? "")?></span>
                    </li>
                <?php endforeach; ?>
            </ul>
        </div>
        <?php endif; ?>
    </section>

    <section id="join" class="reveal">
        <h1>Подача заявки</h1>
        <?php if (empty($settings["recruit_open"])): ?>
            <p style="color:#ff5555;font-size:18px;">Набор временно закрыт.</p>
        <?php else: ?>
            <p class="muted" style="max-width:700px;">
                Решение придёт на email, указанный в заявке.
            </p>
            <div class="box" style="max-width:480px;">
                <div class="box-kicker">Набор открыт</div>
                <h3 class="box-title">Подать заявку</h3>
                <form action="apply.php" method="POST" class="styled-form">
                    <label class="field-label">Тип заявки</label>
                    <select name="type" id="typeSelect" required>
                        <option value="Команда">Вступление в команду</option>
                        <option value="Администратор">Заявка на администратора</option>
                    </select>
                    <div id="formFields"></div>
                    <button class="btn btn-block">Отправить заявку</button>
                </form>
            </div>
        <?php endif; ?>
    </section>

    <section id="contact" class="reveal">
        <h1>Контакты</h1>
        <p>Есть вопрос, идея или предложение по сотрудничеству? Напишите нам — ответим в ближайшее время.</p>
        <div class="auth-wrap">
            <div class="box contact-box">
                <div class="box-kicker">Обратная связь</div>
                <h3 class="box-title">Написать нам</h3>
                <form action="send.php" method="POST" class="styled-form">
                    <label class="field-label">Ваше имя</label>
                    <input type="text" name="name" placeholder="Как к вам обращаться" required>
                    <label class="field-label">Email</label>
                    <input type="email" name="email" placeholder="you@example.com" required>
                    <label class="field-label">Сообщение</label>
                    <textarea name="text" placeholder="Расскажите, чем можем помочь..." required></textarea>
                    <button class="btn btn-block">Отправить сообщение</button>
                </form>
            </div>

            <!-- ПАНЕЛЬ АККАУНТА: теперь вход/регистрация вынесены на отдельные страницы -->
            <div class="box account-box">
                <div class="box-kicker">Аккаунт</div>
                <?php if ($user): ?>
                    <h3 class="box-title">С возвращением, <?=htmlspecialchars($user)?>!</h3>
                    <p class="muted">Вы вошли как <b><?=htmlspecialchars($user)?></b> · роль «<?=htmlspecialchars($role)?>».</p>
                    <?php if ($is_golden): ?>
                        <a href="https://rteam.info/profile.php" class="btn btn-block btn-golden">🎫 Открыть золотой профиль</a>
                    <?php endif; ?>
                    <a href="https://rteam.info/cabinet.php" class="btn btn-block">Профиль</a>
                    <a href="?logout=1" class="btn btn-block btn-ghost">Выйти из аккаунта</a>
                <?php else: ?>
                    <h3 class="box-title">Вход и регистрация</h3>
                    <p class="muted">Создайте аккаунт RTeam, чтобы подавать заявки, участвовать в розыгрышах и следить за рейтингом. Вход администраторов дополнительно защищён 2FA через Telegram-бота.</p>
                    <a href="login.php" class="btn btn-block">Войти в аккаунт</a>
                    <a href="register.php" class="btn btn-block btn-ghost">Создать аккаунт</a>
                    <div class="divider"><span>или быстрый вход</span></div>
                    <a href="google_start.php" class="btn btn-block btn-google">
                        <svg width="18" height="18" viewBox="0 0 18 18" aria-hidden="true"><path fill="#4285F4" d="M17.64 9.2c0-.64-.06-1.25-.16-1.84H9v3.48h4.84a4.14 4.14 0 0 1-1.8 2.72v2.26h2.9c1.7-1.57 2.7-3.88 2.7-6.62z"/><path fill="#34A853" d="M9 18c2.43 0 4.47-.8 5.96-2.18l-2.9-2.26c-.8.54-1.84.86-3.06.86-2.35 0-4.34-1.59-5.05-3.72H.9v2.33A9 9 0 0 0 9 18z"/><path fill="#FBBC05" d="M3.95 10.7A5.4 5.4 0 0 1 3.66 9c0-.59.1-1.17.29-1.7V4.97H.9A9 9 0 0 0 0 9c0 1.45.35 2.83.9 4.03l3.05-2.33z"/><path fill="#EA4335" d="M9 3.58c1.32 0 2.51.45 3.44 1.35l2.58-2.58C13.46.89 11.43 0 9 0A9 9 0 0 0 .9 4.97l3.05 2.33C4.66 5.17 6.65 3.58 9 3.58z"/></svg>
                        Войти через Google
                    </a>
                <?php endif; ?>
            </div>
        </div>
    </section>

    <footer>
        © 2026 Rteam. Все права защищены.
    </footer>
</div>

<script>
/* ==========================================
   ОБЩИЕ ФУНКЦИИ (ДЛЯ ОБОИХ СЦЕНАРИЕВ)
========================================== */
const loopPhrases = [
    "sudo rm -rf /bugs",
    "loading Rteam modules...",
    "initializing core systems...",
    "checking security layer...",
    "compiling ideas...",
    "system online."
];
let loopIndex = 0;
let loopCharIndex = 0;

function startInfiniteCmdLoop() {
    const cmdContent = document.getElementById('cmd-content');
    if (!cmdContent) return;

    cmdContent.innerHTML = `<span style="color:#888;">C:\\RTEAM\\root> </span><span id="cmd-dynamic"></span>`;
    const dynamicSpan = document.getElementById("cmd-dynamic");
    
    const cursor = document.querySelector('.cmd-cursor');
    if (cursor) cursor.style.display = 'none';
    
    dynamicSpan.style.borderRight = '2px solid #ff4444';
    dynamicSpan.style.paddingRight = '4px';

    function typeLoop() {
        let text = loopPhrases[loopIndex];
        dynamicSpan.innerText = text.substring(0, loopCharIndex);

        if (loopCharIndex < text.length) {
            loopCharIndex++;
            setTimeout(typeLoop, 60);
        } else {
            setTimeout(() => {
                loopCharIndex = 0;
                loopIndex = (loopIndex + 1) % loopPhrases.length;
                typeLoop();
            }, 1200);
        }
    }
    typeLoop();
}

function initScrollReveals() {
    const reveals = document.querySelectorAll('.reveal');
    const observer = new IntersectionObserver((entries) => {
        entries.forEach(entry => {
            if(entry.isIntersecting) {
                entry.target.classList.add('active');
            }
        });
    }, { threshold: 0.15 });
    reveals.forEach(sec => observer.observe(sec));
}

/* ==========================================
   ЛОГИКА ЗАПУСКА / ПРОПУСКА АНИМАЦИИ
========================================== */
const cmdBox = document.getElementById('cmd-box');
const cmdContent = document.getElementById('cmd-content');
const overlay = document.getElementById('intro-overlay');
const mainContent = document.getElementById('main-content');
const cmdPlaceholder = document.getElementById('cmd-placeholder');
const introLogoWrap = document.getElementById('intro-logo-wrap');

// Проверяем, нужно ли пропустить интро (класс установлен выше в <head>/<body>)
const isSkip = document.body.classList.contains('skip-intro');

if (!isSkip) {
    // ЗАПИСЫВАЕМ В ПАМЯТЬ БРАУЗЕРА, ЧТОБЫ БОЛЬШЕ НЕ ПОКАЗЫВАТЬ
    localStorage.setItem('rteam_intro_played', '1');

    const bootLines = [
        "C:\\RTEAM\\root> sudo start Rteam_Portal",
        "[OK] loading Rteam modules...",
        "[OK] initializing core systems...",
        "[OK] checking security layer...",
        "[OK] compiling ideas...",
        "System online. Welcome to RTEAM."
    ];

    setTimeout(() => {
        introLogoWrap.style.opacity = '0';
        setTimeout(() => {
            introLogoWrap.style.display = 'none';
            cmdBox.classList.add('visible'); 
            startBootSequence(bootLines);
        }, 500);
    }, 2800); 

    function startBootSequence(lines) {
        let lineIndex = 0; let charIndex = 0;
        cmdContent.innerHTML = "";
        function typeNext() {
            if (lineIndex >= lines.length) { setTimeout(flyCmdToPlace, 800); return; }
            let currentStr = lines[lineIndex];
            if (charIndex === 0) {
                let newLine = document.createElement('div'); newLine.id = "boot-line-" + lineIndex;
                if (lineIndex === 0) newLine.style.color = "#888"; 
                if (lineIndex === lines.length - 1) newLine.style.color = "#00ff6a";
                cmdContent.appendChild(newLine);
            }
            document.getElementById("boot-line-" + lineIndex).innerText = currentStr.substring(0, charIndex + 1);
            charIndex++;
            if (charIndex >= currentStr.length) { charIndex = 0; lineIndex++; setTimeout(typeNext, 200); } 
            else { setTimeout(typeNext, 15 + Math.random() * 25); }
        }
        typeNext();
    }

    function flyCmdToPlace() {
        const startRect = cmdBox.getBoundingClientRect();
        
        mainContent.classList.add('visible');
        overlay.style.backgroundColor = 'transparent';
        overlay.style.pointerEvents = 'none';

        const endRect = cmdPlaceholder.getBoundingClientRect();

        cmdBox.classList.remove('cmd-intro-mode');
        cmdBox.style.position = 'fixed';
        cmdBox.style.top = startRect.top + 'px';
        cmdBox.style.left = startRect.left + 'px';
        cmdBox.style.width = startRect.width + 'px';
        cmdBox.style.margin = '0';
        cmdBox.style.transform = 'none';

        cmdBox.getBoundingClientRect(); // Принудительный пересчет браузером

        cmdBox.style.transition = 'all 1.2s cubic-bezier(0.25, 1, 0.3, 1)';
        cmdBox.style.top = endRect.top + 'px';
        cmdBox.style.left = endRect.left + 'px';
        cmdBox.style.width = endRect.width + 'px';

        setTimeout(() => {
            cmdBox.style.position = 'relative';
            cmdBox.style.top = 'auto';
            cmdBox.style.left = 'auto';
            cmdBox.style.width = '100%';
            cmdBox.style.transition = 'none';
            
            cmdPlaceholder.appendChild(cmdBox);
            document.body.classList.remove('no-scroll');
            overlay.style.display = 'none';

            startInfiniteCmdLoop();
            initScrollReveals();
        }, 1200);
    }
} else {
    // ЕСЛИ АНИМАЦИЯ БЫЛА ПРОПУЩЕНА — СРАЗУ ПОКАЗЫВАЕМ САЙТ И СТАВИМ ТЕРМИНАЛ НА МЕСТО
    cmdBox.classList.remove('cmd-intro-mode');
    cmdBox.style.position = 'relative';
    cmdBox.style.width = '100%';
    cmdBox.style.opacity = '1';
    cmdBox.style.transform = 'none';
    cmdBox.style.left = 'auto';
    cmdBox.style.top = 'auto';
    cmdPlaceholder.appendChild(cmdBox);

    startInfiniteCmdLoop();
    
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initScrollReveals);
    } else {
        initScrollReveals();
    }
}

/* ==========================================
   ДИНАМИЧЕСКИЙ ТЕКСТ И ФОРМЫ
========================================== */
const phrases = [
    "НОВЫЙ ВЗГЛЯД НА РАЗРАБОТКУ",
    "КОМАНДА, КОТОРАЯ ДЕЛАЕТ БОЛЬШЕ",
    "ТЕХНОЛОГИИ, КОТОРЫЕ МЕНЯЮТ ИГРУ",
    "КОД, КОТОРЫЙ ГОВОРИТ ГРОМЧЕ",
    "БУДУЩЕЕ СОЗДАЁТСЯ ЗДЕСЬ",
    "RTEAM — СИЛА В ИДЕЯХ"
];
let pIndex = 0;
const textEl = document.getElementById("dynamicText");

function changePhrase() {
    textEl.style.opacity = 0;
    setTimeout(() => {
        textEl.innerText = phrases[pIndex];
        const colors = ["#ff2a2a","#ff6b00","#ff00c8","#00eaff","#00ff6a"];
        const c = colors[pIndex % colors.length];
        textEl.style.color = c;
        textEl.style.textShadow = `0 0 20px ${c}aa`;
        textEl.style.opacity = 1;
        pIndex = (pIndex + 1) % phrases.length;
    }, 600);
}
setInterval(changePhrase, 3000);
changePhrase();

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
            <label class="field-label">${q}</label>
            <input type="text" name="${name}" required>
        `;
    });
}
if (typeSelect) {
    typeSelect.addEventListener("change", () => render(typeSelect.value));
    render("Команда");
}

// ===== КНОПКА "НАВЕРХ" =====
(function() {
    const btn = document.getElementById('back-to-top');
    if (!btn) return;
    const scrollHost = document.getElementById('main-content') || window;
    function toggle() {
        const y = (scrollHost === window) ? window.scrollY : scrollHost.scrollTop;
        btn.classList.toggle('show', y > 400);
    }
    scrollHost.addEventListener('scroll', toggle);
    window.addEventListener('scroll', toggle);
    btn.addEventListener('click', () => {
        if (scrollHost === window) window.scrollTo({ top: 0, behavior: 'smooth' });
        else scrollHost.scrollTo({ top: 0, behavior: 'smooth' });
        document.getElementById('home')?.scrollIntoView({ behavior: 'smooth' });
    });
})();

// ===== ИГРА В КАЛЬМАРА: 3 сезона, в каждом на выбор 3 игры =====
(function() {
    const inviteEl = document.getElementById('squid-invite-text');
    if (!inviteEl) return; // тема "Игра в кальмара" не активна

    const modal = document.getElementById('squid-game-modal');
    const closeEl = document.getElementById('squid-game-close');

    const screens = {
        guest:      document.getElementById('squid-screen-guest'),
        paused:     document.getElementById('squid-screen-paused'),
        invite:     document.getElementById('squid-screen-invite'),
        picker:     document.getElementById('squid-screen-picker'),
        transition: document.getElementById('squid-screen-transition'),
        s1:         document.getElementById('squid-screen-s1'),
        tug:        document.getElementById('squid-screen-tug'),
        memory:     document.getElementById('squid-screen-memory'),
        s2:         document.getElementById('squid-screen-s2'),
        gauge:      document.getElementById('squid-screen-gauge'),
        simon:      document.getElementById('squid-screen-simon'),
        s3:         document.getElementById('squid-screen-s3'),
        shell:      document.getElementById('squid-screen-shell'),
        duel:       document.getElementById('squid-screen-duel'),
        win:        document.getElementById('squid-screen-win'),
    };
    function showScreen(name) {
        Object.values(screens).forEach(el => el.classList.add('squid-hidden'));
        screens[name].classList.remove('squid-hidden');
    }

    const LOGGED_IN = <?=json_encode((bool)$user)?>;
    const STATE = <?=json_encode([
        "paused"    => !empty($squid_game["paused"]),
        "season"    => $user ? ($squid_game["progress"][$user]["season"] ?? 0) : 0,
        "completed" => $user ? !empty($squid_game["progress"][$user]["completed"]) : false,
    ])?>;

    function reportSeason(season) {
        return fetch('index.php', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: 'squid_action=complete_season&season=' + season
        }).then(r => r.json());
    }

    function seasonWon(season) {
        reportSeason(season).then(() => {
            if (season < 3) {
                const nextNames = { 1: 'Дальгона / Точный укол / Повтори последовательность', 2: 'Стеклянный мост / Напёрстки / Финальная дуэль' };
                goTransition('Сезон ' + season + ' пройден', 'Начинаются игры сезона ' + (season + 1) + ': ' + nextNames[season] + '. Выбери одну из трёх игр.', () => showPicker(season + 1));
            } else {
                showScreen('win');
            }
        });
    }

    function openModal() {
        modal.classList.add('show');
        if (!LOGGED_IN) { showScreen('guest'); return; }
        if (STATE.paused) { showScreen('paused'); return; }
        if (STATE.completed || STATE.season >= 3) { showScreen('win'); return; }
        if (STATE.season >= 1) { showPicker(STATE.season + 1); return; }
        showScreen('invite');
    }
    function closeModal() { modal.classList.remove('show'); }

    inviteEl.addEventListener('click', openModal);
    closeEl.addEventListener('click', closeModal);
    modal.addEventListener('click', (e) => { if (e.target === modal) closeModal(); });
    document.getElementById('squid-start-btn').addEventListener('click', () => showPicker(1));

    function goTransition(title, text, nextFn) {
        document.getElementById('squid-transition-title').textContent = title;
        document.getElementById('squid-transition-text').textContent = text;
        const nextBtn = document.getElementById('squid-transition-next');
        const clone = nextBtn.cloneNode(true); // сброс старых обработчиков
        nextBtn.parentNode.replaceChild(clone, nextBtn);
        clone.addEventListener('click', nextFn);
        showScreen('transition');
    }

    /* ---------- ВЫБОР ИГРЫ СЕЗОНА ---------- */
    const SEASON_GAMES = {
        1: [
            { icon: '🚦', name: 'Красный свет, зелёный свет', desc: 'Двигайся на зелёном, замри на красном.', start: startRedLight },
            { icon: '🪢', name: 'Перетягивание каната', desc: 'Жми как можно быстрее, чтобы перетянуть канат.', start: startTug },
            { icon: '🪵', name: 'Скрипучие половицы', desc: 'Запомни и повтори безопасный путь по плитам.', start: startMemory },
        ],
        2: [
            { icon: '🍬', name: 'Дальгона', desc: 'Вырезай фигуру, попадая в зелёную зону.', start: startDalgona },
            { icon: '🎯', name: 'Точный укол', desc: 'Останови стрелку в зелёной зоне 3 раза подряд.', start: startGauge },
            { icon: '🔵', name: 'Повтори последовательность', desc: 'Запоминай и повторяй растущую цепочку цветов.', start: startSimon },
        ],
        3: [
            { icon: '🌉', name: 'Стеклянный мост', desc: 'Выбирай закалённые панели, чтобы дойти до конца.', start: startBridge },
            { icon: '🥤', name: 'Напёрстки', desc: 'Угадай, под каким стаканом шарик, 3 раунда из 5.', start: startShell },
            { icon: '⚔️', name: 'Финальная дуэль', desc: 'Реагируй быстрее соперника 2 раза из 3.', start: startDuel },
        ],
    };
    function showPicker(season) {
        document.getElementById('squid-picker-season-label').textContent = 'Сезон ' + season + ' из 3';
        const list = document.getElementById('squid-picker-list');
        list.innerHTML = '';
        SEASON_GAMES[season].forEach(g => {
            const card = document.createElement('div');
            card.className = 'squid-pick-card';
            card.innerHTML = '<div class="squid-pick-icon">' + g.icon + '</div><div><b>' + g.name + '</b><span>' + g.desc + '</span></div>';
            card.addEventListener('click', () => g.start(season));
            list.appendChild(card);
        });
        showScreen('picker');
    }

    /* ---------- СЕЗОН 1, ИГРА 1: КРАСНЫЙ СВЕТ, ЗЕЛЁНЫЙ СВЕТ ---------- */
    function startRedLight(season) {
        showScreen('s1');
        const lightEl = document.getElementById('squid-game-light');
        const player = document.getElementById('squid-game-player');
        const moveBtn = document.getElementById('squid-game-move');
        const restartBtn = document.getElementById('squid-game-restart');
        const statusEl = document.getElementById('squid-game-status');
        const field = document.getElementById('squid-game-field');

        let isGreen = true, position = 10, finished = false, lightTimer = null, holdTimer = null;
        function fieldWidth() { return field.clientWidth - 60; }
        function toggleLight() {
            isGreen = !isGreen;
            lightEl.textContent = isGreen ? '🟢' : '🔴';
            lightTimer = setTimeout(toggleLight, isGreen ? (900 + Math.random()*1200) : (1200 + Math.random()*1600));
        }
        function reset() {
            clearTimeout(lightTimer); clearInterval(holdTimer);
            position = 10; finished = false; isGreen = true;
            lightEl.textContent = '🟢'; player.style.left = position + 'px';
            statusEl.textContent = 'Жми и держи «Идти», пока горит зелёный. На красном — замри, иначе проигрыш.';
            statusEl.style.color = '#ccc';
            moveBtn.style.display = 'inline-block'; restartBtn.style.display = 'none';
            lightTimer = setTimeout(toggleLight, 1500 + Math.random()*1000);
        }
        function lose() {
            finished = true; clearTimeout(lightTimer); clearInterval(holdTimer);
            statusEl.textContent = '💥 Тебя заметили на красном свете! Игра окончена.';
            statusEl.style.color = '#ff4444';
            moveBtn.style.display = 'none'; restartBtn.style.display = 'inline-block';
        }
        function win() {
            finished = true; clearTimeout(lightTimer); clearInterval(holdTimer);
            seasonWon(season);
        }
        function startMoving() {
            if (finished) return;
            holdTimer = setInterval(() => {
                if (finished) return;
                if (!isGreen) { lose(); return; }
                position += 4; player.style.left = position + 'px';
                if (position >= fieldWidth()) win();
            }, 40);
        }
        function stopMoving() { clearInterval(holdTimer); }
        moveBtn.onmousedown = startMoving;
        moveBtn.ontouchstart = (e) => { e.preventDefault(); startMoving(); };
        ['mouseup','mouseleave','touchend','touchcancel'].forEach(ev => moveBtn['on'+ev] = stopMoving);
        restartBtn.onclick = reset;
        reset();
    }

    /* ---------- СЕЗОН 1, ИГРА 2: ПЕРЕТЯГИВАНИЕ КАНАТА ---------- */
    function startTug(season) {
        showScreen('tug');
        const fill = document.getElementById('squid-tug-fill');
        const marker = document.getElementById('squid-tug-marker');
        const pullBtn = document.getElementById('squid-tug-pull');
        const restartBtn = document.getElementById('squid-tug-restart');
        const statusEl = document.getElementById('squid-tug-status');

        let meter = 50, finished = false, aiTimer = null;
        function render() {
            fill.style.width = meter + '%';
            marker.style.left = meter + '%';
        }
        function reset() {
            clearInterval(aiTimer);
            meter = 50; finished = false;
            statusEl.textContent = 'Жми «Тяни!» как можно быстрее — соперник тянет канат на свою сторону.';
            statusEl.style.color = '#ccc';
            pullBtn.style.display = 'inline-block'; restartBtn.style.display = 'none';
            render();
            aiTimer = setInterval(() => {
                if (finished) return;
                meter -= 2.2;
                if (meter <= 0) { meter = 0; render(); lose(); return; }
                render();
            }, 350);
        }
        function lose() {
            finished = true; clearInterval(aiTimer);
            statusEl.textContent = '💥 Соперник перетянул канат! Игра окончена.';
            statusEl.style.color = '#ff4444';
            pullBtn.style.display = 'none'; restartBtn.style.display = 'inline-block';
        }
        function win() {
            finished = true; clearInterval(aiTimer);
            seasonWon(season);
        }
        pullBtn.addEventListener('click', () => {
            if (finished) return;
            meter += 4.5;
            if (meter >= 100) { meter = 100; render(); win(); return; }
            render();
        });
        restartBtn.addEventListener('click', reset);
        reset();
    }

    /* ---------- СЕЗОН 1, ИГРА 3: СКРИПУЧИЕ ПОЛОВИЦЫ ---------- */
    function startMemory(season) {
        showScreen('memory');
        const grid = document.getElementById('squid-memory-grid');
        const restartBtn = document.getElementById('squid-memory-restart');
        const statusEl = document.getElementById('squid-memory-status');

        const SIZE = 9, PATH_LEN = 5;
        let path = [], userStep = 0, finished = false, tiles = [];

        function reset() {
            finished = false; userStep = 0;
            statusEl.textContent = 'Запоминай...';
            statusEl.style.color = '#ccc';
            restartBtn.style.display = 'none';
            grid.innerHTML = '';
            tiles = [];
            path = [];
            const pool = Array.from({length: SIZE}, (_, i) => i);
            for (let i = 0; i < PATH_LEN; i++) {
                const idx = Math.floor(Math.random() * pool.length);
                path.push(pool[idx]);
                pool.splice(idx, 1);
            }
            for (let i = 0; i < SIZE; i++) {
                const t = document.createElement('div');
                t.className = 'squid-memory-tile';
                t.dataset.i = i;
                t.addEventListener('click', () => onTileClick(i, t));
                grid.appendChild(t);
                tiles.push(t);
            }
            showPathThenPlay();
        }
        function showPathThenPlay() {
            let i = 0;
            const timer = setInterval(() => {
                if (i > 0) tiles[path[i-1]].classList.remove('lit');
                if (i < path.length) { tiles[path[i]].classList.add('lit'); i++; }
                else {
                    clearInterval(timer);
                    statusEl.textContent = 'Повтори путь, нажимая на плитки в том же порядке.';
                }
            }, 550);
        }
        function onTileClick(i, el) {
            if (finished || userStep >= path.length) return;
            if (i === path[userStep]) {
                el.classList.add('correct');
                userStep++;
                if (userStep >= path.length) win();
            } else {
                el.classList.add('wrong');
                lose();
            }
        }
        function lose() {
            finished = true;
            statusEl.textContent = '💥 Половица скрипнула! Игра окончена.';
            statusEl.style.color = '#ff4444';
            restartBtn.style.display = 'inline-block';
        }
        function win() {
            finished = true;
            seasonWon(season);
        }
        restartBtn.addEventListener('click', reset);
        reset();
    }

    /* ---------- СЕЗОН 2, ИГРА 1: ДАЛЬГОНА ---------- */
    function startDalgona(season) {
        showScreen('s2');
        const track = document.getElementById('squid-dalgona-track');
        const zone = document.getElementById('squid-dalgona-zone');
        const marker = document.getElementById('squid-dalgona-marker');
        const cutBtn = document.getElementById('squid-dalgona-cut');
        const restartBtn = document.getElementById('squid-dalgona-restart');
        const statusEl = document.getElementById('squid-dalgona-status');
        const livesEl = document.getElementById('squid-dalgona-lives');
        const fill = document.getElementById('squid-dalgona-progress-fill');

        let lives = 3, cuts = 0, needed = 6, pos = 0, dir = 1, speed = 2.2, raf = null, finished = false;

        function layoutZone() {
            const w = track.clientWidth;
            const zoneW = Math.max(36, w * 0.22);
            zone.style.width = zoneW + 'px';
            zone.style.left = ((w - zoneW) / 2) + 'px';
            return { w, zoneW, left: (w - zoneW) / 2, right: (w - zoneW) / 2 + zoneW };
        }
        let bounds = layoutZone();

        function tick() {
            pos += dir * speed;
            if (pos <= 0) { pos = 0; dir = 1; }
            if (pos >= bounds.w - 4) { pos = bounds.w - 4; dir = -1; }
            marker.style.left = pos + 'px';
            raf = requestAnimationFrame(tick);
        }
        function reset() {
            cancelAnimationFrame(raf);
            lives = 3; cuts = 0; pos = 0; dir = 1; speed = 2.2; finished = false;
            bounds = layoutZone();
            livesEl.textContent = '❤️❤️❤️';
            fill.style.width = '0%';
            statusEl.textContent = 'Жми «Вырезать» ровно тогда, когда белая полоска внутри зелёной зоны. 3 ошибки — и печенье треснет.';
            statusEl.style.color = '#ccc';
            cutBtn.style.display = 'inline-block'; restartBtn.style.display = 'none';
            raf = requestAnimationFrame(tick);
        }
        function lose() {
            finished = true; cancelAnimationFrame(raf);
            statusEl.textContent = '💥 Печенье треснуло! Игра окончена.';
            statusEl.style.color = '#ff4444';
            cutBtn.style.display = 'none'; restartBtn.style.display = 'inline-block';
        }
        function win() {
            finished = true; cancelAnimationFrame(raf);
            seasonWon(season);
        }
        cutBtn.addEventListener('click', () => {
            if (finished) return;
            const markerCenter = pos + 2;
            if (markerCenter >= bounds.left && markerCenter <= bounds.right) {
                cuts++;
                fill.style.width = Math.min(100, Math.round(cuts / needed * 100)) + '%';
                speed += 0.35;
                if (cuts >= needed) win();
            } else {
                lives--;
                livesEl.textContent = '❤️'.repeat(Math.max(0, lives)) + '🖤'.repeat(3 - Math.max(0, lives));
                if (lives <= 0) lose();
            }
        });
        restartBtn.addEventListener('click', reset);
        reset();
    }

    /* ---------- СЕЗОН 2, ИГРА 2: ТОЧНЫЙ УКОЛ ---------- */
    function startGauge(season) {
        showScreen('gauge');
        const track = document.getElementById('squid-gauge-track');
        const zone = document.getElementById('squid-gauge-zone');
        const marker = document.getElementById('squid-gauge-marker');
        const hitBtn = document.getElementById('squid-gauge-hit');
        const restartBtn = document.getElementById('squid-gauge-restart');
        const statusEl = document.getElementById('squid-gauge-status');
        const livesEl = document.getElementById('squid-gauge-lives');

        let lives = 2, streak = 0, needed = 3, pos = 0, dir = 1, speed = 3, raf = null, finished = false;

        function layoutZone() {
            const w = track.clientWidth;
            const zoneW = Math.max(30, w * 0.18);
            const left = Math.random() * (w - zoneW);
            zone.style.width = zoneW + 'px';
            zone.style.left = left + 'px';
            return { w, zoneW, left, right: left + zoneW };
        }
        let bounds = layoutZone();

        function tick() {
            pos += dir * speed;
            if (pos <= 0) { pos = 0; dir = 1; }
            if (pos >= bounds.w - 4) { pos = bounds.w - 4; dir = -1; }
            marker.style.left = pos + 'px';
            raf = requestAnimationFrame(tick);
        }
        function reset() {
            cancelAnimationFrame(raf);
            lives = 2; streak = 0; pos = 0; dir = 1; speed = 3; finished = false;
            bounds = layoutZone();
            livesEl.textContent = '❤️❤️';
            statusEl.textContent = 'Жми «Удар», когда стрелка в зелёной зоне. Нужно 3 попадания подряд, промахов разрешено 2.';
            statusEl.style.color = '#ccc';
            hitBtn.style.display = 'inline-block'; restartBtn.style.display = 'none';
            raf = requestAnimationFrame(tick);
        }
        function lose() {
            finished = true; cancelAnimationFrame(raf);
            statusEl.textContent = '💥 Промах! Игра окончена.';
            statusEl.style.color = '#ff4444';
            hitBtn.style.display = 'none'; restartBtn.style.display = 'inline-block';
        }
        function win() {
            finished = true; cancelAnimationFrame(raf);
            seasonWon(season);
        }
        hitBtn.addEventListener('click', () => {
            if (finished) return;
            const center = pos + 2;
            if (center >= bounds.left && center <= bounds.right) {
                streak++;
                if (streak >= needed) { win(); return; }
                bounds = layoutZone();
            } else {
                lives--;
                streak = 0;
                livesEl.textContent = '❤️'.repeat(Math.max(0, lives)) + '🖤'.repeat(2 - Math.max(0, lives));
                if (lives <= 0) { lose(); return; }
                bounds = layoutZone();
            }
        });
        restartBtn.addEventListener('click', reset);
        reset();
    }

    /* ---------- СЕЗОН 2, ИГРА 3: ПОВТОРИ ПОСЛЕДОВАТЕЛЬНОСТЬ (SIMON) ---------- */
    function startSimon(season) {
        showScreen('simon');
        const btns = Array.from(document.querySelectorAll('#squid-simon-grid .squid-simon-btn'));
        const restartBtn = document.getElementById('squid-simon-restart');
        const statusEl = document.getElementById('squid-simon-status');

        const TARGET_LEN = 6;
        let sequence = [], userStep = 0, finished = false, accepting = false;

        function reset() {
            finished = false; userStep = 0; accepting = false;
            sequence = [Math.floor(Math.random()*4)];
            statusEl.textContent = 'Смотри...';
            statusEl.style.color = '#ccc';
            restartBtn.style.display = 'none';
            playSequence();
        }
        function playSequence() {
            accepting = false;
            let i = 0;
            const timer = setInterval(() => {
                btns.forEach(b => b.classList.remove('active'));
                if (i < sequence.length) {
                    btns[sequence[i]].classList.add('active');
                    setTimeout(() => btns[sequence[i]].classList.remove('active'), 300);
                    i++;
                } else {
                    clearInterval(timer);
                    accepting = true;
                    userStep = 0;
                    statusEl.textContent = 'Повтори последовательность (' + sequence.length + ' из ' + TARGET_LEN + ').';
                }
            }, 550);
        }
        btns.forEach(b => b.addEventListener('click', () => {
            if (finished || !accepting) return;
            const c = parseInt(b.dataset.c, 10);
            b.classList.add('active');
            setTimeout(() => b.classList.remove('active'), 200);
            if (c === sequence[userStep]) {
                userStep++;
                if (userStep >= sequence.length) {
                    if (sequence.length >= TARGET_LEN) { win(); return; }
                    accepting = false;
                    sequence.push(Math.floor(Math.random()*4));
                    statusEl.textContent = 'Смотри...';
                    setTimeout(playSequence, 700);
                }
            } else {
                lose();
            }
        }));
        function lose() {
            finished = true; accepting = false;
            statusEl.textContent = '💥 Ошибка в последовательности! Игра окончена.';
            statusEl.style.color = '#ff4444';
            restartBtn.style.display = 'inline-block';
        }
        function win() {
            finished = true; accepting = false;
            seasonWon(season);
        }
        restartBtn.addEventListener('click', reset);
        reset();
    }

    /* ---------- СЕЗОН 3, ИГРА 1: СТЕКЛЯННЫЙ МОСТ ---------- */
    function startBridge(season) {
        showScreen('s3');
        const stepsEl = document.getElementById('squid-bridge-steps');
        const statusEl = document.getElementById('squid-bridge-status');
        const livesEl = document.getElementById('squid-bridge-lives');
        const restartBtn = document.getElementById('squid-bridge-restart');

        const TOTAL_STEPS = 6;
        let lives = 2, step = 0, finished = false, safeSide = [];

        function reset() {
            lives = 2; step = 0; finished = false;
            safeSide = Array.from({length: TOTAL_STEPS}, () => Math.random() < 0.5 ? 'left' : 'right');
            livesEl.textContent = '❤️❤️';
            statusEl.textContent = 'Выбирай левую или правую панель. Закалённое стекло выдержит, обычное — разобьётся. У тебя 2 жизни.';
            statusEl.style.color = '#ccc';
            restartBtn.style.display = 'none';
            stepsEl.innerHTML = '';
            renderStep();
        }
        function renderStep() {
            if (step >= TOTAL_STEPS) { win(); return; }
            const row = document.createElement('div');
            row.className = 'squid-bridge-step';
            row.dataset.step = step;
            row.innerHTML = `
                <div class="squid-bridge-tile" data-side="left">⬜</div>
                <div class="squid-bridge-tile" data-side="right">⬜</div>
            `;
            stepsEl.appendChild(row);
            row.querySelectorAll('.squid-bridge-tile').forEach(tile => {
                tile.addEventListener('click', () => choose(row, tile.dataset.side));
            });
        }
        function choose(row, side) {
            if (finished) return;
            const correct = safeSide[step];
            row.querySelectorAll('.squid-bridge-tile').forEach(t => t.classList.add('disabled'));
            row.querySelector(`[data-side="${side}"]`).classList.add(side === correct ? 'chosen-safe' : 'chosen-broken');
            row.querySelector(`[data-side="${side}"]`).textContent = side === correct ? '💎' : '💥';
            if (side !== correct) {
                lives--;
                livesEl.textContent = '❤️'.repeat(Math.max(0, lives)) + '🖤'.repeat(2 - Math.max(0, lives));
                if (lives <= 0) { lose(); return; }
            }
            step++;
            setTimeout(renderStep, 350);
        }
        function lose() {
            finished = true;
            statusEl.textContent = '💥 Стекло разбилось! Игра окончена.';
            statusEl.style.color = '#ff4444';
            restartBtn.style.display = 'inline-block';
        }
        function win() {
            finished = true;
            seasonWon(season);
        }
        restartBtn.addEventListener('click', reset);
        reset();
    }

    /* ---------- СЕЗОН 3, ИГРА 2: НАПЁРСТКИ ---------- */
    function startShell(season) {
        showScreen('shell');
        const cups = Array.from(document.querySelectorAll('#squid-shell-cups .squid-cup'));
        const restartBtn = document.getElementById('squid-shell-restart');
        const statusEl = document.getElementById('squid-shell-status');

        const TOTAL_ROUNDS = 5, NEEDED = 3;
        let round = 0, correct = 0, wrong = 0, finished = false, ballAt = 0, locked = false;

        function reset() {
            round = 0; correct = 0; wrong = 0; finished = false;
            restartBtn.style.display = 'none';
            nextRound();
        }
        function nextRound() {
            if (round >= TOTAL_ROUNDS || correct >= NEEDED || wrong > (TOTAL_ROUNDS - NEEDED)) {
                if (correct >= NEEDED) win(); else lose();
                return;
            }
            round++;
            ballAt = Math.floor(Math.random() * 3);
            locked = false;
            cups.forEach(c => { c.textContent = '🥤'; c.classList.remove('disabled'); });
            statusEl.textContent = 'Раунд ' + round + ' из ' + TOTAL_ROUNDS + '. Верно: ' + correct + ', неверно: ' + wrong + '. Угадай стакан с шариком.';
            statusEl.style.color = '#ccc';
        }
        cups.forEach(c => c.addEventListener('click', () => {
            if (finished || locked) return;
            locked = true;
            const guess = parseInt(c.dataset.i, 10);
            cups.forEach((cc, i) => { if (i === ballAt) cc.textContent = '🎱'; });
            cups.forEach(cc => cc.classList.add('disabled'));
            if (guess === ballAt) correct++; else wrong++;
            setTimeout(nextRound, 700);
        }));
        function lose() {
            finished = true;
            statusEl.textContent = '💥 Недостаточно верных ответов! Игра окончена.';
            statusEl.style.color = '#ff4444';
            restartBtn.style.display = 'inline-block';
        }
        function win() {
            finished = true;
            seasonWon(season);
        }
        restartBtn.addEventListener('click', reset);
        reset();
    }

    /* ---------- СЕЗОН 3, ИГРА 3: ФИНАЛЬНАЯ ДУЭЛЬ ---------- */
    function startDuel(season) {
        showScreen('duel');
        const btn = document.getElementById('squid-duel-btn');
        const restartBtn = document.getElementById('squid-duel-restart');
        const statusEl = document.getElementById('squid-duel-status');
        const scoreEl = document.getElementById('squid-duel-score');

        let myScore = 0, oppScore = 0, finished = false, waitTimer = null, roundActive = false, tooSoon = false, startedAt = 0;

        function reset() {
            myScore = 0; oppScore = 0; finished = false;
            restartBtn.style.display = 'none';
            scoreEl.textContent = 'Ты: 0 — Соперник: 0';
            statusEl.textContent = 'Жди сигнал «ДАВАЙ!» и жми кнопку как можно быстрее. Побеждает тот, кто выиграл 2 раунда из 3.';
            nextRound();
        }
        function nextRound() {
            roundActive = false; tooSoon = false;
            btn.textContent = 'Жди...';
            btn.disabled = false;
            const delay = 1200 + Math.random() * 2200;
            waitTimer = setTimeout(() => {
                roundActive = true;
                startedAt = Date.now();
                btn.textContent = 'ДАВАЙ! 👊';
            }, delay);
        }
        btn.addEventListener('click', () => {
            if (finished) return;
            if (!roundActive) {
                tooSoon = true;
                clearTimeout(waitTimer);
                statusEl.textContent = '⚠️ Рано! Соперник забирает раунд.';
                oppScore++;
                afterRound();
                return;
            }
            const reaction = Date.now() - startedAt;
            const oppReaction = 250 + Math.random() * 350;
            if (reaction <= oppReaction) {
                myScore++;
                statusEl.textContent = 'Твоя реакция: ' + reaction + ' мс — ты быстрее!';
            } else {
                oppScore++;
                statusEl.textContent = 'Твоя реакция: ' + reaction + ' мс — соперник быстрее.';
            }
            afterRound();
        });
        function afterRound() {
            roundActive = false;
            scoreEl.textContent = 'Ты: ' + myScore + ' — Соперник: ' + oppScore;
            if (myScore >= 2) { win(); return; }
            if (oppScore >= 2) { lose(); return; }
            btn.disabled = true;
            setTimeout(() => { btn.disabled = false; nextRound(); }, 1200);
        }
        function lose() {
            finished = true; clearTimeout(waitTimer);
            statusEl.textContent = '💥 Соперник победил в дуэли! Игра окончена.';
            statusEl.style.color = '#ff4444';
            btn.style.display = 'none'; restartBtn.style.display = 'inline-block';
        }
        function win() {
            finished = true; clearTimeout(waitTimer);
            seasonWon(season);
        }
        restartBtn.addEventListener('click', () => { btn.style.display = 'inline-block'; reset(); });
        reset();
    }
})();
</script>

</body>
</html>