<?php
/* ==========================================================
   ИНТЕРФЕЙС ВКЛАДОК «Расписание», «Заказы сайтов», «Отзывы».
   Подключается из _schedule_admin.php. Стили и скрипты — здесь же,
   в общем стиле админ-панели (стекло, акцентный цвет, тёмная тема).
   ========================================================== */

function so_plural($n, $one, $few, $many) {
    $n = abs((int)$n) % 100;
    $n1 = $n % 10;
    if ($n > 10 && $n < 20) return $many;
    return $n1 === 1 ? $one : ($n1 >= 2 && $n1 <= 4 ? $few : $many);
}

function so_admin_css() {
?>
<style>
/* ===== Общие элементы модуля ===== */
.so-bar { display: flex; gap: 12px; flex-wrap: wrap; align-items: center; padding: 12px 14px !important; }
.so-bar form { margin: 0; }
.so-bar select { margin-top: 0; width: auto; min-width: 220px; }
.so-seg { display: inline-flex; gap: 2px; padding: 4px; border-radius: 12px; background: rgba(255,255,255,.04); border: 1px solid var(--line); }
.so-seg a, .so-seg button { padding: 7px 12px; border-radius: 9px; color: var(--soft); font-weight: 600; font-size: 13px; border: 0; background: none; cursor: pointer; text-decoration: none !important; }
.so-seg .on { background: color-mix(in srgb, var(--accent) 20%, transparent); color: #fff; box-shadow: inset 0 0 0 1px color-mix(in srgb, var(--accent) 35%, transparent); }
.so-month { display: flex; align-items: center; gap: 8px; margin: 0 auto; }
.so-month b { font-size: 18px; min-width: 170px; text-align: center; letter-spacing: -.01em; }
.so-nav-btn { width: 36px; height: 36px; border-radius: 10px; border: 1px solid var(--line-2); display: grid; place-items: center; color: #fff !important; background: rgba(255,255,255,.03); text-decoration: none !important; font-size: 18px; }
.so-nav-btn:hover { background: rgba(255,255,255,.08); }
.so-legend { display: flex; gap: 14px; flex-wrap: wrap; font-size: 12.5px; color: var(--soft); align-items: center; }
.so-legend i { display: inline-block; width: 12px; height: 12px; border-radius: 4px; margin-right: 6px; vertical-align: -1px; }
.so-ava { width: 38px; height: 38px; border-radius: 12px; display: grid; place-items: center; font-weight: 800; color: #fff; flex: none;
    background: linear-gradient(135deg, var(--c, var(--accent)), color-mix(in srgb, var(--c, var(--accent)) 45%, #000)); box-shadow: 0 0 0 2px color-mix(in srgb, var(--c, var(--accent)) 30%, transparent); }
.so-ava.sm { width: 28px; height: 28px; border-radius: 9px; font-size: 12px; }
.so-ava.lg { width: 54px; height: 54px; border-radius: 16px; font-size: 22px; }
.so-st { display: inline-block; font-size: 11px; font-weight: 700; padding: 2px 9px; border-radius: 999px; white-space: nowrap;
    color: color-mix(in srgb, var(--c) 72%, #fff); background: color-mix(in srgb, var(--c) 15%, transparent); border: 1px solid color-mix(in srgb, var(--c) 42%, transparent); }
.so-kpis { display: grid; grid-template-columns: repeat(auto-fill, minmax(170px, 1fr)); gap: 12px; }
.so-kpi { padding: 14px 16px; border-radius: 16px; background: linear-gradient(160deg, rgba(26,26,38,.92), rgba(14,14,21,.92)); border: 1px solid var(--line); }
.so-kpi .i { font-size: 18px; }
.so-kpi b { display: block; font-size: 24px; font-weight: 800; letter-spacing: -.02em; margin-top: 4px; font-variant-numeric: tabular-nums; }
.so-kpi span { color: var(--soft); font-size: 12.5px; }
.so-kpi.hot b { background: linear-gradient(135deg, #fff, var(--accent)); -webkit-background-clip: text; background-clip: text; color: transparent; }

/* ===== Календарь сотрудника ===== */
.so-cal-wrap { display: grid; grid-template-columns: minmax(0, 1fr) 330px; gap: 16px; align-items: start; margin-top: 14px; }
.so-cal { padding: 14px !important; margin-top: 0 !important; }
.so-wd, .so-grid { display: grid; grid-template-columns: repeat(7, minmax(0, 1fr)); gap: 6px; }
.so-wd { margin-bottom: 6px; }
.so-wd span { text-align: center; font-size: 11px; text-transform: uppercase; letter-spacing: .1em; color: var(--muted); font-weight: 700; padding: 4px 0; }
.so-day { position: relative; min-height: 108px; padding: 8px 8px 16px; border-radius: 12px; border: 1px solid var(--line); background: rgba(255,255,255,.02);
    display: flex; flex-direction: column; gap: 4px; user-select: none; transition: transform .12s, border-color .15s, background .2s; min-width: 0; }
.so-day .n { font-weight: 750; font-size: 14px; }
.so-day .ti { position: absolute; top: 6px; right: 8px; font-size: 14px; }
.t-work.so-day { background: linear-gradient(180deg, rgba(34,197,94,.11), rgba(34,197,94,.03)); border-color: rgba(34,197,94,.24); }
.t-off.so-day { background: rgba(100,116,139,.05); border-color: rgba(100,116,139,.16); }
.t-off.so-day .n { color: var(--muted); }
.t-vacation.so-day { background: repeating-linear-gradient(135deg, rgba(245,158,11,.17) 0 8px, rgba(245,158,11,.07) 8px 16px); border-color: rgba(245,158,11,.5); }
.t-sick.so-day { background: repeating-linear-gradient(135deg, rgba(239,68,68,.16) 0 8px, rgba(239,68,68,.06) 8px 16px); border-color: rgba(239,68,68,.5); }
.so-day.out { opacity: .32; }
.so-day.today { box-shadow: 0 0 0 2px var(--accent), 0 0 26px -6px var(--accent); }
.so-day.today .n::after { content: "сегодня"; margin-left: 6px; font-size: 10px; font-weight: 700; text-transform: uppercase; letter-spacing: .08em; color: color-mix(in srgb, var(--accent) 70%, #fff); }
.so-day.ovr::after { content: ""; position: absolute; left: 9px; bottom: 7px; width: 6px; height: 6px; border-radius: 50%; background: #fff; opacity: .55; }
.so-day.so-sel, .tm-cell.so-sel { outline: 2px solid #fff; outline-offset: -2px; transform: scale(.97); }
.so-task { display: block; font-size: 11.5px; font-weight: 600; padding: 2px 7px; border-radius: 6px; color: #fff !important; text-decoration: none !important;
    background: hsl(var(--h) 70% 45% / .32); border-left: 3px solid hsl(var(--h) 85% 62%); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.so-task.dl { background: hsl(var(--h) 75% 45% / .62); }
.so-task.warn { box-shadow: inset 0 0 0 1px #fbbf24; }
body.painting [data-date][data-login] { cursor: crosshair; touch-action: none; }
body.painting .so-task { pointer-events: none; }
.so-brush { display: flex; gap: 6px; flex-wrap: wrap; align-items: center; }
.so-brush button { display: inline-flex; gap: 6px; align-items: center; padding: 7px 12px; border-radius: 10px; border: 1px solid var(--line-2); background: rgba(255,255,255,.03); color: var(--soft); font-weight: 600; font-size: 13px; cursor: pointer; transition: all .15s; }
.so-brush button:hover { color: #fff; }
.so-brush button.on { color: #fff; border-color: var(--bc); background: color-mix(in srgb, var(--bc) 22%, transparent); box-shadow: 0 0 0 3px color-mix(in srgb, var(--bc) 18%, transparent); }
.so-side { display: grid; gap: 14px; }
.so-side .card { margin-top: 0 !important; }
.so-person { display: flex; gap: 12px; align-items: center; }
.so-person b { font-size: 16px; display: block; }
.so-stats { display: grid; grid-template-columns: repeat(2, 1fr); gap: 8px; margin-top: 12px; }
.so-stats div { padding: 10px 12px; border-radius: 12px; background: rgba(255,255,255,.03); border: 1px solid var(--line); font-size: 12px; color: var(--soft); }
.so-stats b { display: block; font-size: 20px; color: #fff; font-variant-numeric: tabular-nums; }
.so-week { display: flex; gap: 5px; margin-top: 8px; }
.so-week label { flex: 1; position: relative; }
.so-week input { position: absolute; opacity: 0; pointer-events: none; }
.so-week span { display: block; text-align: center; padding: 9px 0; border-radius: 10px; border: 1px solid var(--line-2); font-weight: 700; font-size: 12px; color: var(--muted); cursor: pointer; transition: all .15s; }
.so-week input:checked + span { background: rgba(34,197,94,.18); border-color: rgba(34,197,94,.5); color: #d1fae5; }
.so-ti { padding: 10px 12px; border-radius: 12px; background: rgba(255,255,255,.03); border: 1px solid var(--line); border-left: 3px solid hsl(var(--h) 85% 62%); margin-top: 8px; }
.so-ti .r { display: flex; justify-content: space-between; gap: 8px; align-items: center; }
.so-ti a.t { color: #fff; font-weight: 650; }
.so-ti .d { font-size: 12px; color: var(--muted); margin-top: 3px; }
.so-ti .so-btns { margin-top: 8px; }

/* ===== Вся команда ===== */
.tm-wrap { overflow-x: auto; padding: 10px !important; margin-top: 14px; }
.tm { border-collapse: separate; border-spacing: 3px; width: max-content; min-width: 100%; }
.tm th { font-size: 11px; color: var(--muted); font-weight: 700; text-align: center; padding: 4px 0; min-width: 30px; }
.tm th span { display: block; font-size: 9.5px; text-transform: uppercase; letter-spacing: .05em; }
.tm th.we { color: #64748b; }
.tm th.today { color: #fff; background: color-mix(in srgb, var(--accent) 28%, transparent); border-radius: 8px; }
.tm .tm-who { position: sticky; left: 0; z-index: 2; background: #101017; min-width: 210px; text-align: left; padding: 6px 10px; border-radius: 10px; }
.tm .tm-who a { display: flex; gap: 9px; align-items: center; color: #fff; text-decoration: none; }
.tm .tm-who small { display: block; color: var(--muted); font-size: 11.5px; font-weight: 500; }
.tm .tm-sum { font-size: 11.5px; color: var(--soft); white-space: nowrap; padding: 0 8px; text-align: left; }
.tm-cell { height: 36px; min-width: 30px; border-radius: 8px; position: relative; transition: transform .1s; }
.tm-cell.t-work { background: rgba(34,197,94,.24); }
.tm-cell.t-off { background: rgba(100,116,139,.12); }
.tm-cell.t-vacation { background: repeating-linear-gradient(135deg, rgba(245,158,11,.6) 0 5px, rgba(245,158,11,.35) 5px 10px); }
.tm-cell.t-sick { background: repeating-linear-gradient(135deg, rgba(239,68,68,.55) 0 5px, rgba(239,68,68,.3) 5px 10px); }
.tm-cell.today { box-shadow: 0 0 0 2px var(--accent); }
.tm-cell.ovr::before { content: ""; position: absolute; top: 4px; right: 4px; width: 4px; height: 4px; border-radius: 50%; background: #fff; opacity: .6; }
.tm-cell i { position: absolute; left: 4px; right: 4px; bottom: 4px; height: 4px; border-radius: 2px; background: hsl(var(--h) 85% 62%); }
.tm-cell b { position: absolute; inset: 0; display: grid; place-items: center; font-size: 13px; }
.tm tr:hover .tm-who { background: #16161f; }

/* ===== Мессенджер заказов ===== */
.so-msgr { display: grid; grid-template-columns: 300px minmax(0, 1fr) 370px; height: calc(100vh - 100px); min-height: 560px; margin-top: 14px; scroll-margin-top: 84px;
    border-radius: 18px; overflow: hidden; border: 1px solid var(--line-2); background: rgba(8,8,13,.9); box-shadow: 0 30px 70px -40px rgba(0,0,0,.9); }
.so-list { border-right: 1px solid var(--line); display: flex; flex-direction: column; min-height: 0; background: #09090f; }
.so-list-top { padding: 12px; border-bottom: 1px solid var(--line); display: grid; gap: 8px; }
.so-list-top input { margin-top: 0; }
.so-filters { display: flex; gap: 4px; flex-wrap: wrap; }
.so-filters button { font-size: 11.5px; padding: 5px 9px; border-radius: 8px; border: 1px solid var(--line); background: none; color: var(--soft); cursor: pointer; font-weight: 600; font-family: inherit; }
.so-filters button.on { color: #fff; border-color: color-mix(in srgb, var(--accent) 55%, transparent); background: color-mix(in srgb, var(--accent) 14%, transparent); }
.so-filters b { opacity: .65; margin-left: 3px; font-weight: 700; }
#soList { flex: 1; overflow-y: auto; }
.so-item { display: block; padding: 12px 14px; border-bottom: 1px solid var(--line); color: var(--text) !important; cursor: pointer; text-decoration: none !important; transition: background .15s; }
.so-item:hover { background: rgba(255,255,255,.03); }
.so-item.on { background: color-mix(in srgb, var(--accent) 11%, transparent); box-shadow: inset 3px 0 0 var(--accent); }
.so-item .r1 { display: flex; justify-content: space-between; gap: 8px; align-items: center; }
.so-item .r1 b { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-size: 13.5px; }
.so-item .r1 time { font-size: 11px; color: var(--muted); white-space: nowrap; }
.so-item .r2 { font-size: 12px; color: var(--muted); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; margin-top: 3px; }
.so-item .r3 { display: flex; gap: 6px; align-items: center; margin-top: 7px; }
.so-item .r3 .who { font-size: 11.5px; color: var(--soft); margin-left: auto; white-space: nowrap; }
.so-unread { min-width: 20px; height: 20px; padding: 0 6px; border-radius: 999px; background: var(--accent); color: #fff; font-size: 11px; font-weight: 800; display: inline-grid; place-items: center; box-shadow: 0 0 10px color-mix(in srgb, var(--accent) 60%, transparent); }
.so-chat { display: flex; flex-direction: column; min-width: 0; min-height: 0; background: radial-gradient(500px 300px at 50% 0%, color-mix(in srgb, var(--accent) 6%, transparent), transparent 70%), #0b0b11; }
.so-chat-head { padding: 12px 16px; border-bottom: 1px solid var(--line); display: flex; gap: 12px; align-items: center; background: rgba(13,13,20,.9); min-height: 64px; }
.so-chat-head .t { min-width: 0; flex: 1; }
.so-chat-head .t b { display: block; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.so-chat-head .t span { font-size: 12px; color: var(--muted); }
.so-online { display: inline-block; width: 8px; height: 8px; border-radius: 50%; background: #4b5563; margin-right: 5px; }
.so-online.on { background: var(--ok); box-shadow: 0 0 8px var(--ok); }
.so-msgs { flex: 1; overflow-y: auto; padding: 18px; display: flex; flex-direction: column; gap: 6px; }
.so-dsep { align-self: center; font-size: 11.5px; color: var(--muted); padding: 3px 12px; border-radius: 999px; background: rgba(255,255,255,.04); margin: 10px 0 4px; }
.so-m { display: flex; gap: 8px; max-width: 80%; align-items: flex-end; animation: rise .25s ease both; }
.so-m.right { align-self: flex-end; flex-direction: row-reverse; }
.so-b { padding: 9px 13px; border-radius: 15px; background: #181822; border: 1px solid var(--line-2); min-width: 0; }
.so-m.client .so-b { border-bottom-left-radius: 5px; }
.so-m.right .so-b { background: color-mix(in srgb, var(--accent) 15%, #12090b); border-color: color-mix(in srgb, var(--accent) 40%, transparent); border-bottom-right-radius: 5px; }
.so-m.right.other .so-b { background: rgba(59,130,246,.1); border-color: rgba(59,130,246,.32); }
.so-m.internal .so-b { background: rgba(245,158,11,.08) !important; border: 1px dashed rgba(245,158,11,.55) !important; }
.so-mh { font-size: 11.5px; font-weight: 700; color: var(--soft); margin-bottom: 3px; display: flex; gap: 6px; align-items: center; flex-wrap: wrap; }
.so-mh .rl { font-weight: 600; font-size: 10.5px; padding: 1px 7px; border-radius: 999px; color: color-mix(in srgb, var(--c, #888) 75%, #fff); background: color-mix(in srgb, var(--c, #888) 16%, transparent); }
.so-mh .lock { color: #fcd34d; }
.so-tx { white-space: pre-wrap; overflow-wrap: anywhere; font-size: 14px; line-height: 1.5; }
.so-tx a { color: inherit; text-decoration: underline; }
.so-mt { font-size: 10.5px; color: var(--muted); text-align: right; margin-top: 3px; }
.so-mt .read { color: #6ee7a0; }
.so-imgs { display: grid; grid-template-columns: repeat(auto-fill, minmax(110px, 1fr)); gap: 6px; margin-top: 6px; }
.so-imgs img { width: 100%; max-height: 200px; object-fit: cover; border-radius: 10px; display: block; cursor: zoom-in; }
.so-fl { display: flex; gap: 8px; align-items: center; margin-top: 6px; padding: 7px 10px; border-radius: 10px; background: rgba(0,0,0,.25); color: inherit !important; font-size: 13px; }
.so-sys { align-self: center; font-size: 12.5px; color: var(--soft); background: rgba(255,255,255,.04); border: 1px dashed var(--line-2); padding: 6px 14px; border-radius: 14px; text-align: center; max-width: 90%; }
.so-xc { align-self: center; width: min(460px, 94%); padding: 12px 14px; border-radius: 14px; background: rgba(255,255,255,.03); border: 1px solid var(--line-2); border-left: 3px solid var(--c, var(--accent)); font-size: 13px; margin: 6px 0; }
.so-xc .k { font-size: 11px; text-transform: uppercase; letter-spacing: .1em; font-weight: 700; color: color-mix(in srgb, var(--c, var(--accent)) 70%, #fff); }
.so-xc b.sum { font-size: 20px; }
.so-typing { padding: 2px 18px 8px; font-size: 12.5px; color: var(--muted); }
.so-typing i { display: inline-block; width: 5px; height: 5px; border-radius: 50%; background: var(--soft); margin-right: 2px; animation: soDot 1.2s infinite; }
.so-typing i:nth-child(2) { animation-delay: .15s; } .so-typing i:nth-child(3) { animation-delay: .3s; }
@keyframes soDot { 0%, 60%, 100% { opacity: .3; transform: none; } 30% { opacity: 1; transform: translateY(-3px); } }
.so-comp { border-top: 1px solid var(--line); padding: 10px 12px 12px; background: rgba(13,13,20,.95); }
.so-quick { display: flex; gap: 6px; flex-wrap: wrap; margin-bottom: 8px; }
.so-quick button { font-size: 12px; padding: 4px 10px; border-radius: 999px; border: 1px solid var(--line); background: rgba(255,255,255,.03); color: var(--soft); cursor: pointer; font-family: inherit; }
.so-quick button:hover { color: #fff; border-color: var(--line-2); }
.so-comp-row { display: flex; gap: 8px; align-items: flex-end; }
.so-comp-row textarea { margin: 0 !important; flex: 1; min-height: 44px; height: 44px; max-height: 170px; resize: none; }
.so-comp-row .btn { margin-top: 0; height: 44px; }
.so-attach { width: 44px; height: 44px; flex: none; display: grid; place-items: center; border-radius: 11px; border: 1px solid var(--line-2); background: rgba(255,255,255,.04); cursor: pointer; font-size: 17px; }
.so-int { display: inline-flex; align-items: center; gap: 2px; font-size: 12.5px; color: var(--soft); cursor: pointer; white-space: nowrap; }
.so-comp.internal textarea { border-color: rgba(245,158,11,.6) !important; background: rgba(245,158,11,.06) !important; }
.so-att { display: flex; flex-wrap: wrap; gap: 6px; margin-bottom: 8px; }
.so-att:empty { display: none; }
.so-att span { font-size: 12px; padding: 4px 9px; border-radius: 9px; background: rgba(255,255,255,.06); }
.so-att button { background: none; border: 0; color: var(--muted); cursor: pointer; }
.so-panel { border-left: 1px solid var(--line); overflow-y: auto; padding: 14px; background: #09090f; min-height: 0; }
.so-sec { padding: 14px; border-radius: 14px; background: rgba(255,255,255,.025); border: 1px solid var(--line); margin-bottom: 12px; }
.so-sec > h4 { margin: 0 0 10px; font-size: 11.5px; text-transform: uppercase; letter-spacing: .1em; color: var(--muted); display: flex; justify-content: space-between; align-items: center; gap: 8px; }
.so-sec label { display: block; margin-top: 8px; }
.so-sec input, .so-sec select, .so-sec textarea { margin-top: 4px; }
.so-sec textarea { height: 64px; }
.so-kv { display: grid; grid-template-columns: auto 1fr; gap: 5px 12px; font-size: 13px; }
.so-kv > span:nth-child(odd) { color: var(--muted); }
.so-kv > span:nth-child(even) { color: #fff; text-align: right; overflow-wrap: anywhere; }
.so-desc { white-space: pre-wrap; font-size: 13px; color: var(--soft); background: rgba(0,0,0,.28); padding: 10px 12px; border-radius: 10px; margin-top: 10px; max-height: 240px; overflow: auto; overflow-wrap: anywhere; }
.so-btns { display: flex; gap: 6px; flex-wrap: wrap; margin-top: 8px; }
.so-btns form { margin: 0; }
.so-btns .btn { margin-top: 0; }
.so-inv { padding: 10px 12px; border-radius: 12px; border: 1px solid var(--line); background: rgba(0,0,0,.22); margin-bottom: 8px; }
.so-inv .top { display: flex; justify-content: space-between; gap: 8px; align-items: center; }
.so-inv b { font-size: 16px; }
.so-inv .d { font-size: 12px; color: var(--muted); margin-top: 2px; }
.so-info { font-size: 12.5px; padding: 8px 10px; border-radius: 10px; background: rgba(59,130,246,.08); border: 1px solid rgba(59,130,246,.25); color: #cfe0ff; margin-top: 8px; }
.so-info.warn { background: rgba(245,158,11,.08); border-color: rgba(245,158,11,.38); color: #ffe2b0; }
.so-info.ok { background: rgba(34,197,94,.08); border-color: rgba(34,197,94,.35); color: #d1fae5; }
.so-pick { display: grid; gap: 6px; max-height: 280px; overflow: auto; margin-top: 6px; }
.so-pick label { display: flex !important; gap: 10px; align-items: center; padding: 8px 10px; border-radius: 10px; border: 1px solid var(--line); cursor: pointer; margin: 0 !important; }
.so-pick label:has(input:checked) { border-color: color-mix(in srgb, var(--accent) 55%, transparent); background: color-mix(in srgb, var(--accent) 8%, transparent); }
.so-pick .pi { flex: 1; min-width: 0; font-size: 12px; color: var(--muted); }
.so-pick .pi b { display: block; font-size: 13.5px; color: #fff; }
.so-chips { display: flex; gap: 5px; flex-wrap: wrap; margin-top: 8px; }
.so-chips .chip { font-size: 11.5px; padding: 3px 9px; }
.so-chips button.chip { cursor: pointer; font-family: inherit; }
.so-empty-c { margin: auto; text-align: center; color: var(--muted); padding: 30px; }
.so-empty-c .big { font-size: 50px; margin-bottom: 6px; }
.so-hist { font-size: 12px; color: var(--soft); max-height: 220px; overflow: auto; }
.so-hist div { padding: 5px 0; border-bottom: 1px dashed var(--line); }
.so-hist span { color: var(--muted); }
.so-lightbox { position: fixed; inset: 0; z-index: 300; background: rgba(0,0,0,.85); display: grid; place-items: center; cursor: zoom-out; padding: 20px; }
.so-lightbox img { max-width: 100%; max-height: 100%; border-radius: 12px; }

/* ===== Отзывы ===== */
.so-rv { margin-top: 12px !important; }
.so-rv .h { display: flex; gap: 12px; align-items: center; flex-wrap: wrap; }
.so-rv .stars { color: #fbbf24; letter-spacing: 1px; font-size: 16px; }
.so-rv .stars .off { color: #2f2f3d; }
.so-rv .txt { white-space: pre-wrap; margin-top: 10px; color: #dcdce6; }
.so-rv .rep { margin-top: 10px; padding: 10px 12px; border-left: 3px solid var(--accent); background: color-mix(in srgb, var(--accent) 7%, transparent); border-radius: 0 10px 10px 0; }

@media (max-width: 1350px) {
    .so-msgr { grid-template-columns: 280px minmax(0, 1fr); height: auto; }
    .so-chat { height: 72vh; min-height: 520px; }
    .so-list { height: 72vh; min-height: 520px; }
    .so-panel { grid-column: 1 / -1; border-left: 0; border-top: 1px solid var(--line); display: grid; grid-template-columns: repeat(auto-fill, minmax(320px, 1fr)); gap: 12px; align-items: start; }
    .so-panel .so-sec { margin-bottom: 0; }
}
@media (max-width: 1100px) { .so-cal-wrap { grid-template-columns: 1fr; } }
@media (max-width: 760px) {
    .so-msgr { grid-template-columns: 1fr; }
    .so-list { height: 260px; min-height: 0; border-right: 0; border-bottom: 1px solid var(--line); }
    .so-day { min-height: 64px; padding: 5px 5px 12px; }
    .so-day .ti { font-size: 11px; top: 4px; right: 4px; }
    .so-day.today .n::after { display: none; }
    .so-task { font-size: 0; height: 5px; padding: 0; border-left-width: 0; }
    .so-month b { min-width: 0; font-size: 16px; }
    .so-m { max-width: 94%; }
}
</style>
<?php
}

/* ==========================================================
   РАСПИСАНИЕ
   ========================================================== */

function so_render_schedule() {
    $me = so_me();
    $staff = so_staff();
    $leader = so_leader();
    $see_all = $leader || so_manager();
    if (!$staff) { echo '<div class="empty">В команде пока никого нет.</div>'; return; }
    $who = (string)($_GET['who'] ?? $me);
    if (!$see_all || !isset($staff[$who])) $who = isset($staff[$me]) ? $me : (string)array_key_first($staff);
    $view = ($see_all && ($_GET['view'] ?? '') === 'team') ? 'team' : 'cal';
    $m = preg_match('/^\d{4}-(0[1-9]|1[0-2])$/', (string)($_GET['m'] ?? '')) ? (string)$_GET['m'] : date('Y-m');
    $t0 = strtotime($m . '-01');
    $dim = (int)date('t', $t0);
    $first = $m . '-01';
    $last = $m . '-' . sprintf('%02d', $dim);
    $today = date('Y-m-d');
    $sched = so_sched();
    $idx = so_index();
    $types = so_day_types();
    $url = function (array $p) use ($who, $m, $view) {
        return '?' . http_build_query(array_merge(['tab' => 'schedule', 'who' => $who, 'm' => $m, 'view' => $view], $p));
    };
    // сначала кодеры, потом остальные
    $coders = array_filter($staff, function ($s, $l) { return so_is_coder_u($l); }, ARRAY_FILTER_USE_BOTH);
    $others = array_diff_key($staff, $coders);
    ?>
    <div class="card so-bar" style="margin-top:0;">
        <?php if ($see_all): ?>
            <form method="GET"><input type="hidden" name="tab" value="schedule"><input type="hidden" name="m" value="<?= so_h($m) ?>"><input type="hidden" name="view" value="cal">
                <select name="who" onchange="this.form.submit()" aria-label="Сотрудник">
                    <?php foreach (['🧑‍💻 Кодеры и разработчики' => $coders, '👥 Остальная команда' => $others] as $gl => $list): if (!$list) continue; ?>
                        <optgroup label="<?= so_h($gl) ?>"><?php foreach ($list as $l => $s): ?><option value="<?= so_h($l) ?>" <?= $l === $who && $view === 'cal' ? 'selected' : '' ?>><?= so_h($l . ' — ' . $s['label']) ?></option><?php endforeach; ?></optgroup>
                    <?php endforeach; ?>
                </select>
            </form>
        <?php else: $s = $staff[$who]; ?>
            <div class="so-person"><div class="so-ava sm" style="--c:<?= so_h($s['color']) ?>"><?= so_h(mb_strtoupper(mb_substr($who, 0, 1))) ?></div><b>Мой календарь</b></div>
        <?php endif; ?>
        <div class="so-month">
            <a class="so-nav-btn" href="<?= so_h($url(['m' => date('Y-m', strtotime('-1 month', $t0))])) ?>" title="Предыдущий месяц">‹</a>
            <b><?= so_month_name(date('n', $t0)) . ' ' . date('Y', $t0) ?></b>
            <a class="so-nav-btn" href="<?= so_h($url(['m' => date('Y-m', strtotime('+1 month', $t0))])) ?>" title="Следующий месяц">›</a>
            <?php if ($m !== date('Y-m')): ?><a class="btn ghost sm" href="<?= so_h($url(['m' => date('Y-m')])) ?>">Сегодня</a><?php endif; ?>
        </div>
        <?php if ($see_all): ?>
            <div class="so-seg"><a class="<?= $view === 'cal' ? 'on' : '' ?>" href="<?= so_h($url(['view' => 'cal'])) ?>">📅 Календарь</a><a class="<?= $view === 'team' ? 'on' : '' ?>" href="<?= so_h($url(['view' => 'team'])) ?>">👥 Вся команда</a></div>
        <?php endif; ?>
    </div>

    <?php if ($leader): ?>
        <div class="card so-bar" id="soBrush" data-csrf="<?= so_h(so_csrf()) ?>">
            <b style="font-size:13px;">🖌 Кисть</b>
            <div class="so-brush">
                <button type="button" class="on" data-type="none" style="--bc:#94a3b8">✋ Просмотр</button>
                <?php foreach ($types as $k => $t): ?><button type="button" data-type="<?= so_h($k) ?>" style="--bc:<?= so_h($t[2]) ?>"><?= $t[1] ?> <?= so_h($t[0]) ?></button><?php endforeach; ?>
                <button type="button" data-type="reset" style="--bc:#a78bfa">↺ По графику</button>
            </div>
            <span class="muted" style="font-size:12px;">Выберите кисть и нажмите или проведите по дням — сохраняется сразу. Точка в углу — день изменён вручную.</span>
        </div>
    <?php else: ?>
        <div class="callout"><span class="c-ico">📅</span><div>Рабочие дни, выходные и отпуск ставят руководители. Задачи по заказам появляются в календаре автоматически, когда администратор назначает вас кодером.</div></div>
    <?php endif; ?>

    <div class="so-legend" style="margin-top:12px;">
        <?php foreach ($types as $k => $t): ?><span><i style="background:<?= so_h($t[2]) ?>"></i><?= so_h($t[0]) ?></span><?php endforeach; ?>
        <span><i style="background:hsl(200 85% 62%)"></i>Задача по заказу</span><span>🏁 Сдача</span>
    </div>

    <?php
    if ($view === 'team') { so_render_team($staff, $coders, $others, $sched, $idx, $m, $dim, $today, $leader); return; }

    $s = $staff[$who];
    $tasks = array_values(array_filter(so_tasks_of($who, $idx), function ($t) { return $t['start'] !== '' && $t['deadline'] !== ''; }));
    $offset = (int)date('N', $t0) - 1;
    $cells = (int)ceil(($offset + $dim) / 7) * 7;
    $gstart = strtotime("-$offset days", $t0);
    $cnt = array_fill_keys(array_keys($types), 0);
    foreach (so_dates($first, $last) as $d) $cnt[so_day_type($sched, $who, $d)]++;
    ?>
    <div class="so-cal-wrap">
        <div class="card so-cal">
            <div class="so-wd"><?php foreach (['Пн', 'Вт', 'Ср', 'Чт', 'Пт', 'Сб', 'Вс'] as $w): ?><span><?= $w ?></span><?php endforeach; ?></div>
            <div class="so-grid">
                <?php for ($i = 0; $i < $cells; $i++):
                    $d = date('Y-m-d', strtotime("+$i days", $gstart));
                    $type = so_day_type($sched, $who, $d);
                    $ovr = isset($sched['days'][$who][$d]);
                    $cls = 'so-day t-' . $type . ($ovr ? ' ovr' : '') . ($d === $today ? ' today' : '') . (substr($d, 0, 7) !== $m ? ' out' : '');
                    $title = (int)substr($d, 8) . ' ' . so_month_name(substr($d, 5, 2), true) . ' · ' . $types[$type][0];
                    ?>
                    <div class="<?= $cls ?>" data-date="<?= $d ?>" data-login="<?= so_h($who) ?>" title="<?= so_h($title) ?>">
                        <span class="n"><?= (int)substr($d, 8) ?></span><span class="ti"><?= $type !== 'work' ? $types[$type][1] : '' ?></span>
                        <?php foreach ($tasks as $t): if ($t['start'] > $d || $t['deadline'] < $d || $t['status'] === 'done') continue; $dl = $t['deadline'] === $d; ?>
                            <a class="so-task<?= $dl ? ' dl' : '' ?><?= in_array($type, ['vacation', 'sick'], true) ? ' warn' : '' ?>" style="--h:<?= so_hue($t['id']) ?>" href="?tab=orders&amp;o=<?= (int)$t['id'] ?>" title="№<?= (int)$t['id'] ?> · <?= so_h($t['title']) ?><?= $dl ? ' — сдача' : '' ?>"><?= $dl ? '🏁 ' : '' ?><?= so_h($t['title']) ?></a>
                        <?php endforeach; ?>
                    </div>
                <?php endfor; ?>
            </div>
        </div>

        <div class="so-side">
            <div class="card">
                <div class="so-person">
                    <div class="so-ava lg" style="--c:<?= so_h($s['color']) ?>"><?= so_h(mb_strtoupper(mb_substr($who, 0, 1))) ?></div>
                    <div><b><?= so_h($who) ?><?= $who === $me ? ' <span class="muted" style="font-weight:400; font-size:13px;">(вы)</span>' : '' ?></b><?= rt_role_badge($s['role'], $s['dir'], $who) ?></div>
                </div>
                <?php $tt = so_day_type($sched, $who, $today); ?>
                <div class="so-chips"><span class="chip" style="border-color:<?= so_h($types[$tt][2]) ?>">Сегодня: <?= $types[$tt][1] ?> <?= so_h($types[$tt][0]) ?></span>
                    <?php if (!empty($sched['hours'][$who])): ?><span class="chip">🕘 <?= so_h($sched['hours'][$who]) ?></span><?php endif; ?></div>
                <div class="so-stats">
                    <div><b><?= $cnt['work'] ?></b>рабочих дней</div><div><b><?= $cnt['off'] ?></b>выходных</div>
                    <div><b><?= $cnt['vacation'] ?></b>дней отпуска</div><div><b><?= $cnt['sick'] ?></b>больничных</div>
                </div>
            </div>

            <?php so_render_task_list($who, $idx); ?>

            <?php if ($leader): $week = so_week_of($sched, $who); ?>
                <form class="card" method="POST" action="?tab=schedule">
                    <h3>🗓 График по неделе</h3>
                    <input type="hidden" name="action" value="so"><input type="hidden" name="op" value="week"><input type="hidden" name="so_csrf" value="<?= so_h(so_csrf()) ?>">
                    <input type="hidden" name="login" value="<?= so_h($who) ?>"><input type="hidden" name="so_back" value="<?= so_h('admin.php' . $url([])) ?>">
                    <div class="so-week"><?php foreach (['Пн', 'Вт', 'Ср', 'Чт', 'Пт', 'Сб', 'Вс'] as $i => $w): ?><label><input type="checkbox" name="wd[]" value="<?= $i ?>" <?= $week[$i] ? 'checked' : '' ?>><span><?= $w ?></span></label><?php endforeach; ?></div>
                    <label style="display:block; margin-top:10px;">Часы работы <span class="muted">(необязательно)</span></label>
                    <input type="text" name="hours" maxlength="40" placeholder="Например: 10:00–19:00" value="<?= so_h($sched['hours'][$who] ?? '') ?>">
                    <button class="btn primary sm" type="submit" style="margin-top:10px;">Сохранить график</button>
                    <div class="muted" style="font-size:12px; margin-top:8px;">Отмеченные дни — рабочие. Отпуск и отдельные дни ставятся кистью или формой ниже.</div>
                </form>
                <form class="card" method="POST" action="?tab=schedule">
                    <h3>🌴 Отпуск или период</h3>
                    <input type="hidden" name="action" value="so"><input type="hidden" name="op" value="range"><input type="hidden" name="so_csrf" value="<?= so_h(so_csrf()) ?>">
                    <input type="hidden" name="login" value="<?= so_h($who) ?>"><input type="hidden" name="so_back" value="<?= so_h('admin.php' . $url([])) ?>">
                    <div class="row"><div class="grow"><label>С</label><input type="date" name="from" required value="<?= so_h($today) ?>"></div><div class="grow"><label>По</label><input type="date" name="to" required></div></div>
                    <label style="display:block; margin-top:8px;">Что поставить</label>
                    <select name="type"><option value="vacation">🌴 Отпуск</option><option value="sick">🤒 Больничный</option><option value="off">🌙 Выходные</option><option value="work">💼 Рабочие дни</option><option value="reset">↺ Вернуть по графику</option></select>
                    <button class="btn primary sm" type="submit" style="margin-top:10px;">Применить</button>
                </form>
            <?php endif; ?>
        </div>
    </div>
    <?php so_schedule_js();
}

function so_render_task_list($who, $idx) {
    $me = so_me();
    $ts = so_task_statuses();
    $st = so_statuses();
    $tasks = array_values(array_filter(so_tasks_of($who, $idx), function ($t) { return $t['status'] !== 'done'; }));
    $done = count(array_filter(so_tasks_of($who, $idx), function ($t) { return $t['status'] === 'done'; }));
    $csrf = so_csrf();
    ?>
    <div class="card">
        <h3>📋 <?= $who === $me ? 'Мои задачи' : 'Задачи' ?> <span class="chip"><?= count($tasks) ?> в работе · <?= $done ?> сдано</span></h3>
        <?php if (!$tasks): ?><div class="muted" style="font-size:13px;">Активных задач нет. Когда администратор назначит заказ, он появится здесь и в календаре.</div><?php endif; ?>
        <?php foreach ($tasks as $t): $late = $t['deadline'] !== '' && $t['deadline'] < date('Y-m-d'); $tsx = $ts[$t['task']] ?? ['?', '#888']; ?>
            <div class="so-ti" style="--h:<?= so_hue($t['id']) ?>">
                <div class="r"><a class="t" href="?tab=orders&amp;o=<?= (int)$t['id'] ?>">№<?= (int)$t['id'] ?> · <?= so_h($t['title']) ?></a><span class="so-st" style="--c:<?= so_h($tsx[1]) ?>"><?= so_h($tsx[0]) ?></span></div>
                <div class="d">📅 <?= so_h(so_dshort($t['start'])) ?> — 🏁 <?= so_h(so_dshort($t['deadline'])) ?><?= $late ? ' · <b style="color:#ff9b9b">просрочено</b>' : '' ?> · <?= so_h($st[$t['status']][0] ?? '') ?><?= $t['unread_coder'] && $who === $me ? ' · 💬 ' . (int)$t['unread_coder'] . ' новых' : '' ?></div>
                <?php if ($who === $me || so_manager()): ?>
                    <div class="so-btns">
                        <?php if ($t['task'] === 'new'): ?><form method="POST" action="?tab=schedule"><input type="hidden" name="action" value="so"><input type="hidden" name="op" value="task"><input type="hidden" name="to" value="work"><input type="hidden" name="o" value="<?= (int)$t['id'] ?>"><input type="hidden" name="so_csrf" value="<?= so_h($csrf) ?>"><input type="hidden" name="so_back" value="admin.php?tab=schedule"><button class="btn ok sm" type="submit">✓ Принять задачу</button></form><?php endif; ?>
                        <?php if ($t['task'] === 'work'): ?><form method="POST" action="?tab=schedule" onsubmit="return confirm('Сдать работу на проверку администратору?');"><input type="hidden" name="action" value="so"><input type="hidden" name="op" value="task"><input type="hidden" name="to" value="review"><input type="hidden" name="o" value="<?= (int)$t['id'] ?>"><input type="hidden" name="so_csrf" value="<?= so_h($csrf) ?>"><input type="hidden" name="so_back" value="admin.php?tab=schedule"><button class="btn blue sm" type="submit">📤 Сдать на проверку</button></form><?php endif; ?>
                        <a class="btn ghost sm" href="?tab=orders&amp;o=<?= (int)$t['id'] ?>">💬 Открыть заказ</a>
                    </div>
                <?php endif; ?>
            </div>
        <?php endforeach; ?>
    </div>
    <?php
}

function so_render_team($staff, $coders, $others, $sched, $idx, $m, $dim, $today, $leader) {
    $types = so_day_types();
    $wd = ['Пн', 'Вт', 'Ср', 'Чт', 'Пт', 'Сб', 'Вс'];
    $dates = so_dates($m . '-01', $m . '-' . sprintf('%02d', $dim));
    ?>
    <div class="card tm-wrap">
        <table class="tm">
            <thead><tr><th class="tm-who" style="text-align:left;">Сотрудник</th>
                <?php foreach ($dates as $d): $n = (int)date('N', strtotime($d)); ?><th class="<?= $n >= 6 ? 'we' : '' ?><?= $d === $today ? ' today' : '' ?>"><span><?= $wd[$n - 1] ?></span><?= (int)substr($d, 8) ?></th><?php endforeach; ?>
                <th class="tm-sum">Итого</th></tr></thead>
            <tbody>
            <?php foreach ([$coders, $others] as $gi => $list): if (!$list) continue; ?>
                <tr><td class="tm-who" style="background:transparent;"><span class="muted" style="font-size:11px; text-transform:uppercase; letter-spacing:.1em; font-weight:700;"><?= $gi ? '👥 Остальная команда' : '🧑‍💻 Кодеры и разработчики' ?></span></td></tr>
                <?php foreach ($list as $l => $s):
                    $tasks = array_values(array_filter(so_tasks_of($l, $idx), function ($t) { return $t['start'] !== '' && $t['status'] !== 'done'; }));
                    $sum = ['work' => 0, 'vacation' => 0, 'sick' => 0]; ?>
                    <tr>
                        <td class="tm-who"><a href="?tab=schedule&amp;who=<?= urlencode($l) ?>&amp;m=<?= so_h($m) ?>&amp;view=cal"><span class="so-ava sm" style="--c:<?= so_h($s['color']) ?>"><?= so_h(mb_strtoupper(mb_substr($l, 0, 1))) ?></span><span><?= so_h($l) ?><small><?= so_h($s['label']) ?><?= $tasks ? ' · ' . count($tasks) . ' ' . so_plural(count($tasks), 'задача', 'задачи', 'задач') : '' ?></small></span></a></td>
                        <?php foreach ($dates as $d):
                            $type = so_day_type($sched, $l, $d);
                            if (isset($sum[$type])) $sum[$type]++;
                            $on = array_values(array_filter($tasks, function ($t) use ($d) { return $t['start'] <= $d && $t['deadline'] >= $d; }));
                            $dl = array_values(array_filter($on, function ($t) use ($d) { return $t['deadline'] === $d; }));
                            $title = $l . ' · ' . (int)substr($d, 8) . ' ' . so_month_name(substr($d, 5, 2), true) . ' · ' . $types[$type][0];
                            foreach ($on as $t) $title .= "\n№" . $t['id'] . ' ' . $t['title'] . ($t['deadline'] === $d ? ' — сдача' : '');
                            ?>
                            <td class="tm-cell t-<?= $type ?><?= isset($sched['days'][$l][$d]) ? ' ovr' : '' ?><?= $d === $today ? ' today' : '' ?>" data-date="<?= $d ?>" data-login="<?= so_h($l) ?>" title="<?= so_h($title) ?>"><?php if ($dl): ?><b>🏁</b><?php elseif ($on): ?><i style="--h:<?= so_hue($on[0]['id']) ?>"></i><?php endif; ?></td>
                        <?php endforeach; ?>
                        <td class="tm-sum">💼 <?= $sum['work'] ?><?= $sum['vacation'] ? ' · 🌴 ' . $sum['vacation'] : '' ?><?= $sum['sick'] ? ' · 🤒 ' . $sum['sick'] : '' ?></td>
                    </tr>
                <?php endforeach; ?>
            <?php endforeach; ?>
            </tbody>
        </table>
    </div>
    <?php if (!$leader): ?><p class="muted" style="font-size:12.5px;">Нажмите на сотрудника, чтобы открыть его календарь.</p><?php endif; ?>
    <?php so_schedule_js();
}

function so_schedule_js() {
    static $done = false;
    if ($done) return;
    $done = true;
    $types = so_day_types();
    ?>
<script>
(() => {
    const bar = document.getElementById('soBrush');
    if (!bar) return;
    const ICONS = <?= json_encode(array_map(function ($t) { return $t[1]; }, $types), JSON_UNESCAPED_UNICODE) ?>;
    const NAMES = <?= json_encode(array_map(function ($t) { return $t[0]; }, $types), JSON_UNESCAPED_UNICODE) ?>;
    let brush = null, down = false, picked = new Map();
    bar.addEventListener('click', e => {
        const b = e.target.closest('[data-type]'); if (!b) return;
        brush = b.dataset.type === 'none' ? null : b.dataset.type;
        bar.querySelectorAll('[data-type]').forEach(x => x.classList.toggle('on', x === b));
        document.body.classList.toggle('painting', !!brush);
    });
    const cellAt = (x, y) => { const el = document.elementFromPoint(x, y); return el ? el.closest('[data-date][data-login]') : null; };
    const pick = c => { const k = c.dataset.login + '|' + c.dataset.date; if (picked.has(k)) return; picked.set(k, c); c.classList.add('so-sel'); };
    document.addEventListener('pointerdown', e => {
        if (!brush || e.button > 0) return;
        const c = e.target.closest('[data-date][data-login]'); if (!c) return;
        e.preventDefault(); down = true; picked = new Map(); pick(c);
    });
    document.addEventListener('pointermove', e => { if (!down) return; const c = cellAt(e.clientX, e.clientY); if (c) pick(c); });
    const finish = async () => {
        if (!down) return; down = false;
        const byLogin = {};
        picked.forEach((c, k) => { (byLogin[c.dataset.login] = byLogin[c.dataset.login] || []).push(c.dataset.date); });
        const cells = [...picked.values()];
        for (const [login, dates] of Object.entries(byLogin)) {
            const fd = new FormData();
            fd.append('action', 'so'); fd.append('op', 'paint'); fd.append('so_csrf', bar.dataset.csrf); fd.append('is_ajax', '1');
            fd.append('login', login); fd.append('type', brush); fd.append('dates', JSON.stringify(dates));
            try {
                const r = await fetch('admin.php?tab=schedule', { method: 'POST', body: fd }).then(r => r.json());
                if (!r.ok) { showToast(r.msg || 'Не сохранилось', 'error'); continue; }
                Object.entries(r.days || {}).forEach(([d, v]) => {
                    document.querySelectorAll(`[data-login="${CSS.escape(login)}"][data-date="${d}"]`).forEach(c => {
                        c.classList.remove('t-work', 't-off', 't-vacation', 't-sick'); c.classList.add('t-' + v.type); c.classList.toggle('ovr', v.ovr);
                        const ti = c.querySelector('.ti'); if (ti) ti.textContent = v.type === 'work' ? '' : (ICONS[v.type] || '');
                        c.querySelectorAll('.so-task').forEach(t => t.classList.toggle('warn', v.type === 'vacation' || v.type === 'sick'));
                        c.title = c.title.replace(/·[^·\n]*$/m, '· ' + (NAMES[v.type] || v.type));
                    });
                });
                showToast(r.msg, 'success');
            } catch (err) { showToast('Ошибка сети — изменения не сохранены', 'error'); }
        }
        cells.forEach(c => c.classList.remove('so-sel'));
    };
    document.addEventListener('pointerup', finish);
    document.addEventListener('pointercancel', finish);
})();
</script>
    <?php
}

/* ==========================================================
   ЗАКАЗЫ САЙТОВ
   ========================================================== */

function so_render_orders() {
    $mgr = so_manager();
    $leader = so_leader();
    $cfg = so_config();
    $idx = so_index();
    $me = so_me();
    $month = date('Y-m');
    $k = ['new' => 0, 'unans' => 0, 'work' => 0, 'pending' => 0, 'paid_m' => 0, 'done_m' => 0, 'mine' => 0];
    foreach ($idx as $e) {
        if ($e['status'] === 'new') $k['new']++;
        if ($e['unread_staff'] > 0 && $e['status'] !== 'cancel') $k['unans']++;
        if (in_array($e['status'], ['assigned', 'work', 'review'], true)) $k['work']++;
        $k['pending'] += $e['pending_sum'];
        foreach ($e['paid_list'] ?? [] as $p) if ($p[0] && date('Y-m', $p[0]) === $month) $k['paid_m'] += $p[1];
        if ($e['status'] === 'done' && $e['done_at'] && date('Y-m', $e['done_at']) === $month) $k['done_m']++;
        if (($e['coder'] ?? null) === $me && !in_array($e['status'], ['done', 'cancel'], true)) $k['mine']++;
    }
    $open = (int)($_GET['o'] ?? 0);
    ?>
    <?php if ($mgr): ?>
        <div class="so-kpis">
            <div class="so-kpi<?= $k['new'] ? ' hot' : '' ?>"><div class="i">🆕</div><b><?= $k['new'] ?></b><span>новых заказов</span></div>
            <div class="so-kpi<?= $k['unans'] ? ' hot' : '' ?>"><div class="i">💬</div><b><?= $k['unans'] ?></b><span>ждут ответа</span></div>
            <div class="so-kpi"><div class="i">⚙️</div><b><?= $k['work'] ?></b><span>в работе у кодеров</span></div>
            <div class="so-kpi"><div class="i">⏳</div><b><?= so_money($k['pending']) ?></b><span>ждут оплаты</span></div>
            <div class="so-kpi"><div class="i">💰</div><b><?= so_money($k['paid_m']) ?></b><span>оплачено в <?= so_h(mb_strtolower(so_month_name(date('n')))) ?></span></div>
            <div class="so-kpi"><div class="i">✅</div><b><?= $k['done_m'] ?></b><span>сдано в этом месяце</span></div>
        </div>
        <?php if (!so_platega_ready($cfg)): ?>
            <div class="callout warn"><span class="c-ico">💳</span><div><b>Platega ещё не подключена.</b> Счета можно выставлять с ручной отметкой оплаты. <?= $leader ? 'Чтобы клиенты платили прямо из чата, впишите Merchant ID и секрет в <a href="#soSettings">настройках ниже</a> — это нужно сделать один раз.' : 'Ключи вписывает руководитель — один раз.' ?></div></div>
        <?php endif; ?>
        <?php if (empty($cfg['orders_open'])): ?><div class="callout danger"><span class="c-ico">⏸</span><div>Приём новых заказов на сайте выключен. <?= $leader ? 'Включить можно в настройках ниже.' : '' ?></div></div><?php endif; ?>
    <?php else: ?>
        <div class="callout"><span class="c-ico">🧑‍💻</span><div>Здесь заказы, где вы — кодер (<?= $k['mine'] ?> в работе). Можно переписываться с клиентом и администратором, принимать задачу и сдавать её на проверку. Сроки — в разделе <a href="?tab=schedule">«Расписание»</a>.</div></div>
    <?php endif; ?>

    <div class="row" style="margin-top:12px; justify-content:space-between;">
        <div class="muted" style="font-size:12.5px;">Клиенты оформляют заказ на странице <a href="order.php" target="_blank" rel="noopener">order.php</a>, отзывы — <a href="reviews.php" target="_blank" rel="noopener">reviews.php</a>. Чат обновляется сам.</div>
        <label class="switch-row" style="font-size:12.5px;"><input type="checkbox" id="soSound" checked> Звук новых сообщений</label>
    </div>

    <div class="so-msgr" id="soMsgr">
        <aside class="so-list">
            <div class="so-list-top">
                <input type="search" id="soQ" placeholder="Поиск: №, клиент, кодер…" autocomplete="off">
                <div class="so-filters" id="soFilters">
                    <button type="button" class="on" data-f="active">Активные<b></b></button>
                    <button type="button" data-f="new">Новые<b></b></button>
                    <button type="button" data-f="work">В работе<b></b></button>
                    <button type="button" data-f="pay">Оплата<b></b></button>
                    <button type="button" data-f="mine">Мои<b></b></button>
                    <button type="button" data-f="done">Готовые<b></b></button>
                    <button type="button" data-f="cancel">Отменённые<b></b></button>
                    <button type="button" data-f="all">Все<b></b></button>
                </div>
            </div>
            <div id="soList"><div class="so-empty-c">Загрузка…</div></div>
        </aside>
        <section class="so-chat">
            <div class="so-empty-c" id="soChatEmpty"><div class="big">🛒</div><b style="color:#fff;">Выберите заказ слева</b><div style="margin-top:6px;">Новые заявки с сайта появляются здесь сами — без перезагрузки.</div></div>
            <div id="soChatBox" style="display:none; flex-direction:column; min-height:0; flex:1;">
                <div class="so-chat-head">
                    <div class="so-ava" id="soCAva">?</div>
                    <div class="t"><b id="soCTitle">…</b><span><span class="so-online" id="soCOnline"></span><span id="soCSub">…</span></span></div>
                    <span class="so-st" id="soCStatus" style="--c:#888">…</span>
                </div>
                <div class="so-msgs" id="soMsgs"></div>
                <div class="so-typing" id="soTyping" hidden><i></i><i></i><i></i> клиент печатает…</div>
                <form class="so-comp" id="soComp">
                    <?php if ($mgr): ?>
                    <div class="so-quick">
                        <button type="button" data-q="Здравствуйте! Спасибо за заявку 🙌 Я администратор RTeam, буду вести ваш заказ. Пара уточняющих вопросов:">👋 Приветствие</button>
                        <button type="button" data-q="Посчитали стоимость и сроки — сейчас выставлю счёт, оплатить можно прямо в этом чате кнопкой «Оплатить».">💳 Про счёт</button>
                        <button type="button" data-q="Предложил(а) вам несколько свободных кодеров — выберите того, кто понравится, кнопкой «Выбрать» в карточке выше.">🧑‍💻 Выбор кодера</button>
                        <button type="button" data-q="Работа готова! Посмотрите, пожалуйста, и напишите, всё ли устраивает — если нужны правки, поправим.">✅ Работа готова</button>
                    </div>
                    <?php endif; ?>
                    <div class="so-att" id="soAtt"></div>
                    <div class="so-comp-row">
                        <label class="so-attach" title="Прикрепить файлы">📎<input type="file" id="soFiles" multiple hidden></label>
                        <textarea id="soText" rows="1" maxlength="4000" placeholder="Сообщение клиенту… (Enter — отправить)"></textarea>
                        <button class="btn primary" type="submit">➤</button>
                    </div>
                    <div class="row" style="margin-top:6px; justify-content:space-between;">
                        <label class="so-int"><input type="checkbox" id="soInternal"> 🔒 Внутренняя заметка (клиент не увидит)</label>
                        <span class="muted" style="font-size:11.5px;">Shift+Enter — новая строка</span>
                    </div>
                </form>
            </div>
        </section>
        <aside class="so-panel" id="soPanel"><div class="so-empty-c"><div class="big">📋</div>Здесь появятся детали заказа, кодер и счета.</div></aside>
    </div>

    <?php if ($leader) so_render_settings($cfg); ?>
    <?php so_orders_js($open);
}

/** Панель справа: детали заказа, кодер, счета, управление. Отдаётся и при загрузке, и через AJAX. */
function so_render_panel(array $o, $acc) {
    $staff = so_staff();
    $st = so_statuses();
    $ts = so_task_statuses();
    $cat = so_catalog();
    $cfg = so_config();
    $me = so_me();
    $mgr = $acc === 'manager';
    $csrf = so_csrf();
    $id = (int)$o['id'];
    $b = $o['brief'];
    $type = $cat['types'][$b['type'] ?? 'other'] ?? ['name' => '—', 'icon' => '✨'];
    $base = so_base_url();
    $client_link = $base . '/order.php?o=' . $id . '&t=' . $o['token'];
    $hidden = function ($op, array $extra = []) use ($csrf, $id) {
        $h = '<input type="hidden" name="action" value="so"><input type="hidden" name="op" value="' . so_h($op) . '"><input type="hidden" name="o" value="' . $id . '"><input type="hidden" name="so_csrf" value="' . so_h($csrf) . '">';
        foreach ($extra as $k => $v) $h .= '<input type="hidden" name="' . so_h($k) . '" value="' . so_h($v) . '">';
        return $h;
    };
    $paid = so_paid_total($o);
    $est = $o['estimate'] ?? ['min' => 0, 'max' => 0, 'days' => 7];
    ob_start();
    ?>
    <div class="so-sec">
        <h4><span>Заказ №<?= $id ?></span><span class="so-st" style="--c:<?= so_h($st[$o['status']][2] ?? '#888') ?>"><?= ($st[$o['status']][1] ?? '') . ' ' . so_h($st[$o['status']][0] ?? $o['status']) ?></span></h4>
        <div class="so-kv">
            <span>Клиент</span><span><?= so_h($o['client']['name']) ?><?= !empty($o['client']['account']) ? ' <span class="chip">👤 ' . so_h($o['client']['account']) . '</span>' : '' ?></span>
            <?php if ($mgr): $cl = so_contact_link($o['client']['contact_type'], $o['client']['contact']); $ctn = ['telegram' => '✈️ Telegram', 'whatsapp' => '🟢 WhatsApp', 'email' => '✉️ Почта', 'phone' => '📞 Телефон'][$o['client']['contact_type']] ?? ''; ?>
                <span><?= $ctn ?></span><span><?= $cl ? '<a href="' . so_h($cl) . '" target="_blank" rel="noopener">' . so_h($o['client']['contact']) . '</a>' : so_h($o['client']['contact']) ?></span>
            <?php endif; ?>
            <span>Создан</span><span><?= date('d.m.Y H:i', $o['created']) ?></span>
            <span>Ведёт</span><span><?= !empty($o['manager']) ? so_h($o['manager']) : '<span class="muted">никто</span>' ?></span>
            <?php if ($mgr): ?><span>Оплачено</span><span><?= so_money($paid) ?></span><?php endif; ?>
            <?php if ($mgr && function_exists('ip_tag') && can('bans.manage') && !empty($o['client']['ip'])): ?><span>IP</span><span><?= ip_tag(['ip' => $o['client']['ip'], 'src' => 'при заказе', 'login' => $o['client']['account']], 'Заказ сайта №' . $id) ?></span><?php endif; ?>
        </div>
        <?php if ($mgr): ?>
            <div class="so-btns">
                <form class="so-f" method="POST"><?= $hidden('status') ?><select name="status" data-autosubmit style="margin:0; padding:6px 10px; font-size:12.5px; width:auto;"><?php foreach ($st as $k => $s): ?><option value="<?= $k ?>" <?= $k === $o['status'] ? 'selected' : '' ?>><?= $s[1] . ' ' . so_h($s[0]) ?></option><?php endforeach; ?></select></form>
                <?php if (($o['manager'] ?? null) !== $me): ?><form class="so-f" method="POST"><?= $hidden('take') ?><button class="btn ghost sm" type="submit">🙋 Взять себе</button></form><?php endif; ?>
                <button class="btn ghost sm" type="button" data-copy="<?= so_h($client_link) ?>" title="Отправьте клиенту, если он потерял ссылку">🔗 Ссылка клиента</button>
            </div>
        <?php endif; ?>
    </div>

    <div class="so-sec">
        <h4><span>🧑‍💻 Кодер</span><?php if (!empty($o['coder'])): ?><a class="btn ghost sm" style="margin:0;" href="?tab=schedule&amp;who=<?= urlencode($o['coder']['login']) ?>&amp;m=<?= so_h(substr($o['coder']['start'] ?: date('Y-m'), 0, 7)) ?>">📅 Календарь</a><?php endif; ?></h4>
        <?php if (!empty($o['coder'])): $c = $o['coder']; $tsx = $ts[$c['task']] ?? ['?', '#888']; $late = $c['deadline'] < date('Y-m-d') && !in_array($o['status'], ['done', 'cancel'], true); ?>
            <div class="so-person"><div class="so-ava" style="--c:<?= so_h($c['color'] ?? '#888') ?>"><?= so_h(mb_strtoupper(mb_substr($c['login'], 0, 1))) ?></div>
                <div style="flex:1; min-width:0;"><b><?= so_h($c['login']) ?></b><span class="muted" style="font-size:12px;"><?= so_h($c['label'] ?? '') ?> · назначил(а) <?= so_h($c['by'] ?? '') ?></span></div>
                <span class="so-st" style="--c:<?= so_h($tsx[1]) ?>"><?= so_h($tsx[0]) ?></span></div>
            <div class="so-kv" style="margin-top:10px;"><span>Старт</span><span><?= so_h(so_dshort($c['start'])) ?></span><span>Сдача</span><span<?= $late ? ' style="color:#ff9b9b"' : '' ?>><?= so_h(so_dshort($c['deadline'])) ?><?= $late ? ' · просрочено' : '' ?></span></div>
            <?php if (!empty($c['note'])): ?><div class="so-desc"><?= so_h($c['note']) ?></div><?php endif; ?>
            <div class="so-btns">
                <?php $is_coder = $c['login'] === $me; ?>
                <?php if ($c['task'] === 'new' && ($is_coder || $mgr)): ?><form class="so-f" method="POST"><?= $hidden('task', ['to' => 'work']) ?><button class="btn ok sm" type="submit">✓ Принять задачу</button></form><?php endif; ?>
                <?php if ($c['task'] === 'work' && ($is_coder || $mgr)): ?><form class="so-f" method="POST" data-confirm="Сдать работу на проверку?"><?= $hidden('task', ['to' => 'review']) ?><button class="btn blue sm" type="submit">📤 Сдать на проверку</button></form><?php endif; ?>
                <?php if ($mgr && in_array($c['task'], ['review', 'work'], true) && $o['status'] !== 'done'): ?><form class="so-f" method="POST" data-confirm="Принять работу и завершить заказ? Клиенту придёт предложение оставить отзыв."><?= $hidden('complete') ?><button class="btn ok sm" type="submit">🎉 Принять и завершить</button></form><?php endif; ?>
                <?php if ($mgr && $c['task'] === 'review'): ?><form class="so-f" method="POST"><?= $hidden('task', ['to' => 'back']) ?><button class="btn orange sm" type="submit">🔁 На доработку</button></form><?php endif; ?>
                <?php if ($mgr && $o['status'] !== 'done'): ?><form class="so-f" method="POST" data-confirm="Снять кодера с заказа? Задача пропадёт из его календаря."><?= $hidden('unassign') ?><button class="btn ghost danger sm" type="submit">Снять</button></form><?php endif; ?>
            </div>
        <?php elseif ($mgr && !in_array($o['status'], ['done', 'cancel'], true)):
            // данные о занятости для подсказок в форме
            $sched = so_sched(); $idx = so_index(); $revs = so_reviews_published();
            $today = date('Y-m-d');
            $cdata = [];
            foreach ($staff as $l => $s) {
                $days = '';
                foreach (so_dates($today, date('Y-m-d', strtotime('+179 days'))) as $d) $days .= ['work' => 'w', 'off' => 'o', 'vacation' => 'v', 'sick' => 's'][so_day_type($sched, $l, $d)];
                $cs = so_coder_stats($l, $idx, $revs);
                $busy = 0;
                foreach (so_tasks_of($l, $idx) as $t) if (!in_array($t['status'], ['done', 'cancel'], true)) $busy++;
                $cdata[$l] = ['label' => $s['label'], 'color' => $s['color'], 'coder' => so_is_coder_u($l), 'days' => $days, 'busy' => $busy,
                              'rating' => $cs['rating'], 'reviews' => $cs['reviews'], 'done' => $cs['done'], 'hours' => $sched['hours'][$l] ?? ''];
            }
            uasort($cdata, function ($a, $b) { return [$b['coder'], $a['busy']] <=> [$a['coder'], $b['busy']]; });
            $want = ($b['deadline'] ?? '') ?: '';
            ?>
            <script type="application/json" class="so-cdata"><?= json_encode(['today' => $today, 'est' => max(3, (int)($est['days'] ?: 7)), 'want' => $want, 'coders' => $cdata], JSON_UNESCAPED_UNICODE | JSON_HEX_TAG | JSON_HEX_AMP) ?></script>
            <div class="so-seg" style="margin-bottom:10px;"><button type="button" class="on" data-tabs="assign">Назначить</button><button type="button" data-tabs="offer">Дать выбор клиенту</button></div>
            <form class="so-f" method="POST" data-pane="assign" data-confirm-warn>
                <?= $hidden('assign') ?>
                <label>Кодер</label>
                <select name="coder" data-coder-pick required>
                    <option value="">— выберите —</option>
                    <?php $grp = null; foreach ($cdata as $l => $c): $g = $c['coder'] ? 'Кодеры и разработчики' : 'Остальная команда'; if ($g !== $grp) { echo ($grp ? '</optgroup>' : '') . '<optgroup label="' . so_h($g) . '">'; $grp = $g; } ?>
                        <option value="<?= so_h($l) ?>"><?= so_h($l) ?> · <?= so_h($c['label']) ?> · задач: <?= (int)$c['busy'] ?><?= $c['rating'] ? ' · ★' . $c['rating'] : '' ?></option>
                    <?php endforeach; echo $grp ? '</optgroup>' : ''; ?>
                </select>
                <div class="row"><div class="grow"><label>Старт</label><input type="date" name="start" required data-start></div><div class="grow"><label>Сдача</label><input type="date" name="deadline" required data-deadline></div></div>
                <div class="so-info" data-coder-info hidden></div>
                <label>Задача для кодера <span class="muted">(видит только команда)</span></label>
                <textarea name="note" maxlength="2000" placeholder="Что сделать, на что обратить внимание, ссылки на макеты…"></textarea>
                <button class="btn primary sm" type="submit" style="margin-top:8px;">📅 Назначить и поставить в календарь</button>
            </form>
            <form class="so-f" method="POST" data-pane="offer" hidden>
                <?= $hidden('offer') ?>
                <div class="muted" style="font-size:12.5px;">Клиент увидит карточки кодеров с рейтингом, числом сданных проектов и датой старта и выберет сам. Задача сразу встанет в календарь выбранного.</div>
                <div class="so-pick">
                    <?php foreach ($cdata as $l => $c): if (!$c['coder'] && count(array_filter($cdata, function ($x) { return $x['coder']; })) >= 2) continue; ?>
                        <label><input type="checkbox" name="coders[]" value="<?= so_h($l) ?>"><span class="so-ava sm" style="--c:<?= so_h($c['color']) ?>"><?= so_h(mb_strtoupper(mb_substr($l, 0, 1))) ?></span>
                            <span class="pi"><b><?= so_h($l) ?></b><?= so_h($c['label']) ?> · задач: <?= (int)$c['busy'] ?> · <?= $c['rating'] ? '★ ' . $c['rating'] . ' (' . $c['reviews'] . ')' : 'без отзывов' ?> · сдал: <?= (int)$c['done'] ?></span></label>
                    <?php endforeach; ?>
                </div>
                <label>Срок сдачи <span class="muted">(пусто — посчитаем по рабочим дням кодера)</span></label>
                <input type="date" name="deadline" min="<?= so_h($today) ?>" value="<?= so_h($want) ?>">
                <label>Комментарий клиенту</label>
                <textarea name="note" maxlength="600" placeholder="Например: все трое свободны на этой неделе, у anya больше опыта с магазинами"></textarea>
                <button class="btn primary sm" type="submit" style="margin-top:8px;">🧑‍💻 Отправить выбор клиенту</button>
            </form>
            <?php foreach ($o['offers'] as $of): if ($of['status'] !== 'open') continue; ?>
                <div class="so-info" style="margin-top:10px;">⏳ Клиент выбирает из: <b><?= so_h(implode(', ', array_column($of['coders'], 'login'))) ?></b>
                    <form class="so-f" method="POST" style="display:inline;"><?= $hidden('offer_cancel', ['offer' => $of['id']]) ?><button class="btn ghost sm" type="submit" style="margin:6px 0 0;">Отозвать</button></form></div>
            <?php endforeach; ?>
        <?php else: ?>
            <div class="muted" style="font-size:13px;">Кодер не назначен.</div>
        <?php endif; ?>
    </div>

    <?php if ($mgr): ?>
    <div class="so-sec">
        <h4><span>💳 Счета</span><span class="muted" style="text-transform:none; letter-spacing:0;">оплачено <?= so_money($paid) ?></span></h4>
        <?php foreach (array_reverse($o['invoices']) as $inv):
            $sl = ['pending' => ['⏳ ждёт оплаты', '#f59e0b'], 'paid' => ['✅ оплачен', '#22c55e'], 'cancelled' => ['отменён', '#6b7280'], 'refunded' => ['возврат', '#6b7280']][$inv['status']] ?? [$inv['status'], '#888']; ?>
            <div class="so-inv">
                <div class="top"><b><?= so_money($inv['amount']) ?></b><span class="so-st" style="--c:<?= $sl[1] ?>"><?= $sl[0] ?></span></div>
                <div class="d"><?= so_h($inv['desc']) ?> · <?= date('d.m H:i', $inv['created']) ?> · <?= !empty($inv['tx']) ? 'Platega' : 'вручную' ?><?= !empty($inv['paid_by']) ? ' · подтвердил: ' . so_h($inv['paid_by']) : '' ?></div>
                <?php if ($inv['status'] === 'pending'): ?>
                    <div class="so-btns">
                        <?php if (!empty($inv['tx'])): ?><form class="so-f" method="POST"><?= $hidden('inv_check', ['inv' => $inv['id']]) ?><button class="btn blue sm" type="submit">🔄 Проверить в Platega</button></form><?php endif; ?>
                        <form class="so-f" method="POST" data-confirm="Отметить счёт оплаченным вручную?"><?= $hidden('inv_paid', ['inv' => $inv['id']]) ?><button class="btn ok sm" type="submit">✓ Оплачен</button></form>
                        <form class="so-f" method="POST" data-confirm="Отменить счёт?"><?= $hidden('inv_cancel', ['inv' => $inv['id']]) ?><button class="btn ghost danger sm" type="submit">Отменить</button></form>
                        <?php if (!empty($inv['url'])): ?><button class="btn ghost sm" type="button" data-copy="<?= so_h($inv['url']) ?>">🔗</button><?php endif; ?>
                    </div>
                <?php endif; ?>
            </div>
        <?php endforeach; ?>
        <?php if (!in_array($o['status'], ['cancel'], true)): $left = max(0, (float)$est['min'] - $paid); ?>
            <form class="so-f" method="POST" data-confirm="Выставить счёт клиенту?">
                <?= $hidden('invoice') ?>
                <div class="so-chips">
                    <?php if ($est['min']): ?><button class="chip" type="button" data-fill-amount="<?= (int)round($est['min'] / 2) ?>" data-fill-desc="Предоплата 50% · заказ №<?= $id ?>">50% предоплата</button>
                    <button class="chip" type="button" data-fill-amount="<?= (int)$est['min'] ?>" data-fill-desc="Разработка сайта · заказ №<?= $id ?>">Вся сумма</button><?php endif; ?>
                    <?php if ($paid > 0 && $left > 0): ?><button class="chip" type="button" data-fill-amount="<?= (int)$left ?>" data-fill-desc="Доплата · заказ №<?= $id ?>">Доплата</button><?php endif; ?>
                </div>
                <div class="row"><div class="grow" style="flex-basis:110px;"><label>Сумма, ₽</label><input type="number" name="amount" min="1" step="1" required value="<?= $left > 0 ? (int)$left : '' ?>"></div>
                    <div class="grow"><label>За что</label><input type="text" name="desc" maxlength="160" value="Разработка сайта · заказ №<?= $id ?>"></div></div>
                <div class="seg" style="margin-top:8px;">
                    <label><input type="radio" name="mode" value="platega" <?= so_platega_ready($cfg) ? 'checked' : 'disabled' ?>><span>💳 Platega<?= so_platega_ready($cfg) ? '' : ' (не подключена)' ?></span></label>
                    <label><input type="radio" name="mode" value="manual" <?= so_platega_ready($cfg) ? '' : 'checked' ?>><span>🧾 Вручную</span></label>
                </div>
                <button class="btn primary sm" type="submit" style="margin-top:10px;">Выставить счёт в чат</button>
            </form>
        <?php endif; ?>
    </div>
    <?php endif; ?>

    <div class="so-sec">
        <h4><span><?= $type['icon'] ?> Бриф</span><span class="muted" style="text-transform:none; letter-spacing:0;"><?= so_h($type['name']) ?></span></h4>
        <div class="so-kv">
            <?php if (!empty($b['project'])): ?><span>Проект</span><span><?= so_h($b['project']) ?></span><?php endif; ?>
            <?php if ($est['min']): ?><span>Оценка</span><span><?= so_money($est['min']) ?> – <?= so_money($est['max']) ?></span><span>Срок</span><span>≈ <?= (int)$est['days'] ?> раб. дн.</span><?php endif; ?>
            <span>Бюджет</span><span><?= !empty($b['budget']) ? 'до ' . so_money($b['budget']) : 'не указан' ?></span>
            <span>Запуск</span><span><?= !empty($b['deadline']) ? so_h(date('d.m.Y', strtotime($b['deadline']))) : 'не указан' ?> · <?= so_h(so_urgency()[$b['urgency'] ?? 'normal'][0] ?? '') ?></span>
            <span>Стиль</span><span><?= so_h(so_styles()[$b['style'] ?? 'free'] ?? '') ?></span>
            <?php if (!empty($b['has'])): $hn = ['design' => 'макет', 'logo' => 'логотип', 'content' => 'контент', 'domain' => 'домен']; ?><span>Есть</span><span><?= so_h(implode(', ', array_map(function ($h) use ($hn) { return $hn[$h] ?? $h; }, $b['has']))) ?></span><?php endif; ?>
        </div>
        <?php if (!empty($b['features'])): ?><div class="so-chips"><?php foreach ($b['features'] as $f): if (isset($cat['features'][$f])): ?><span class="chip"><?= $cat['features'][$f]['icon'] ?> <?= so_h($cat['features'][$f]['name']) ?></span><?php endif; endforeach; ?></div><?php endif; ?>
        <div class="so-desc"><?= so_h($b['description']) ?></div>
        <?php if (!empty($b['refs'])): ?><div class="so-desc" data-linkify><?= so_h($b['refs']) ?></div><?php endif; ?>
    </div>

    <?php if ($mgr): ?>
    <div class="so-sec">
        <h4><span>⚙️ Управление</span></h4>
        <div class="so-btns">
            <?php if (!in_array($o['status'], ['done', 'cancel'], true)): ?><form class="so-f" method="POST" data-confirm="Завершить заказ? Клиенту придёт предложение оставить отзыв."><?= $hidden('complete') ?><button class="btn ok sm" type="submit">🎉 Завершить</button></form>
                <form class="so-f" method="POST" data-confirm="Отменить заказ?"><?= $hidden('cancel') ?><button class="btn ghost danger sm" type="submit">✖ Отменить</button></form><?php endif; ?>
            <?php if (in_array($o['status'], ['done', 'cancel'], true)): ?><form class="so-f" method="POST"><?= $hidden('reopen') ?><button class="btn ghost sm" type="submit">↺ Открыть снова</button></form><?php endif; ?>
            <?php if (so_leader()): ?><form class="so-f" method="POST" data-confirm="Удалить заказ №<?= $id ?> вместе с перепиской и файлами? Это нельзя отменить."><?= $hidden('delete') ?><button class="btn ghost danger sm" type="submit">🗑 Удалить</button></form><?php endif; ?>
        </div>
    </div>
    <?php endif; ?>

    <details class="so-sec"><summary style="cursor:pointer; font-size:11.5px; text-transform:uppercase; letter-spacing:.1em; color:var(--muted); font-weight:700;">📜 История (<?= count($o['history'] ?? []) ?>)</summary>
        <div class="so-hist" style="margin-top:8px;"><?php foreach (array_reverse($o['history'] ?? []) as $h): ?><div><span><?= date('d.m H:i', $h['t']) ?></span> · <b><?= so_h($h['who']) ?></b> — <?= so_h($h['what']) ?></div><?php endforeach; ?></div>
    </details>
    <?php
    return ob_get_clean();
}

function so_render_settings(array $cfg) {
    $csrf = so_csrf();
    $ready = so_platega_ready($cfg);
    $roles = rt_roles();
    $cat = so_catalog();
    $editing = !$ready || isset($_GET['keys']);
    ?>
    <h3 id="soSettings" style="margin-top:30px !important;">⚙️ Настройки заказов (только руководители)</h3>
    <div class="grid-2" style="align-items:start;">
        <div class="card" style="margin-top:0;">
            <h3>💳 Platega — приём оплаты <?= $ready ? '<span class="chip on">подключена</span>' : '<span class="chip">не подключена</span>' ?></h3>
            <?php if ($ready && !$editing): ?>
                <div class="so-kv"><span>Merchant ID</span><span><code><?= so_h($cfg['platega_id']) ?></code></span><span>Секрет</span><span>•••••••• (скрыт)</span>
                    <span>Способ оплаты</span><span><?= so_h(so_platega_methods()[(int)$cfg['platega_method']] ?? '') ?></span>
                    <?php if (!empty($cfg['platega_set_by'])): ?><span>Вписал(а)</span><span><?= so_h($cfg['platega_set_by']) ?>, <?= date('d.m.Y', (int)$cfg['platega_set_at']) ?></span><?php endif; ?></div>
                <div class="so-btns" style="margin-top:12px;">
                    <form method="POST" action="?tab=orders"><input type="hidden" name="action" value="so"><input type="hidden" name="op" value="cfg_test"><input type="hidden" name="so_csrf" value="<?= so_h($csrf) ?>"><input type="hidden" name="so_back" value="admin.php?tab=orders#soSettings"><button class="btn blue sm" type="submit">🔌 Проверить связь</button></form>
                    <a class="btn ghost sm" href="?tab=orders&amp;keys=1#soSettings">✏️ Изменить ключи</a>
                </div>
            <?php else: ?>
                <p class="meta">Ключи вписываются <b>один раз</b> — дальше счета создаются автоматически. Секрет хранится на сервере в защищённом файле и в админке больше не показывается.</p>
                <form method="POST" action="?tab=orders">
                    <input type="hidden" name="action" value="so"><input type="hidden" name="op" value="cfg_keys"><input type="hidden" name="so_csrf" value="<?= so_h($csrf) ?>"><input type="hidden" name="so_back" value="admin.php?tab=orders#soSettings">
                    <label>Merchant ID <?= !empty($cfg['platega_id']) ? '· ✓ ' . so_h($cfg['platega_id']) : '' ?></label>
                    <input type="text" name="platega_id" autocomplete="off" placeholder="<?= !empty($cfg['platega_id']) ? 'оставьте пустым, чтобы не менять' : 'из кабинета Platega' ?>">
                    <label style="display:block; margin-top:10px;">Секретный ключ (X-Secret)</label>
                    <input type="password" name="platega_secret" autocomplete="new-password" placeholder="<?= !empty($cfg['platega_secret']) ? 'оставьте пустым, чтобы не менять' : 'API-ключ из кабинета Platega' ?>">
                    <label style="display:block; margin-top:10px;">Способ оплаты</label>
                    <select name="platega_method"><?php foreach (so_platega_methods() as $mv => $ml): ?><option value="<?= $mv ?>" <?= (int)$cfg['platega_method'] === $mv ? 'selected' : '' ?>><?= so_h($ml) ?></option><?php endforeach; ?></select>
                    <?php if ($ready): ?><label class="switch-row" style="margin-top:10px;"><input type="checkbox" name="platega_clear"> удалить ключи (оплата Platega выключится)</label><?php endif; ?>
                    <div class="so-btns" style="margin-top:12px;"><button class="btn primary" type="submit">💾 Сохранить ключи</button><?php if ($ready): ?><a class="btn ghost" href="?tab=orders#soSettings">Отмена</a><?php endif; ?></div>
                </form>
            <?php endif; ?>
            <p class="muted" style="font-size:12.5px; margin:12px 0 0;">В кабинете Platega укажите адрес уведомлений (callback): <code><?= so_h(so_base_url() . '/order_pay_callback.php') ?></code> <button class="btn ghost sm" type="button" data-copy="<?= so_h(so_base_url() . '/order_pay_callback.php') ?>">Копировать</button><br>
                Если уведомление не придёт, оплата всё равно подтвердится, когда клиент вернётся на страницу заказа, или по кнопке «Проверить в Platega».</p>
        </div>

        <form class="card" method="POST" action="?tab=orders" style="margin-top:0;">
            <h3>🔐 Доступ и приём заказов</h3>
            <input type="hidden" name="action" value="so"><input type="hidden" name="op" value="cfg_main"><input type="hidden" name="so_csrf" value="<?= so_h($csrf) ?>"><input type="hidden" name="so_back" value="admin.php?tab=orders#soSettings">
            <label class="switch-row"><input type="checkbox" name="orders_open" <?= !empty($cfg['orders_open']) ? 'checked' : '' ?>> Принимать новые заказы на сайте</label>
            <label class="switch-row" style="margin-top:10px;"><input type="checkbox" name="reviews_auto" <?= !empty($cfg['reviews_auto']) ? 'checked' : '' ?>> Публиковать отзывы по завершённым заказам сразу (остальные — после проверки)</label>
            <label style="display:block; margin-top:12px;">Подсказка клиенту о времени ответа</label>
            <input type="text" name="reply_hint" maxlength="120" value="<?= so_h($cfg['reply_hint']) ?>">
            <div class="row" style="align-items:flex-start; margin-top:12px;">
                <div class="grow"><label>Кто ведёт заказы <span class="muted">(пусто — руководители и администраторы)</span></label>
                    <div class="so-pick" style="max-height:180px;"><?php foreach ($roles as $r => $ri): if ($r === 'Пользователь') continue; ?><label><input type="checkbox" name="manager_roles[]" value="<?= so_h($r) ?>" <?= in_array($r, (array)$cfg['manager_roles'], true) ? 'checked' : '' ?>><span class="pi"><b><?= ($ri['icon'] ?? '') . ' ' . so_h($r) ?></b></span></label><?php endforeach; ?></div></div>
                <div class="grow"><label>Кто кодеры-исполнители <span class="muted">(пусто — отдел разработчиков и кодеров)</span></label>
                    <div class="so-pick" style="max-height:180px;"><?php foreach ($roles as $r => $ri): if ($r === 'Пользователь') continue; ?><label><input type="checkbox" name="coder_roles[]" value="<?= so_h($r) ?>" <?= in_array($r, (array)$cfg['coder_roles'], true) ? 'checked' : '' ?>><span class="pi"><b><?= ($ri['icon'] ?? '') . ' ' . so_h($r) ?></b></span></label><?php endforeach; ?></div></div>
            </div>
            <details style="margin-top:12px;"><summary style="cursor:pointer; color:var(--soft); font-weight:600;">💰 Цены в калькуляторе на странице заказа</summary>
                <div class="row" style="align-items:flex-start; margin-top:8px;">
                    <div class="grow"><?php foreach ($cat['types'] as $kk => $t): ?><label style="display:block; margin-top:6px;"><?= $t['icon'] . ' ' . so_h($t['name']) ?>, от ₽</label><input type="number" min="0" step="500" name="price_t[<?= so_h($kk) ?>]" value="<?= (int)$t['price'] ?>"><?php endforeach; ?></div>
                    <div class="grow"><?php foreach ($cat['features'] as $kk => $f): ?><label style="display:block; margin-top:6px;"><?= $f['icon'] . ' ' . so_h($f['name']) ?>, +₽</label><input type="number" min="0" step="500" name="price_f[<?= so_h($kk) ?>]" value="<?= (int)$f['price'] ?>"><?php endforeach; ?></div>
                </div></details>
            <button class="btn primary" type="submit" style="margin-top:14px;">💾 Сохранить настройки</button>
        </form>
    </div>
    <?php
}

function so_orders_js($open) {
    $st = array_map(function ($s) { return ['name' => $s[0], 'icon' => $s[1], 'color' => $s[2]]; }, so_statuses());
    $js = ['csrf' => so_csrf(), 'me' => so_me(), 'mgr' => so_manager(), 'open' => (int)$open, 'statuses' => $st];
    ?>
<script>
(() => {
const SO = <?= json_encode($js, JSON_UNESCAPED_UNICODE | JSON_HEX_TAG | JSON_HEX_AMP) ?>;
const $ = (s, r = document) => r.querySelector(s), $$ = (s, r = document) => [...r.querySelectorAll(s)];
const esc = s => String(s ?? '').replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
const linkify = s => esc(s).replace(/\bhttps?:\/\/[^\s<]+[^\s<.,;:!?)"']/g, u => `<a href="${u}" target="_blank" rel="noopener nofollow">${u}</a>`);
const money = n => Math.round(n).toLocaleString('ru-RU') + ' ₽';
const hm = t => new Date(t * 1000).toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit' });
const dayName = t => { const d = new Date(t * 1000), n = new Date(), y = new Date(); y.setDate(n.getDate() - 1);
    return d.toDateString() === n.toDateString() ? 'Сегодня' : d.toDateString() === y.toDateString() ? 'Вчера' : d.toLocaleDateString('ru-RU', { day: 'numeric', month: 'long' }); };
const ago = t => { const s = Date.now() / 1000 - t; if (s < 60) return 'сейчас'; if (s < 3600) return Math.floor(s / 60) + ' мин'; if (s < 86400) return hm(t); return new Date(t * 1000).toLocaleDateString('ru-RU', { day: 'numeric', month: 'short' }); };
const toast = (m, t) => (window.showToast ? showToast(m, t) : alert(m));

const S = { cur: 0, after: 0, rev: -1, clientSeen: 0, chatTimer: null, listTimer: null, f: 'active', q: '', files: [], lastDay: '', pendingPanel: null, state: null, unread: {}, firstList: true };
const msgs = $('#soMsgs'), panel = $('#soPanel');

async function post(op, fd = new FormData()) {
    fd.append('action', 'so'); fd.append('op', op); fd.append('so_csrf', SO.csrf); fd.append('is_ajax', '1');
    if (S.cur && !fd.has('o')) fd.append('o', S.cur);
    const r = await fetch('admin.php?tab=orders', { method: 'POST', body: fd });
    const txt = await r.text();
    try { return JSON.parse(txt); } catch (e) { return { ok: false, msg: txt.trim() === 'forbidden' ? 'Недостаточно прав' : 'Ошибка сервера' }; }
}

/* ---------- звук ---------- */
let actx;
function ping() { if (!$('#soSound') || !$('#soSound').checked) return; try { actx = actx || new (window.AudioContext || window.webkitAudioContext)(); const t = actx.currentTime;
    [660, 990].forEach((f, i) => { const o = actx.createOscillator(), g = actx.createGain(); o.frequency.value = f; g.gain.setValueAtTime(.0001, t + i * .1); g.gain.exponentialRampToValueAtTime(.1, t + i * .1 + .02); g.gain.exponentialRampToValueAtTime(.0001, t + i * .1 + .22); o.connect(g).connect(actx.destination); o.start(t + i * .1); o.stop(t + i * .1 + .25); }); } catch (e) {} }

/* ---------- список заказов ---------- */
async function loadList() {
    clearTimeout(S.listTimer);
    try {
        const j = await fetch(`admin.php?tab=orders&so_ajax=list&f=${S.f}&q=${encodeURIComponent(S.q)}`).then(r => r.json());
        if (j.ok) {
            let fresh = false;
            j.items.forEach(it => { const before = S.unread[it.id]; if (!S.firstList && (before === undefined ? it.status === 'new' : it.unread > before) && it.id !== S.cur) fresh = true; S.unread[it.id] = it.unread; });
            if (fresh) ping();
            S.firstList = false;
            $$('#soFilters button').forEach(b => { b.querySelector('b').textContent = j.counts[b.dataset.f] ? ' ' + j.counts[b.dataset.f] : ''; });
            $('#soList').innerHTML = j.items.length ? j.items.map(it => `
                <a class="so-item${it.id === S.cur ? ' on' : ''}" href="?tab=orders&o=${it.id}" data-o="${it.id}">
                    <div class="r1"><b>№${it.id} · ${esc(it.title)}</b><time>${ago(it.act)}</time></div>
                    <div class="r2">${it.last_from === 'client' ? '👤 ' : ''}${esc(it.last_text || '—')}</div>
                    <div class="r3"><span class="so-st" style="--c:${esc(it.status_color)}">${esc(it.status_label)}</span>${it.pending ? '<span class="so-st" style="--c:#f59e0b">💳 ' + money(it.pending_sum) + '</span>' : ''}
                    <span class="who">${esc(it.client)}${it.coder ? ' · 🧑‍💻 ' + esc(it.coder) : ''}</span>${it.unread ? `<span class="so-unread">${it.unread}</span>` : ''}</div></a>`).join('')
                : '<div class="so-empty-c" style="padding:30px 14px;">Заказов в этом списке нет</div>';
        }
    } catch (e) {}
    S.listTimer = setTimeout(loadList, document.visibilityState === 'visible' ? 4000 : 15000);
}
$('#soFilters').addEventListener('click', e => { const b = e.target.closest('[data-f]'); if (!b) return; S.f = b.dataset.f; $$('#soFilters button').forEach(x => x.classList.toggle('on', x === b)); loadList(); });
let qT; $('#soQ').addEventListener('input', e => { clearTimeout(qT); qT = setTimeout(() => { S.q = e.target.value.trim(); loadList(); }, 250); });
$('#soList').addEventListener('click', e => { const a = e.target.closest('[data-o]'); if (!a) return; e.preventDefault(); openChat(+a.dataset.o); });

/* ---------- чат ---------- */
function openChat(id) {
    clearTimeout(S.chatTimer);
    Object.assign(S, { cur: id, after: 0, rev: -1, clientSeen: 0, lastDay: '', pendingPanel: null, state: null, cards: {} });
    msgs.innerHTML = ''; panel.innerHTML = '<div class="so-empty-c">Загрузка…</div>';
    $('#soChatEmpty').style.display = 'none'; $('#soChatBox').style.display = 'flex';
    $$('#soList .so-item').forEach(a => a.classList.toggle('on', +a.dataset.o === id));
    history.replaceState(null, '', '?tab=orders&o=' + id);
    const box = $('#soMsgr'); if (box.getBoundingClientRect().top > 120) box.scrollIntoView({ behavior: 'smooth', block: 'start' });
    loadChat(true);
}
const fileUrl = f => `admin.php?tab=orders&so_file=${f.id}&o=${S.cur}`;
function filesHtml(files) {
    if (!files || !files.length) return '';
    const im = files.filter(f => f.img), dc = files.filter(f => !f.img);
    return (im.length ? '<div class="so-imgs">' + im.map(f => `<img src="${fileUrl(f)}" alt="${esc(f.name)}" loading="lazy" data-zoom>`).join('') + '</div>' : '')
        + dc.map(f => `<a class="so-fl" href="${fileUrl(f)}" target="_blank" rel="noopener">📄 ${esc(f.name)} <span class="muted">${Math.max(1, Math.round(f.size / 1024))} КБ</span></a>`).join('');
}
function cardHtml(m) {
    const s = S.state || { invoices: [], offers: [] };
    if (m.kind === 'invoice') {
        const inv = s.invoices.find(i => String(i.id) === String(m.ref));
        if (!inv) return '<div class="so-xc">Счёт удалён</div>';
        const c = { pending: '#f59e0b', paid: '#22c55e' }[inv.status] || '#6b7280', l = { pending: '⏳ ждёт оплаты', paid: '✅ оплачен', cancelled: 'отменён', refunded: 'возврат' }[inv.status] || inv.status;
        return `<div class="so-xc" style="--c:${c}"><div class="k">💳 Счёт · ${esc(m.who)}</div><b class="sum">${money(inv.amount)}</b> <span class="so-st" style="--c:${c}">${l}</span><div class="muted">${esc(inv.desc)} · ${inv.tx ? 'Platega' : 'вручную'}</div></div>`;
    }
    if (m.kind === 'offer') {
        const of = s.offers.find(o => String(o.id) === String(m.ref));
        if (!of) return '<div class="so-xc">Предложение удалено</div>';
        const l = { open: '⏳ клиент выбирает', chosen: '✅ выбран: ' + (of.chosen || ''), closed: 'закрыто', cancelled: 'отозвано' }[of.status] || of.status;
        return `<div class="so-xc" style="--c:#06b6d4"><div class="k">🧑‍💻 Выбор кодера · ${esc(m.who)}</div><div>${of.coders.map(c => `<b>${esc(c.login)}</b>`).join(', ')}</div><div class="muted">${esc(l)}${of.deadline ? ' · сдача ' + esc(of.deadline) : ''}</div></div>`;
    }
    if (m.kind === 'review') return `<div class="so-xc" style="--c:#fbbf24"><div class="k">⭐ Запрос отзыва</div><div class="muted">${s.review ? 'Клиент оставил отзыв — см. вкладку «Отзывы»' : 'Клиенту показана форма отзыва'}</div></div>`;
    return '';
}
function msgHtml(m) {
    let pre = '';
    const dn = dayName(m.t); if (dn !== S.lastDay) { S.lastDay = dn; pre = `<div class="so-dsep">${dn}</div>`; }
    if (['invoice', 'offer', 'review'].includes(m.kind)) return pre + `<div data-card="${m.id}">${cardHtml(m)}</div>`;
    if (m.from === 'system') return pre + `<div class="so-sys">${linkify(m.text)}</div>`;
    const right = m.from === 'staff', mine = m.login === SO.me;
    const cls = ['so-m', m.from === 'client' ? 'client' : '', right ? 'right' : '', right && !mine ? 'other' : '', m.internal ? 'internal' : ''].join(' ');
    const c = m.color || '#888';
    return pre + `<div class="${cls}"><div class="so-ava sm" style="--c:${esc(m.from === 'client' ? '#64748b' : c)}">${esc((m.who || '?').slice(0, 1).toUpperCase())}</div><div class="so-b">
        <div class="so-mh">${esc(m.from === 'client' ? m.who + ' · клиент' : m.who)}${m.role && m.from !== 'client' ? `<span class="rl" style="--c:${esc(c)}">${esc(m.role)}</span>` : ''}${m.internal ? '<span class="lock">🔒 заметка</span>' : ''}</div>
        ${m.text ? `<div class="so-tx">${linkify(m.text)}</div>` : ''}${filesHtml(m.files)}
        <div class="so-mt">${hm(m.t)}${right && !m.internal ? ` <span data-tick="${m.id}">✓</span>` : ''}</div></div></div>`;
}
function ticks() { $$('[data-tick]', msgs).forEach(el => { const r = +el.dataset.tick <= S.clientSeen; el.textContent = r ? '✓✓' : '✓'; el.classList.toggle('read', r); el.title = r ? 'Клиент прочитал' : 'Доставлено'; }); }
function refreshCards() { $$('[data-card]', msgs).forEach(el => { const m = S.cards[el.dataset.card]; if (m) el.innerHTML = cardHtml(m); }); }
S.cards = {};

function panelBusy() { const a = document.activeElement; return panel.contains(a) && /^(INPUT|TEXTAREA|SELECT)$/.test(a.tagName) || panel.dataset.dirty === '1'; }
function setPanel(html) {
    if (panelBusy()) { S.pendingPanel = html; return; }
    const top = panel.scrollTop;
    panel.innerHTML = html; panel.dataset.dirty = '0'; S.pendingPanel = null;
    panel.scrollTop = top;
    $$('[data-linkify]', panel).forEach(el => { el.innerHTML = linkify(el.textContent); });
    initCoderPick();
}
panel.addEventListener('input', () => { panel.dataset.dirty = '1'; });
panel.addEventListener('focusout', () => { setTimeout(() => { if (S.pendingPanel && !panel.contains(document.activeElement) && panel.dataset.dirty !== '1') setPanel(S.pendingPanel); }, 200); });

async function loadChat(initial = false) {
    clearTimeout(S.chatTimer);
    const id = S.cur; if (!id) return;
    const vis = document.visibilityState === 'visible';
    try {
        const j = await fetch(`admin.php?tab=orders&so_ajax=chat&o=${id}&after=${S.after}&rev=${S.rev}&seen=${vis ? 1 : 0}`).then(r => r.json());
        if (id !== S.cur) return;
        if (!j.ok) { panel.innerHTML = `<div class="so-empty-c">${esc(j.msg)}</div>`; return; }
        S.state = j.state;
        const st = SO.statuses[j.state.status] || { name: j.state.status, color: '#888', icon: '' };
        $('#soCTitle').textContent = `№${j.state.id} · ${j.state.title}`;
        $('#soCAva').textContent = (j.state.client || '?').slice(0, 1).toUpperCase(); $('#soCAva').style.setProperty('--c', '#64748b');
        $('#soCStatus').textContent = st.icon + ' ' + st.name; $('#soCStatus').style.setProperty('--c', st.color);
        $('#soCOnline').classList.toggle('on', j.client_online);
        $('#soCSub').textContent = j.client_typing ? 'печатает…' : (j.client_online ? `${j.state.client} · на странице заказа` : `${j.state.client} · не в сети`);
        $('#soTyping').hidden = !j.client_typing;
        const stick = initial || msgs.scrollHeight - msgs.scrollTop - msgs.clientHeight < 160;
        let fromClient = 0;
        j.messages.forEach(m => {
            if (m.id <= S.after) return; S.after = m.id;
            if (['invoice', 'offer', 'review'].includes(m.kind)) S.cards[m.id] = m;
            msgs.insertAdjacentHTML('beforeend', msgHtml(m));
            if (!initial && m.from === 'client') fromClient++;
        });
        if (fromClient) ping();
        refreshCards();
        if (stick && j.messages.length) msgs.scrollTop = msgs.scrollHeight;
        if (j.client_seen !== S.clientSeen) { S.clientSeen = j.client_seen; } ticks();
        if (j.panel !== undefined) { S.rev = j.rev; setPanel(j.panel); }
    } catch (e) {}
    S.chatTimer = setTimeout(loadChat, vis ? 2000 : 8000);
}
document.addEventListener('visibilitychange', () => { if (document.visibilityState === 'visible') { if (S.cur) loadChat(); loadList(); } });

/* ---------- отправка ---------- */
const ta = $('#soText'), comp = $('#soComp');
const grow = () => { ta.style.height = 'auto'; ta.style.height = Math.min(170, ta.scrollHeight) + 'px'; };
let lastTyping = 0;
ta.addEventListener('input', () => { grow(); if (Date.now() - lastTyping > 2500 && ta.value.trim() && !$('#soInternal').checked) { lastTyping = Date.now(); post('typing').catch(() => {}); } });
ta.addEventListener('keydown', e => { if (e.key === 'Enter' && !e.shiftKey && !e.isComposing) { e.preventDefault(); comp.requestSubmit(); } });
$('#soInternal').addEventListener('change', e => { comp.classList.toggle('internal', e.target.checked); ta.placeholder = e.target.checked ? 'Внутренняя заметка для команды — клиент её не увидит' : 'Сообщение клиенту… (Enter — отправить)'; });
$$('.so-quick [data-q]').forEach(b => b.addEventListener('click', () => { ta.value = (ta.value ? ta.value + '\n' : '') + b.dataset.q; grow(); ta.focus(); }));
$('#soFiles').addEventListener('change', e => { [...e.target.files].forEach(f => { if (S.files.length < 5) S.files.push(f); }); e.target.value = ''; att(); });
function att() { $('#soAtt').innerHTML = S.files.map((f, i) => `<span>📎 ${esc(f.name)} <button type="button" data-rm="${i}">✕</button></span>`).join(''); }
$('#soAtt').addEventListener('click', e => { const b = e.target.closest('[data-rm]'); if (b) { S.files.splice(+b.dataset.rm, 1); att(); } });
comp.addEventListener('submit', async e => {
    e.preventDefault();
    const text = ta.value.trim(); if (!text && !S.files.length) return;
    const fd = new FormData(); fd.append('text', text); if ($('#soInternal').checked) fd.append('internal', '1'); S.files.forEach(f => fd.append('files[]', f));
    const keep = [text, S.files.slice()]; ta.value = ''; grow(); S.files = []; att();
    const j = await post('send', fd);
    if (!j.ok) { toast(j.msg, 'error'); ta.value = keep[0]; S.files = keep[1]; att(); grow(); return; }
    loadChat(); loadList();
});

/* ---------- панель: формы и кнопки ---------- */
panel.addEventListener('change', e => { if (e.target.matches('[data-autosubmit]')) e.target.form.requestSubmit(); });
panel.addEventListener('submit', async e => {
    const f = e.target.closest('form.so-f'); if (!f) return;
    e.preventDefault();
    if (f.dataset.confirm && !confirm(f.dataset.confirm)) return;
    if (f.hasAttribute('data-confirm-warn')) { const w = f.querySelector('[data-coder-info].warn'); if (w && !w.hidden && !confirm(w.textContent + '\n\nВсё равно назначить?')) return; }
    const btn = f.querySelector('button[type="submit"]'); if (btn) btn.disabled = true;
    const fd = new FormData(f); const op = fd.get('op'); fd.delete('op'); fd.delete('action'); fd.delete('so_csrf');
    const j = await post(op, fd);
    if (btn) btn.disabled = false;
    toast(j.msg, j.ok ? 'success' : 'error');
    if (j.ok) { panel.dataset.dirty = '0'; S.rev = -1; if (op === 'delete') { S.cur = 0; $('#soChatBox').style.display = 'none'; $('#soChatEmpty').style.display = ''; panel.innerHTML = ''; history.replaceState(null, '', '?tab=orders'); } else loadChat(); loadList(); }
});
document.addEventListener('click', e => {
    const cp = e.target.closest('[data-copy]');
    if (cp) { const v = cp.dataset.copy; (navigator.clipboard ? navigator.clipboard.writeText(v) : Promise.reject()).then(() => toast('Скопировано', 'success')).catch(() => prompt('Скопируйте:', v)); return; }
    const fill = e.target.closest('[data-fill-amount]');
    if (fill) { const f = fill.closest('form'); f.amount.value = fill.dataset.fillAmount; f.desc.value = fill.dataset.fillDesc; panel.dataset.dirty = '1'; return; }
    const tb = e.target.closest('[data-tabs]');
    if (tb) { const sec = tb.closest('.so-sec'); $$('[data-tabs]', sec).forEach(x => x.classList.toggle('on', x === tb)); $$('[data-pane]', sec).forEach(p => { p.hidden = p.dataset.pane !== tb.dataset.tabs; }); return; }
    const z = e.target.closest('[data-zoom]');
    if (z) { const lb = document.createElement('div'); lb.className = 'so-lightbox'; lb.innerHTML = `<img src="${z.src}" alt="">`; lb.onclick = () => lb.remove(); document.body.appendChild(lb); }
});

/* ---------- подсказки при назначении кодера: свободные дни, отпуск, загрузка ---------- */
function initCoderPick() {
    const dataEl = $('.so-cdata', panel); if (!dataEl) return;
    const D = JSON.parse(dataEl.textContent);
    const f = $('form[data-pane="assign"]', panel); if (!f) return;
    const sel = $('[data-coder-pick]', f), sIn = $('[data-start]', f), dIn = $('[data-deadline]', f), info = $('[data-coder-info]', f);
    const base = new Date(D.today + 'T00:00:00');
    const iso = i => { const d = new Date(base); d.setDate(d.getDate() + i); return d.toISOString().slice(0, 10); };
    const idxOf = s => Math.round((new Date(s + 'T00:00:00') - base) / 86400000);
    const fmt = s => new Date(s + 'T00:00:00').toLocaleDateString('ru-RU', { day: 'numeric', month: 'short' });
    sIn.min = D.today; dIn.min = D.today;
    const suggest = () => {
        const c = D.coders[sel.value]; if (!c) { info.hidden = true; return; }
        let i = c.days.indexOf('w'); if (i < 0) i = 0;
        sIn.value = iso(i);
        let left = D.est, j = i;
        for (; j < c.days.length && left > 0; j++) if (c.days[j] === 'w') left--;
        let dl = iso(Math.max(i, j - 1));
        if (D.want && D.want > dl) dl = D.want;
        dIn.value = dl; check();
    };
    const check = () => {
        const c = D.coders[sel.value]; if (!c || !sIn.value || !dIn.value) { info.hidden = true; return; }
        const a = Math.max(0, idxOf(sIn.value)), b = Math.min(c.days.length - 1, idxOf(dIn.value));
        let w = 0, v = 0, s = 0;
        for (let i = a; i <= b; i++) { const t = c.days[i]; if (t === 'w') w++; else if (t === 'v') v++; else if (t === 's') s++; }
        const warn = [];
        if (dIn.value < sIn.value) warn.push('сдача раньше старта');
        if (c.days[a] && c.days[a] !== 'w') warn.push('в день старта у кодера не рабочий день');
        if (v) warn.push(`в этот период ${v} дн. отпуска`);
        if (s) warn.push(`${s} дн. больничного`);
        if (w < Math.ceil(D.est * 0.6)) warn.push(`всего ${w} рабочих дней при оценке ≈${D.est}`);
        if (c.busy >= 3) warn.push(`уже ${c.busy} задач в работе`);
        info.hidden = false;
        info.className = 'so-info ' + (warn.length ? 'warn' : 'ok');
        info.textContent = (warn.length ? '⚠ ' + warn.join(', ') : `✓ ${w} рабочих дней в периоде, отпуска нет`) + ` · ★ ${c.rating ?? '—'} · сдал проектов: ${c.done}${c.hours ? ' · ' + c.hours : ''}` + (c.days.indexOf('w') > 0 ? ` · ближайший рабочий день: ${fmt(iso(c.days.indexOf('w')))}` : '');
    };
    sel.addEventListener('change', suggest); sIn.addEventListener('change', check); dIn.addEventListener('change', check);
}

loadList();
if (SO.open) openChat(SO.open);
})();
</script>
    <?php
}

/* ==========================================================
   ОТЗЫВЫ (модерация)
   ========================================================== */

function so_render_reviews() {
    $all = so_reviews()['items'];
    usort($all, function ($a, $b) { return $b['t'] <=> $a['t']; });
    $f = (string)($_GET['f'] ?? 'pending');
    $counts = ['pending' => 0, 'published' => 0, 'hidden' => 0, 'all' => count($all)];
    foreach ($all as $r) $counts[$r['status']] = ($counts[$r['status']] ?? 0) + 1;
    if ($f === 'pending' && !$counts['pending']) $f = 'all';
    $pub = so_review_stats(array_filter($all, function ($r) { return $r['status'] === 'published'; }));
    $list = array_values(array_filter($all, function ($r) use ($f) { return $f === 'all' || $r['status'] === $f; }));
    $csrf = so_csrf();
    $hidden = function ($op, $id) use ($csrf, $f) {
        return '<input type="hidden" name="action" value="so"><input type="hidden" name="op" value="' . $op . '"><input type="hidden" name="id" value="' . (int)$id . '"><input type="hidden" name="so_csrf" value="' . so_h($csrf) . '"><input type="hidden" name="so_back" value="admin.php?tab=reviews&amp;f=' . so_h($f) . '">';
    };
    $sl = ['pending' => ['на модерации', '#f59e0b'], 'published' => ['опубликован', '#22c55e'], 'hidden' => ['скрыт', '#6b7280']];
    ?>
    <div class="so-kpis">
        <div class="so-kpi"><div class="i">⭐</div><b><?= $pub['count'] ? number_format($pub['avg'], 1, ',', '') : '—' ?></b><span>средняя оценка на сайте</span></div>
        <div class="so-kpi"><div class="i">✅</div><b><?= $counts['published'] ?></b><span>опубликовано</span></div>
        <div class="so-kpi<?= $counts['pending'] ? ' hot' : '' ?>"><div class="i">⏳</div><b><?= $counts['pending'] ?></b><span>ждут проверки</span></div>
        <div class="so-kpi"><div class="i">🙈</div><b><?= $counts['hidden'] ?></b><span>скрыто</span></div>
    </div>
    <div class="row" style="margin-top:14px; justify-content:space-between;">
        <div class="so-seg">
            <?php foreach (['pending' => 'На модерации', 'published' => 'Опубликованные', 'hidden' => 'Скрытые', 'all' => 'Все'] as $k => $l): ?><a class="<?= $f === $k ? 'on' : '' ?>" href="?tab=reviews&amp;f=<?= $k ?>"><?= $l ?> · <?= $counts[$k] ?? 0 ?></a><?php endforeach; ?>
        </div>
        <a class="btn ghost sm" href="reviews.php" target="_blank" rel="noopener" style="margin:0;">Открыть страницу отзывов ↗</a>
    </div>
    <?php if (!$list): ?><div class="empty">Отзывов в этом списке нет.</div><?php endif; ?>
    <?php foreach ($list as $r): $s = $sl[$r['status']] ?? ['?', '#888']; ?>
        <div class="card so-rv">
            <div class="h">
                <div class="so-ava" style="--c:hsl(<?= crc32($r['name']) % 360 ?> 65% 50%)"><?= so_h(mb_strtoupper(mb_substr($r['name'], 0, 1))) ?></div>
                <div><b><?= so_h($r['name']) ?></b><div class="stars"><?= str_repeat('★', (int)$r['rating']) ?><span class="off"><?= str_repeat('★', 5 - (int)$r['rating']) ?></span></div></div>
                <span class="so-st" style="--c:<?= $s[1] ?>"><?= $s[0] ?></span>
                <?php if (!empty($r['verified'])): ?><span class="chip on">✓ проверенный заказ</span><?php endif; ?>
                <?php if (!empty($r['order_id'])): ?><a class="chip" href="?tab=orders&amp;o=<?= (int)$r['order_id'] ?>">заказ №<?= (int)$r['order_id'] ?></a><?php endif; ?>
                <?php if (!empty($r['coder'])): ?><span class="chip">🧑‍💻 <?= so_h($r['coder']) ?></span><?php endif; ?>
                <span class="muted" style="margin-left:auto; font-size:12px;"><?= date('d.m.Y H:i', $r['t']) ?><?php if (function_exists('ip_tag') && can('bans.manage') && !empty($r['ip'])): ?> · <?= ip_tag(['ip' => $r['ip'], 'src' => 'отзыв', 'login' => null], 'Отзыв: ' . mb_strimwidth($r['text'], 0, 50, '…')) ?><?php endif; ?></span>
            </div>
            <div class="txt"><?= so_h($r['text']) ?></div>
            <?php if (!empty($r['reply']['text'])): ?><div class="rep"><b style="font-size:12.5px;">💬 Ответ команды</b> <span class="muted" style="font-size:12px;">· <?= so_h($r['reply']['by']) ?></span><div style="white-space:pre-wrap; margin-top:4px;"><?= so_h($r['reply']['text']) ?></div></div><?php endif; ?>
            <div class="so-btns" style="margin-top:12px;">
                <?php if ($r['status'] !== 'published'): ?><form method="POST" action="?tab=reviews"><?= $hidden('rv_status', $r['id']) ?><input type="hidden" name="status" value="published"><button class="btn ok sm" type="submit">✓ Опубликовать</button></form><?php endif; ?>
                <?php if ($r['status'] !== 'hidden'): ?><form method="POST" action="?tab=reviews"><?= $hidden('rv_status', $r['id']) ?><input type="hidden" name="status" value="hidden"><button class="btn ghost sm" type="submit">🙈 Скрыть</button></form><?php endif; ?>
                <form method="POST" action="?tab=reviews" onsubmit="return confirm('Удалить отзыв навсегда?');"><?= $hidden('rv_delete', $r['id']) ?><button class="btn ghost danger sm" type="submit">🗑 Удалить</button></form>
                <details class="act" style="flex-basis:100%;"><summary class="btn ghost sm" style="margin-top:4px;">💬 <?= !empty($r['reply']['text']) ? 'Изменить ответ' : 'Ответить от команды' ?></summary>
                    <form method="POST" action="?tab=reviews" class="act-panel" style="max-width:640px; width:100%;"><?= $hidden('rv_reply', $r['id']) ?>
                        <textarea name="reply" maxlength="1500" placeholder="Ответ увидят все на странице отзывов. Пусто — удалить ответ."><?= so_h($r['reply']['text'] ?? '') ?></textarea>
                        <button class="btn primary sm" type="submit">Сохранить ответ</button></form></details>
            </div>
        </div>
    <?php endforeach;
}
