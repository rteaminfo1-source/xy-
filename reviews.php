<?php
/* ==========================================================
   ОТЗЫВЫ КЛИЕНТОВ: средняя оценка, распределение по звёздам,
   фильтры, ответы команды и форма «Оставить отзыв».
   Отзывы по завершённым заказам помечаются «Проверенный заказ»
   (и публикуются сразу, если это включено в админке), остальные —
   после проверки во вкладке «Отзывы» админ-панели.
   ========================================================== */
session_start();
$account = (isset($_SESSION['user']) && is_string($_SESSION['user']) && $_SESSION['user'] !== '') ? $_SESSION['user'] : null;
session_write_close();

require_once __DIR__ . '/_orders_lib.php';

/* ---------- приём отзыва (fetch с этой же страницы) ---------- */
if (($_GET['a'] ?? '') === 'submit') {
    header('Content-Type: application/json; charset=utf-8');
    $fail = function ($m, $c = 400) { http_response_code($c); echo json_encode(['ok' => false, 'error' => $m], JSON_UNESCAPED_UNICODE); exit; };
    if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST' || ($_SERVER['HTTP_X_SO'] ?? '') !== '1') $fail('Обновите страницу и попробуйте ещё раз.', 403);
    if (trim((string)($_POST['website'] ?? '')) !== '') { echo json_encode(['ok' => true, 'published' => false]); exit; }
    $ts = (int)($_POST['ts'] ?? 0);
    if ($ts > 0 && time() - $ts < 4) $fail('Слишком быстро — перечитайте отзыв и отправьте ещё раз.');
    $rating = (int)($_POST['rating'] ?? 0);
    if ($rating < 1 || $rating > 5) $fail('Поставьте оценку от 1 до 5 звёзд.');
    $name = so_clip($_POST['name'] ?? '', 60);
    if (mb_strlen($name) < 2) $fail('Как вас подписать?');
    $text = so_clip($_POST['text'] ?? '', 2000);
    if (mb_strlen($text) < 15) $fail('Расскажите чуть подробнее — хотя бы пару предложений.');
    $order = null;
    $oid = (int)($_POST['o'] ?? 0);
    if ($oid) {
        $o = so_order_load($oid);
        $t = (string)($_POST['t'] ?? '');
        if ($o && (($t !== '' && hash_equals((string)$o['token'], $t)) || ($account !== null && ($o['client']['account'] ?? null) === $account))) {
            if (!empty($o['review_id'])) $fail('По этому заказу отзыв уже есть — спасибо!');
            $order = $o;
        }
    }
    if (!so_rate_ok('review:' . so_ip(), 3, 3600)) $fail('Вы уже оставили отзыв недавно. Попробуйте позже.', 429);
    $rv = so_review_add($name, $rating, $text, $order);
    if ($order) {
        so_order_update($order['id'], function (&$x) use ($rv) { $x['review_id'] = $rv['id']; so_bump($x); return true; });
    }
    echo json_encode(['ok' => true, 'published' => $rv['status'] === 'published'], JSON_UNESCAPED_UNICODE);
    exit;
}

$accent  = so_accent();
$items   = so_reviews_published();
$stats   = so_review_stats($items);
$verified_n = count(array_filter($items, function ($r) { return !empty($r['verified']); }));
$replied_n  = count(array_filter($items, function ($r) { return !empty($r['reply']['text']); }));

// Пришёл по ссылке из заказа — отзыв будет «проверенным»
$from_order = null;
$qo = (int)($_GET['o'] ?? 0); $qt = (string)($_GET['t'] ?? '');
if ($qo) {
    $o = so_order_load($qo);
    if ($o && (($qt !== '' && hash_equals((string)$o['token'], $qt)) || ($account !== null && ($o['client']['account'] ?? null) === $account)) && empty($o['review_id'])) $from_order = $o;
}

