<?php
/* ==========================================================
   ВКЛАДКИ АДМИНКИ: «Расписание», «Заказы сайтов» и «Отзывы».
   Подключается из admin.php после проверки входа:
       require_once __DIR__ . '/_schedule_admin.php';

   Кто что может (определяется по ролям и отделам из _roles.php):
   — руководители (владельцы сайта, «Руководитель», отдел руководителей)
     ставят сотрудникам рабочие дни, выходные, отпуск и больничный,
     один раз вписывают ключи Platega и настраивают доступ;
   — администраторы заказов (руководители + административный отдел,
     или роли, выбранные в настройках) ведут чат с клиентом, выбирают
     кодера или дают выбор клиенту, выставляют счета;
   — кодеры (отдел разработчиков и кодеров) видят свои задачи
     в календаре, принимают их и сдают на проверку;
   — свой календарь (только просмотр) есть у каждого в команде.
   ========================================================== */

require_once __DIR__ . '/_orders_lib.php';
require_once __DIR__ . '/_schedule_admin_ui.php';

/* ---------------- КТО ЕСТЬ КТО ---------------- */

function so_me() {
    return (string)($GLOBALS['user'] ?? '');
}

/** Все сотрудники: логин => роль, подпись, цвет, отдел. */
function so_staff() {
    static $cache = null;
    if ($cache !== null) return $cache;
    $out = [];
    foreach ((array)($GLOBALS['users'] ?? []) as $login => $u) {
        if (!is_array($u)) continue;
        $login = (string)$login;
        $r = (string)($u['role'] ?? 'Пользователь');
        if (!rt_is_staff($r, $login)) continue;
        $d = (string)($u['direction'] ?? '');
        $ri = rt_role_info($r) ?: [];
        $out[$login] = ['role' => $r, 'dir' => $d, 'label' => rt_role_label($r, $d), 'color' => $ri['color'] ?? '#718096',
                        'icon' => $ri['icon'] ?? '👤', 'dept' => rt_user_dept($r, $d)];
    }
    uksort($out, 'strcasecmp');
    return $cache = $out;
}

function so_is_leader_u($login) {
    if (rt_is_owner($login)) return true;
    $s = so_staff()[$login] ?? null;
    if (!$s) return false;
    if (defined('RT_TOP_ROLE') && $s['role'] === RT_TOP_ROLE) return true;
    return $s['dept'] === 'leader' || mb_stripos($s['role'], 'руковод') !== false;
}

function so_is_manager_u($login) {
    if (so_is_leader_u($login)) return true;
    $s = so_staff()[$login] ?? null;
    if (!$s) return false;
    $roles = (array)so_config()['manager_roles'];
    return $roles ? in_array($s['role'], $roles, true) : $s['dept'] === 'admin';
}

function so_is_coder_u($login) {
    $s = so_staff()[$login] ?? null;
    if (!$s) return false;
    $roles = (array)so_config()['coder_roles'];
    return $roles ? in_array($s['role'], $roles, true) : $s['dept'] === 'dev';
}

function so_leader()  { return so_is_leader_u(so_me()); }
function so_manager() { return so_is_manager_u(so_me()); }

/** Доступ к заказу: 'manager' — полный, 'coder' — назначенный кодер, null — нет. */
function so_access(array $o) {
    if (so_manager()) return 'manager';
    if (!empty($o['coder']) && $o['coder']['login'] === so_me()) return 'coder';
    return null;
}

function so_access_entry(array $e) {
    if (so_manager()) return 'manager';
    return ($e['coder'] ?? null) === so_me() ? 'coder' : null;
}

/** Видимость вкладок в меню (admin.php → tab_allowed). null — вкладка не наша. */
function so_tab_allowed($key) {
    if ($key === 'schedule') return so_me() !== '' && (isset(so_staff()[so_me()]) || rt_is_owner(so_me()));
    if ($key === 'orders')   return so_manager() || (bool)so_tasks_of(so_me());
    if ($key === 'reviews')  return so_manager();
    return null;
}

/** Числа для меню и колокольчика. */
function so_counts() {
    static $c = null;
    if ($c !== null) return $c;
    $me = so_me();
    $mgr = so_manager();
    $orders = 0; $tasks = 0;
    foreach (so_index() as $e) {
        if ($e['status'] === 'cancel') continue;
        if ($mgr) { if ($e['status'] === 'new' || $e['unread_staff'] > 0) $orders++; }
        elseif (($e['coder'] ?? null) === $me && $e['unread_coder'] > 0) $orders++;
    }
    foreach (so_tasks_of($me) as $t) if ($t['task'] === 'new' && $t['status'] !== 'done') $tasks++;
    $reviews = 0;
    if ($mgr) foreach (so_reviews()['items'] as $r) if (($r['status'] ?? '') === 'pending') $reviews++;
    return $c = ['orders' => $orders, 'schedule' => $tasks, 'reviews' => $reviews];
}

function so_csrf() {
    if (empty($_SESSION['so_csrf'])) $_SESSION['so_csrf'] = bin2hex(random_bytes(16));
    return $_SESSION['so_csrf'];
}

