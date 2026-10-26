<?php
/* ==========================================================
   ЗАКАЗ САЙТА: красивая форма-мастер с живым расчётом стоимости,
   после отправки — чат с администратором, который обновляется сам
   (без перезагрузки страницы). В чате клиент видит выбор кодера,
   счета Platega, статус работы и может оставить отзыв.
   ========================================================== */
session_start();
$account = (isset($_SESSION['user']) && is_string($_SESSION['user']) && $_SESSION['user'] !== '') ? $_SESSION['user'] : null;
session_write_close();

require_once __DIR__ . '/_orders_lib.php';

$cfg       = so_config();
$cat       = so_catalog();
$accent    = so_accent();
$site      = so_site_settings();
$reviews   = so_reviews_published();
$rs        = so_review_stats($reviews);
$done_total = count(array_filter(so_index(), function ($e) { return $e['status'] === 'done'; }));
$styles    = so_styles();
$urgency   = so_urgency();
$js_cfg = [
    'types'    => $cat['types'],
    'features' => $cat['features'],
    'urgency'  => array_map(function ($u) { return ['name' => $u[0], 'k' => $u[1], 'd' => $u[2]]; }, $urgency),
    'statuses' => array_map(function ($s) { return ['name' => $s[0], 'icon' => $s[1], 'color' => $s[2]]; }, so_statuses()),
    'tasks'    => array_map(function ($s) { return ['name' => $s[0], 'color' => $s[1]]; }, so_task_statuses()),
    'account'  => $account,
    'open'     => !empty($cfg['orders_open']),
    'today'    => date('Y-m-d'),
];
header('Referrer-Policy: strict-origin-when-cross-origin');
header('X-Content-Type-Options: nosniff');
?>
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="referrer" content="strict-origin-when-cross-origin">
<title>Заказать сайт — RTeam</title>
<meta name="description" content="Закажите сайт у команды RTeam: лендинг, магазин, корпоративный сайт или веб-сервис. Расчёт стоимости онлайн, личный кодер, чат с администратором и оплата через Platega.">
<link rel="icon" href="favicon.jpeg" type="image/jpeg">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800;900&display=swap" rel="stylesheet">
<style>
:root {
    --accent: <?= so_h($accent) ?>;
    --accent-2: color-mix(in srgb, var(--accent) 55%, #ff9a3c);
    --bg: #06060a; --panel: rgba(18,18,27,.78); --panel-2: rgba(26,26,38,.85);
    --line: rgba(255,255,255,.08); --line-2: rgba(255,255,255,.14);
    --text: #ececf3; --soft: #b6b6c6; --muted: #777789;
    --ok: #22c55e; --warn: #f59e0b; --info: #3b82f6; --danger: #ef4444;
    --r: 18px; --shadow: 0 30px 80px -40px rgba(0,0,0,.9);
}
* { box-sizing: border-box; }
html { scroll-behavior: smooth; }
body { margin: 0; min-height: 100vh; font-family: Inter, "Segoe UI", system-ui, -apple-system, Arial, sans-serif; color: var(--text); background: var(--bg);
    font-size: 15px; line-height: 1.55; -webkit-font-smoothing: antialiased; letter-spacing: -.006em; overflow-x: hidden; }
a { color: color-mix(in srgb, var(--accent) 65%, #fff); text-decoration: none; }
button, input, textarea, select { font: inherit; color: inherit; }
::selection { background: color-mix(in srgb, var(--accent) 45%, transparent); }
[hidden] { display: none !important; }

/* Фон: мягкое «северное сияние» + точечная сетка */
.bg { position: fixed; inset: 0; z-index: -1; overflow: hidden; pointer-events: none; }
.bg::before, .bg::after { content: ""; position: absolute; width: 70vmax; height: 70vmax; border-radius: 50%; filter: blur(90px); opacity: .5; }
.bg::before { background: radial-gradient(circle, color-mix(in srgb, var(--accent) 38%, transparent), transparent 62%); top: -35vmax; right: -20vmax; animation: drift 22s ease-in-out infinite alternate; }
.bg::after { background: radial-gradient(circle, rgba(88,80,255,.26), transparent 62%); bottom: -40vmax; left: -25vmax; animation: drift 28s ease-in-out infinite alternate-reverse; }
.bg i { position: absolute; inset: 0; background-image: radial-gradient(rgba(255,255,255,.05) 1px, transparent 1px); background-size: 26px 26px;
    -webkit-mask-image: linear-gradient(180deg, #000, transparent 75%); mask-image: linear-gradient(180deg, #000, transparent 75%); }
@keyframes drift { from { transform: translate(0,0) scale(1); } to { transform: translate(-6vmax, 5vmax) scale(1.12); } }

/* Шапка */
.nav { position: sticky; top: 0; z-index: 50; display: flex; align-items: center; gap: 18px; padding: 14px clamp(16px, 4vw, 48px);
    background: rgba(6,6,10,.62); backdrop-filter: blur(16px); -webkit-backdrop-filter: blur(16px); border-bottom: 1px solid var(--line); }
.brand { display: flex; align-items: center; gap: 10px; color: #fff; font-weight: 800; letter-spacing: .1em; }
.logo { width: 34px; height: 34px; border-radius: 10px; display: grid; place-items: center; font-weight: 900; color: #fff;
    background: linear-gradient(135deg, var(--accent), color-mix(in srgb, var(--accent) 45%, #4b0000)); box-shadow: 0 8px 22px -6px color-mix(in srgb, var(--accent) 70%, transparent); }
.links { display: flex; gap: 4px; margin-left: 10px; }
.links a { color: var(--soft); padding: 8px 12px; border-radius: 10px; font-weight: 500; font-size: 14px; transition: color .15s, background .15s; }
.links a:hover { color: #fff; background: rgba(255,255,255,.05); }
.links a.on { color: #fff; background: color-mix(in srgb, var(--accent) 16%, transparent); box-shadow: inset 0 0 0 1px color-mix(in srgb, var(--accent) 30%, transparent); }
.nav-right { margin-left: auto; display: flex; align-items: center; gap: 10px; }
.me { color: var(--soft); font-size: 13.5px; }
.me b { color: #fff; }

/* Кнопки */
.btn { position: relative; display: inline-flex; align-items: center; justify-content: center; gap: 8px; padding: 12px 20px; border-radius: 13px; border: 1px solid transparent;
    background: rgba(255,255,255,.06); color: #fff; font-weight: 650; cursor: pointer; transition: transform .15s, filter .15s, background .15s, box-shadow .15s; white-space: nowrap; text-decoration: none; }
.btn:hover { transform: translateY(-1px); filter: brightness(1.1); }
.btn:active { transform: translateY(0); }
.btn.primary { background: linear-gradient(135deg, var(--accent), var(--accent-2)); box-shadow: inset 0 1px 0 rgba(255,255,255,.3), 0 14px 34px -12px color-mix(in srgb, var(--accent) 80%, transparent); }
.btn.ghost { background: transparent; border-color: var(--line-2); color: var(--soft); }
.btn.ghost:hover { color: #fff; background: rgba(255,255,255,.05); }
.btn.sm { padding: 8px 13px; font-size: 13px; border-radius: 10px; }
.btn.lg { padding: 16px 28px; font-size: 16px; border-radius: 15px; }
.btn[disabled] { opacity: .5; pointer-events: none; }
.btn .spin { width: 16px; height: 16px; border: 2px solid rgba(255,255,255,.35); border-top-color: #fff; border-radius: 50%; animation: spin .7s linear infinite; }
@keyframes spin { to { transform: rotate(360deg); } }
.dot { width: 8px; height: 8px; border-radius: 50%; background: var(--accent); box-shadow: 0 0 10px var(--accent); }

.wrap { width: min(1200px, 100% - 32px); margin: 0 auto; }

/* HERO */
.hero { position: relative; padding: clamp(48px, 9vw, 110px) 0 40px; text-align: center; }
.kicker { display: inline-flex; align-items: center; gap: 8px; padding: 7px 14px; border-radius: 999px; font-size: 13px; font-weight: 600; color: #ffd9d9;
    background: color-mix(in srgb, var(--accent) 12%, transparent); border: 1px solid color-mix(in srgb, var(--accent) 35%, transparent); }
.kicker .dot { animation: blink 1.6s ease-in-out infinite; }
@keyframes blink { 50% { opacity: .35; } }
.hero h1 { margin: 22px auto 16px; max-width: 900px; font-size: clamp(36px, 6.4vw, 72px); line-height: 1.03; letter-spacing: -.045em; font-weight: 900; }
.grad { background: linear-gradient(120deg, #fff 10%, color-mix(in srgb, var(--accent) 70%, #fff) 45%, var(--accent-2) 90%); -webkit-background-clip: text; background-clip: text; color: transparent; }
.hero p.lead { margin: 0 auto; max-width: 640px; color: var(--soft); font-size: clamp(16px, 2vw, 19px); }
.hero-cta { display: flex; justify-content: center; gap: 12px; flex-wrap: wrap; margin-top: 30px; }
.stats { display: flex; justify-content: center; gap: 12px; flex-wrap: wrap; margin-top: 38px; }
.stat { display: flex; align-items: center; gap: 12px; padding: 12px 18px; border-radius: 16px; background: var(--panel); border: 1px solid var(--line); text-align: left; }
.stat .si { width: 40px; height: 40px; border-radius: 12px; display: grid; place-items: center; font-size: 19px; background: rgba(255,255,255,.05); }
.stat b { display: block; font-size: 17px; color: #fff; }
.stat span { font-size: 12.5px; color: var(--muted); }
.stars { color: #fbbf24; letter-spacing: 1px; }

/* Как это работает */
.section-head { text-align: center; margin: 70px 0 26px; }
.section-head .eyebrow { font-size: 12px; font-weight: 700; letter-spacing: .16em; text-transform: uppercase; color: color-mix(in srgb, var(--accent) 70%, #fff); }
.section-head h2 { margin: 8px 0 0; font-size: clamp(26px, 3.8vw, 40px); letter-spacing: -.03em; }
.how { display: grid; grid-template-columns: repeat(5, 1fr); gap: 12px; counter-reset: how; }
.how-step { position: relative; padding: 20px 18px; border-radius: var(--r); background: var(--panel); border: 1px solid var(--line); transition: transform .2s, border-color .2s; }
.how-step:hover { transform: translateY(-4px); border-color: color-mix(in srgb, var(--accent) 35%, transparent); }
.how-step .hi { font-size: 26px; }
.how-step h3 { margin: 12px 0 6px; font-size: 16px; }
.how-step p { margin: 0; color: var(--soft); font-size: 13.5px; }
.how-step::after { counter-increment: how; content: "0" counter(how); position: absolute; top: 16px; right: 18px; font-weight: 800; font-size: 13px; color: var(--muted); }

/* Мастер заказа */
.wizard { display: grid; grid-template-columns: 1fr 360px; gap: 18px; align-items: start; margin-top: 10px; }
.glass { background: linear-gradient(180deg, rgba(24,24,35,.9), rgba(13,13,20,.92)) padding-box,
                     linear-gradient(160deg, rgba(255,255,255,.14), rgba(255,255,255,.03) 50%, color-mix(in srgb, var(--accent) 25%, transparent)) border-box;
    border: 1px solid transparent; border-radius: 22px; box-shadow: var(--shadow); }
.wz { padding: clamp(18px, 3vw, 30px); }
.wz-progress { display: flex; gap: 8px; margin-bottom: 26px; }
.wz-progress .pg { flex: 1; cursor: pointer; }
.wz-progress .bar { height: 5px; border-radius: 99px; background: rgba(255,255,255,.08); overflow: hidden; }
.wz-progress .bar i { display: block; height: 100%; width: 0; border-radius: 99px; background: linear-gradient(90deg, var(--accent), var(--accent-2)); transition: width .45s cubic-bezier(.2,.8,.2,1); }
.wz-progress .pg.done .bar i, .wz-progress .pg.cur .bar i { width: 100%; }
.wz-progress .lbl { margin-top: 8px; font-size: 12.5px; color: var(--muted); font-weight: 600; }
.wz-progress .pg.cur .lbl { color: #fff; }
.step { display: none; animation: stepIn .4s cubic-bezier(.2,.8,.2,1) both; }
.step.on { display: block; }
@keyframes stepIn { from { opacity: 0; transform: translateX(14px); } to { opacity: 1; transform: none; } }
.step h2 { margin: 0 0 4px; font-size: 24px; letter-spacing: -.02em; }
.step .sub { margin: 0 0 20px; color: var(--soft); }
.types { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 12px; }
.tcard { position: relative; display: flex; flex-direction: column; gap: 6px; padding: 18px; border-radius: 16px; cursor: pointer; background: rgba(255,255,255,.025);
    border: 1px solid var(--line); transition: transform .18s, border-color .18s, background .18s, box-shadow .18s; }
.tcard:hover { transform: translateY(-3px); border-color: var(--line-2); }
.tcard input { position: absolute; opacity: 0; pointer-events: none; }
.tcard .ti { font-size: 30px; line-height: 1; }
.tcard b { font-size: 15.5px; color: #fff; }
.tcard .td { font-size: 12.5px; color: var(--muted); }
.tcard .tp { margin-top: auto; padding-top: 8px; font-weight: 700; font-size: 13.5px; color: color-mix(in srgb, var(--accent) 60%, #fff); }
.tcard.on { border-color: color-mix(in srgb, var(--accent) 70%, transparent); background: color-mix(in srgb, var(--accent) 10%, transparent);
    box-shadow: 0 0 0 4px color-mix(in srgb, var(--accent) 14%, transparent), 0 18px 40px -24px var(--accent); }
.tcard.on::after { content: "✓"; position: absolute; top: 12px; right: 12px; width: 24px; height: 24px; border-radius: 50%; display: grid; place-items: center; font-size: 13px; font-weight: 800;
    background: var(--accent); color: #fff; }
.feats { display: grid; grid-template-columns: repeat(auto-fill, minmax(230px, 1fr)); gap: 10px; }
.fchip { display: flex; align-items: center; gap: 12px; padding: 13px 14px; border-radius: 14px; cursor: pointer; border: 1px solid var(--line); background: rgba(255,255,255,.025); transition: all .15s; user-select: none; }
.fchip:hover { border-color: var(--line-2); }
.fchip input { position: absolute; opacity: 0; pointer-events: none; }
.fchip .fi { width: 34px; height: 34px; border-radius: 10px; display: grid; place-items: center; background: rgba(255,255,255,.05); font-size: 17px; flex: none; }
.fchip .fn { font-weight: 600; font-size: 14px; }
.fchip .fp { display: block; font-size: 12px; color: var(--muted); font-weight: 500; }
.fchip .ck { margin-left: auto; width: 22px; height: 22px; border-radius: 7px; border: 1.5px solid var(--line-2); display: grid; place-items: center; font-size: 12px; color: transparent; transition: all .15s; flex: none; }
.fchip.on { border-color: color-mix(in srgb, var(--accent) 60%, transparent); background: color-mix(in srgb, var(--accent) 9%, transparent); }
.fchip.on .ck { background: var(--accent); border-color: var(--accent); color: #fff; }
.field { margin-top: 18px; }
.field > label, .lbl-row { display: block; font-size: 13px; font-weight: 600; color: var(--soft); margin-bottom: 8px; }
.lbl-row { display: flex; justify-content: space-between; align-items: baseline; }
.lbl-row b { color: #fff; font-size: 15px; }
.inp, textarea.inp { width: 100%; padding: 13px 15px; border-radius: 13px; border: 1px solid rgba(255,255,255,.1); background: rgba(5,5,9,.65); color: #fff; outline: none; transition: border-color .15s, box-shadow .15s; }
.inp:hover { border-color: rgba(255,255,255,.18); }
.inp:focus { border-color: color-mix(in srgb, var(--accent) 70%, transparent); box-shadow: 0 0 0 4px color-mix(in srgb, var(--accent) 16%, transparent); }
textarea.inp { min-height: 120px; resize: vertical; line-height: 1.5; }
.inp::placeholder { color: #5d5d6e; }
input[type="date"].inp { color-scheme: dark; }
.grid2 { display: grid; grid-template-columns: 1fr 1fr; gap: 14px; }
.seg { display: flex; flex-wrap: wrap; gap: 8px; }
.seg label { cursor: pointer; }
.seg input { position: absolute; opacity: 0; pointer-events: none; }
.seg span { display: inline-flex; align-items: center; gap: 7px; padding: 10px 14px; border-radius: 12px; border: 1px solid var(--line-2); background: rgba(255,255,255,.03); color: var(--soft); font-size: 13.5px; font-weight: 600; transition: all .15s; }
.seg input:checked + span { color: #fff; border-color: color-mix(in srgb, var(--accent) 65%, transparent); background: color-mix(in srgb, var(--accent) 16%, transparent); }
.seg input:focus-visible + span { outline: 2px solid var(--accent); outline-offset: 2px; }
input[type="range"] { -webkit-appearance: none; appearance: none; width: 100%; height: 6px; border-radius: 99px; outline: none;
    background: linear-gradient(90deg, var(--accent) var(--p, 0%), rgba(255,255,255,.1) var(--p, 0%)); }
input[type="range"]::-webkit-slider-thumb { -webkit-appearance: none; width: 22px; height: 22px; border-radius: 50%; background: #fff; border: 4px solid var(--accent); cursor: pointer; box-shadow: 0 4px 14px rgba(0,0,0,.5); }
input[type="range"]::-moz-range-thumb { width: 16px; height: 16px; border-radius: 50%; background: #fff; border: 4px solid var(--accent); cursor: pointer; }
.agree { display: flex; gap: 10px; align-items: flex-start; margin-top: 18px; color: var(--soft); font-size: 13.5px; cursor: pointer; }
.agree input { margin-top: 3px; accent-color: var(--accent); width: 18px; height: 18px; }
.hp { position: absolute !important; left: -9999px !important; width: 1px; height: 1px; overflow: hidden; }
.wz-nav { display: flex; justify-content: space-between; align-items: center; gap: 10px; margin-top: 28px; padding-top: 20px; border-top: 1px solid var(--line); }
.wz-nav .hint { color: var(--muted); font-size: 13px; }
.err { margin-top: 14px; padding: 12px 14px; border-radius: 12px; background: rgba(239,68,68,.1); border: 1px solid rgba(239,68,68,.35); color: #ffd0d0; font-size: 14px; animation: shake .4s; }
@keyframes shake { 20%, 60% { transform: translateX(-5px); } 40%, 80% { transform: translateX(5px); } }

/* Итог справа */
.summary { position: sticky; top: 86px; padding: 24px; }
.summary .s-kicker { font-size: 12px; text-transform: uppercase; letter-spacing: .14em; color: var(--muted); font-weight: 700; }
.s-type { display: flex; gap: 14px; align-items: center; margin: 14px 0 16px; }
.s-type .si { width: 54px; height: 54px; border-radius: 16px; display: grid; place-items: center; font-size: 28px; background: color-mix(in srgb, var(--accent) 14%, transparent); border: 1px solid color-mix(in srgb, var(--accent) 30%, transparent); }
.s-type b { font-size: 17px; display: block; }
.s-type span { color: var(--muted); font-size: 13px; }
.s-list { list-style: none; margin: 0; padding: 0; max-height: 220px; overflow: auto; }
.s-list li { display: flex; justify-content: space-between; gap: 10px; padding: 7px 0; font-size: 13.5px; color: var(--soft); border-bottom: 1px dashed var(--line); }
.s-list li span:last-child { color: #fff; white-space: nowrap; }
.s-price { margin-top: 18px; padding: 18px; border-radius: 16px; background: radial-gradient(260px 120px at 100% 0%, color-mix(in srgb, var(--accent) 25%, transparent), transparent), rgba(255,255,255,.03); border: 1px solid var(--line); }
.s-price .pl { font-size: 12.5px; color: var(--muted); }
.s-price .pv { font-size: 30px; font-weight: 850; letter-spacing: -.03em; margin-top: 2px; font-variant-numeric: tabular-nums; }
.s-price .pd { margin-top: 6px; font-size: 13.5px; color: var(--soft); }
.s-note { margin: 14px 0 0; font-size: 12.5px; color: var(--muted); }
.s-perks { list-style: none; padding: 0; margin: 16px 0 0; display: grid; gap: 8px; }
.s-perks li { display: flex; gap: 10px; font-size: 13.5px; color: var(--soft); }

/* FAQ + отзывы */
.faq { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
.faq details { padding: 18px 20px; border-radius: 16px; background: var(--panel); border: 1px solid var(--line); }
.faq summary { cursor: pointer; font-weight: 650; list-style: none; display: flex; justify-content: space-between; gap: 10px; }
.faq summary::-webkit-details-marker { display: none; }
.faq summary::after { content: "+"; color: var(--muted); font-size: 20px; line-height: 1; transition: transform .2s; }
.faq details[open] summary::after { transform: rotate(45deg); }
.faq p { margin: 10px 0 0; color: var(--soft); font-size: 14px; }
.rv-row { display: grid; grid-template-columns: repeat(3, 1fr); gap: 14px; }
.rv { padding: 20px; border-radius: 18px; background: var(--panel); border: 1px solid var(--line); }
.rv-head { display: flex; gap: 12px; align-items: center; }
.ava { width: 40px; height: 40px; border-radius: 50%; display: grid; place-items: center; font-weight: 800; color: #fff; flex: none; background: var(--c, var(--accent)); background: linear-gradient(135deg, var(--c, var(--accent)), color-mix(in srgb, var(--c, var(--accent)) 45%, #000)); }
.rv-head b { display: block; }
.rv-head .muted { font-size: 12.5px; }
.rv p { margin: 12px 0 0; color: var(--soft); font-size: 14px; display: -webkit-box; -webkit-line-clamp: 5; -webkit-box-orient: vertical; overflow: hidden; }
.muted { color: var(--muted); }
.center { text-align: center; }
footer { margin-top: 80px; padding: 30px 16px 40px; text-align: center; color: var(--muted); font-size: 13px; border-top: 1px solid var(--line); }

/* ========= ЧАТ ЗАКАЗА ========= */
.chat-view { padding: 26px 0 40px; }
.chat-top { display: flex; align-items: center; gap: 16px; flex-wrap: wrap; }
.ct-title { min-width: 0; }
.ct-kicker { font-size: 12.5px; color: var(--muted); font-weight: 600; letter-spacing: .04em; }
.ct-title h2 { margin: 2px 0 0; font-size: clamp(20px, 3vw, 28px); letter-spacing: -.02em; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.pill { display: inline-flex; align-items: center; gap: 6px; padding: 6px 12px; border-radius: 999px; font-size: 13px; font-weight: 700; color: #fff;
    background: color-mix(in srgb, var(--c, #888) 20%, transparent); border: 1px solid color-mix(in srgb, var(--c, #888) 45%, transparent); }
.chat-top .pill { margin-left: auto; }
.stepper { list-style: none; margin: 22px 0 18px; padding: 0; display: grid; grid-template-columns: repeat(5, 1fr); gap: 6px; }
.stepper li { position: relative; padding-top: 34px; text-align: center; font-size: 12.5px; color: var(--muted); font-weight: 600; }
.stepper li::before { content: attr(data-i); position: absolute; top: 0; left: 50%; transform: translateX(-50%); width: 26px; height: 26px; border-radius: 50%; display: grid; place-items: center;
    background: #15151f; border: 1.5px solid var(--line-2); font-size: 12px; color: var(--muted); z-index: 1; transition: all .3s; }
.stepper li::after { content: ""; position: absolute; top: 12px; left: calc(-50% + 16px); right: calc(50% + 16px); height: 2px; background: var(--line-2); }
.stepper li:first-child::after { display: none; }
.stepper li.done { color: var(--soft); }
.stepper li.done::before { content: "✓"; background: var(--ok); border-color: var(--ok); color: #fff; }
.stepper li.done::after, .stepper li.cur::after { background: linear-gradient(90deg, var(--ok), var(--accent)); }
.stepper li.cur { color: #fff; }
.stepper li.cur::before { background: var(--accent); border-color: var(--accent); color: #fff; box-shadow: 0 0 0 6px color-mix(in srgb, var(--accent) 22%, transparent); animation: pulse 2s infinite; }
@keyframes pulse { 50% { box-shadow: 0 0 0 10px color-mix(in srgb, var(--accent) 6%, transparent); } }
.cancel-banner { margin: 0 0 16px; padding: 14px 16px; border-radius: 14px; background: rgba(107,114,128,.15); border: 1px solid rgba(107,114,128,.4); color: #d1d5db; }
.chat-grid { display: grid; grid-template-columns: 1fr 340px; gap: 18px; align-items: start; }
.chat-card { display: flex; flex-direction: column; height: clamp(480px, calc(100vh - 270px), 820px); overflow: hidden; scroll-margin-top: 80px; }
.chat-head { display: flex; align-items: center; gap: 12px; padding: 16px 18px; border-bottom: 1px solid var(--line); background: rgba(255,255,255,.02); }
.adm-ava { position: relative; width: 44px; height: 44px; border-radius: 14px; display: grid; place-items: center; font-weight: 900; color: #fff; background: linear-gradient(135deg, var(--accent), var(--accent-2)); }
.adm-ava::after { content: ""; position: absolute; right: -3px; bottom: -3px; width: 13px; height: 13px; border-radius: 50%; background: #4b5563; border: 3px solid #12121a; }
.adm-ava.on::after { background: var(--ok); }
.chat-head .who b { display: block; }
.chat-head .who span { font-size: 12.5px; color: var(--muted); }
.chat-head .hint { margin-left: auto; font-size: 12.5px; color: var(--muted); text-align: right; }
.msgs { flex: 1; overflow-y: auto; padding: 22px 18px; display: flex; flex-direction: column; gap: 6px; scroll-behavior: smooth; }
.day-sep { align-self: center; margin: 12px 0 6px; padding: 4px 12px; border-radius: 999px; font-size: 12px; color: var(--muted); background: rgba(255,255,255,.04); }
.m { display: flex; gap: 10px; max-width: 78%; animation: msgIn .3s cubic-bezier(.2,.8,.2,1) both; }
@keyframes msgIn { from { opacity: 0; transform: translateY(8px) scale(.98); } to { opacity: 1; transform: none; } }
.m.me { align-self: flex-end; flex-direction: row-reverse; }
.m .ava { width: 34px; height: 34px; font-size: 14px; margin-top: 18px; }
.m.me .ava { display: none; }
.m .bub { padding: 11px 14px; border-radius: 18px; background: var(--panel-2); border: 1px solid var(--line); min-width: 0; }
.m.me .bub { background: linear-gradient(135deg, color-mix(in srgb, var(--accent) 85%, #000), color-mix(in srgb, var(--accent-2) 70%, #000)); border-color: transparent; border-bottom-right-radius: 6px; color: #fff; }
.m.them .bub { border-bottom-left-radius: 6px; }
.m .mh { font-size: 12px; font-weight: 700; color: var(--soft); margin: 0 0 4px 4px; display: flex; gap: 6px; align-items: center; }
.m .mh .role { font-weight: 600; font-size: 11px; padding: 1px 8px; border-radius: 999px; color: color-mix(in srgb, var(--c, #888) 75%, #fff); background: color-mix(in srgb, var(--c, #888) 16%, transparent); }
.m .tx { white-space: pre-wrap; word-wrap: break-word; overflow-wrap: anywhere; }
.m .tx a { color: inherit; text-decoration: underline; }
.m .mt { display: flex; justify-content: flex-end; gap: 5px; align-items: center; margin-top: 4px; font-size: 11px; opacity: .65; }
.m .mt .tick.read { color: #a7f3d0; opacity: 1; }
.m .imgs { display: grid; grid-template-columns: repeat(auto-fill, minmax(120px, 1fr)); gap: 6px; margin-top: 8px; }
.m .imgs img { width: 100%; max-height: 220px; object-fit: cover; border-radius: 12px; display: block; cursor: zoom-in; }
.m .fl { display: flex; align-items: center; gap: 10px; margin-top: 8px; padding: 9px 12px; border-radius: 12px; background: rgba(0,0,0,.2); color: inherit; }
.m .fl:hover { background: rgba(0,0,0,.3); }
.m .fl small { display: block; opacity: .7; }
.sys { align-self: center; max-width: 90%; text-align: center; margin: 8px 0; padding: 8px 16px; border-radius: 14px; font-size: 13px; color: var(--soft); background: rgba(255,255,255,.04); border: 1px dashed var(--line-2); animation: msgIn .3s both; }
/* Карточки в чате: счёт, выбор кодера, отзыв */
.xcard { align-self: stretch; margin: 10px 0; padding: 18px; border-radius: 18px; background: linear-gradient(180deg, rgba(30,30,44,.9), rgba(16,16,24,.9)) padding-box, linear-gradient(135deg, color-mix(in srgb, var(--c, var(--accent)) 60%, transparent), rgba(255,255,255,.05)) border-box; border: 1px solid transparent; animation: msgIn .35s both; }
.xcard .xk { font-size: 12px; text-transform: uppercase; letter-spacing: .12em; font-weight: 700; color: color-mix(in srgb, var(--c, var(--accent)) 70%, #fff); }
.xcard h4 { margin: 6px 0 4px; font-size: 18px; }
.inv-sum { font-size: 34px; font-weight: 850; letter-spacing: -.03em; font-variant-numeric: tabular-nums; }
.inv-row { display: flex; align-items: center; justify-content: space-between; gap: 14px; flex-wrap: wrap; }
.inv-actions { display: flex; gap: 8px; flex-wrap: wrap; margin-top: 14px; }
.badge { display: inline-flex; align-items: center; gap: 6px; padding: 4px 11px; border-radius: 999px; font-size: 12.5px; font-weight: 700; }
.b-pending { color: #fde68a; background: rgba(245,158,11,.12); border: 1px solid rgba(245,158,11,.35); }
.b-paid { color: #bbf7d0; background: rgba(34,197,94,.14); border: 1px solid rgba(34,197,94,.4); }
.b-cancelled, .b-refunded { color: #d1d5db; background: rgba(107,114,128,.15); border: 1px solid rgba(107,114,128,.4); }
.coders { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 10px; margin-top: 14px; }
.coder { padding: 14px; border-radius: 15px; background: rgba(255,255,255,.035); border: 1px solid var(--line); display: flex; flex-direction: column; gap: 8px; transition: border-color .15s, transform .15s; }
.coder:hover { transform: translateY(-2px); border-color: var(--line-2); }
.coder.chosen { border-color: rgba(34,197,94,.6); background: rgba(34,197,94,.08); }
.coder .ch { display: flex; gap: 10px; align-items: center; }
.coder .ch b { display: block; }
.coder .meta { font-size: 12.5px; color: var(--soft); display: grid; gap: 3px; }
.star-pick { display: flex; gap: 6px; font-size: 32px; margin: 10px 0; }
.star-pick button { background: none; border: 0; cursor: pointer; padding: 0; color: #3b3b4c; transition: transform .12s, color .12s; line-height: 1; }
.star-pick button.on { color: #fbbf24; text-shadow: 0 0 18px rgba(251,191,36,.5); }
.star-pick button:hover { transform: scale(1.15); }
.typing { padding: 4px 22px 8px; font-size: 13px; color: var(--muted); display: flex; align-items: center; gap: 8px; }
.dots { display: inline-flex; gap: 3px; }
.dots i { width: 6px; height: 6px; border-radius: 50%; background: var(--soft); animation: bounce 1.2s infinite; }
.dots i:nth-child(2) { animation-delay: .15s; } .dots i:nth-child(3) { animation-delay: .3s; }
@keyframes bounce { 0%, 60%, 100% { transform: none; opacity: .4; } 30% { transform: translateY(-4px); opacity: 1; } }
.composer { padding: 12px 14px 14px; border-top: 1px solid var(--line); background: rgba(255,255,255,.02); }
.comp-row { display: flex; align-items: flex-end; gap: 10px; }
.icon-btn { width: 46px; height: 46px; flex: none; border-radius: 14px; display: grid; place-items: center; font-size: 19px; cursor: pointer; background: rgba(255,255,255,.05); border: 1px solid var(--line); transition: background .15s; }
.icon-btn:hover { background: rgba(255,255,255,.09); }
.composer textarea { flex: 1; min-height: 46px; max-height: 180px; resize: none; padding: 12px 15px; border-radius: 14px; border: 1px solid rgba(255,255,255,.1); background: rgba(5,5,9,.7); color: #fff; outline: none; line-height: 1.45; }
.composer textarea:focus { border-color: color-mix(in srgb, var(--accent) 70%, transparent); box-shadow: 0 0 0 4px color-mix(in srgb, var(--accent) 14%, transparent); }
.send { width: 46px; height: 46px; flex: none; border-radius: 14px; border: 0; cursor: pointer; font-size: 18px; color: #fff; background: linear-gradient(135deg, var(--accent), var(--accent-2)); box-shadow: 0 10px 24px -10px var(--accent); transition: transform .15s; }
.send:hover { transform: scale(1.06); }
.att-list { display: flex; flex-wrap: wrap; gap: 6px; margin-bottom: 8px; }
.att-list:empty { display: none; }
.att { display: inline-flex; align-items: center; gap: 6px; padding: 5px 10px; border-radius: 10px; background: rgba(255,255,255,.06); font-size: 12.5px; }
.att button { background: none; border: 0; color: var(--muted); cursor: pointer; font-size: 14px; padding: 0 2px; }
.side { display: grid; gap: 14px; position: sticky; top: 86px; }
.side-card { padding: 18px; }
.side-card h3 { margin: 0 0 12px; font-size: 13px; text-transform: uppercase; letter-spacing: .12em; color: var(--muted); }
.kv { display: grid; grid-template-columns: auto 1fr; gap: 6px 14px; font-size: 13.5px; }
.kv span:nth-child(odd) { color: var(--muted); }
.kv span:nth-child(even) { color: #fff; text-align: right; }
.chips { display: flex; flex-wrap: wrap; gap: 6px; margin-top: 12px; }
.chip { padding: 4px 10px; border-radius: 999px; font-size: 12px; background: rgba(255,255,255,.05); border: 1px solid var(--line); color: var(--soft); }
.coder-big { display: flex; gap: 12px; align-items: center; }
.coder-big .ava { width: 48px; height: 48px; font-size: 19px; border-radius: 15px; }
.progress { height: 8px; border-radius: 99px; background: rgba(255,255,255,.07); overflow: hidden; margin-top: 12px; }
.progress i { display: block; height: 100%; border-radius: 99px; background: linear-gradient(90deg, var(--ok), #a3e635); transition: width .6s; }
.link-box { display: flex; gap: 8px; margin-top: 8px; }
.link-box input { flex: 1; min-width: 0; padding: 10px 12px; border-radius: 11px; border: 1px solid var(--line-2); background: rgba(5,5,9,.6); color: var(--soft); font-size: 12px; }

/* Мои заказы (выезжающая панель) */
.drawer { position: fixed; inset: 0; z-index: 100; display: flex; justify-content: flex-end; background: rgba(0,0,0,.5); backdrop-filter: blur(4px); opacity: 0; pointer-events: none; transition: opacity .2s; }
.drawer.open { opacity: 1; pointer-events: auto; }
.drawer-box { width: min(420px, 100%); height: 100%; overflow-y: auto; padding: 22px; background: #0d0d14; border-left: 1px solid var(--line-2); transform: translateX(30px); transition: transform .25s; }
.drawer.open .drawer-box { transform: none; }
.drawer-head { display: flex; align-items: center; justify-content: space-between; margin-bottom: 16px; }
.drawer-head h3 { margin: 0; font-size: 20px; }
.mine-item { display: flex; gap: 12px; align-items: center; padding: 14px; border-radius: 15px; background: rgba(255,255,255,.03); border: 1px solid var(--line); margin-bottom: 8px; color: var(--text); transition: border-color .15s; cursor: pointer; }
.mine-item:hover { border-color: var(--line-2); }
.mine-item .mi { flex: 1; min-width: 0; }
.mine-item b { display: block; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.mine-item .muted { font-size: 12.5px; }
.unread { min-width: 22px; height: 22px; padding: 0 7px; border-radius: 999px; display: grid; place-items: center; background: var(--accent); color: #fff; font-size: 12px; font-weight: 800; }

/* Тосты и просмотр картинки */
#toasts { position: fixed; z-index: 200; bottom: 18px; left: 50%; transform: translateX(-50%); display: flex; flex-direction: column; gap: 8px; align-items: center; pointer-events: none; }
.toast { padding: 12px 18px; border-radius: 13px; background: #16161f; border: 1px solid var(--line-2); box-shadow: var(--shadow); font-size: 14px; animation: msgIn .3s both; pointer-events: auto; max-width: min(92vw, 460px); }
.toast.error { border-color: rgba(239,68,68,.5); }
.toast.ok { border-color: rgba(34,197,94,.5); }
.lightbox { position: fixed; inset: 0; z-index: 150; display: grid; place-items: center; background: rgba(0,0,0,.85); cursor: zoom-out; padding: 20px; }
.lightbox img { max-width: 100%; max-height: 100%; border-radius: 12px; }

.reveal { opacity: 0; transform: translateY(18px); transition: opacity .6s ease, transform .6s cubic-bezier(.2,.8,.2,1); }
.reveal.in { opacity: 1; transform: none; }

@media (max-width: 1100px) {
    .wizard, .chat-grid { grid-template-columns: 1fr; }
    .summary, .side { position: static; }
    .how { grid-template-columns: repeat(2, 1fr); }
    .rv-row { grid-template-columns: 1fr; }
}
.show-sm { display: none; }
@media (max-width: 760px) {
    .links { display: none; }
    .show-sm { display: inline-flex; }
    .stats { display: grid; grid-template-columns: 1fr 1fr; }
    .stat { padding: 10px 12px; }
    .stat .si { width: 34px; height: 34px; font-size: 16px; }
    .nav { gap: 10px; }
    .how, .faq, .grid2 { grid-template-columns: 1fr; }
    .stepper li { font-size: 0; }
    .m { max-width: 92%; }
    .chat-card { height: 76vh; min-height: 460px; border-radius: 18px; }
    .chat-head .hint { display: none; }
    .wz-progress .lbl { font-size: 0; }
    .nav-right .btn .lbl { display: none; }
}
@media (prefers-reduced-motion: reduce) { *, *::before, *::after { animation: none !important; transition: none !important; } .reveal { opacity: 1; transform: none; } }
</style>
</head>
<body>
<div class="bg"><i></i></div>

<header class="nav">
    <a class="brand" href="index.php"><span class="logo">R</span><span>RTEAM</span></a>
    <nav class="links">
        <a href="index.php">Главная</a>
        <a href="projects.php">Проекты</a>
        <a href="order.php" class="on">Заказать сайт</a>
        <a href="reviews.php">Отзывы</a>
    </nav>
    <div class="nav-right">
        <a class="btn ghost sm show-sm" href="reviews.php" title="Отзывы">⭐</a>
        <button class="btn ghost sm" type="button" id="btnMine">📂 <span class="lbl">Мои заказы</span> <span class="dot" id="mineDot" hidden></span></button>
        <?php if ($account): ?><span class="me">👤 <b><?= so_h($account) ?></b></span><?php else: ?><a class="btn ghost sm" href="index.php">Войти</a><?php endif; ?>
    </div>
</header>

<main>
<!-- ===================== ФОРМА ЗАКАЗА ===================== -->
<div id="viewForm">
    <section class="hero wrap">
        <span class="kicker"><span class="dot"></span><?= !empty($cfg['orders_open']) ? 'Принимаем заказы — свободные кодеры есть' : 'Приём заказов временно на паузе' ?></span>
        <h1>Сайт, который <span class="grad">работает на вас</span> — под ключ</h1>
        <p class="lead">Опишите идею за пару минут. Администратор сразу ответит в чате, подберёт вам кодера, поставит задачу в его рабочий календарь и будет держать вас в курсе до самого запуска.</p>
        <div class="hero-cta">
            <a class="btn primary lg" href="#start">Рассчитать стоимость →</a>
            <a class="btn ghost lg" href="reviews.php">⭐ Отзывы клиентов</a>
        </div>
        <div class="stats">
            <div class="stat"><div class="si">⭐</div><div><b><?= $rs['count'] ? so_h(number_format($rs['avg'], 1, ',', '')) . ' <span class="stars">★</span>' : 'Новые отзывы' ?></b><span><?= $rs['count'] ? 'по ' . $rs['count'] . ' отзывам' : 'будьте первым' ?></span></div></div>
            <div class="stat"><div class="si">⚡</div><div><b>~15 минут</b><span>до первого ответа</span></div></div>
            <div class="stat"><div class="si">🧑‍💻</div><div><b>Личный кодер</b><span>можно выбрать самому</span></div></div>
            <div class="stat"><div class="si">💳</div><div><b>Platega</b><span>СБП и банковские карты</span></div></div>
            <?php if ($done_total > 0): ?><div class="stat"><div class="si">🚀</div><div><b><?= $done_total ?>+</b><span>сданных проектов</span></div></div><?php endif; ?>
        </div>
    </section>

    <section class="wrap">
        <div class="section-head reveal"><div class="eyebrow">Как это работает</div><h2>От идеи до запуска — пять шагов</h2></div>
        <div class="how">
            <div class="how-step reveal"><div class="hi">📝</div><h3>Заявка</h3><p>Выбираете тип сайта и нужные функции — стоимость считается сразу.</p></div>
            <div class="how-step reveal"><div class="hi">💬</div><h3>Чат с администратором</h3><p>Открывается сразу после отправки. Ответы приходят без перезагрузки.</p></div>
            <div class="how-step reveal"><div class="hi">🧑‍💻</div><h3>Выбор кодера</h3><p>Администратор предложит кодеров с рейтингом — выберите своего.</p></div>
            <div class="how-step reveal"><div class="hi">📅</div><h3>Работа по календарю</h3><p>Задача встаёт в рабочий календарь кодера с точным сроком сдачи.</p></div>
            <div class="how-step reveal"><div class="hi">💳</div><h3>Оплата и запуск</h3><p>Счёт через Platega прямо в чате. Принимаете работу — и сайт онлайн.</p></div>
        </div>
    </section>

    <section class="wrap" id="start">
        <div class="section-head reveal"><div class="eyebrow">Оформление заказа</div><h2>Соберите свой сайт</h2></div>
        <div class="wizard">
            <form class="glass wz" id="orderForm" novalidate autocomplete="on">
                <div class="wz-progress" id="wzProgress">
                    <div class="pg cur" data-go="1"><div class="bar"><i></i></div><div class="lbl">1. Тип сайта</div></div>
                    <div class="pg" data-go="2"><div class="bar"><i></i></div><div class="lbl">2. Функции</div></div>
                    <div class="pg" data-go="3"><div class="bar"><i></i></div><div class="lbl">3. Детали</div></div>
                    <div class="pg" data-go="4"><div class="bar"><i></i></div><div class="lbl">4. Контакты</div></div>
                </div>

                <div class="step on" data-step="1">
                    <h2>Что будем создавать?</h2>
                    <p class="sub">Выберите, что ближе всего к вашей задаче. Не уверены — берите «Другое», разберёмся в чате.</p>
                    <div class="types">
                        <?php foreach ($cat['types'] as $k => $t): ?>
                            <label class="tcard"><input type="radio" name="type" value="<?= so_h($k) ?>">
                                <span class="ti"><?= $t['icon'] ?></span><b><?= so_h($t['name']) ?></b><span class="td"><?= so_h($t['desc']) ?></span>
                                <span class="tp"><?= $t['price'] > 0 ? 'от ' . so_money($t['price']) . ($t['days'] ? ' · ~' . $t['days'] . ' дн.' : '') : 'Цена по договорённости' ?></span>
                            </label>
                        <?php endforeach; ?>
                    </div>
                </div>

                <div class="step" data-step="2">
                    <h2>Какие функции нужны?</h2>
                    <p class="sub">Адаптивная вёрстка под телефоны и базовая защита уже включены. Отметьте всё, что нужно дополнительно.</p>
                    <div class="feats">
                        <?php foreach ($cat['features'] as $k => $f): ?>
                            <label class="fchip"><input type="checkbox" name="features[]" value="<?= so_h($k) ?>">
                                <span class="fi"><?= $f['icon'] ?></span><span><span class="fn"><?= so_h($f['name']) ?></span><span class="fp">+<?= so_money($f['price']) ?></span></span><span class="ck">✓</span>
                            </label>
                        <?php endforeach; ?>
                    </div>
                    <div class="field"><label for="fProject">Название проекта <span class="muted">(необязательно)</span></label>
                        <input class="inp" id="fProject" name="project" maxlength="80" placeholder="Например: Магазин кроссовок «Sneakerhead»"></div>
                </div>

                <div class="step" data-step="3">
                    <h2>Расскажите о проекте</h2>
                    <p class="sub">Чем подробнее — тем точнее цена и быстрее старт.</p>
                    <div class="field"><label for="fDesc">Опишите задачу *</label>
                        <textarea class="inp" id="fDesc" name="description" maxlength="5000" placeholder="Для кого сайт, что должен уметь, какие страницы нужны, что вам нравится и не нравится…"></textarea></div>
                    <div class="field"><label>Стиль дизайна</label>
                        <div class="seg"><?php foreach ($styles as $k => $s): ?><label><input type="radio" name="style" value="<?= so_h($k) ?>" <?= $k === 'free' ? 'checked' : '' ?>><span><?= so_h($s) ?></span></label><?php endforeach; ?></div></div>
                    <div class="field"><label>Что у вас уже есть</label>
                        <div class="seg">
                            <label><input type="checkbox" name="has[]" value="design"><span>🎨 Дизайн-макет</span></label>
                            <label><input type="checkbox" name="has[]" value="logo"><span>🔷 Логотип</span></label>
                            <label><input type="checkbox" name="has[]" value="content"><span>📝 Тексты и фото</span></label>
                            <label><input type="checkbox" name="has[]" value="domain"><span>🌐 Домен / хостинг</span></label>
                        </div></div>
                    <div class="field"><label for="fRefs">Сайты, которые вам нравятся <span class="muted">(ссылки)</span></label>
                        <textarea class="inp" id="fRefs" name="refs" maxlength="1500" style="min-height:70px;" placeholder="https://…"></textarea></div>
                    <div class="field">
                        <div class="lbl-row"><span>Бюджет</span><b id="budgetLbl">Пока не знаю</b></div>
                        <input type="range" id="fBudget" name="budget" min="0" max="500000" step="5000" value="0">
                    </div>
                    <div class="grid2">
                        <div class="field"><label for="fDeadline">Желаемая дата запуска</label><input class="inp" type="date" id="fDeadline" name="deadline" min="<?= date('Y-m-d') ?>"></div>
                        <div class="field"><label>Срочность</label>
                            <div class="seg"><?php foreach ($urgency as $k => $u): ?><label><input type="radio" name="urgency" value="<?= so_h($k) ?>" <?= $k === 'normal' ? 'checked' : '' ?>><span><?= $k === 'urgent' ? '🔥 ' : '' ?><?= so_h($u[0]) ?></span></label><?php endforeach; ?></div></div>
                    </div>
                </div>

                <div class="step" data-step="4">
                    <h2>Как с вами связаться?</h2>
                    <p class="sub">Основное общение — в чате заказа на этой странице. Контакт нужен, чтобы предупредить вас о новом ответе.</p>
                    <div class="field"><label for="fName">Ваше имя *</label><input class="inp" id="fName" name="name" maxlength="60" autocomplete="name" placeholder="Как к вам обращаться" value="<?= so_h($account ?? '') ?>"></div>
                    <div class="field"><label>Удобный способ связи</label>
                        <div class="seg" id="ctSeg">
                            <label><input type="radio" name="contact_type" value="telegram" checked><span>✈️ Telegram</span></label>
                            <label><input type="radio" name="contact_type" value="whatsapp"><span>🟢 WhatsApp</span></label>
                            <label><input type="radio" name="contact_type" value="email"><span>✉️ Почта</span></label>
                            <label><input type="radio" name="contact_type" value="phone"><span>📞 Телефон</span></label>
                        </div></div>
                    <div class="field"><label for="fContact">Контакт *</label><input class="inp" id="fContact" name="contact" maxlength="120" placeholder="@username"></div>
                    <label class="agree"><input type="checkbox" id="fAgree"> <span>Согласен(на), что команда RTeam использует эти данные только для связи по моему заказу.</span></label>
                    <div class="hp" aria-hidden="true"><label>Сайт <input type="text" name="website" tabindex="-1" autocomplete="off"></label></div>
                    <input type="hidden" name="ts" value="<?= time() ?>">
                </div>

                <div id="formErr" class="err" hidden></div>
                <div class="wz-nav">
                    <button class="btn ghost" type="button" id="btnBack" hidden>← Назад</button>
                    <span class="hint" id="wzHint">Шаг 1 из 4</span>
                    <button class="btn primary" type="button" id="btnNext">Далее →</button>
                    <button class="btn primary" type="submit" id="btnSubmit" hidden>🚀 Отправить и открыть чат</button>
                </div>
            </form>

            <aside class="glass summary" id="summary">
                <div class="s-kicker">Ваш проект</div>
                <div class="s-type"><div class="si" id="sumIcon">✨</div><div><b id="sumType">Выберите тип сайта</b><span id="sumProject">Название появится здесь</span></div></div>
                <ul class="s-list" id="sumList"><li><span>Адаптив под телефоны</span><span>включено</span></li></ul>
                <div class="s-price">
                    <div class="pl">Ориентировочная стоимость</div>
                    <div class="pv" id="sumPrice">—</div>
                    <div class="pd" id="sumDays">Срок появится после выбора типа</div>
                </div>
                <p class="s-note">Точную цену администратор назовёт в чате после уточнения деталей. Счёт выставляется прямо в чате, оплата — через Platega.</p>
                <ul class="s-perks">
                    <li>🛡 <span>Договорённости и вся переписка сохраняются в заказе</span></li>
                    <li>📅 <span>Срок закреплён в календаре кодера</span></li>
                    <li>🔁 <span>Правки до полного соответствия брифу</span></li>
                </ul>
            </aside>
        </div>
    </section>

    <section class="wrap">
        <div class="section-head reveal"><div class="eyebrow">Частые вопросы</div><h2>Что важно знать</h2></div>
        <div class="faq">
            <details class="reveal"><summary>Как быстро ответит администратор?</summary><p><?= so_h($cfg['reply_hint']) ?>. Чат обновляется сам — оставьте вкладку открытой или вернитесь позже по ссылке на заказ: вся переписка сохранится.</p></details>
            <details class="reveal"><summary>Можно выбрать кодера самому?</summary><p>Да. Администратор пришлёт в чат несколько свободных кодеров с рейтингом по отзывам и датой, с которой они могут начать. Нажмите «Выбрать» — задача сразу встанет в календарь кодера.</p></details>
            <details class="reveal"><summary>Как проходит оплата?</summary><p>Администратор выставляет счёт прямо в чате. Кнопка «Оплатить» ведёт на защищённую страницу Platega (СБП или карта). Оплата подтверждается автоматически.</p></details>
            <details class="reveal"><summary>А если нужно что-то поменять в процессе?</summary><p>Пишите в чат заказа — администратор и кодер видят всю историю. Небольшие правки входят в стоимость, крупные изменения обсуждаются отдельно.</p></details>
        </div>
    </section>

    <?php if ($reviews): ?>
    <section class="wrap">
        <div class="section-head reveal"><div class="eyebrow">Отзывы</div><h2>Нам доверяют</h2></div>
        <div class="rv-row">
            <?php foreach (array_slice($reviews, 0, 3) as $r): $hue = crc32($r['name']) % 360; ?>
                <div class="rv reveal">
                    <div class="rv-head"><div class="ava" style="--c:hsl(<?= $hue ?> 70% 52%)"><?= so_h(mb_strtoupper(mb_substr($r['name'], 0, 1))) ?></div>
                        <div><b><?= so_h($r['name']) ?></b><span class="muted"><span class="stars"><?= str_repeat('★', (int)$r['rating']) ?></span><?= !empty($r['type']) ? ' · ' . so_h($r['type']) : '' ?></span></div></div>
                    <p><?= so_h($r['text']) ?></p>
                </div>
            <?php endforeach; ?>
        </div>
        <div class="center" style="margin-top:20px;"><a class="btn ghost" href="reviews.php">Все отзывы (<?= $rs['count'] ?>) →</a></div>
    </section>
    <?php endif; ?>
</div>

<!-- ===================== ЧАТ ЗАКАЗА ===================== -->
<section class="wrap chat-view" id="viewChat" hidden>
    <div class="chat-top">
        <button class="btn ghost sm" type="button" id="btnNew">← Новый заказ</button>
        <div class="ct-title"><div class="ct-kicker">Заказ №<span id="cNum"></span> · <span id="cDate"></span></div><h2 id="cTitle">…</h2></div>
        <span class="pill" id="cStatus">…</span>
    </div>
    <ol class="stepper" id="cSteps">
        <li data-i="1">Заявка</li><li data-i="2">Обсуждение</li><li data-i="3">Кодер</li><li data-i="4">Разработка</li><li data-i="5">Готово</li>
    </ol>
    <div class="cancel-banner" id="cCancel" hidden>Заказ отменён. Если хотите продолжить — просто напишите в чат, администратор увидит.</div>
    <div class="chat-grid">
        <div class="glass chat-card">
            <div class="chat-head">
                <div class="adm-ava" id="cAdmAva">R</div>
                <div class="who"><b>Администратор RTeam</b><span id="cOnline">загрузка…</span></div>
                <div class="hint"><?= so_h($cfg['reply_hint']) ?></div>
            </div>
            <div class="msgs" id="cMsgs" aria-live="polite"></div>
            <div class="typing" id="cTyping" hidden><span class="dots"><i></i><i></i><i></i></span><span><b id="cTypingWho">Администратор</b> печатает…</span></div>
            <form class="composer" id="cForm">
                <div class="att-list" id="cAtt"></div>
                <div class="comp-row">
                    <label class="icon-btn" title="Прикрепить файлы (до 5 шт., до 8 МБ)">📎<input type="file" id="cFiles" multiple hidden accept=".jpg,.jpeg,.png,.gif,.webp,.pdf,.txt,.zip,.rar,.7z,.doc,.docx,.xls,.xlsx,.pptx,.psd,.fig"></label>
                    <textarea id="cText" rows="1" maxlength="4000" placeholder="Напишите сообщение… (Enter — отправить, Shift+Enter — новая строка)"></textarea>
                    <button class="send" type="submit" title="Отправить">➤</button>
                </div>
            </form>
        </div>
        <aside class="side">
            <div class="glass side-card" id="sOrder"></div>
            <div class="glass side-card" id="sCoder"></div>
            <div class="glass side-card" id="sPay"></div>
            <div class="glass side-card">
                <h3>🔑 Ссылка на заказ</h3>
                <div class="muted" style="font-size:13px;">Сохраните её — по ней можно вернуться в этот чат с любого устройства.</div>
                <div class="link-box"><input id="cLink" readonly><button class="btn sm" type="button" id="btnCopy">Копировать</button></div>
            </div>
        </aside>
    </div>
</section>
</main>

<div class="drawer" id="mineDrawer">
    <div class="drawer-box">
        <div class="drawer-head"><h3>📂 Мои заказы</h3><button class="btn ghost sm" type="button" id="btnMineClose">✕</button></div>
        <div id="mineList"><div class="muted">Загрузка…</div></div>
        <button class="btn primary" type="button" style="width:100%; margin-top:12px;" id="btnMineNew">＋ Новый заказ</button>
    </div>
</div>
<div id="toasts"></div>

<footer>© <?= date('Y') ?> RTeam · <a href="index.php">rteam.info</a> · <a href="reviews.php">Отзывы</a> · Оплата через Platega</footer>

<script>
const CFG = <?= json_encode($js_cfg, JSON_UNESCAPED_UNICODE | JSON_HEX_TAG | JSON_HEX_AMP) ?>;
const API = 'order_api.php';
const $ = (s, r = document) => r.querySelector(s), $$ = (s, r = document) => [...r.querySelectorAll(s)];
const esc = s => String(s ?? '').replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
const money = n => Math.round(n).toLocaleString('ru-RU') + ' ₽';
const fmtDate = d => d ? new Date(d + 'T00:00:00').toLocaleDateString('ru-RU', { day: 'numeric', month: 'long' }) : '';
const fmtTime = t => new Date(t * 1000).toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit' });
const fmtDay = t => { const d = new Date(t * 1000), n = new Date(); const y = new Date(); y.setDate(n.getDate() - 1);
    if (d.toDateString() === n.toDateString()) return 'Сегодня'; if (d.toDateString() === y.toDateString()) return 'Вчера';
    return d.toLocaleDateString('ru-RU', { day: 'numeric', month: 'long', year: d.getFullYear() === n.getFullYear() ? undefined : 'numeric' }); };
const hue = s => { let h = 0; for (const c of String(s)) h = (h * 31 + c.charCodeAt(0)) % 360; return h; };
const linkify = s => esc(s).replace(/\bhttps?:\/\/[^\s<]+[^\s<.,;:!?)"']/g, u => `<a href="${u}" target="_blank" rel="noopener nofollow">${u}</a>`);

function toast(text, type = '') {
    const t = document.createElement('div'); t.className = 'toast ' + type; t.textContent = text;
    $('#toasts').appendChild(t); setTimeout(() => { t.style.transition = 'opacity .4s'; t.style.opacity = '0'; setTimeout(() => t.remove(), 400); }, 4200);
}
async function api(a, data = null, extra = '') {
    const opt = data ? { method: 'POST', body: data, headers: { 'X-SO': '1' } } : {};
    const r = await fetch(API + '?a=' + a + extra, opt);
    let j; try { j = await r.json(); } catch (e) { throw new Error('Сервер ответил неожиданно (' + r.status + ')'); }
    if (!j.ok) throw new Error(j.error || 'Ошибка');
    return j;
}

/* ---------- сохранённые ссылки на заказы (в этом браузере) ---------- */
const store = {
    get() { try { return JSON.parse(localStorage.getItem('rtOrders') || '[]'); } catch (e) { return []; } },
    add(o, t) { try { const l = store.get().filter(x => x.o !== o); l.unshift({ o, t }); localStorage.setItem('rtOrders', JSON.stringify(l.slice(0, 50))); } catch (e) {} },
};

/* ---------- появление блоков при прокрутке ---------- */
const io = 'IntersectionObserver' in window ? new IntersectionObserver(es => es.forEach(e => { if (e.isIntersecting) { e.target.classList.add('in'); io.unobserve(e.target); } }), { threshold: .12 }) : null;
$$('.reveal').forEach(el => io ? io.observe(el) : el.classList.add('in'));

/* ======================= МАСТЕР ЗАКАЗА ======================= */
const form = $('#orderForm');
let step = 1;
const STEPS = 4;

function selType() { const r = $('input[name="type"]:checked', form); return r ? r.value : ''; }
function selFeatures() { return $$('input[name="features[]"]:checked', form).map(i => i.value); }

function estimate() {
    const t = CFG.types[selType()];
    if (!t) return null;
    let price = t.price, days = t.days;
    selFeatures().forEach(f => { if (CFG.features[f]) { price += CFG.features[f].price; days += CFG.features[f].days; } });
    const u = CFG.urgency[($('input[name="urgency"]:checked', form) || {}).value] || CFG.urgency.normal;
    price *= u.k; days = days ? Math.max(3, Math.round(days * u.d)) : 0;
    return { min: Math.round(price / 1000) * 1000, max: Math.round(price * 1.4 / 1000) * 1000, days };
}

let shownPrice = 0;
function animatePrice(el, to) {
    const from = shownPrice, start = performance.now(), dur = 450;
    const tick = now => { const k = Math.min(1, (now - start) / dur), e = 1 - Math.pow(1 - k, 3); shownPrice = from + (to - from) * e;
        el.textContent = 'от ' + money(k < 1 ? Math.round(shownPrice / 1000) * 1000 : to); if (k < 1) requestAnimationFrame(tick); };
    requestAnimationFrame(tick);
}

function updateSummary() {
    $$('.tcard', form).forEach(l => l.classList.toggle('on', l.querySelector('input').checked));
    $$('.fchip', form).forEach(l => l.classList.toggle('on', l.querySelector('input').checked));
    const t = CFG.types[selType()];
    $('#sumIcon').textContent = t ? t.icon : '✨';
    $('#sumType').textContent = t ? t.name : 'Выберите тип сайта';
    $('#sumProject').textContent = $('#fProject').value.trim() || (t ? t.desc : 'Название появится здесь');
    let html = '<li><span>📱 Адаптив под телефоны</span><span>включено</span></li>';
    if (t && t.price) html = `<li><span>${t.icon} ${esc(t.name)}</span><span>от ${money(t.price)}</span></li>` + html;
    selFeatures().forEach(f => { const x = CFG.features[f]; if (x) html += `<li><span>${x.icon} ${esc(x.name)}</span><span>+${money(x.price)}</span></li>`; });
    $('#sumList').innerHTML = html;
    const e = estimate(), pv = $('#sumPrice');
    if (!e) { pv.textContent = '—'; $('#sumDays').textContent = 'Срок появится после выбора типа'; shownPrice = 0; }
    else if (!e.min) { pv.textContent = 'Индивидуально'; $('#sumDays').textContent = 'Посчитаем в чате'; shownPrice = 0; }
    else { animatePrice(pv, e.min); $('#sumDays').textContent = `до ${money(e.max)} · ≈ ${e.days} раб. ${e.days % 10 === 1 && e.days % 100 !== 11 ? 'день' : (e.days % 10 >= 2 && e.days % 10 <= 4 && (e.days % 100 < 10 || e.days % 100 >= 20) ? 'дня' : 'дней')}`; }
}

function showStep(n) {
    step = Math.max(1, Math.min(STEPS, n));
    $$('.step', form).forEach(s => s.classList.toggle('on', +s.dataset.step === step));
    $$('#wzProgress .pg').forEach(p => { const i = +p.dataset.go; p.classList.toggle('done', i < step); p.classList.toggle('cur', i === step); });
    $('#btnBack').hidden = step === 1;
    $('#btnNext').hidden = step === STEPS;
    $('#btnSubmit').hidden = step !== STEPS;
    $('#wzHint').textContent = 'Шаг ' + step + ' из ' + STEPS;
    $('#formErr').hidden = true;
}

function stepError(n) {
    if (n >= 1 && !selType()) return 'Выберите тип сайта — так мы поймём масштаб задачи.';
    if (n >= 3 && $('#fDesc').value.trim().length < 15) return 'Опишите задачу хотя бы парой предложений.';
    if (n >= 4) {
        if ($('#fName').value.trim().length < 2) return 'Как к вам обращаться?';
        if ($('#fContact').value.trim().length < 3) return 'Оставьте контакт для связи.';
        if (!$('#fAgree').checked) return 'Нужно согласие на использование контактов для связи по заказу.';
    }
    return '';
}
function showErr(msg) { const e = $('#formErr'); e.textContent = msg; e.hidden = false; e.style.animation = 'none'; e.offsetHeight; e.style.animation = ''; }

$('#btnNext').addEventListener('click', () => { const e = stepError(step); if (e) return showErr(e); showStep(step + 1); form.scrollIntoView({ behavior: 'smooth', block: 'start' }); });
$('#btnBack').addEventListener('click', () => showStep(step - 1));
$$('#wzProgress .pg').forEach(p => p.addEventListener('click', () => { const to = +p.dataset.go; if (to < step) return showStep(to); const e = stepError(to - 1); if (e) return showErr(e); showStep(to); }));
form.addEventListener('change', e => { updateSummary(); if (e.target.name === 'type' && step === 1) setTimeout(() => showStep(2), 260); });
form.addEventListener('input', e => { if (e.target.id === 'fProject') updateSummary(); });
form.addEventListener('keydown', e => { if (e.key === 'Enter' && e.target.tagName === 'INPUT' && step < STEPS) { e.preventDefault(); $('#btnNext').click(); } });

const budget = $('#fBudget');
const budgetUpd = () => { const v = +budget.value; budget.style.setProperty('--p', (v / budget.max * 100) + '%'); $('#budgetLbl').textContent = v ? 'до ' + money(v) + (v >= +budget.max ? '+' : '') : 'Пока не знаю'; };
budget.addEventListener('input', budgetUpd); budgetUpd();

const placeholders = { telegram: '@username', whatsapp: '+7 900 000-00-00', email: 'you@mail.ru', phone: '+7 900 000-00-00' };
$('#ctSeg').addEventListener('change', e => { $('#fContact').placeholder = placeholders[e.target.value] || ''; $('#fContact').type = e.target.value === 'email' ? 'email' : 'text'; });

form.addEventListener('submit', async e => {
    e.preventDefault();
    for (let n = 1; n <= STEPS; n++) { const err = stepError(n); if (err) { showStep(n === 4 ? 4 : n); return showErr(err); } }
    if (!CFG.open) return showErr('Приём заказов временно приостановлен. Загляните чуть позже!');
    const btn = $('#btnSubmit'); btn.disabled = true; const old = btn.innerHTML; btn.innerHTML = '<span class="spin"></span> Отправляем…';
    try {
        const j = await api('create', new FormData(form));
        if (!j.id) throw new Error('Не удалось создать заказ');
        store.add(j.id, j.token);
        toast('Заявка отправлена! Открываем чат с администратором…', 'ok');
        openOrder(j.id, j.token, true);
    } catch (err) { showErr(err.message); }
    finally { btn.disabled = false; btn.innerHTML = old; }
});
updateSummary(); showStep(1);

/* ======================= ЧАТ ЗАКАЗА ======================= */
const C = { id: 0, t: '', after: 0, rev: -1, state: null, staffSeen: 0, timer: null, busy: false, fails: 0, unreadHidden: 0, lastDay: '', files: [] };
const msgsEl = $('#cMsgs');
const STEP_OF = { new: 0, talk: 1, choose: 2, assigned: 2, work: 3, review: 3, done: 4, cancel: -1 };

function openOrder(id, t, fresh = false) {
    clearTimeout(C.timer);
    Object.assign(C, { id, t, after: 0, rev: -1, state: null, staffSeen: 0, fails: 0, lastDay: '', files: [] });
    msgsEl.innerHTML = ''; $('#cAtt').innerHTML = '';
    $('#viewForm').hidden = true; $('#viewChat').hidden = false;
    $('#cNum').textContent = id;
    const url = location.pathname + '?o=' + id + '&t=' + t;
    history.replaceState(null, '', url);
    $('#cLink').value = location.origin + url;
    window.scrollTo({ top: 0, behavior: fresh ? 'smooth' : 'auto' });
    poll(true);
    const q = new URLSearchParams(location.search);
    if (q.get('paid')) setTimeout(() => checkPay(q.get('paid'), true), 600);
}

function nearBottom() { return msgsEl.scrollHeight - msgsEl.scrollTop - msgsEl.clientHeight < 140; }
function toBottom(force) { if (force || nearBottom()) requestAnimationFrame(() => { msgsEl.scrollTop = msgsEl.scrollHeight; }); }
const fileUrl = f => API + '?a=file&o=' + C.id + '&t=' + encodeURIComponent(C.t) + '&f=' + f.id;
const fsize = b => b > 1048576 ? (b / 1048576).toFixed(1) + ' МБ' : Math.max(1, Math.round(b / 1024)) + ' КБ';

function renderFiles(files) {
    if (!files || !files.length) return '';
    const imgs = files.filter(f => f.img), docs = files.filter(f => !f.img);
    let h = '';
    if (imgs.length) h += '<div class="imgs">' + imgs.map(f => `<img src="${fileUrl(f)}" alt="${esc(f.name)}" loading="lazy" data-zoom>`).join('') + '</div>';
    h += docs.map(f => `<a class="fl" href="${fileUrl(f)}" target="_blank" rel="noopener">📄 <span>${esc(f.name)}<small>${fsize(f.size)}</small></span></a>`).join('');
    return h;
}

function msgNode(m) {
    const day = fmtDay(m.t);
    let pre = '';
    if (day !== C.lastDay) { C.lastDay = day; pre = `<div class="day-sep">${day}</div>`; }
    if (m.kind === 'invoice') return pre + `<div class="xcard" data-kind="invoice" data-ref="${esc(m.ref)}" style="--c:#22c55e"></div>`;
    if (m.kind === 'offer') return pre + `<div class="xcard" data-kind="offer" data-ref="${esc(m.ref)}" style="--c:#06b6d4"></div>`;
    if (m.kind === 'review') return pre + `<div class="xcard" data-kind="review" style="--c:#fbbf24"></div>`;
    if (m.from === 'system') return pre + `<div class="sys">${linkify(m.text)}</div>`;
    const me = m.from === 'client';
    const c = m.color || `hsl(${hue(m.who)} 65% 50%)`;
    return pre + `<div class="m ${me ? 'me' : 'them'}" data-id="${m.id}">
        <div class="ava" style="--c:${esc(c)}">${esc((m.who || 'R').slice(0, 1).toUpperCase())}</div>
        <div>${me ? '' : `<div class="mh">${esc(m.who)}${m.role ? `<span class="role" style="--c:${esc(c)}">${esc(m.role)}</span>` : ''}</div>`}
        <div class="bub">${m.text ? `<div class="tx">${linkify(m.text)}</div>` : ''}${renderFiles(m.files)}
        <div class="mt">${fmtTime(m.t)}${me ? `<span class="tick" data-tick="${m.id}">✓</span>` : ''}</div></div></div></div>`;
}

function renderCards() {
    const s = C.state; if (!s) return;
    $$('.xcard[data-kind="invoice"]', msgsEl).forEach(el => {
        const inv = s.invoices.find(i => String(i.id) === el.dataset.ref);
        if (!inv) { el.innerHTML = '<div class="muted">Счёт удалён</div>'; return; }
        const st = { pending: ['b-pending', '⏳ Ожидает оплаты'], paid: ['b-paid', '✅ Оплачен'], cancelled: ['b-cancelled', 'Отменён'], refunded: ['b-refunded', 'Возвращён'] }[inv.status] || ['b-pending', inv.status];
        el.style.setProperty('--c', inv.status === 'paid' ? '#22c55e' : (inv.status === 'pending' ? '#f59e0b' : '#6b7280'));
        el.innerHTML = `<div class="xk">💳 Счёт на оплату</div>
            <div class="inv-row"><div><h4>${esc(inv.desc)}</h4><div class="inv-sum">${money(inv.amount)}</div></div><span class="badge ${st[0]}">${st[1]}</span></div>
            ${inv.status === 'pending' ? (inv.url ? `<div class="inv-actions"><a class="btn primary" href="${esc(inv.url)}" target="_blank" rel="noopener">Оплатить через Platega →</a>
                <button class="btn ghost" type="button" data-check="${esc(inv.id)}">Я оплатил(а) — проверить</button></div>
                <div class="muted" style="font-size:12.5px; margin-top:10px;">СБП или банковская карта · оплата подтверждается автоматически</div>`
                : `<div class="muted" style="margin-top:10px;">Реквизиты для оплаты пришлёт администратор в чате. После оплаты счёт отметят вручную.</div>`) : ''}
            ${inv.status === 'paid' && inv.paid_at ? `<div class="muted" style="margin-top:8px; font-size:13px;">Оплачено ${new Date(inv.paid_at * 1000).toLocaleString('ru-RU', { day: 'numeric', month: 'long', hour: '2-digit', minute: '2-digit' })}</div>` : ''}`;
    });
    $$('.xcard[data-kind="offer"]', msgsEl).forEach(el => {
        const of = s.offers.find(o => String(o.id) === el.dataset.ref);
        if (!of) { el.innerHTML = '<div class="muted">Предложение отозвано</div>'; return; }
        const open = of.status === 'open';
        el.innerHTML = `<div class="xk">🧑‍💻 Выберите кодера</div>
            <h4>${open ? 'Кто будет делать ваш сайт?' : (of.status === 'chosen' ? 'Кодер выбран' : 'Предложение закрыто')}</h4>
            ${of.note ? `<div class="muted">${linkify(of.note)}</div>` : ''}
            ${of.deadline ? `<div class="muted" style="margin-top:4px;">Срок сдачи: <b style="color:#fff">${fmtDate(of.deadline)}</b></div>` : ''}
            <div class="coders">${of.coders.map(c => `
                <div class="coder ${of.chosen === c.login ? 'chosen' : ''}">
                    <div class="ch"><div class="ava" style="--c:${esc(c.color || '#888')}">${esc(c.login.slice(0, 1).toUpperCase())}</div><div><b>${esc(c.login)}</b><span class="muted" style="font-size:12.5px;">${esc(c.role)}</span></div></div>
                    <div class="meta">
                        <span>${c.rating ? `<span class="stars">★</span> ${String(c.rating).replace('.', ',')} · ${c.reviews} отз.` : '✨ Пока без отзывов'}</span>
                        <span>🚀 Сдал проектов: <b style="color:#fff">${c.done}</b></span>
                        <span>📅 Может начать: <b style="color:#fff">${fmtDate(c.next_free)}</b></span>
                        <span>⚙️ Сейчас задач: ${c.busy}</span>
                    </div>
                    ${open ? `<button class="btn primary sm" type="button" data-choose="${esc(c.login)}" data-offer="${esc(of.id)}">Выбрать ${esc(c.login)}</button>` : (of.chosen === c.login ? '<span class="badge b-paid">✓ Выбран</span>' : '')}
                </div>`).join('')}</div>`;
    });
    $$('.xcard[data-kind="review"]', msgsEl).forEach(el => {
        if (s.review) { el.innerHTML = '<div class="xk">⭐ Отзыв</div><h4>Спасибо за ваш отзыв!</h4><div class="muted">Он помогает нам становиться лучше. <a href="reviews.php">Все отзывы →</a></div>'; return; }
        if (el.dataset.built) return;
        el.dataset.built = '1';
        el.innerHTML = `<div class="xk">⭐ Оцените работу</div><h4>Как вам результат?</h4>
            <div class="star-pick" data-v="0">${[1, 2, 3, 4, 5].map(i => `<button type="button" data-star="${i}" aria-label="${i} из 5">★</button>`).join('')}</div>
            <textarea class="inp" rows="3" placeholder="Пара слов о работе кодера и команды…" style="min-height:90px;"></textarea>
            <button class="btn primary" type="button" data-review style="margin-top:10px;">Отправить отзыв</button>`;
    });
}

function renderState() {
    const s = C.state; if (!s) return;
    const st = CFG.statuses[s.status] || { name: s.status, color: '#888', icon: '' };
    $('#cTitle').textContent = s.title;
    $('#cDate').textContent = new Date(s.created * 1000).toLocaleDateString('ru-RU', { day: 'numeric', month: 'long' });
    const pill = $('#cStatus'); pill.textContent = st.icon + ' ' + st.name; pill.style.setProperty('--c', st.color);
    const cur = STEP_OF[s.status] ?? 0;
    $$('#cSteps li').forEach((li, i) => { li.classList.toggle('done', cur >= 0 && (i < cur || (s.status === 'done' && i === 4))); li.classList.toggle('cur', i === cur && s.status !== 'done'); });
    $('#cCancel').hidden = s.status !== 'cancel';
    document.title = `Заказ №${s.id} · ${st.name} — RTeam`;

    $('#sOrder').innerHTML = `<h3>${s.type.icon} Ваш заказ</h3>
        <div class="kv"><span>Тип</span><span>${esc(s.type.name)}</span>
        ${s.estimate && s.estimate.min ? `<span>Оценка</span><span>${money(s.estimate.min)} – ${money(s.estimate.max)}</span>` : ''}
        ${s.deadline ? `<span>Желаемый срок</span><span>${fmtDate(s.deadline)}</span>` : ''}
        <span>Статус</span><span>${esc(st.name)}</span></div>
        ${s.features.length ? `<div class="chips">${s.features.map(f => `<span class="chip">${esc(f)}</span>`).join('')}</div>` : ''}`;

    if (s.coder) {
        const ts = CFG.tasks[s.coder.task] || { name: s.coder.task, color: '#888' };
        const pct = { new: 10, work: 55, review: 85, done: 100 }[s.coder.task] ?? 10;
        $('#sCoder').innerHTML = `<h3>🧑‍💻 Ваш кодер</h3>
            <div class="coder-big"><div class="ava" style="--c:${esc(s.coder.color || '#888')}">${esc(s.coder.name.slice(0, 1).toUpperCase())}</div>
            <div><b>${esc(s.coder.name)}</b><div class="muted" style="font-size:13px;">${esc(s.coder.role)}</div></div></div>
            <div class="kv" style="margin-top:14px;"><span>Задача</span><span style="color:${esc(ts.color)}">${esc(ts.name)}</span>
            ${s.coder.start ? `<span>Старт</span><span>${fmtDate(s.coder.start)}</span>` : ''}${s.coder.deadline ? `<span>Сдача</span><span>${fmtDate(s.coder.deadline)}</span>` : ''}</div>
            <div class="progress"><i style="width:${pct}%"></i></div>`;
    } else {
        $('#sCoder').innerHTML = `<h3>🧑‍💻 Кодер</h3><div class="muted" style="font-size:13.5px;">${s.status === 'choose' ? 'Администратор предложил кодеров — выберите в чате 👆' : 'Администратор подберёт кодера после обсуждения деталей.'}</div>`;
    }
    const pending = s.invoices.filter(i => i.status === 'pending');
    $('#sPay').innerHTML = `<h3>💳 Оплата</h3>
        <div class="kv"><span>Оплачено</span><span>${money(s.paid)}</span>${pending.length ? `<span>К оплате</span><span style="color:#fde68a">${money(pending.reduce((a, i) => a + i.amount, 0))}</span>` : ''}</div>
        ${!s.invoices.length ? '<div class="muted" style="font-size:13px; margin-top:8px;">Счёт появится в чате, когда администратор согласует стоимость.</div>' : ''}`;
    renderCards();
    updateTicks();
}

function updateTicks() { $$('[data-tick]', msgsEl).forEach(el => { const read = +el.dataset.tick <= C.staffSeen; el.textContent = read ? '✓✓' : '✓'; el.classList.toggle('read', read); el.title = read ? 'Прочитано' : 'Доставлено'; }); }

function appendMessages(list, initial) {
    if (!list.length) return;
    const stick = initial || nearBottom();
    let newStaff = 0;
    list.forEach(m => {
        if (m.id <= C.after) return;
        C.after = m.id;
        msgsEl.insertAdjacentHTML('beforeend', msgNode(m));
        if (!initial && m.from !== 'client') newStaff++;
    });
    renderCards();
    if (stick) toBottom(true);
    if (newStaff) notifyNew(newStaff, list[list.length - 1]);
}

let typingTimer = null;
async function poll(initial = false) {
    clearTimeout(C.timer);
    if (!C.id) return;
    const id = C.id;
    try {
        const visible = document.visibilityState === 'visible';
        const j = await api('poll', null, `&o=${id}&t=${encodeURIComponent(C.t)}&after=${C.after}&rev=${C.rev}&seen=${visible ? 1 : 0}`);
        if (id !== C.id) return;
        C.fails = 0;
        if (j.state) { C.state = j.state; C.rev = j.state.rev; }
        appendMessages(j.messages, initial);
        if (j.state) renderState();
        if (j.staff_seen !== C.staffSeen) { C.staffSeen = j.staff_seen; updateTicks(); }
        $('#cTyping').hidden = !j.typing; if (j.typing) { $('#cTypingWho').textContent = j.typing; toBottom(); }
        $('#cAdmAva').classList.toggle('on', j.staff_online);
        $('#cOnline').textContent = j.typing ? 'печатает…' : (j.staff_online ? 'в сети' : 'ответит, как только увидит');
    } catch (e) {
        C.fails++;
        if (C.fails === 1 && /доступ|не найден/i.test(e.message)) { toast(e.message, 'error'); return; }
    }
    const visible = document.visibilityState === 'visible';
    C.timer = setTimeout(poll, C.fails ? Math.min(30000, 2000 * 2 ** C.fails) : (visible ? 2000 : 8000));
}
document.addEventListener('visibilitychange', () => { if (document.visibilityState === 'visible' && C.id) { C.unreadHidden = 0; document.title = document.title.replace(/^\(\d+\) /, ''); poll(); } });

/* звук и уведомление о новом ответе, если вкладка свёрнута */
let actx = null;
function ping() { try { actx = actx || new (window.AudioContext || window.webkitAudioContext)(); const t = actx.currentTime;
    [880, 1320].forEach((f, i) => { const o = actx.createOscillator(), g = actx.createGain(); o.frequency.value = f; o.type = 'sine';
        g.gain.setValueAtTime(0.0001, t + i * .12); g.gain.exponentialRampToValueAtTime(0.12, t + i * .12 + .02); g.gain.exponentialRampToValueAtTime(0.0001, t + i * .12 + .25);
        o.connect(g).connect(actx.destination); o.start(t + i * .12); o.stop(t + i * .12 + .3); }); } catch (e) {} }
function notifyNew(n, m) {
    if (document.visibilityState === 'visible') { ping(); return; }
    C.unreadHidden += n; ping();
    document.title = `(${C.unreadHidden}) ` + document.title.replace(/^\(\d+\) /, '');
    if ('Notification' in window && Notification.permission === 'granted') {
        try { new Notification('RTeam · заказ №' + C.id, { body: m.text ? m.text.slice(0, 120) : 'Новое сообщение', icon: 'favicon.jpeg' }); } catch (e) {}
    }
}

/* отправка сообщения */
const cText = $('#cText');
const autoGrow = () => { cText.style.height = 'auto'; cText.style.height = Math.min(180, cText.scrollHeight) + 'px'; };
let lastTyping = 0;
cText.addEventListener('input', () => { autoGrow(); const now = Date.now(); if (now - lastTyping > 2500 && cText.value.trim()) { lastTyping = now; const fd = new FormData(); fd.append('o', C.id); fd.append('t', C.t); api('typing', fd).catch(() => {}); } });
cText.addEventListener('keydown', e => { if (e.key === 'Enter' && !e.shiftKey && !e.isComposing) { e.preventDefault(); $('#cForm').requestSubmit(); } });
$('#cFiles').addEventListener('change', e => { [...e.target.files].forEach(f => { if (C.files.length < 5) C.files.push(f); }); e.target.value = ''; renderAtt(); });
function renderAtt() { $('#cAtt').innerHTML = C.files.map((f, i) => `<span class="att">📎 ${esc(f.name)} <button type="button" data-rm="${i}">✕</button></span>`).join(''); }
$('#cAtt').addEventListener('click', e => { const b = e.target.closest('[data-rm]'); if (b) { C.files.splice(+b.dataset.rm, 1); renderAtt(); } });
$('#cForm').addEventListener('submit', async e => {
    e.preventDefault();
    const text = cText.value.trim();
    if (!text && !C.files.length) return;
    if (C.files.some(f => f.size > 8 * 1048576)) return toast('Файл больше 8 МБ — сожмите его или пришлите ссылкой.', 'error');
    const fd = new FormData(); fd.append('o', C.id); fd.append('t', C.t); fd.append('text', text); C.files.forEach(f => fd.append('files[]', f));
    const keep = { text, files: C.files.slice() };
    cText.value = ''; autoGrow(); C.files = []; renderAtt();
    try {
        const j = await api('send', fd);
        appendMessages([j.message], false); toBottom(true);
        if ('Notification' in window && Notification.permission === 'default') Notification.requestPermission().catch(() => {});
    } catch (err) { toast(err.message, 'error'); cText.value = keep.text; C.files = keep.files; renderAtt(); autoGrow(); }
});

/* клики по карточкам в чате */
msgsEl.addEventListener('click', async e => {
    const z = e.target.closest('[data-zoom]');
    if (z) { const lb = document.createElement('div'); lb.className = 'lightbox'; lb.innerHTML = `<img src="${z.src}" alt="">`; lb.onclick = () => lb.remove(); document.body.appendChild(lb); return; }
    const ch = e.target.closest('[data-choose]');
    if (ch) {
        if (!confirm(`Выбрать кодера ${ch.dataset.choose}? Задача сразу встанет в его календарь.`)) return;
        ch.disabled = true;
        const fd = new FormData(); fd.append('o', C.id); fd.append('t', C.t); fd.append('offer', ch.dataset.offer); fd.append('coder', ch.dataset.choose);
        try { await api('choose', fd); toast('Готово! Кодер получил задачу 🎉', 'ok'); C.rev = -1; poll(); } catch (err) { toast(err.message, 'error'); ch.disabled = false; }
        return;
    }
    const chk = e.target.closest('[data-check]');
    if (chk) return checkPay(chk.dataset.check, false, chk);
    const star = e.target.closest('[data-star]');
    if (star) { const box = star.parentElement; box.dataset.v = star.dataset.star; $$('button', box).forEach(b => b.classList.toggle('on', +b.dataset.star <= +box.dataset.v)); return; }
    const rv = e.target.closest('[data-review]');
    if (rv) {
        const card = rv.closest('.xcard'), v = +$('.star-pick', card).dataset.v, text = $('textarea', card).value.trim();
        if (!v) return toast('Поставьте оценку звёздами', 'error');
        rv.disabled = true;
        const fd = new FormData(); fd.append('o', C.id); fd.append('t', C.t); fd.append('rating', v); fd.append('text', text);
        try { const j = await api('review', fd); toast(j.published ? 'Спасибо! Отзыв опубликован ⭐' : 'Спасибо! Отзыв появится после проверки ⭐', 'ok'); C.rev = -1; poll(); }
        catch (err) { toast(err.message, 'error'); rv.disabled = false; }
    }
});

async function checkPay(inv, silent, btn) {
    if (btn) { btn.disabled = true; btn.innerHTML = '<span class="spin"></span> Проверяем…'; }
    const fd = new FormData(); fd.append('o', C.id); fd.append('t', C.t); fd.append('inv', inv);
    try { const j = await api('check_pay', fd); if (!silent || j.status === 'paid') toast(j.msg, j.status === 'paid' ? 'ok' : ''); C.rev = -1; poll(); }
    catch (err) { if (!silent) toast(err.message, 'error'); }
    finally { if (btn) { btn.disabled = false; btn.textContent = 'Я оплатил(а) — проверить'; } }
    const q = new URLSearchParams(location.search); if (q.get('paid')) { q.delete('paid'); history.replaceState(null, '', location.pathname + '?' + q.toString()); }
}

$('#btnCopy').addEventListener('click', () => { const i = $('#cLink'); i.select(); (navigator.clipboard ? navigator.clipboard.writeText(i.value) : Promise.reject()).then(() => toast('Ссылка скопирована', 'ok')).catch(() => { document.execCommand('copy'); toast('Ссылка скопирована', 'ok'); }); });
function newOrder() { clearTimeout(C.timer); C.id = 0; history.replaceState(null, '', location.pathname); document.title = 'Заказать сайт — RTeam'; $('#viewChat').hidden = true; $('#viewForm').hidden = false; window.scrollTo({ top: 0 }); }
$('#btnNew').addEventListener('click', newOrder);

/* ---------- Мои заказы ---------- */
const drawer = $('#mineDrawer');
async function loadMine() {
    const fd = new FormData(); fd.append('tokens', JSON.stringify(store.get()));
    try {
        const j = await api('mine', fd);
        j.orders.forEach(o => store.add(o.id, o.t));
        $('#mineDot').hidden = !j.orders.some(o => o.unread > 0);
        $('#mineList').innerHTML = j.orders.length ? j.orders.map(o => `
            <div class="mine-item" data-o="${o.id}" data-t="${esc(o.t)}"><div class="mi"><b>${esc(o.title)}</b><span class="muted">№${o.id} · ${new Date(o.created * 1000).toLocaleDateString('ru-RU')}</span></div>
            <span class="pill" style="--c:${esc(o.color)}; font-size:11.5px; padding:4px 9px;">${esc(o.status_label)}</span>${o.unread ? `<span class="unread">${o.unread}</span>` : ''}</div>`).join('')
            : '<div class="muted">Здесь появятся заказы, оформленные в этом браузере' + (CFG.account ? ' или с вашего аккаунта' : '') + '.</div>';
    } catch (e) { $('#mineList').innerHTML = '<div class="muted">Не удалось загрузить: ' + esc(e.message) + '</div>'; }
}
$('#btnMine').addEventListener('click', () => { drawer.classList.add('open'); loadMine(); });
$('#btnMineClose').addEventListener('click', () => drawer.classList.remove('open'));
drawer.addEventListener('click', e => { if (e.target === drawer) drawer.classList.remove('open'); const it = e.target.closest('.mine-item'); if (it) { drawer.classList.remove('open'); openOrder(+it.dataset.o, it.dataset.t); } });
$('#btnMineNew').addEventListener('click', () => { drawer.classList.remove('open'); newOrder(); });
document.addEventListener('keydown', e => { if (e.key === 'Escape') { drawer.classList.remove('open'); $$('.lightbox').forEach(l => l.remove()); } });

/* ---------- старт: открыть заказ по ссылке или последний из этого браузера ---------- */
(() => {
    const q = new URLSearchParams(location.search);
    const o = +q.get('o'), t = q.get('t') || '';
    if (o && t) { store.add(o, t); openOrder(o, t); }
    else if (o && CFG.account) openOrder(o, '');
    if (store.get().length || CFG.account) loadMine();
})();
</script>
</body>
</html>