$months = ['января', 'февраля', 'марта', 'апреля', 'мая', 'июня', 'июля', 'августа', 'сентября', 'октября', 'ноября', 'декабря'];
$fmt = function ($t) use ($months) { return (int)date('j', $t) . ' ' . $months[(int)date('n', $t) - 1] . ' ' . date('Y', $t); };
$plural = function ($n, $a, $b, $c) { $n = abs($n) % 100; $n1 = $n % 10; return ($n > 10 && $n < 20) ? $c : ($n1 > 1 && $n1 < 5 ? $b : ($n1 === 1 ? $a : $c)); };
header('Referrer-Policy: strict-origin-when-cross-origin');
header('X-Content-Type-Options: nosniff');
?>
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="referrer" content="strict-origin-when-cross-origin">
<title>Отзывы клиентов — RTeam</title>
<meta name="description" content="Отзывы клиентов команды RTeam: средняя оценка <?= so_h(number_format($stats['avg'], 1, ',', '')) ?> из 5 по <?= (int)$stats['count'] ?> отзывам. Лендинги, магазины, корпоративные сайты и веб-сервисы.">
<link rel="icon" href="favicon.jpeg" type="image/jpeg">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800;900&display=swap" rel="stylesheet">
<style>
:root {
    --accent: <?= so_h($accent) ?>; --accent-2: color-mix(in srgb, var(--accent) 55%, #ff9a3c); --gold: #fbbf24;
    --bg: #06060a; --panel: rgba(18,18,27,.78); --line: rgba(255,255,255,.08); --line-2: rgba(255,255,255,.14);
    --text: #ececf3; --soft: #b6b6c6; --muted: #777789; --ok: #22c55e; --shadow: 0 30px 80px -40px rgba(0,0,0,.9);
}
* { box-sizing: border-box; }
html { scroll-behavior: smooth; }
body { margin: 0; min-height: 100vh; font-family: Inter, "Segoe UI", system-ui, -apple-system, Arial, sans-serif; color: var(--text); background: var(--bg);
    font-size: 15px; line-height: 1.55; -webkit-font-smoothing: antialiased; letter-spacing: -.006em; overflow-x: hidden; }
a { color: color-mix(in srgb, var(--accent) 65%, #fff); text-decoration: none; }
button, input, textarea, select { font: inherit; color: inherit; }
[hidden] { display: none !important; }
.bg { position: fixed; inset: 0; z-index: -1; overflow: hidden; pointer-events: none; }
.bg::before, .bg::after { content: ""; position: absolute; width: 70vmax; height: 70vmax; border-radius: 50%; filter: blur(90px); opacity: .45; }
.bg::before { background: radial-gradient(circle, rgba(251,191,36,.22), transparent 62%); top: -38vmax; left: 10vmax; animation: drift 24s ease-in-out infinite alternate; }
.bg::after { background: radial-gradient(circle, color-mix(in srgb, var(--accent) 30%, transparent), transparent 62%); bottom: -42vmax; right: -20vmax; animation: drift 30s ease-in-out infinite alternate-reverse; }
.bg i { position: absolute; inset: 0; background-image: radial-gradient(rgba(255,255,255,.05) 1px, transparent 1px); background-size: 26px 26px; -webkit-mask-image: linear-gradient(180deg, #000, transparent 70%); mask-image: linear-gradient(180deg, #000, transparent 70%); }
@keyframes drift { to { transform: translate(-6vmax, 5vmax) scale(1.12); } }

.nav { position: sticky; top: 0; z-index: 50; display: flex; align-items: center; gap: 18px; padding: 14px clamp(16px, 4vw, 48px); background: rgba(6,6,10,.62); backdrop-filter: blur(16px); -webkit-backdrop-filter: blur(16px); border-bottom: 1px solid var(--line); }
.brand { display: flex; align-items: center; gap: 10px; color: #fff; font-weight: 800; letter-spacing: .1em; }
.logo { width: 34px; height: 34px; border-radius: 10px; display: grid; place-items: center; font-weight: 900; color: #fff; background: linear-gradient(135deg, var(--accent), color-mix(in srgb, var(--accent) 45%, #4b0000)); }
.links { display: flex; gap: 4px; margin-left: 10px; }
.links a { color: var(--soft); padding: 8px 12px; border-radius: 10px; font-weight: 500; font-size: 14px; }
.links a:hover { color: #fff; background: rgba(255,255,255,.05); }
.links a.on { color: #fff; background: color-mix(in srgb, var(--accent) 16%, transparent); box-shadow: inset 0 0 0 1px color-mix(in srgb, var(--accent) 30%, transparent); }
.nav-right { margin-left: auto; display: flex; gap: 10px; }
.btn { display: inline-flex; align-items: center; justify-content: center; gap: 8px; padding: 12px 20px; border-radius: 13px; border: 1px solid transparent; background: rgba(255,255,255,.06); color: #fff; font-weight: 650; cursor: pointer; transition: transform .15s, filter .15s; white-space: nowrap; }
.btn:hover { transform: translateY(-1px); filter: brightness(1.1); }
.btn.primary { background: linear-gradient(135deg, var(--accent), var(--accent-2)); box-shadow: inset 0 1px 0 rgba(255,255,255,.3), 0 14px 34px -12px color-mix(in srgb, var(--accent) 80%, transparent); }
.btn.gold { background: linear-gradient(135deg, #fbbf24, #f59e0b); color: #241800; box-shadow: 0 14px 34px -14px rgba(251,191,36,.8); }
.btn.ghost { background: transparent; border-color: var(--line-2); color: var(--soft); }
.btn.sm { padding: 8px 13px; font-size: 13px; border-radius: 10px; }
.btn[disabled] { opacity: .5; pointer-events: none; }
.wrap { width: min(1200px, 100% - 32px); margin: 0 auto; }
.glass { background: linear-gradient(180deg, rgba(24,24,35,.9), rgba(13,13,20,.92)) padding-box, linear-gradient(160deg, rgba(255,255,255,.14), rgba(255,255,255,.03) 50%, rgba(251,191,36,.2)) border-box; border: 1px solid transparent; border-radius: 22px; box-shadow: var(--shadow); }

/* HERO с итоговой оценкой */
.hero { display: grid; grid-template-columns: 1.1fr 1fr; gap: 28px; align-items: center; padding: clamp(40px, 7vw, 90px) 0 30px; }
.eyebrow { font-size: 12px; font-weight: 700; letter-spacing: .16em; text-transform: uppercase; color: #fcd34d; }
.hero h1 { margin: 10px 0 14px; font-size: clamp(34px, 5.6vw, 64px); line-height: 1.04; letter-spacing: -.045em; font-weight: 900; }
.grad { background: linear-gradient(120deg, #fff 10%, #fde68a 50%, #f59e0b 95%); -webkit-background-clip: text; background-clip: text; color: transparent; }
.hero p { margin: 0; color: var(--soft); font-size: 18px; max-width: 520px; }
.hero-cta { display: flex; gap: 12px; flex-wrap: wrap; margin-top: 26px; }
.score { padding: 28px; display: grid; grid-template-columns: auto 1fr; gap: 26px; align-items: center; }
.score-big { text-align: center; }
.score-big .num { font-size: 76px; font-weight: 900; letter-spacing: -.05em; line-height: 1; background: linear-gradient(180deg, #fff, #fcd34d); -webkit-background-clip: text; background-clip: text; color: transparent; }
.score-big .cnt { margin-top: 8px; color: var(--muted); font-size: 13px; }
.stars { position: relative; display: inline-block; font-size: 20px; letter-spacing: 2px; line-height: 1; color: #2c2c3a; }
.stars::before { content: "★★★★★"; }
.stars i { position: absolute; inset: 0 auto 0 0; overflow: hidden; color: var(--gold); white-space: nowrap; font-style: normal; text-shadow: 0 0 16px rgba(251,191,36,.45); }
.stars i::before { content: "★★★★★"; }
.dist { display: grid; gap: 8px; }
.dist button { display: grid; grid-template-columns: 34px 1fr 34px; gap: 10px; align-items: center; padding: 4px 6px; border-radius: 9px; background: none; border: 0; cursor: pointer; color: var(--soft); font-size: 13px; text-align: left; transition: background .15s; }
.dist button:hover, .dist button.on { background: rgba(255,255,255,.05); color: #fff; }
.dist .bar { height: 9px; border-radius: 99px; background: rgba(255,255,255,.07); overflow: hidden; }
.dist .bar i { display: block; height: 100%; width: 0; border-radius: 99px; background: linear-gradient(90deg, #f59e0b, #fde68a); transition: width 1s cubic-bezier(.2,.8,.2,1); }
.dist .n { text-align: right; color: var(--muted); font-variant-numeric: tabular-nums; }
.badges { display: flex; gap: 10px; flex-wrap: wrap; margin-top: 18px; grid-column: 1 / -1; }
.badge { display: inline-flex; align-items: center; gap: 7px; padding: 7px 12px; border-radius: 999px; font-size: 12.5px; font-weight: 600; background: rgba(255,255,255,.04); border: 1px solid var(--line); color: var(--soft); }

/* Панель фильтров */
.toolbar { position: sticky; top: 64px; z-index: 20; display: flex; gap: 10px; flex-wrap: wrap; align-items: center; margin: 26px 0 18px; padding: 10px; border-radius: 16px; background: rgba(10,10,16,.8); backdrop-filter: blur(12px); border: 1px solid var(--line); }
.chip { padding: 8px 14px; border-radius: 11px; border: 1px solid var(--line); background: rgba(255,255,255,.03); color: var(--soft); font-weight: 600; font-size: 13.5px; cursor: pointer; transition: all .15s; }
.chip:hover { color: #fff; }
.chip.on { color: #fff; border-color: rgba(251,191,36,.55); background: rgba(251,191,36,.12); }
.toolbar .grow { flex: 1; }
.inp { padding: 10px 13px; border-radius: 11px; border: 1px solid rgba(255,255,255,.1); background: rgba(5,5,9,.65); color: #fff; outline: none; transition: border-color .15s, box-shadow .15s; }
.inp:focus { border-color: rgba(251,191,36,.6); box-shadow: 0 0 0 4px rgba(251,191,36,.14); }
select.inp { cursor: pointer; }

/* Карточки отзывов (masonry) */
.grid { columns: 3 320px; column-gap: 16px; }
.card { break-inside: avoid; margin: 0 0 16px; padding: 20px; border-radius: 20px; background: linear-gradient(180deg, rgba(22,22,32,.92), rgba(14,14,21,.92)) padding-box, linear-gradient(160deg, rgba(255,255,255,.12), rgba(255,255,255,.03)) border-box; border: 1px solid transparent;
    transition: transform .2s, box-shadow .2s; animation: rise .5s cubic-bezier(.2,.8,.2,1) both; }
.card:hover { transform: translateY(-3px); box-shadow: 0 24px 50px -30px rgba(0,0,0,.95); }
@keyframes rise { from { opacity: 0; transform: translateY(14px); } to { opacity: 1; transform: none; } }
.card-head { display: flex; gap: 12px; align-items: center; }
.ava { width: 44px; height: 44px; border-radius: 50%; display: grid; place-items: center; font-weight: 800; font-size: 17px; color: #fff; flex: none; background: linear-gradient(135deg, var(--c), color-mix(in srgb, var(--c) 40%, #000)); box-shadow: 0 0 0 3px color-mix(in srgb, var(--c) 25%, transparent); }
.card-head b { display: block; font-size: 15.5px; }
.card-head .date { font-size: 12.5px; color: var(--muted); }
.card .stars { font-size: 16px; margin-top: 14px; }
.card .text { margin: 10px 0 0; color: #d9d9e4; white-space: pre-line; word-wrap: break-word; }
.card .text.clamp { display: -webkit-box; -webkit-line-clamp: 7; -webkit-box-orient: vertical; overflow: hidden; }
.more { margin-top: 6px; background: none; border: 0; padding: 0; color: #fcd34d; font-weight: 600; cursor: pointer; font-size: 13.5px; }
.tags { display: flex; flex-wrap: wrap; gap: 6px; margin-top: 14px; }
.tag { padding: 3px 10px; border-radius: 999px; font-size: 12px; font-weight: 600; color: var(--soft); background: rgba(255,255,255,.05); border: 1px solid var(--line); }
.tag.v { color: #bbf7d0; background: rgba(34,197,94,.12); border-color: rgba(34,197,94,.35); }
.reply { margin-top: 14px; padding: 12px 14px; border-radius: 14px; background: color-mix(in srgb, var(--accent) 8%, transparent); border-left: 3px solid var(--accent); }
.reply .rh { font-size: 12.5px; font-weight: 700; color: color-mix(in srgb, var(--accent) 60%, #fff); }
.reply p { margin: 4px 0 0; color: var(--soft); font-size: 14px; white-space: pre-line; }
.empty { text-align: center; padding: 60px 20px; border-radius: 22px; border: 1px dashed var(--line-2); color: var(--soft); }
.empty .big { font-size: 54px; }
.load-more { display: flex; justify-content: center; margin: 10px 0 0; }

/* Форма отзыва */
.write { margin-top: 60px; display: grid; grid-template-columns: 1fr 1.2fr; gap: 0; overflow: hidden; }
.write-side { padding: 34px; background: radial-gradient(400px 260px at 0% 0%, rgba(251,191,36,.2), transparent 70%), rgba(255,255,255,.015); border-right: 1px solid var(--line); }
.write-side h2 { margin: 8px 0 12px; font-size: 30px; letter-spacing: -.03em; }
.write-side p { color: var(--soft); margin: 0 0 18px; }
.write-side ul { margin: 0; padding: 0; list-style: none; display: grid; gap: 10px; color: var(--soft); font-size: 14px; }
.write-form { padding: 34px; }
.field { margin-top: 16px; }
.field label { display: block; font-size: 13px; font-weight: 600; color: var(--soft); margin-bottom: 8px; }
.field .inp { width: 100%; }
textarea.inp { min-height: 140px; resize: vertical; }
.star-pick { display: flex; gap: 6px; }
.star-pick button { background: none; border: 0; padding: 0; cursor: pointer; font-size: 40px; line-height: 1; color: #2c2c3a; transition: transform .12s, color .12s; }
.star-pick button.on { color: var(--gold); text-shadow: 0 0 20px rgba(251,191,36,.5); }
.star-pick button:hover { transform: scale(1.15) rotate(-6deg); }
.star-lbl { margin-left: 10px; align-self: center; color: var(--soft); font-weight: 600; }
.note { margin-top: 14px; padding: 12px 14px; border-radius: 12px; font-size: 14px; }
.note.err { background: rgba(239,68,68,.1); border: 1px solid rgba(239,68,68,.35); color: #ffd0d0; }
.note.ok { background: rgba(34,197,94,.1); border: 1px solid rgba(34,197,94,.35); color: #d1fae5; }
.note.v { background: rgba(34,197,94,.08); border: 1px solid rgba(34,197,94,.3); color: #bbf7d0; margin-top: 0; margin-bottom: 6px; }
.hp { position: absolute !important; left: -9999px !important; }
footer { margin-top: 80px; padding: 30px 16px 40px; text-align: center; color: var(--muted); font-size: 13px; border-top: 1px solid var(--line); }
@media (max-width: 960px) { .hero, .write { grid-template-columns: 1fr; } .write-side { border-right: 0; border-bottom: 1px solid var(--line); } .score { grid-template-columns: 1fr; } }
@media (max-width: 760px) { .links { display: none; } .toolbar { top: 60px; } .toolbar .grow { flex-basis: 100%; } }
@media (prefers-reduced-motion: reduce) { *, *::before, *::after { animation: none !important; transition: none !important; } }
</style>
</head>
<body>
<div class="bg"><i></i></div>
<header class="nav">
    <a class="brand" href="index.php"><span class="logo">R</span><span>RTEAM</span></a>
    <nav class="links">
        <a href="index.php">Главная</a>
        <a href="projects.php">Проекты</a>
        <a href="order.php">Заказать сайт</a>
        <a href="reviews.php" class="on">Отзывы</a>
    </nav>
    <div class="nav-right"><a class="btn primary sm" href="order.php">Заказать сайт</a></div>
</header>

<main class="wrap">
    <section class="hero">
        <div>
            <div class="eyebrow">⭐ Отзывы клиентов</div>
            <h1>Что говорят <span class="grad">наши клиенты</span></h1>
            <p>Честные отзывы о сайтах, которые мы сделали. Отметка «Проверенный заказ» — значит, человек действительно заказывал у нас и принял работу.</p>
            <div class="hero-cta">
                <a class="btn gold" href="#write">✍️ Оставить отзыв</a>
                <a class="btn ghost" href="order.php">Заказать сайт →</a>
            </div>
        </div>
        <div class="glass score">
            <div class="score-big">
                <div class="num"><?= $stats['count'] ? so_h(number_format($stats['avg'], 1, ',', '')) : '—' ?></div>
                <span class="stars" style="font-size:24px;"><i style="width:<?= $stats['avg'] / 5 * 100 ?>%"></i></span>
                <div class="cnt"><?= $stats['count'] ?> <?= $plural($stats['count'], 'отзыв', 'отзыва', 'отзывов') ?></div>
            </div>
            <div class="dist" id="dist">
                <?php foreach ($stats['dist'] as $star => $n): $pct = $stats['count'] ? round($n / $stats['count'] * 100) : 0; ?>
                    <button type="button" data-rate="<?= $star ?>" title="Показать отзывы на <?= $star ?> ★"><span><?= $star ?> ★</span><span class="bar"><i data-w="<?= $pct ?>"></i></span><span class="n"><?= $n ?></span></button>
                <?php endforeach; ?>
            </div>
            <div class="badges">
                <span class="badge">✅ <?= $verified_n ?> из проверенных заказов</span>
                <span class="badge">💬 <?= $replied_n ?> с ответом команды</span>
            </div>
        </div>
    </section>

    <?php if ($items): ?>
    <div class="toolbar" id="toolbar">
        <button class="chip on" type="button" data-f="all">Все · <?= $stats['count'] ?></button>
        <button class="chip" type="button" data-f="5">★ 5</button>
        <button class="chip" type="button" data-f="4">★ 4</button>
        <button class="chip" type="button" data-f="low">★ 3 и ниже</button>
        <button class="chip" type="button" data-f="v">✅ Проверенные</button>
        <input class="inp grow" type="search" id="q" placeholder="Поиск по отзывам…">
        <select class="inp" id="sort">
            <option value="new">Сначала новые</option>
            <option value="best">Сначала лучшие</option>
            <option value="worst">Сначала критичные</option>
            <option value="reply">С ответом команды</option>
        </select>
    </div>
    <div class="grid" id="grid">
        <?php foreach ($items as $r): $hue = crc32($r['name']) % 360; $long = mb_strlen($r['text']) > 380; ?>
            <article class="card" data-rate="<?= (int)$r['rating'] ?>" data-v="<?= !empty($r['verified']) ? 1 : 0 ?>" data-t="<?= (int)$r['t'] ?>" data-reply="<?= !empty($r['reply']['text']) ? 1 : 0 ?>">
                <div class="card-head">
                    <div class="ava" style="--c:hsl(<?= $hue ?> 70% 52%)"><?= so_h(mb_strtoupper(mb_substr($r['name'], 0, 1))) ?></div>
                    <div><b><?= so_h($r['name']) ?></b><span class="date"><?= so_h($fmt($r['t'])) ?></span></div>
                </div>
                <span class="stars"><i style="width:<?= (int)$r['rating'] * 20 ?>%"></i></span>
                <p class="text<?= $long ? ' clamp' : '' ?>"><?= so_h($r['text']) ?></p>
                <?php if ($long): ?><button class="more" type="button">Читать полностью</button><?php endif; ?>
                <div class="tags">
                    <?php if (!empty($r['verified'])): ?><span class="tag v">✓ Проверенный заказ</span><?php endif; ?>
                    <?php if (!empty($r['type'])): ?><span class="tag"><?= so_h($r['type']) ?></span><?php endif; ?>
                    <?php if (!empty($r['coder'])): ?><span class="tag">🧑‍💻 <?= so_h($r['coder']) ?></span><?php endif; ?>
                </div>
                <?php if (!empty($r['reply']['text'])): ?>
                    <div class="reply"><div class="rh">💬 Ответ команды RTeam</div><p><?= so_h($r['reply']['text']) ?></p></div>
                <?php endif; ?>
            </article>
        <?php endforeach; ?>
    </div>
    <div class="empty" id="noMatch" hidden><div class="big">🔍</div><p>Под этот фильтр отзывов нет.</p></div>
    <div class="load-more"><button class="btn ghost" type="button" id="btnMore" hidden>Показать ещё</button></div>
    <?php else: ?>
        <div class="empty"><div class="big">🌱</div><h2>Отзывов пока нет</h2><p>Станьте первым — расскажите, как прошла работа с нами.</p><a class="btn gold" href="#write">Оставить отзыв</a></div>
    <?php endif; ?>

    <section class="glass write" id="write">
        <div class="write-side">
            <div class="eyebrow">Ваше мнение</div>
            <h2>Оставьте отзыв</h2>
            <p>Расскажите, как прошла работа: что понравилось, что можно улучшить. Мы читаем каждый отзыв.</p>
            <ul>
                <li>✅ Отзывы по завершённым заказам получают отметку «Проверенный заказ»</li>
                <li>🛡 Остальные публикуются после быстрой проверки на спам</li>
                <li>💬 Команда отвечает на отзывы прямо здесь</li>
            </ul>
        </div>
        <form class="write-form" id="rvForm" novalidate>
            <?php if ($from_order): ?>
                <div class="note v">✓ Отзыв по заказу №<?= (int)$from_order['id'] ?><?= !empty($from_order['coder']) ? ' · кодер ' . so_h($from_order['coder']['login']) : '' ?></div>
                <input type="hidden" name="o" value="<?= (int)$from_order['id'] ?>"><input type="hidden" name="t" value="<?= so_h($qt) ?>">
            <?php endif; ?>
            <div class="field"><label>Ваша оценка</label>
                <div class="star-pick" id="starPick"><?php for ($i = 1; $i <= 5; $i++): ?><button type="button" data-star="<?= $i ?>" aria-label="<?= $i ?> из 5">★</button><?php endfor; ?><span class="star-lbl" id="starLbl">Нажмите на звезду</span></div>
                <input type="hidden" name="rating" id="fRating" value="0"></div>
            <div class="field"><label for="fName">Имя</label><input class="inp" id="fName" name="name" maxlength="60" placeholder="Как вас подписать" value="<?= so_h($from_order['client']['name'] ?? ($account ?? '')) ?>"></div>
            <div class="field"><label for="fText">Отзыв</label><textarea class="inp" id="fText" name="text" maxlength="2000" placeholder="Какой сайт делали, как прошла работа, довольны ли результатом…"></textarea></div>
            <div class="hp" aria-hidden="true"><input type="text" name="website" tabindex="-1" autocomplete="off"></div>
            <input type="hidden" name="ts" value="<?= time() ?>">
            <div id="rvNote" class="note" hidden></div>
            <button class="btn gold" type="submit" style="margin-top:18px; width:100%;">Отправить отзыв</button>
        </form>
    </section>
</main>

<footer>© <?= date('Y') ?> RTeam · <a href="index.php">rteam.info</a> · <a href="order.php">Заказать сайт</a></footer>

<script>
const $ = (s, r = document) => r.querySelector(s), $$ = (s, r = document) => [...r.querySelectorAll(s)];
// полоски распределения «вырастают» при загрузке
requestAnimationFrame(() => $$('#dist .bar i').forEach(i => { i.style.width = i.dataset.w + '%'; }));

const grid = $('#grid');
if (grid) {
    const cards = $$('.card', grid);
    const PAGE = 12;
    let filter = 'all', shown = PAGE;
    const apply = () => {
        const q = $('#q').value.trim().toLowerCase(), sort = $('#sort').value;
        let list = cards.filter(c => {
            const r = +c.dataset.rate;
            if (filter === '5' && r !== 5) return false;
            if (filter === '4' && r !== 4) return false;
            if (filter === 'low' && r > 3) return false;
            if (/^[123]$/.test(filter) && r !== +filter) return false;
            if (filter === 'v' && c.dataset.v !== '1') return false;
            return !q || c.textContent.toLowerCase().includes(q);
        });
        const by = { new: (a, b) => b.dataset.t - a.dataset.t, best: (a, b) => b.dataset.rate - a.dataset.rate || b.dataset.t - a.dataset.t,
                     worst: (a, b) => a.dataset.rate - b.dataset.rate || b.dataset.t - a.dataset.t, reply: (a, b) => b.dataset.reply - a.dataset.reply || b.dataset.t - a.dataset.t }[sort];
        list.sort(by);
        cards.forEach(c => { c.hidden = true; });
        list.forEach((c, i) => { grid.appendChild(c); c.hidden = i >= shown; });
        $('#noMatch').hidden = list.length > 0;
        $('#btnMore').hidden = list.length <= shown;
    };
    const setFilter = f => { filter = f; shown = PAGE; $$('#toolbar .chip').forEach(c => c.classList.toggle('on', c.dataset.f === f)); $$('#dist button').forEach(b => b.classList.toggle('on', b.dataset.rate === f)); apply(); };
    $$('#toolbar .chip').forEach(c => c.addEventListener('click', () => setFilter(c.dataset.f)));
    $$('#dist button').forEach(b => b.addEventListener('click', () => { setFilter(b.dataset.rate); $('#toolbar').scrollIntoView({ behavior: 'smooth', block: 'start' }); }));
    $('#q').addEventListener('input', () => { shown = PAGE; apply(); });
    $('#sort').addEventListener('change', apply);
    $('#btnMore').addEventListener('click', () => { shown += PAGE; apply(); });
    grid.addEventListener('click', e => { const b = e.target.closest('.more'); if (b) { b.previousElementSibling.classList.remove('clamp'); b.remove(); } });
    apply();
}

// выбор звёзд
const labels = ['', 'Плохо', 'Так себе', 'Нормально', 'Хорошо', 'Отлично!'];
const pick = v => { $('#fRating').value = v; $$('#starPick button').forEach(b => b.classList.toggle('on', +b.dataset.star <= v)); $('#starLbl').textContent = v ? labels[v] : 'Нажмите на звезду'; };
$$('#starPick button').forEach(b => {
    b.addEventListener('click', () => pick(+b.dataset.star));
    b.addEventListener('mouseenter', () => $$('#starPick button').forEach(x => x.classList.toggle('on', +x.dataset.star <= +b.dataset.star)));
});
$('#starPick').addEventListener('mouseleave', () => pick(+$('#fRating').value));

$('#rvForm').addEventListener('submit', async e => {
    e.preventDefault();
    const note = $('#rvNote'), btn = e.target.querySelector('button[type="submit"]');
    const show = (t, ok) => { note.hidden = false; note.className = 'note ' + (ok ? 'ok' : 'err'); note.textContent = t; };
    if (+$('#fRating').value < 1) return show('Поставьте оценку звёздами.');
    btn.disabled = true;
    try {
        const r = await fetch('reviews.php?a=submit', { method: 'POST', body: new FormData(e.target), headers: { 'X-SO': '1' } });
        const j = await r.json();
        if (!j.ok) throw new Error(j.error || 'Ошибка');
        show(j.published ? 'Спасибо! Отзыв опубликован — обновите страницу, чтобы увидеть его.' : 'Спасибо! Отзыв появится на сайте после проверки.', true);
        e.target.reset(); pick(0);
        $$('input[name="o"], input[name="t"], .note.v', e.target).forEach(x => x.remove());
    } catch (err) { show(err.message); btn.disabled = false; return; }
    setTimeout(() => { btn.disabled = false; }, 4000);
});
</script>
</body>
</html>