function so_log($msg, $type = 'orders') {
    $logs = load_json('logs.json', []);
    $logs[] = ['time' => date('Y-m-d H:i:s'), 'type' => $type, 'msg' => $msg];
    save_json('logs.json', $logs);
}

function so_json(array $a) {
    header('Content-Type: application/json; charset=utf-8');
    header('Cache-Control: no-store');
    echo json_encode($a, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES | JSON_INVALID_UTF8_SUBSTITUTE);
    exit;
}

/** Ответ на действие: JSON для fetch, всплывашка + возврат на страницу для обычной формы. */
function so_reply($ok, $msg, array $extra = []) {
    if (!empty($_POST['is_ajax'])) so_json(['ok' => (bool)$ok, 'msg' => $msg] + $extra);
    flash($msg, $ok ? 'success' : 'error');
    $back = (string)($_POST['so_back'] ?? '');
    if (!preg_match('~^admin\.php\?tab=[a-z_]+[A-Za-z0-9_=&%.\-]*(#[a-z0-9_-]+)?$~', $back)) $back = 'admin.php?tab=' . urlencode((string)($_GET['tab'] ?? 'orders'));
    header('Location: ' . $back);
    exit;
}

function so_hue($id) {
    return ((int)$id * 47) % 360;
}

function so_month_name($m, $gen = false) {
    $n = ['Январь', 'Февраль', 'Март', 'Апрель', 'Май', 'Июнь', 'Июль', 'Август', 'Сентябрь', 'Октябрь', 'Ноябрь', 'Декабрь'];
    $g = ['января', 'февраля', 'марта', 'апреля', 'мая', 'июня', 'июля', 'августа', 'сентября', 'октября', 'ноября', 'декабря'];
    return ($gen ? $g : $n)[max(1, min(12, (int)$m)) - 1];
}

function so_dshort($d) {
    if (!$d) return '—';
    $t = strtotime($d);
    return (int)date('j', $t) . ' ' . mb_substr(so_month_name(date('n', $t), true), 0, 3);
}

/** Ссылка «написать клиенту» по его контакту. */
function so_contact_link($type, $c) {
    $c = trim((string)$c);
    if ($type === 'telegram') {
        if (preg_match('~(?:t\.me/|^@?)([A-Za-z0-9_]{4,32})$~', $c, $m)) return 'https://t.me/' . $m[1];
        $digits = preg_replace('/\D/', '', $c);
        return $digits ? 'https://t.me/+' . $digits : '';
    }
    if ($type === 'whatsapp') { $d = preg_replace('/\D/', '', $c); return $d ? 'https://wa.me/' . $d : ''; }
    if ($type === 'email') return filter_var($c, FILTER_VALIDATE_EMAIL) ? 'mailto:' . $c : '';
    if ($type === 'phone') { $d = preg_replace('/[^\d+]/', '', $c); return $d ? 'tel:' . $d : ''; }
    return '';
}

/* ---------------- ВХОДНАЯ ТОЧКА ИЗ admin.php ---------------- */

function so_admin_handle() {
    if (isset($_GET['so_file'])) {
        $o = so_order_load((int)($_GET['o'] ?? 0));
        if (!$o || !so_access($o)) { http_response_code(404); exit; }
        so_send_file($o, (string)$_GET['so_file'], true);
    }
    if (isset($_GET['so_ajax'])) so_admin_ajax((string)$_GET['so_ajax']);
    if (($_SERVER['REQUEST_METHOD'] ?? '') === 'POST' && ($_POST['action'] ?? '') === 'so') so_admin_post();
}

/* ---------------- AJAX: список заказов и чат ---------------- */

