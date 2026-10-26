<?php
/* ==========================================================
   API СТРАНИЦЫ ЗАКАЗА (order.php) — всё, что делает клиент:
   создать заказ, получать новые сообщения без перезагрузки,
   писать в чат, выбрать кодера, проверить оплату, оставить отзыв.
   Доступ к заказу — по секретной ссылке (o + t) или по аккаунту сайта.
   ========================================================== */
session_start();
$account = isset($_SESSION['user']) && is_string($_SESSION['user']) && $_SESSION['user'] !== '' ? $_SESSION['user'] : null;
session_write_close();  // не держим сессию, пока чат опрашивает сервер

require_once __DIR__ . '/_orders_lib.php';

header('Content-Type: application/json; charset=utf-8');
header('X-Content-Type-Options: nosniff');
header('Cache-Control: no-store');

function api_out(array $a, $code = 200) {
    http_response_code($code);
    echo json_encode($a, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES | JSON_INVALID_UTF8_SUBSTITUTE);
    exit;
}
function api_fail($msg, $code = 400) {
    api_out(['ok' => false, 'error' => $msg], $code);
}

$a = (string)($_GET['a'] ?? $_POST['a'] ?? '');
$is_post = ($_SERVER['REQUEST_METHOD'] ?? 'GET') === 'POST';

// Защита от подделки запросов с чужих сайтов: наши fetch-запросы шлют этот заголовок,
// а обычная форма с другого сайта так не умеет.
if ($is_post && ($_SERVER['HTTP_X_SO'] ?? '') !== '1') api_fail('Обновите страницу и попробуйте ещё раз.', 403);

/** Заказ, к которому у посетителя есть доступ: секретная ссылка или свой аккаунт. */
function api_order($account) {
    $id = (int)($_REQUEST['o'] ?? 0);
    $t  = (string)($_REQUEST['t'] ?? '');
    $o  = $id > 0 ? so_order_load($id) : null;
    if (!$o) api_fail('Заказ не найден', 404);
    if ($t !== '' && hash_equals((string)$o['token'], $t)) return $o;
    if ($account !== null && ($o['client']['account'] ?? null) === $account) return $o;
    api_fail('Нет доступа к этому заказу', 403);
}

function api_messages(array $o, $after) {
    $out = [];
    foreach ($o['messages'] as $m) {
        if ($m['id'] <= $after || !empty($m['internal'])) continue;
        $out[] = so_msg_public($m);
    }
    return $out;
}