function so_admin_ajax($what) {
    $mgr = so_manager();
    $me = so_me();

    if ($what === 'list') {
        session_write_close();
        $st = so_statuses();
        $f = (string)($_GET['f'] ?? 'active');
        $q = mb_strtolower(trim((string)($_GET['q'] ?? '')));
        $items = [];
        $counts = ['active' => 0, 'new' => 0, 'work' => 0, 'pay' => 0, 'done' => 0, 'cancel' => 0, 'all' => 0, 'mine' => 0];
        foreach (so_index() as $e) {
            if (!so_access_entry($e)) continue;
            $active = !in_array($e['status'], ['done', 'cancel'], true);
            $groups = ['all' => true, 'active' => $active, 'new' => $e['status'] === 'new' || $e['unread_staff'] > 0,
                       'work' => in_array($e['status'], ['assigned', 'work', 'review'], true), 'pay' => $e['pending'] > 0,
                       'done' => $e['status'] === 'done', 'cancel' => $e['status'] === 'cancel',
                       'mine' => ($e['manager'] ?? null) === $me || ($e['coder'] ?? null) === $me];
            foreach ($groups as $g => $in) if ($in) $counts[$g]++;
            if (empty($groups[$f])) continue;
            if ($q !== '' && mb_strpos(mb_strtolower($e['id'] . ' ' . $e['title'] . ' ' . $e['client'] . ' ' . ($e['coder'] ?? '')), $q) === false) continue;
            $items[] = $e + ['status_label' => $st[$e['status']][0] ?? $e['status'], 'status_color' => $st[$e['status']][2] ?? '#888',
                             'unread' => $mgr ? $e['unread_staff'] : $e['unread_coder']];
        }
        usort($items, function ($a, $b) { return $b['act'] <=> $a['act']; });
        so_json(['ok' => true, 'items' => array_slice($items, 0, 300), 'counts' => $counts]);
    }

    if ($what === 'chat') {
        session_write_close();
        $o = so_order_load((int)($_GET['o'] ?? 0));
        if (!$o) so_json(['ok' => false, 'msg' => 'Заказ не найден']);
        $acc = so_access($o);
        if (!$acc) so_json(['ok' => false, 'msg' => 'Нет доступа к этому заказу']);
        $side = $acc === 'manager' ? 'staff' : 'coder';
        $after = max(0, (int)($_GET['after'] ?? 0));
        $msgs = [];
        $last = 0;
        foreach ($o['messages'] as $m) {
            $last = max($last, $m['id']);
            if ($m['id'] > $after) $msgs[] = $m;
        }
        if (!empty($_GET['seen'])) {
            if ($last > (int)($o['seen'][$side] ?? 0)) {
                so_order_update($o['id'], function (&$x) use ($side, $last) { $x['seen'][$side] = max((int)($x['seen'][$side] ?? 0), $last); return true; });
            }
            so_live_touch($o['id'], $side === 'staff' ? 'staff_online' : 'coder_online');
        }
        $live = so_live($o['id']);
        $res = [
            'ok' => true, 'messages' => $msgs, 'rev' => (int)$o['rev'],
            'client_typing' => (time() - (int)($live['client_typing'] ?? 0)) < 6,
            'client_online' => (time() - (int)($live['client_online'] ?? 0)) < 30,
            'client_seen' => (int)($o['seen']['client'] ?? 0),
            'state' => ['status' => $o['status'], 'invoices' => $o['invoices'], 'offers' => $o['offers'], 'review' => !empty($o['review_id']),
                        'title' => so_order_title($o), 'client' => $o['client']['name'], 'id' => $o['id'], 'coder' => $o['coder']['login'] ?? null],
        ];
        if ((int)($_GET['rev'] ?? -1) !== (int)$o['rev']) $res['panel'] = so_render_panel($o, $acc);
        so_json($res);
    }

    so_json(['ok' => false, 'msg' => 'Неизвестный запрос']);
}

/* ---------------- ДЕЙСТВИЯ (POST action=so, op=…) ---------------- */

function so_need_order() {
    $o = so_order_load((int)($_POST['o'] ?? 0));
    if (!$o) so_reply(false, 'Заказ не найден.');
    $acc = so_access($o);
    if (!$acc) so_reply(false, 'Нет доступа к этому заказу.');
    return [$o, $acc];
}

function so_need_manager() {
    if (!so_manager()) so_reply(false, 'Это может только администратор заказов или руководитель.');
}

function so_need_leader() {
    if (!so_leader()) so_reply(false, 'Это может только руководитель.');
}

function so_admin_post() {
    if (!hash_equals(so_csrf(), (string)($_POST['so_csrf'] ?? ''))) so_reply(false, 'Страница устарела — обновите её и повторите действие.');
    $op = (string)($_POST['op'] ?? '');
    $me = so_me();
    $staff = so_staff();
    $mine = $staff[$me] ?? ['label' => 'Администратор', 'color' => '#ff2a2a'];

    switch ($op) {

    /* ===== ЧАТ ===== */
    case 'send': {
        [$o, $acc] = so_need_order();
        $text = so_clip($_POST['text'] ?? '', 4000);
        [$files, $err] = so_store_uploads('files');
        if ($err !== '') so_reply(false, $err);
        if ($text === '' && !$files) so_reply(false, 'Пустое сообщение.');
        $internal = !empty($_POST['internal']);
        $m = so_order_update($o['id'], function (&$x) use ($acc, $me, $mine, $text, $files, $internal) {
            $m = so_msg_push($x, 'staff', ['who' => $me, 'login' => $me, 'role' => $mine['label'], 'color' => $mine['color'],
                                           'text' => $text, 'files' => $files, 'internal' => $internal]);
            $x['seen'][$acc === 'manager' ? 'staff' : 'coder'] = $m['id'];
            if ($acc === 'manager' && empty($x['manager'])) { $x['manager'] = $me; so_bump($x); }
            if ($acc === 'manager' && !$internal && $x['status'] === 'new') { $x['status'] = 'talk'; so_bump($x); }
            return $m;
        });
        so_reply((bool)$m, $m ? 'Отправлено' : 'Не удалось отправить', ['message' => $m]);
    }

    case 'typing': {
        [$o] = so_need_order();
        so_live_touch($o['id'], 'staff_typing', $me);
        so_json(['ok' => true]);
    }

    /* ===== СТАТУС И ОТВЕТСТВЕННЫЙ ===== */
    case 'status': {
        so_need_manager();
        [$o] = so_need_order();
        $st = (string)($_POST['status'] ?? '');
        if (!isset(so_statuses()[$st])) so_reply(false, 'Нет такого статуса.');
        so_order_update($o['id'], function (&$x) use ($st, $me) {
            if ($x['status'] === $st) return false;
            $x['status'] = $st;
            so_history($x, $me, 'Статус → ' . so_statuses()[$st][0]);
            so_bump($x);
            return true;
        });
        so_reply(true, 'Статус обновлён: ' . so_statuses()[$st][0]);
    }

    case 'take': {
        so_need_manager();
        [$o] = so_need_order();
        so_order_update($o['id'], function (&$x) use ($me) { $x['manager'] = $me; so_history($x, $me, 'Взял(а) заказ'); so_bump($x); return true; });
        so_reply(true, 'Заказ закреплён за вами');
    }

    /* ===== КОДЕР: НАЗНАЧИТЬ, ПРЕДЛОЖИТЬ ВЫБОР, СНЯТЬ ===== */
    case 'assign': {
        so_need_manager();
        [$o] = so_need_order();
        $login = (string)($_POST['coder'] ?? '');
        if (!isset($staff[$login])) so_reply(false, 'Выберите сотрудника из команды.');
        $start = (string)($_POST['start'] ?? '');
        $deadline = (string)($_POST['deadline'] ?? '');
        if (!so_valid_date($start) || !so_valid_date($deadline)) so_reply(false, 'Укажите даты начала и сдачи.');
        if ($deadline < $start) so_reply(false, 'Сдача не может быть раньше начала.');
        $note = so_clip($_POST['note'] ?? '', 2000);
        $c = $staff[$login];
        so_order_update($o['id'], function (&$x) use ($login, $c, $start, $deadline, $note, $me) {
            $x['coder'] = ['login' => $login, 'label' => $c['label'], 'color' => $c['color'], 'assigned_at' => time(), 'by' => $me,
                           'start' => $start, 'deadline' => $deadline, 'note' => $note, 'task' => 'new', 'task_at' => time()];
            foreach ($x['offers'] as &$of) if ($of['status'] === 'open') $of['status'] = 'closed';
            unset($of);
            if (!in_array($x['status'], ['done', 'cancel'], true)) $x['status'] = 'assigned';
            if (empty($x['manager'])) $x['manager'] = $me;
            so_msg_push($x, 'system', ['text' => 'Над вашим сайтом будет работать ' . $login . ' (' . $c['label'] . '). Задача стоит в его календаре: старт ' . so_dshort($start) . ', сдача ' . date('d.m.Y', strtotime($deadline)) . '.']);
            if ($note !== '') so_msg_push($x, 'staff', ['who' => $me, 'login' => $me, 'role' => 'Задача для кодера', 'color' => '#14b8a6', 'text' => '📌 ' . $note, 'internal' => true]);
            so_history($x, $me, 'Назначил(а) кодера ' . $login . ' (' . $start . ' — ' . $deadline . ')');
            so_bump($x);
            return true;
        });
        so_log("$me назначил(а) кодера $login на заказ №{$o['id']} ($start — $deadline)");
        so_reply(true, '✅ Задача отправлена в календарь ' . $login);
    }

    case 'unassign': {
        so_need_manager();
        [$o] = so_need_order();
        so_order_update($o['id'], function (&$x) use ($me) {
            if (empty($x['coder'])) return false;
            $was = $x['coder']['login'];
            $x['coder'] = null;
            if (!in_array($x['status'], ['done', 'cancel'], true)) $x['status'] = 'talk';
            so_msg_push($x, 'system', ['text' => 'Администратор подбирает для вашего заказа другого кодера.']);
            so_history($x, $me, 'Снял(а) кодера ' . $was);
            so_bump($x);
            return true;
        });
        so_reply(true, 'Кодер снят, задача убрана из календаря');
    }

    case 'offer': {
        so_need_manager();
        [$o] = so_need_order();
        $logins = array_values(array_unique(array_filter((array)($_POST['coders'] ?? []), function ($l) use ($staff) { return is_string($l) && isset($staff[$l]); })));
        if (count($logins) < 1 || count($logins) > 6) so_reply(false, 'Отметьте от 1 до 6 кодеров.');
        $deadline = (string)($_POST['deadline'] ?? '');
        if ($deadline !== '' && !so_valid_date($deadline)) $deadline = '';
        $note = so_clip($_POST['note'] ?? '', 600);
        $sched = so_sched(); $idx = so_index(); $revs = so_reviews_published();
        $today = date('Y-m-d');
        $snaps = [];
        foreach ($logins as $l) {
            $s = $staff[$l];
            $cs = so_coder_stats($l, $idx, $revs);
            $from = so_next_workday($sched, $l, $today);
            $snaps[] = ['login' => $l, 'role' => $s['label'], 'color' => $s['color'], 'rating' => $cs['rating'], 'reviews' => $cs['reviews'],
                        'done' => $cs['done'], 'next_free' => $from, 'busy' => so_availability($sched, $l, $from, $deadline ?: date('Y-m-d', strtotime($from . ' +30 days')), $idx)['busy']];
        }
        $oid = 'f' . base_convert((string)time(), 10, 36) . bin2hex(random_bytes(2));
        so_order_update($o['id'], function (&$x) use ($oid, $snaps, $deadline, $note, $me, $mine) {
            foreach ($x['offers'] as &$of) if ($of['status'] === 'open') $of['status'] = 'closed';
            unset($of);
            $x['offers'][] = ['id' => $oid, 'coders' => $snaps, 'deadline' => $deadline, 'note' => $note, 'status' => 'open', 'chosen' => null, 'created' => time(), 'by' => $me];
            so_msg_push($x, 'staff', ['who' => $me, 'login' => $me, 'role' => $mine['label'], 'color' => $mine['color'], 'kind' => 'offer', 'ref' => $oid]);
            if (!in_array($x['status'], ['done', 'cancel'], true) && empty($x['coder'])) $x['status'] = 'choose';
            if (empty($x['manager'])) $x['manager'] = $me;
            so_history($x, $me, 'Предложил(а) клиенту выбрать кодера: ' . implode(', ', array_column($snaps, 'login')));
            so_bump($x);
            return true;
        });
        so_reply(true, 'Клиент получил карточку с выбором кодера');
    }

    case 'offer_cancel': {
        so_need_manager();
        [$o] = so_need_order();
        $fid = (string)($_POST['offer'] ?? '');
        so_order_update($o['id'], function (&$x) use ($fid, $me) {
            $i = so_find_offer($x, $fid);
            if ($i === null || $x['offers'][$i]['status'] !== 'open') return false;
            $x['offers'][$i]['status'] = 'cancelled';
            if ($x['status'] === 'choose') $x['status'] = 'talk';
            so_history($x, $me, 'Отозвал(а) выбор кодера');
            so_bump($x);
            return true;
        });
        so_reply(true, 'Предложение отозвано');
    }

    /* ===== ЗАДАЧА КОДЕРА ===== */
    case 'task': {
        [$o, $acc] = so_need_order();
        $to = (string)($_POST['to'] ?? '');
        if (empty($o['coder'])) so_reply(false, 'У заказа нет кодера.');
        $is_coder = $o['coder']['login'] === $me;
        $allowed = ['work' => $is_coder || $acc === 'manager', 'review' => $is_coder || $acc === 'manager', 'back' => $acc === 'manager'];
        if (empty($allowed[$to])) so_reply(false, 'Нет прав на это действие.');
        so_order_update($o['id'], function (&$x) use ($to, $me) {
            $c = &$x['coder'];
            $who = $c['login'];
            if ($to === 'work') {
                if ($c['task'] === 'work') return false;
                $first = $c['task'] === 'new';
                $c['task'] = 'work';
                $x['status'] = 'work';
                so_msg_push($x, 'system', ['text' => $first ? '⚙️ ' . $who . ' принял(а) задачу и приступил(а) к работе.' : '⚙️ ' . $who . ' продолжает работу над сайтом.']);
            } elseif ($to === 'review') {
                $c['task'] = 'review';
                $x['status'] = 'review';
                so_msg_push($x, 'system', ['text' => '🔍 ' . $who . ' сдал(а) работу — администратор проверяет результат.']);
            } elseif ($to === 'back') {
                $c['task'] = 'work';
                $x['status'] = 'work';
                so_msg_push($x, 'system', ['text' => '🔁 Работа отправлена на доработку — ' . $who . ' внесёт правки.']);
            }
            $c['task_at'] = time();
            unset($c);
            so_history($x, $me, 'Задача → ' . $to);
            so_bump($x);
            return true;
        });
        $labels = ['work' => 'Задача в работе', 'review' => 'Работа сдана на проверку', 'back' => 'Отправлено на доработку'];
        so_reply(true, $labels[$to]);
    }

    /* ===== СЧЕТА PLATEGA ===== */
    case 'invoice': {
        so_need_manager();
        [$o] = so_need_order();
        $amount = round((float)str_replace([' ', ','], ['', '.'], (string)($_POST['amount'] ?? '0')), 2);
        if ($amount < 1 || $amount > 10000000) so_reply(false, 'Сумма — от 1 до 10 000 000 ₽.');
        $desc = so_clip($_POST['desc'] ?? '', 160);
        if (mb_strlen($desc) < 3) $desc = 'Разработка сайта, заказ №' . $o['id'];
        $cfg = so_config();
        $mode = ($_POST['mode'] ?? 'platega') === 'manual' || !so_platega_ready($cfg) ? 'manual' : 'platega';
        $inv_id = base_convert((string)time(), 10, 36) . bin2hex(random_bytes(2));
        $inv = ['id' => $inv_id, 'amount' => $amount, 'desc' => $desc, 'status' => 'pending', 'created' => time(), 'by' => $me, 'tx' => null, 'url' => null];
        if ($mode === 'platega') {
            $base = so_base_url() . '/order.php?o=' . $o['id'] . '&t=' . $o['token'];
            $r = so_platega_create($cfg, $amount, mb_substr('RTeam: ' . $desc, 0, 120), $base . '&paid=' . $inv_id, $base . '&fail=' . $inv_id, 'ord:' . $o['id'] . ':' . $inv_id);
            if (!$r['ok']) so_reply(false, 'Platega: ' . $r['error']);
            $inv['tx'] = $r['tx'];
            $inv['url'] = $r['url'];
        }
        so_order_update($o['id'], function (&$x) use ($inv, $me, $mine) {
            $x['invoices'][] = $inv;
            so_msg_push($x, 'staff', ['who' => $me, 'login' => $me, 'role' => $mine['label'], 'color' => $mine['color'], 'kind' => 'invoice', 'ref' => $inv['id']]);
            so_history($x, $me, 'Счёт ' . so_money($inv['amount']) . ' (' . ($inv['tx'] ? 'Platega' : 'вручную') . ')');
            so_bump($x);
            return true;
        });
        so_log("$me выставил(а) счёт " . so_money($amount) . " по заказу №{$o['id']}" . ($mode === 'platega' ? ' (Platega)' : ' (вручную)'));
        so_reply(true, $mode === 'platega' ? '💳 Счёт Platega отправлен клиенту в чат' : '🧾 Счёт отправлен (оплата по реквизитам, отметьте вручную)');
    }

    case 'inv_check': {
        so_need_manager();
        [$o] = so_need_order();
        $r = so_invoice_check($o['id'], (string)($_POST['inv'] ?? ''), $me);
        so_reply($r['ok'], $r['msg']);
    }

    case 'inv_paid':
    case 'inv_cancel': {
        so_need_manager();
        [$o] = so_need_order();
        $inv = (string)($_POST['inv'] ?? '');
        $ok = so_invoice_apply($o['id'], $inv, $op === 'inv_paid' ? 'paid' : 'cancelled', $me);
        if ($ok && $op === 'inv_paid') so_log("$me вручную отметил(а) оплату счёта по заказу №{$o['id']}");
        so_reply((bool)$ok, $ok ? ($op === 'inv_paid' ? '✅ Счёт отмечен оплаченным' : 'Счёт отменён') : 'Статус счёта уже такой');
    }

    /* ===== ЗАВЕРШИТЬ / ОТМЕНИТЬ / УДАЛИТЬ ===== */
    case 'complete': {
        so_need_manager();
        [$o] = so_need_order();
        so_order_update($o['id'], function (&$x) use ($me) {
            if ($x['status'] === 'done') return false;
            $x['status'] = 'done';
            $x['done_at'] = time();
            if (!empty($x['coder'])) { $x['coder']['task'] = 'done'; $x['coder']['task_at'] = time(); }
            so_msg_push($x, 'system', ['text' => '🎉 Заказ готов и принят! Спасибо, что выбрали RTeam.']);
            if (empty($x['review_id'])) so_msg_push($x, 'system', ['kind' => 'review']);
            so_history($x, $me, 'Заказ завершён');
            so_bump($x);
            return true;
        });
        so_log("$me завершил(а) заказ №{$o['id']}");
        so_reply(true, '🎉 Заказ завершён, клиенту предложено оставить отзыв');
    }

    case 'cancel':
    case 'reopen': {
        so_need_manager();
        [$o] = so_need_order();
        $reason = so_clip($_POST['reason'] ?? '', 300);
        so_order_update($o['id'], function (&$x) use ($op, $me, $reason) {
            if ($op === 'cancel') {
                if ($x['status'] === 'cancel') return false;
                $x['status_before'] = $x['status'];
                $x['status'] = 'cancel';
                so_msg_push($x, 'system', ['text' => 'Заказ отменён' . ($reason !== '' ? ': ' . $reason : '.')]);
            } else {
                $x['status'] = $x['status_before'] ?? (!empty($x['coder']) ? 'work' : 'talk');
                if ($x['status'] === 'cancel' || $x['status'] === 'done') $x['status'] = !empty($x['coder']) ? 'work' : 'talk';
                so_msg_push($x, 'system', ['text' => 'Заказ снова в работе.']);
            }
            so_history($x, $me, $op === 'cancel' ? 'Отменил(а) заказ' : 'Открыл(а) заказ снова');
            so_bump($x);
            return true;
        });
        so_reply(true, $op === 'cancel' ? 'Заказ отменён' : 'Заказ снова открыт');
    }

    case 'delete': {
        so_need_leader();
        [$o] = so_need_order();
        foreach ($o['messages'] as $m) foreach ($m['files'] ?? [] as $f) @unlink(SO_DIR . '/files/' . $f['id'] . '.bin');
        @unlink(so_path(so_order_file($o['id'])));
        @unlink(so_path('live/order_' . (int)$o['id']));
        so_index_del($o['id']);
        so_log("$me удалил(а) заказ №{$o['id']} ({$o['client']['name']})");
        if (empty($_POST['is_ajax'])) $_POST['so_back'] = 'admin.php?tab=orders';
        so_reply(true, 'Заказ удалён');
    }

    /* ===== РАСПИСАНИЕ (только руководители) ===== */
    case 'paint':
    case 'range': {
        so_need_leader();
        $login = (string)($_POST['login'] ?? '');
        if (!isset($staff[$login])) so_reply(false, 'Сотрудник не найден.');
        $type = (string)($_POST['type'] ?? '');
        if ($type !== 'reset' && !isset(so_day_types()[$type])) so_reply(false, 'Неизвестный тип дня.');
        if ($op === 'paint') {
            $dates = array_values(array_filter(array_slice((array)json_decode((string)($_POST['dates'] ?? '[]'), true), 0, 400), 'so_valid_date'));
        } else {
            $from = (string)($_POST['from'] ?? ''); $to = (string)($_POST['to'] ?? '');
            if (!so_valid_date($from) || !so_valid_date($to) || $to < $from) so_reply(false, 'Проверьте даты периода.');
            $dates = so_dates($from, $to, 366);
        }
        if (!$dates) so_reply(false, 'Не выбрано ни одного дня.');
        $result = so_tx('schedule', [], function (&$s) use ($login, $dates, $type) {
            $s += ['week' => [], 'days' => [], 'hours' => []];
            $out = [];
            foreach ($dates as $d) {
                $default = so_week_of($s, $login)[(int)date('N', strtotime($d)) - 1] ? 'work' : 'off';
                if ($type === 'reset' || $type === $default) unset($s['days'][$login][$d]);
                else $s['days'][$login][$d] = $type;
                $out[$d] = ['type' => $s['days'][$login][$d] ?? $default, 'ovr' => isset($s['days'][$login][$d])];
            }
            if (empty($s['days'][$login])) unset($s['days'][$login]);
            return $out;
        });
        $tn = $type === 'reset' ? 'по графику' : so_day_types()[$type][0];
        if (count($dates) > 1 || $op === 'range') so_log("$me: $login — «{$tn}» на " . count($dates) . ' дн. (' . reset($dates) . ' — ' . end($dates) . ')', 'schedule');
        so_reply(true, $login . ': ' . count($dates) . ' дн. → ' . $tn, ['days' => $result]);
    }

    case 'week': {
        so_need_leader();
        $login = (string)($_POST['login'] ?? '');
        if (!isset($staff[$login])) so_reply(false, 'Сотрудник не найден.');
        $on = array_map('intval', (array)($_POST['wd'] ?? []));
        $week = [];
        for ($i = 0; $i < 7; $i++) $week[] = in_array($i, $on, true) ? 1 : 0;
        $hours = so_clip($_POST['hours'] ?? '', 40);
        so_tx('schedule', [], function (&$s) use ($login, $week, $hours) {
            $s += ['week' => [], 'days' => [], 'hours' => []];
            $s['week'][$login] = $week;
            if ($hours !== '') $s['hours'][$login] = $hours; else unset($s['hours'][$login]);
            return true;
        });
        so_log("$me изменил(а) график $login: " . implode('', $week) . ($hours !== '' ? ", $hours" : ''), 'schedule');
        so_reply(true, 'График ' . $login . ' сохранён');
    }

    /* ===== НАСТРОЙКИ (только руководители) ===== */
    case 'cfg_keys': {
        so_need_leader();
        $id = trim((string)($_POST['platega_id'] ?? ''));
        $secret = trim((string)($_POST['platega_secret'] ?? ''));
        $method = (int)($_POST['platega_method'] ?? 2);
        if (!isset(so_platega_methods()[$method])) $method = 2;
        $clear = !empty($_POST['platega_clear']);
        if (strlen($id) > 120 || strlen($secret) > 600) so_reply(false, 'Слишком длинное значение ключа.');
        so_config_update(function (&$c) use ($id, $secret, $method, $clear, $me) {
            if ($clear) { $c['platega_id'] = ''; $c['platega_secret'] = ''; }
            else {
                if ($id !== '') $c['platega_id'] = $id;
                if ($secret !== '') $c['platega_secret'] = $secret;
            }
            $c['platega_method'] = $method;
            $c['platega_set_by'] = $me;
            $c['platega_set_at'] = time();
            return true;
        });
        so_log($clear ? "$me удалил(а) ключи Platega для заказов сайтов" : "$me сохранил(а) ключи Platega для заказов сайтов", 'orders');
        $ready = so_platega_ready(so_config(true));
        so_reply(true, $clear ? 'Ключи Platega удалены' : ($ready ? '✅ Platega подключена — счета будут создаваться автоматически' : 'Сохранено. Для оплаты нужны и Merchant ID, и секрет.'));
    }

    case 'cfg_test': {
        so_need_leader();
        $cfg = so_config();
        if (!so_platega_ready($cfg)) so_reply(false, 'Сначала впишите Merchant ID и секрет.');
        $r = so_platega_http($cfg, 'GET', '/transaction/00000000-0000-0000-0000-000000000000');
        if ($r['error'] !== '') so_reply(false, $r['error']);
        if ($r['code'] === 401 || $r['code'] === 403) so_reply(false, 'Platega не приняла ключи (код ' . $r['code'] . '). Проверьте Merchant ID и секрет.');
        so_reply(true, '✅ Связь с Platega есть, ключи приняты (ответ ' . $r['code'] . ').');
    }

    case 'cfg_main': {
        so_need_leader();
        $roles = array_keys(rt_roles());
        $mr = array_values(array_intersect((array)($_POST['manager_roles'] ?? []), $roles));
        $cr = array_values(array_intersect((array)($_POST['coder_roles'] ?? []), $roles));
        $cat = so_catalog();
        $prices = ['types' => [], 'features' => []];
        foreach ($cat['types'] as $k => $t) if (isset($_POST['price_t'][$k])) $prices['types'][$k] = max(0, min(10000000, (int)$_POST['price_t'][$k]));
        foreach ($cat['features'] as $k => $f) if (isset($_POST['price_f'][$k])) $prices['features'][$k] = max(0, min(10000000, (int)$_POST['price_f'][$k]));
        so_config_update(function (&$c) use ($mr, $cr, $prices) {
            $c['manager_roles'] = $mr;
            $c['coder_roles'] = $cr;
            $c['reviews_auto'] = !empty($_POST['reviews_auto']);
            $c['orders_open'] = !empty($_POST['orders_open']);
            $c['reply_hint'] = so_clip($_POST['reply_hint'] ?? '', 120) ?: 'Обычно отвечаем в течение 15 минут';
            $c['prices'] = $prices;
            return true;
        });
        so_log("$me изменил(а) настройки заказов сайтов", 'orders');
        so_reply(true, 'Настройки сохранены');
    }

    /* ===== ОТЗЫВЫ ===== */
    case 'rv_status':
    case 'rv_reply':
    case 'rv_delete': {
        so_need_manager();
        $id = (int)($_POST['id'] ?? 0);
        $status = (string)($_POST['status'] ?? '');
        $reply = so_clip($_POST['reply'] ?? '', 1500);
        if ($op === 'rv_status' && !in_array($status, ['published', 'hidden', 'pending'], true)) so_reply(false, 'Неизвестный статус.');
        $ok = so_reviews_update(function (&$d) use ($op, $id, $status, $reply, $me) {
            foreach ($d['items'] as $i => &$r) {
                if ((int)$r['id'] !== $id) continue;
                if ($op === 'rv_status') $r['status'] = $status;
                elseif ($op === 'rv_reply') $r['reply'] = $reply !== '' ? ['text' => $reply, 'by' => $me, 't' => time()] : null;
                else { unset($d['items'][$i]); $d['items'] = array_values($d['items']); }
                return true;
            }
            return false;
        });
        $msgs = ['rv_status' => ['published' => '⭐ Отзыв опубликован', 'hidden' => 'Отзыв скрыт с сайта', 'pending' => 'Отзыв возвращён на модерацию'][$status] ?? 'Готово',
                 'rv_reply' => $reply !== '' ? 'Ответ команды сохранён' : 'Ответ удалён', 'rv_delete' => 'Отзыв удалён'];
        so_reply((bool)$ok, $ok ? $msgs[$op] : 'Отзыв не найден');
    }

    }
    so_reply(false, 'Неизвестное действие.');
}