switch ($a) {

/* ---------- НОВЫЙ ЗАКАЗ ---------- */
case 'create': {
    if (!$is_post) api_fail('Нужен POST', 405);
    $cfg = so_config();
    if (empty($cfg['orders_open'])) api_fail('Приём заказов временно приостановлен. Загляните чуть позже!');
    if (trim((string)($_POST['website'] ?? '')) !== '') api_out(['ok' => true, 'id' => 0, 'token' => '']); // ловушка для ботов
    $ts = (int)($_POST['ts'] ?? 0);
    if ($ts > 0 && time() - $ts < 3) api_fail('Слишком быстро. Проверьте форму и отправьте ещё раз.');
    $ip = so_ip();
    if (!so_rate_ok('create:' . $ip, 3, 600) || !so_rate_ok('create-day:' . $ip, 12, 86400)) api_fail('Вы уже отправили несколько заявок. Подождите немного или напишите в открытый чат заказа.', 429);

    $cat = so_catalog();
    $type = (string)($_POST['type'] ?? '');
    if (!isset($cat['types'][$type])) api_fail('Выберите тип сайта.');
    $features = array_values(array_unique(array_filter((array)($_POST['features'] ?? []), function ($f) use ($cat) { return is_string($f) && isset($cat['features'][$f]); })));
    $style = (string)($_POST['style'] ?? 'free');
    if (!isset(so_styles()[$style])) $style = 'free';
    $has = array_values(array_intersect(array_filter((array)($_POST['has'] ?? []), 'is_string'), ['design', 'logo', 'content', 'domain']));
    $urgency = (string)($_POST['urgency'] ?? 'normal');
    if (!isset(so_urgency()[$urgency])) $urgency = 'normal';
    $deadline = (string)($_POST['deadline'] ?? '');
    if ($deadline !== '' && (!so_valid_date($deadline) || $deadline < date('Y-m-d'))) $deadline = '';
    $description = so_clip($_POST['description'] ?? '', 5000);
    if (mb_strlen($description) < 15) api_fail('Опишите задачу хотя бы парой предложений — так администратор быстрее посчитает стоимость.');
    $name = so_clip($_POST['name'] ?? '', 60);
    if (mb_strlen($name) < 2) api_fail('Как к вам обращаться?');
    $ctype = (string)($_POST['contact_type'] ?? 'telegram');
    if (!in_array($ctype, ['telegram', 'whatsapp', 'email', 'phone'], true)) $ctype = 'telegram';
    $contact = so_clip($_POST['contact'] ?? '', 120);
    if (mb_strlen($contact) < 3) api_fail('Оставьте контакт для связи.');
    if ($ctype === 'email' && !filter_var($contact, FILTER_VALIDATE_EMAIL)) api_fail('Похоже, в почте опечатка.');

    $o = so_order_create([
        'name' => $name, 'contact_type' => $ctype, 'contact' => $contact, 'account' => $account,
        'type' => $type, 'project' => so_clip($_POST['project'] ?? '', 80), 'features' => $features, 'style' => $style, 'has' => $has,
        'refs' => so_clip($_POST['refs'] ?? '', 1500), 'description' => $description,
        'budget' => max(0, min(10000000, (int)($_POST['budget'] ?? 0))), 'deadline' => $deadline, 'urgency' => $urgency,
    ]);
    api_out(['ok' => true, 'id' => $o['id'], 'token' => $o['token']]);
}

/* ---------- ОБНОВЛЕНИЯ ЧАТА (опрос раз в пару секунд) ---------- */
case 'poll': {
    $o = api_order($account);
    $after = max(0, (int)($_GET['after'] ?? 0));
    $rev = (int)($_GET['rev'] ?? -1);
    $msgs = api_messages($o, $after);
    if (!empty($_GET['seen'])) {
        $last = 0;
        foreach ($o['messages'] as $m) if (empty($m['internal'])) $last = max($last, $m['id']);
        if ($last > (int)($o['seen']['client'] ?? 0)) {
            so_order_update($o['id'], function (&$x) use ($last) { $x['seen']['client'] = max((int)($x['seen']['client'] ?? 0), $last); return true; });
        }
        so_live_touch($o['id'], 'client_online');
    }
    $live = so_live($o['id']);
    $res = [
        'ok' => true,
        'messages' => $msgs,
        'typing' => (time() - (int)($live['staff_typing'] ?? 0)) < 6 ? (string)($live['staff_typing_who'] ?? 'Администратор') : '',
        'staff_online' => (time() - (int)($live['staff_online'] ?? 0)) < 60,
        'staff_seen' => (int)($o['seen']['staff'] ?? 0),
    ];
    if ($rev !== (int)($o['rev'] ?? 0)) $res['state'] = so_client_state($o);
    api_out($res);
}

/* ---------- НАПИСАТЬ В ЧАТ ---------- */
case 'send': {
    if (!$is_post) api_fail('Нужен POST', 405);
    $o = api_order($account);
    if (!so_rate_ok('send:' . $o['id'] . ':' . so_ip(), 25, 60)) api_fail('Слишком много сообщений подряд — подождите минутку.', 429);
    $text = so_clip($_POST['text'] ?? '', 4000);
    [$files, $err] = so_store_uploads('files');
    if ($err !== '') api_fail($err);
    if ($text === '' && !$files) api_fail('Пустое сообщение');
    $msg = so_order_update($o['id'], function (&$x) use ($text, $files) {
        $m = so_msg_push($x, 'client', ['who' => $x['client']['name'], 'text' => $text, 'files' => $files]);
        $x['seen']['client'] = $m['id'];
        if ($x['status'] === 'cancel') { $x['status'] = 'talk'; so_bump($x); so_history($x, $x['client']['name'], 'Клиент снова написал — заказ открыт'); }
        return $m;
    });
    if (!$msg) api_fail('Не удалось отправить, попробуйте ещё раз.', 500);
    api_out(['ok' => true, 'message' => so_msg_public($msg)]);
}

/* ---------- «КЛИЕНТ ПЕЧАТАЕТ…» ---------- */
case 'typing': {
    if (!$is_post) api_fail('Нужен POST', 405);
    $o = api_order($account);
    so_live_touch($o['id'], 'client_typing');
    api_out(['ok' => true]);
}

/* ---------- КЛИЕНТ ВЫБРАЛ КОДЕРА ИЗ ПРЕДЛОЖЕННЫХ ---------- */
case 'choose': {
    if (!$is_post) api_fail('Нужен POST', 405);
    $o = api_order($account);
    $offer_id = (string)($_POST['offer'] ?? '');
    $login = (string)($_POST['coder'] ?? '');
    $sched = so_sched();
    $res = so_order_update($o['id'], function (&$x) use ($offer_id, $login, $sched) {
        $i = so_find_offer($x, $offer_id);
        if ($i === null || $x['offers'][$i]['status'] !== 'open') return ['err' => 'Это предложение уже неактуально.'];
        $snap = null;
        foreach ($x['offers'][$i]['coders'] as $c) if ($c['login'] === $login) $snap = $c;
        if (!$snap) return ['err' => 'Такого кодера нет в предложении.'];
        $start = so_next_workday($sched, $login, date('Y-m-d'));
        $deadline = $x['offers'][$i]['deadline'] ?: so_add_workdays($sched, $login, $start, max(3, (int)($x['estimate']['days'] ?? 7)));
        if ($deadline < $start) $deadline = $start;
        $x['offers'][$i]['status'] = 'chosen';
        $x['offers'][$i]['chosen'] = $login;
        $x['coder'] = ['login' => $login, 'label' => $snap['role'], 'color' => $snap['color'], 'assigned_at' => time(), 'by' => 'клиент',
                       'start' => $start, 'deadline' => $deadline, 'note' => '', 'task' => 'new', 'task_at' => time()];
        $x['status'] = 'assigned';
        so_msg_push($x, 'system', ['text' => 'Вы выбрали кодера ' . $login . '. Задача добавлена в его календарь: с ' . date('d.m', strtotime($start)) . ' по ' . date('d.m.Y', strtotime($deadline)) . '.']);
        so_history($x, $x['client']['name'], 'Клиент выбрал кодера ' . $login);
        so_bump($x);
        return ['ok' => true];
    });
    if (!empty($res['err'])) api_fail($res['err']);
    api_out(['ok' => true]);
}

/* ---------- ПРОВЕРИТЬ ОПЛАТУ (вернулся со страницы Platega) ---------- */
case 'check_pay': {
    if (!$is_post) api_fail('Нужен POST', 405);
    $o = api_order($account);
    if (!so_rate_ok('pay:' . $o['id'] . ':' . so_ip(), 12, 60)) api_fail('Подождите немного и проверьте снова.', 429);
    $r = so_invoice_check($o['id'], (string)($_POST['inv'] ?? ''), 'проверка клиентом');
    api_out(['ok' => $r['ok'], 'msg' => $r['msg'], 'status' => $r['status'] ?? '']);
}

/* ---------- ОТЗЫВ О ЗАВЕРШЁННОМ ЗАКАЗЕ ---------- */
case 'review': {
    if (!$is_post) api_fail('Нужен POST', 405);
    $o = api_order($account);
    if ($o['status'] !== 'done') api_fail('Отзыв можно оставить, когда заказ будет готов.');
    if (!empty($o['review_id'])) api_fail('Вы уже оставили отзыв по этому заказу — спасибо!');
    $rating = (int)($_POST['rating'] ?? 0);
    if ($rating < 1 || $rating > 5) api_fail('Поставьте оценку от 1 до 5 звёзд.');
    $text = so_clip($_POST['text'] ?? '', 2000);
    if (mb_strlen($text) < 10) api_fail('Напишите пару слов о работе — это очень поможет другим клиентам.');
    $name = so_clip($_POST['name'] ?? $o['client']['name'], 60) ?: $o['client']['name'];
    $rv = so_review_add($name, $rating, $text, $o);
    so_order_update($o['id'], function (&$x) use ($rv) {
        $x['review_id'] = $rv['id'];
        so_msg_push($x, 'system', ['text' => '⭐ Спасибо за отзыв! ' . ($rv['status'] === 'published' ? 'Он уже на странице отзывов.' : 'Он появится на сайте после проверки.')]);
        so_bump($x);
        return true;
    });
    api_out(['ok' => true, 'published' => $rv['status'] === 'published']);
}

/* ---------- «МОИ ЗАКАЗЫ»: по сохранённым ссылкам и по аккаунту ---------- */
case 'mine': {
    $list = [];
    $tokens = json_decode((string)($_POST['tokens'] ?? '[]'), true);
    $want = [];
    foreach (array_slice((array)$tokens, 0, 50) as $p) {
        if (is_array($p) && isset($p['o'], $p['t'])) $want[(int)$p['o']] = (string)$p['t'];
    }
    $st = so_statuses();
    $seen = [];
    $add = function ($o) use (&$list, &$seen, $st) {
        if (isset($seen[$o['id']])) return;
        $seen[$o['id']] = 1;
        $list[] = ['id' => $o['id'], 't' => $o['token'], 'title' => so_order_title($o), 'status' => $o['status'],
                   'status_label' => $st[$o['status']][0] ?? $o['status'], 'color' => $st[$o['status']][2] ?? '#888',
                   'created' => $o['created'], 'updated' => $o['updated'], 'unread' => so_unread($o, 'client')];
    };
    foreach ($want as $id => $t) {
        $o = so_order_load($id);
        if ($o && hash_equals((string)$o['token'], $t)) $add($o);
    }
    if ($account !== null) {
        foreach (so_index() as $e) {
            if (($e['account'] ?? null) !== $account || isset($seen[$e['id']])) continue;
            $o = so_order_load($e['id']);
            if ($o) $add($o);
        }
    }
    usort($list, function ($x, $y) { return $y['updated'] <=> $x['updated']; });
    api_out(['ok' => true, 'orders' => $list, 'account' => $account]);
}

/* ---------- ВЛОЖЕНИЕ ИЗ ЧАТА ---------- */
case 'file': {
    $o = api_order($account);
    so_send_file($o, (string)($_GET['f'] ?? ''), false);
}

default:
    api_fail('Неизвестное действие', 404);
}