/* ==========================================================
   ИНТЕРФЕЙС
   ========================================================== */

function so_admin_render($tab) {
    so_admin_css();
    if ($tab === 'schedule') so_render_schedule();
    elseif ($tab === 'orders') so_render_orders();
    elseif ($tab === 'reviews') so_render_reviews();
}

/** Небольшая карточка «Мои задачи» на главной админки. */
function so_home_card() {
    $tasks = array_values(array_filter(so_tasks_of(so_me()), function ($t) { return $t['status'] !== 'done' && $t['task'] !== 'done'; }));
    if (!$tasks) return;
    $ts = so_task_statuses();
    echo '<div class="card"><h3>📅 Мои задачи <span class="chip">' . count($tasks) . '</span></h3><ul class="list">';
    foreach (array_slice($tasks, 0, 6) as $t) {
        $late = $t['deadline'] !== '' && $t['deadline'] < date('Y-m-d');
        echo '<li><span><a href="?tab=orders&amp;o=' . (int)$t['id'] . '"><b>№' . (int)$t['id'] . ' · ' . so_h($t['title']) . '</b></a><br><span class="muted" style="font-size:12px;">'
           . so_h(so_dshort($t['start'])) . ' — ' . so_h(so_dshort($t['deadline'])) . '</span></span>'
           . '<span class="badge ' . ($late ? 'badge-dec' : 'badge-new') . '">' . ($late ? 'просрочено' : so_h($ts[$t['task']][0] ?? $t['task'])) . '</span></li>';
    }
    echo '</ul><a class="btn ghost sm" style="margin-top:12px;" href="?tab=schedule">Открыть календарь →</a></div>';
}
