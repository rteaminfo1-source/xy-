<?php
/* ==========================================================
   ЗАКАЗЫ САЙТОВ, РАСПИСАНИЕ СОТРУДНИКОВ И ОТЗЫВЫ — общее ядро.
   Подключают: admin.php (через _schedule_admin.php), order.php,
   order_api.php, order_pay_callback.php и reviews.php.

   Где лежат данные: папка _orders_data/ рядом с этим файлом.
   Каждый файл данных — .php с «заглушкой» в первой строке, поэтому,
   даже если открыть его прямой ссылкой, PHP сразу завершится и ничего
   не покажет: ключи Platega, контакты клиентов и переписка наружу не
   попадут ни на Apache, ни на IIS, ни на nginx. Вложения из чата
   хранятся под случайными именами .bin, папка закрыта .htaccess и
   web.config.
   ========================================================== */

if (!defined('SO_DIR')) define('SO_DIR', __DIR__ . '/_orders_data');
const SO_GUARD = "<?php http_response_code(404); exit; ?>\n";
const SO_DEFAULT_WEEK = [1, 1, 1, 1, 1, 0, 0];   // пн‑пт рабочие, сб‑вс выходные
if (!defined('SO_PLATEGA_URL')) define('SO_PLATEGA_URL', 'https://app.platega.io');
const SO_MAX_FILE = 8 * 1024 * 1024;               // 8 МБ на одно вложение

/* ---------- ХРАНИЛИЩЕ ---------- */

function so_boot() {
    static $done = false;
    if ($done) return;
    $done = true;
    foreach ([SO_DIR, SO_DIR . '/orders', SO_DIR . '/files', SO_DIR . '/live'] as $d) {
        if (!is_dir($d)) @mkdir($d, 0755, true);
    }
    if (!is_file(SO_DIR . '/.htaccess')) {
        @file_put_contents(SO_DIR . '/.htaccess', "<IfModule mod_authz_core.c>\n    Require all denied\n</IfModule>\n<IfModule !mod_authz_core.c>\n    Order allow,deny\n    Deny from all\n</IfModule>\n");
    }
    if (!is_file(SO_DIR . '/web.config')) {
        @file_put_contents(SO_DIR . '/web.config', "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<configuration>\n  <system.webServer>\n    <security>\n      <requestFiltering>\n        <fileExtensions allowUnlisted=\"false\" />\n      </requestFiltering>\n    </security>\n  </system.webServer>\n</configuration>\n");
    }
    if (!is_file(SO_DIR . '/index.html')) @file_put_contents(SO_DIR . '/index.html', '');
}

function so_path($name) {
    return SO_DIR . '/' . $name . '.php';
}

function so_decode($raw, $default) {
    if (!is_string($raw) || $raw === '') return $default;
    if (strncmp($raw, '<?php', 5) === 0) {
        $p = strpos($raw, "\n");
        $raw = $p === false ? '' : substr($raw, $p + 1);
    }
    $d = json_decode($raw, true);
    return is_array($d) ? $d : $default;
}

function so_read($name, $default = []) {
    so_boot();
    $f = so_path($name);
    if (!is_file($f)) return $default;
    $fh = @fopen($f, 'rb');
    if (!$fh) return $default;
    flock($fh, LOCK_SH);
    $raw = stream_get_contents($fh);
    flock($fh, LOCK_UN);
    fclose($fh);
    return so_decode($raw, $default);
}

/**
 * Прочитать → изменить → записать под одной блокировкой файла.
 * $fn получает данные по ссылке; если вернёт false — файл не трогаем.
 * Возвращает то, что вернула $fn (или null, если файл не открылся).
 */
function so_tx($name, $default, callable $fn) {
    so_boot();
    $fh = @fopen(so_path($name), 'c+b');
    if (!$fh) return null;
    flock($fh, LOCK_EX);
    $data = so_decode(stream_get_contents($fh), $default);
    $result = $fn($data);
    if ($result !== false) {
        $json = json_encode($data, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES | JSON_INVALID_UTF8_SUBSTITUTE);
        if ($json !== false) {
            ftruncate($fh, 0);
            rewind($fh);
            fwrite($fh, SO_GUARD . $json);
            fflush($fh);
        }
    }
    flock($fh, LOCK_UN);
    fclose($fh);
    return $result;
}

function so_write($name, $data) {
    return so_tx($name, [], function (&$d) use ($data) { $d = $data; return true; });
}

/* ---------- МЕЛОЧИ ---------- */

function so_h($s) {
    return htmlspecialchars((string)$s, ENT_QUOTES, 'UTF-8');
}

function so_ip() {
    if (function_exists('rt_client_ip')) return (string)rt_client_ip();
    return (string)($_SERVER['REMOTE_ADDR'] ?? '0.0.0.0');
}

function so_clip($s, $max) {
    $s = trim(str_replace("\r", '', (string)$s));
    return mb_substr($s, 0, $max);
}

function so_money($n) {
    return number_format((float)$n, 0, ',', ' ') . ' ₽';
}

function so_base_url() {
    $https = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off')
          || strtolower((string)($_SERVER['HTTP_X_FORWARDED_PROTO'] ?? '')) === 'https'
          || (string)($_SERVER['SERVER_PORT'] ?? '') === '443';
    $host = preg_replace('/[^A-Za-z0-9.\-:\[\]]/', '', (string)($_SERVER['HTTP_HOST'] ?? 'rteam.info'));
    $dir  = rtrim(str_replace('\\', '/', dirname((string)($_SERVER['SCRIPT_NAME'] ?? '/index.php'))), '/');
    return ($https ? 'https' : 'http') . '://' . $host . $dir;
}

/** Настройки сайта (settings.json в корне) — нужны публичным страницам ради цвета акцента и названия. */
function so_site_settings() {
    $raw = @file_get_contents(__DIR__ . '/settings.json');
    $s = $raw ? json_decode($raw, true) : null;
    return is_array($s) ? $s : [];
}

function so_accent() {
    $a = (string)(so_site_settings()['accent'] ?? '');
    return preg_match('/^#[0-9a-fA-F]{3,8}$/', $a) ? $a : '#ff2a2a';
}

/** Простейший ограничитель частоты: не больше $max событий за $window секунд на ключ. */
function so_rate_ok($key, $max, $window) {
    $now = time();
    $k = substr(hash('sha256', $key), 0, 24);
    return (bool)so_tx('live/ratelimit', [], function (&$d) use ($k, $max, $window, $now) {
        foreach ($d as $kk => $list) {
            $d[$kk] = array_values(array_filter((array)$list, function ($t) use ($now) { return $t > $now - 86400; }));
            if (!$d[$kk]) unset($d[$kk]);
        }
        $recent = array_filter($d[$k] ?? [], function ($t) use ($now, $window) { return $t > $now - $window; });
        if (count($recent) >= $max) return 0;
        $d[$k][] = $now;
        return 1;
    });
}

/* ---------- НАСТРОЙКИ МОДУЛЯ ---------- */

function so_config($fresh = false) {
    static $cache = null;
    if ($cache !== null && !$fresh) return $cache;
    return $cache = so_read('config', []) + [
        'platega_id'      => '',
        'platega_secret'  => '',
        'platega_method'  => 2,
        'platega_set_by'  => '',
        'platega_set_at'  => 0,
        'manager_roles'   => [],     // пусто — администраторы и руководители (по отделу)
        'coder_roles'     => [],     // пусто — отдел разработчиков и кодеров
        'reviews_auto'    => true,   // отзывы по завершённым заказам публикуются сразу
        'reply_hint'      => 'Обычно отвечаем в течение 15 минут',
        'prices'          => [],
        'orders_open'     => true,
    ];
}

function so_config_update(callable $fn) {
    $r = so_tx('config', [], $fn);
    so_config(true);
    return $r;
}

function so_platega_ready($cfg = null) {
    $cfg = $cfg ?: so_config();
    return trim((string)$cfg['platega_id']) !== '' && trim((string)$cfg['platega_secret']) !== '';
}

function so_platega_methods() {
    return [
        2  => 'СБП (QR-код и оплата из приложения банка)',
        11 => 'Банковские карты',
        0  => 'Покупатель выбирает сам (если включено в кабинете Platega)',
    ];
}

/* ---------- КАТАЛОГ: ТИПЫ САЙТОВ И ФУНКЦИИ ---------- */

function so_catalog() {
    $types = [
        'landing'   => ['Лендинг',                 '🚀', 15000,  7,  'Одна продающая страница: оффер, преимущества, форма заявки'],
        'card'      => ['Сайт-визитка',            '💼', 20000,  10, 'Несколько страниц о вас, услугах и контактах'],
        'corporate' => ['Корпоративный сайт',      '🏢', 45000,  21, 'Компания, услуги, команда, новости, вакансии'],
        'shop'      => ['Интернет-магазин',        '🛍️', 70000,  30, 'Каталог, корзина, оплата, личный кабинет покупателя'],
        'webapp'    => ['Веб-сервис / приложение', '⚙️', 120000, 45, 'Сложная логика, кабинеты, интеграции, API'],
        'redesign'  => ['Редизайн сайта',          '🎨', 25000,  14, 'Новый современный вид для вашего текущего сайта'],
        'other'     => ['Другое',                  '✨', 0,      0,  'Опишите задачу — посчитаем индивидуально'],
    ];
    $features = [
        'admin'      => ['Админ-панель',                 '🛠', 15000, 5],
        'payments'   => ['Онлайн-оплата',                '💳', 10000, 3],
        'account'    => ['Личный кабинет',               '👤', 20000, 7],
        'catalog'    => ['Каталог с фильтрами',          '🗂', 12000, 4],
        'booking'    => ['Онлайн-запись / бронь',        '📅', 12000, 4],
        'blog'       => ['Блог / новости',               '📰', 6000,  2],
        'multilang'  => ['Мультиязычность',              '🌍', 8000,  3],
        'telegram'   => ['Интеграция с Telegram',        '🤖', 7000,  2],
        'seo'        => ['SEO-оптимизация',              '📈', 9000,  3],
        'animations' => ['Анимации и эффекты',           '✨', 6000,  2],
        'chat'       => ['Онлайн-чат на сайте',          '💬', 5000,  1],
        'hosting'    => ['Домен и хостинг под ключ',     '🌐', 3000,  1],
    ];
    $prices = so_config()['prices'];
    $out = ['types' => [], 'features' => []];
    foreach ($types as $k => $t) {
        $p = isset($prices['types'][$k]) ? (int)$prices['types'][$k] : $t[2];
        $out['types'][$k] = ['name' => $t[0], 'icon' => $t[1], 'price' => $p, 'days' => $t[3], 'desc' => $t[4]];
    }
    foreach ($features as $k => $f) {
        $p = isset($prices['features'][$k]) ? (int)$prices['features'][$k] : $f[2];
        $out['features'][$k] = ['name' => $f[0], 'icon' => $f[1], 'price' => $p, 'days' => $f[3]];
    }
    return $out;
}

function so_styles() {
    return ['minimal' => 'Минимализм', 'dark' => 'Тёмный, технологичный', 'bright' => 'Яркий и смелый',
            'corporate' => 'Строгий деловой', 'premium' => 'Премиум / люкс', 'free' => 'На ваш вкус'];
}

function so_urgency() {
    return ['relax' => ['Не горит', 1.0, 1.15], 'normal' => ['Обычные сроки', 1.0, 1.0], 'urgent' => ['Срочно', 1.3, 0.6]];
}

/** Ориентировочная цена и срок — та же формула, что и в форме на order.php. */
function so_estimate($type, array $features, $urgency) {
    $cat = so_catalog();
    $t = $cat['types'][$type] ?? $cat['types']['other'];
    $price = $t['price'];
    $days  = $t['days'];
    foreach ($features as $f) {
        if (isset($cat['features'][$f])) { $price += $cat['features'][$f]['price']; $days += $cat['features'][$f]['days']; }
    }
    $u = so_urgency()[$urgency] ?? so_urgency()['normal'];
    $price = $price * $u[1];
    $days  = $days ? max(3, (int)round($days * $u[2])) : 0;
    $min = (int)(round($price / 1000) * 1000);
    $max = (int)(round($price * 1.4 / 1000) * 1000);
    return ['min' => $min, 'max' => $max, 'days' => $days];
}

/* ---------- ЗАКАЗЫ ---------- */

function so_statuses() {
    return [
        'new'      => ['Новый',          '🆕', '#3b82f6'],
        'talk'     => ['Обсуждение',     '💬', '#8b5cf6'],
        'choose'   => ['Выбор кодера',   '🧑‍💻', '#06b6d4'],
        'assigned' => ['Кодер назначен', '📌', '#14b8a6'],
        'work'     => ['В работе',       '⚙️', '#f59e0b'],
        'review'   => ['На проверке',    '🔍', '#ec4899'],
        'done'     => ['Готов',          '✅', '#22c55e'],
        'cancel'   => ['Отменён',        '✖',  '#6b7280'],
    ];
}

function so_task_statuses() {
    return [
        'new'    => ['Новая задача',      '#3b82f6'],
        'work'   => ['В работе',          '#f59e0b'],
        'review' => ['Сдана на проверку', '#ec4899'],
        'done'   => ['Принята',           '#22c55e'],
    ];
}

function so_order_file($id) {
    return 'orders/order_' . (int)$id;
}

function so_order_load($id) {
    $o = so_read(so_order_file($id), []);
    return !empty($o['id']) ? $o : null;
}

/** Изменить заказ под блокировкой. $fn(&$order) — вернуть false, чтобы ничего не сохранять. */
function so_order_update($id, callable $fn) {
    $saved = null;
    $r = so_tx(so_order_file($id), [], function (&$o) use ($fn, &$saved) {
        if (empty($o['id'])) return false;
        $r = $fn($o);
        if ($r === false) return false;
        $o['updated'] = time();
        $saved = $o;
        return $r === null ? true : $r;
    });
    if ($saved) so_index_put($saved);
    return $r;
}

/* Индекс заказов: короткая сводка по каждому заказу в одном файле —
   меню, счётчики, список и календари читают только его. */
function so_index_entry(array $o) {
    $last = end($o['messages']) ?: null;
    $last_pub = null;
    foreach (array_reverse($o['messages']) as $m) { if (empty($m['internal'])) { $last_pub = $m; break; } }
    $pending = 0; $pending_sum = 0;
    foreach ($o['invoices'] ?? [] as $inv) if (($inv['status'] ?? '') === 'pending') { $pending++; $pending_sum += (float)$inv['amount']; }
    return [
        'id'       => (int)$o['id'],
        'title'    => so_order_title($o),
        'type'     => $o['brief']['type'] ?? 'other',
        'client'   => $o['client']['name'] ?? '',
        'account'  => $o['client']['account'] ?? null,
        'status'   => $o['status'],
        'manager'  => $o['manager'] ?? null,
        'coder'    => $o['coder']['login'] ?? null,
        'task'     => $o['coder']['task'] ?? null,
        'start'    => $o['coder']['start'] ?? '',
        'deadline' => $o['coder']['deadline'] ?? '',
        'want'     => $o['brief']['deadline'] ?? '',
        'created'  => (int)$o['created'],
        'act'      => (int)($o['act'] ?? $o['created']),
        'last_t'   => $last ? (int)$last['t'] : (int)$o['created'],
        'last_from' => $last['from'] ?? '',
        'last_text' => $last ? mb_substr(($last['internal'] ? '🔒 ' : '') . ($last['text'] !== '' ? $last['text'] : (!empty($last['files']) ? '📎 файл' : '')), 0, 90) : '',
        'unread_staff' => so_unread($o, 'staff'),
        'unread_coder' => so_unread($o, 'coder'),
        'unread_client' => so_unread($o, 'client'),
        'pending'  => $pending,
        'pending_sum' => $pending_sum,
        'paid'     => so_paid_total($o),
        'paid_list' => array_values(array_map(function ($i) { return [(int)($i['paid_at'] ?? 0), (float)$i['amount']]; },
                          array_filter($o['invoices'] ?? [], function ($i) { return ($i['status'] ?? '') === 'paid'; }))),
        'done_at'  => (int)($o['done_at'] ?? 0),
        'estimate' => $o['estimate']['min'] ?? 0,
        'open_offer' => (bool)array_filter($o['offers'] ?? [], function ($of) { return $of['status'] === 'open'; }),
        'last_pub_from' => $last_pub['from'] ?? '',
    ];
}

function so_index_put(array $o) {
    $e = so_index_entry($o);
    so_tx('orders/index', [], function (&$d) use ($e) { $d[(string)$e['id']] = $e; return true; });
}

function so_index_del($id) {
    so_tx('orders/index', [], function (&$d) use ($id) { unset($d[(string)(int)$id]); return true; });
}

function so_index($fresh = false) {
    static $cache = null;
    if ($cache !== null && !$fresh) return $cache;
    so_boot();
    if (!is_file(so_path('orders/index'))) {
        $d = [];
        foreach (so_order_ids() as $id) { $o = so_order_load($id); if ($o) $d[(string)$id] = so_index_entry($o); }
        so_write('orders/index', $d);
        return $cache = $d;
    }
    return $cache = so_read('orders/index', []);
}

function so_order_ids() {
    so_boot();
    $ids = [];
    foreach ((array)glob(SO_DIR . '/orders/order_*.php') as $f) {
        if (preg_match('/order_(\d+)\.php$/', $f, $m)) $ids[] = (int)$m[1];
    }
    rsort($ids);
    return $ids;
}

function so_orders_all() {
    $out = [];
    foreach (so_order_ids() as $id) {
        $o = so_order_load($id);
        if ($o) $out[] = $o;
    }
    return $out;
}

function so_next_order_id() {
    return (int)so_tx('meta', ['order_seq' => 1000], function (&$m) {
        $m['order_seq'] = (int)($m['order_seq'] ?? 1000) + 1;
        return $m['order_seq'];
    });
}

function so_order_title($o) {
    $cat = so_catalog();
    $type = $cat['types'][$o['brief']['type'] ?? 'other']['name'] ?? 'Сайт';
    $project = (string)($o['brief']['project'] ?? '');
    return $project !== '' ? $project : $type;
}

/** Создать заказ из формы. $in — уже проверенные поля. Возвращает заказ. */
function so_order_create(array $in) {
    $id = so_next_order_id();
    $now = time();
    $o = [
        'id'      => $id,
        'token'   => bin2hex(random_bytes(16)),
        'created' => $now,
        'updated' => $now,
        'rev'     => 1,
        'status'  => 'new',
        'client'  => [
            'name'    => $in['name'],
            'contact_type' => $in['contact_type'],
            'contact' => $in['contact'],
            'account' => $in['account'] ?? null,
            'ip'      => so_ip(),
        ],
        'brief'   => [
            'type'        => $in['type'],
            'project'     => $in['project'],
            'features'    => $in['features'],
            'style'       => $in['style'],
            'has'         => $in['has'],
            'refs'        => $in['refs'],
            'description' => $in['description'],
            'budget'      => $in['budget'],
            'deadline'    => $in['deadline'],
            'urgency'     => $in['urgency'],
        ],
        'estimate' => so_estimate($in['type'], $in['features'], $in['urgency']),
        'messages' => [],
        'msg_seq'  => 0,
        'seen'     => ['client' => 0, 'staff' => 0, 'coder' => 0],
        'manager'  => null,
        'coder'    => null,
        'offers'   => [],
        'invoices' => [],
        'review_id' => null,
        'history'  => [['t' => $now, 'who' => $in['name'], 'what' => 'Заказ создан']],
    ];
    so_msg_push($o, 'system', ['text' => 'Заказ №' . $id . ' создан. Администратор уже получил уведомление и скоро ответит здесь, в этом чате.']);
    so_write(so_order_file($id), $o);
    so_index_put($o);
    return $o;
}

/** Добавить сообщение в уже загруженный заказ (сохранять — вызывающему). */
function so_msg_push(array &$o, $from, array $m) {
    $o['msg_seq'] = (int)($o['msg_seq'] ?? 0) + 1;
    $msg = [
        'id'       => $o['msg_seq'],
        'from'     => $from,                        // client | staff | system
        'who'      => (string)($m['who'] ?? ''),
        'login'    => $m['login'] ?? null,
        'role'     => (string)($m['role'] ?? ''),
        'color'    => (string)($m['color'] ?? ''),
        'text'     => (string)($m['text'] ?? ''),
        'files'    => array_values((array)($m['files'] ?? [])),
        'internal' => !empty($m['internal']),
        'kind'     => (string)($m['kind'] ?? 'text'),  // text | invoice | offer | review | system
        'ref'      => $m['ref'] ?? null,
        't'        => time(),
    ];
    $o['messages'][] = $msg;
    $o['act'] = $msg['t'];
    return $msg;
}

function so_history(array &$o, $who, $what) {
    $o['history'][] = ['t' => time(), 'who' => (string)$who, 'what' => (string)$what];
    if (count($o['history']) > 300) $o['history'] = array_slice($o['history'], -300);
}

function so_bump(array &$o) {
    $o['rev'] = (int)($o['rev'] ?? 0) + 1;
    $o['act'] = time();
}

function so_find_invoice(array $o, $inv_id) {
    foreach ($o['invoices'] ?? [] as $i => $inv) if ((string)$inv['id'] === (string)$inv_id) return $i;
    return null;
}

function so_find_offer(array $o, $offer_id) {
    foreach ($o['offers'] ?? [] as $i => $of) if ((string)$of['id'] === (string)$offer_id) return $i;
    return null;
}

function so_paid_total(array $o) {
    $s = 0;
    foreach ($o['invoices'] ?? [] as $inv) if (($inv['status'] ?? '') === 'paid') $s += (float)$inv['amount'];
    return $s;
}

/**
 * Сколько непрочитанных сообщений у стороны: client — ответы команды,
 * staff (администратор) — сообщения клиента и кодера, coder — клиента и администраторов.
 */
function so_unread(array $o, $side) {
    $seen = (int)($o['seen'][$side] ?? 0);
    $coder = $o['coder']['login'] ?? null;
    $n = 0;
    foreach ($o['messages'] ?? [] as $m) {
        if ($m['id'] <= $seen || $m['from'] === 'system') continue;
        $by_coder = $coder !== null && ($m['login'] ?? null) === $coder;
        if ($side === 'client') { if ($m['from'] === 'staff' && empty($m['internal'])) $n++; }
        elseif ($side === 'staff') { if ($m['from'] === 'client' || $by_coder) $n++; }
        elseif ($m['from'] === 'client' || ($m['from'] === 'staff' && !$by_coder)) $n++;
    }
    return $n;
}

/** Сообщение для клиента: без внутренних заметок и служебных полей. */
function so_msg_public(array $m) {
    $files = [];
    foreach ($m['files'] ?? [] as $f) $files[] = ['id' => $f['id'], 'name' => $f['name'], 'size' => $f['size'], 'img' => !empty($f['img'])];
    return ['id' => $m['id'], 'from' => $m['from'], 'who' => $m['who'], 'role' => $m['role'], 'color' => $m['color'],
            'text' => $m['text'], 'files' => $files, 'kind' => $m['kind'], 'ref' => $m['ref'], 't' => $m['t']];
}

/** Состояние заказа, которое видит клиент (статус, кодер, счета, предложения кодеров). */
function so_client_state(array $o) {
    $st = so_statuses();
    $cat = so_catalog();
    $invoices = [];
    foreach ($o['invoices'] ?? [] as $inv) {
        $invoices[] = ['id' => $inv['id'], 'amount' => (float)$inv['amount'], 'desc' => $inv['desc'], 'status' => $inv['status'],
                       'created' => $inv['created'], 'paid_at' => $inv['paid_at'] ?? null,
                       'url' => ($inv['status'] === 'pending') ? ($inv['url'] ?? '') : '', 'manual' => empty($inv['tx'])];
    }
    $offers = [];
    foreach ($o['offers'] ?? [] as $of) {
        $offers[] = ['id' => $of['id'], 'status' => $of['status'], 'chosen' => $of['chosen'] ?? null, 'deadline' => $of['deadline'] ?? '',
                     'note' => $of['note'] ?? '', 'coders' => $of['coders']];
    }
    $coder = null;
    if (!empty($o['coder'])) {
        $c = $o['coder'];
        $coder = ['name' => $c['login'], 'role' => $c['label'] ?? '', 'color' => $c['color'] ?? '', 'task' => $c['task'] ?? 'new',
                  'start' => $c['start'] ?? '', 'deadline' => $c['deadline'] ?? ''];
    }
    $type = $o['brief']['type'] ?? 'other';
    return [
        'id'        => $o['id'],
        'rev'       => (int)($o['rev'] ?? 0),
        'status'    => $o['status'],
        'status_label' => $st[$o['status']][0] ?? $o['status'],
        'created'   => $o['created'],
        'title'     => so_order_title($o),
        'type'      => ['key' => $type, 'name' => $cat['types'][$type]['name'] ?? $type, 'icon' => $cat['types'][$type]['icon'] ?? '✨'],
        'features'  => array_values(array_map(function ($f) use ($cat) { return $cat['features'][$f]['name'] ?? $f; }, $o['brief']['features'] ?? [])),
        'estimate'  => $o['estimate'] ?? null,
        'deadline'  => $o['brief']['deadline'] ?? '',
        'client'    => ['name' => $o['client']['name']],
        'coder'     => $coder,
        'invoices'  => $invoices,
        'offers'    => $offers,
        'paid'      => so_paid_total($o),
        'review'    => !empty($o['review_id']),
        'staff_seen' => (int)($o['seen']['staff'] ?? 0),
    ];
}

/* ---------- ЖИВОЙ ЧАТ: «печатает…», кто в сети ---------- */

function so_live_touch($id, $field, $who = '') {
    so_tx('live/order_' . (int)$id, [], function (&$d) use ($field, $who) {
        $d[$field] = time();
        if ($who !== '') $d[$field . '_who'] = $who;
        return true;
    });
}

function so_live($id) {
    return so_read('live/order_' . (int)$id, []);
}

/* ---------- ВЛОЖЕНИЯ ---------- */

function so_file_exts() {
    return ['jpg' => 'image/jpeg', 'jpeg' => 'image/jpeg', 'png' => 'image/png', 'gif' => 'image/gif', 'webp' => 'image/webp',
            'pdf' => 'application/pdf', 'txt' => 'text/plain', 'zip' => 'application/zip', 'rar' => 'application/vnd.rar',
            '7z' => 'application/x-7z-compressed', 'doc' => 'application/msword',
            'docx' => 'application/vnd.openxmlformats-officedocument.wordprocessingml.document',
            'xls' => 'application/vnd.ms-excel', 'xlsx' => 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet',
            'pptx' => 'application/vnd.openxmlformats-officedocument.presentationml.presentation',
            'psd' => 'image/vnd.adobe.photoshop', 'fig' => 'application/octet-stream'];
}

/**
 * Сохранить загруженные файлы ($_FILES['files'] с multiple). Возвращает [список файлов, ошибка или ''].
 * Не больше 5 файлов по 8 МБ, только разрешённые типы.
 */
function so_store_uploads($field) {
    if (empty($_FILES[$field]) || !is_array($_FILES[$field]['name'] ?? null)) {
        if (!empty($_FILES[$field]['name']) && is_string($_FILES[$field]['name'])) {
            $one = $_FILES[$field];
            $_FILES[$field] = ['name' => [$one['name']], 'type' => [$one['type']], 'tmp_name' => [$one['tmp_name']], 'error' => [$one['error']], 'size' => [$one['size']]];
        } else {
            return [[], ''];
        }
    }
    $F = $_FILES[$field];
    $out = [];
    $exts = so_file_exts();
    $n = count($F['name']);
    if ($n > 5) return [[], 'Не больше 5 файлов за раз.'];
    for ($i = 0; $i < $n; $i++) {
        if (($F['error'][$i] ?? UPLOAD_ERR_NO_FILE) === UPLOAD_ERR_NO_FILE) continue;
        if ($F['error'][$i] !== UPLOAD_ERR_OK) return [[], 'Файл «' . $F['name'][$i] . '» не загрузился (слишком большой?).'];
        if ($F['size'][$i] > SO_MAX_FILE) return [[], 'Файл «' . $F['name'][$i] . '» больше 8 МБ.'];
        $orig = (string)$F['name'][$i];
        $ext = strtolower(pathinfo($orig, PATHINFO_EXTENSION));
        if (!isset($exts[$ext])) return [[], 'Тип файла «' . $orig . '» не поддерживается.'];
        $tmp = $F['tmp_name'][$i];
        if (!is_uploaded_file($tmp)) return [[], 'Ошибка загрузки файла.'];
        $is_img = in_array($ext, ['jpg', 'jpeg', 'png', 'gif', 'webp'], true);
        if ($is_img && @getimagesize($tmp) === false) return [[], 'Файл «' . $orig . '» не похож на картинку.'];
        $fid = bin2hex(random_bytes(16));
        if (!@move_uploaded_file($tmp, SO_DIR . '/files/' . $fid . '.bin')) return [[], 'Не удалось сохранить файл на сервере.'];
        $safe = preg_replace('/[\x00-\x1f"\\\\\/<>|:*?]/u', '_', $orig);
        $out[] = ['id' => $fid, 'name' => mb_substr($safe, 0, 120), 'size' => (int)$F['size'][$i], 'mime' => $exts[$ext], 'img' => $is_img];
    }
    return [$out, ''];
}

/** Найти файл в переписке заказа и отдать его браузеру. */
function so_send_file(array $o, $fid, $allow_internal) {
    if (!preg_match('/^[0-9a-f]{32}$/', (string)$fid)) { http_response_code(404); exit; }
    $meta = null;
    foreach ($o['messages'] ?? [] as $m) {
        if (!empty($m['internal']) && !$allow_internal) continue;
        foreach ($m['files'] ?? [] as $f) if ($f['id'] === $fid) $meta = $f;
    }
    $path = SO_DIR . '/files/' . $fid . '.bin';
    if (!$meta || !is_file($path)) { http_response_code(404); echo 'Файл не найден'; exit; }
    $inline = !empty($meta['img']) && in_array($meta['mime'], ['image/jpeg', 'image/png', 'image/gif', 'image/webp'], true);
    if (function_exists('session_status') && session_status() === PHP_SESSION_ACTIVE) session_write_close();
    header('Content-Type: ' . ($inline ? $meta['mime'] : 'application/octet-stream'));
    header('Content-Length: ' . filesize($path));
    header('X-Content-Type-Options: nosniff');
    header("Content-Security-Policy: default-src 'none'; img-src 'self'; style-src 'unsafe-inline'; sandbox");
    header('Cache-Control: private, max-age=86400');
    header('Content-Disposition: ' . ($inline ? 'inline' : 'attachment') . '; filename="' . preg_replace('/[^A-Za-z0-9._-]/', '_', $meta['name']) . '"; filename*=UTF-8\'\'' . rawurlencode($meta['name']));
    readfile($path);
    exit;
}

/* ---------- РАСПИСАНИЕ СОТРУДНИКОВ ---------- */

function so_day_types() {
    return [
        'work'     => ['Рабочий',    '💼', '#22c55e'],
        'off'      => ['Выходной',   '🌙', '#64748b'],
        'vacation' => ['Отпуск',     '🌴', '#f59e0b'],
        'sick'     => ['Больничный', '🤒', '#ef4444'],
    ];
}

function so_sched() {
    $s = so_read('schedule', []);
    return $s + ['week' => [], 'days' => [], 'hours' => [], 'notes' => []];
}

function so_week_of($sched, $login) {
    $w = $sched['week'][$login] ?? SO_DEFAULT_WEEK;
    return is_array($w) && count($w) === 7 ? array_map('intval', $w) : SO_DEFAULT_WEEK;
}

function so_day_type($sched, $login, $date) {
    if (isset($sched['days'][$login][$date])) return $sched['days'][$login][$date];
    $dow = (int)date('N', strtotime($date)) - 1;
    return so_week_of($sched, $login)[$dow] ? 'work' : 'off';
}

function so_dates($from, $to, $limit = 400) {
    $out = [];
    $t = strtotime($from);
    $end = strtotime($to);
    if ($t === false || $end === false) return $out;
    while ($t <= $end && count($out) < $limit) {
        $out[] = date('Y-m-d', $t);
        $t = strtotime('+1 day', $t);
    }
    return $out;
}

function so_valid_date($d) {
    if (!is_string($d) || !preg_match('/^(\d{4})-(\d{2})-(\d{2})$/', $d, $m)) return false;
    return checkdate((int)$m[2], (int)$m[3], (int)$m[1]);
}

/** Ближайший рабочий день сотрудника начиная с $from (включительно). */
function so_next_workday($sched, $login, $from) {
    foreach (so_dates($from, date('Y-m-d', strtotime($from . ' +180 days'))) as $d) {
        if (so_day_type($sched, $login, $d) === 'work') return $d;
    }
    return $from;
}

/** Дата, когда наберётся $n рабочих дней сотрудника, считая с $from. */
function so_add_workdays($sched, $login, $from, $n) {
    $left = max(1, (int)$n);
    foreach (so_dates($from, date('Y-m-d', strtotime($from . ' +400 days'))) as $d) {
        if (so_day_type($sched, $login, $d) === 'work' && --$left <= 0) return $d;
    }
    return date('Y-m-d', strtotime($from . ' +' . (int)$n . ' days'));
}

/** Задачи сотрудника — записи индекса заказов, где он кодер (кроме отменённых). */
function so_tasks_of($login, $index = null) {
    $index = $index ?? so_index();
    $out = [];
    foreach ($index as $e) {
        if (($e['coder'] ?? null) !== $login || $e['status'] === 'cancel') continue;
        $out[] = $e;
    }
    usort($out, function ($a, $b) { return strcmp($a['deadline'] ?: '9999', $b['deadline'] ?: '9999'); });
    return $out;
}

/** Свободен ли сотрудник: рабочие дни, отпуск и загрузка в окне [from, to]. */
function so_availability($sched, $login, $from, $to, $index = null) {
    $work = 0; $vac = 0; $off = 0;
    foreach (so_dates($from, $to, 200) as $d) {
        $t = so_day_type($sched, $login, $d);
        if ($t === 'work') $work++;
        elseif ($t === 'vacation' || $t === 'sick') $vac++;
        else $off++;
    }
    $busy = 0;
    foreach (so_tasks_of($login, $index) as $t) {
        if ($t['task'] === 'done' || $t['status'] === 'done') continue;
        if ($t['start'] === '' || $t['deadline'] === '' || ($t['start'] <= $to && $t['deadline'] >= $from)) $busy++;
    }
    $today = date('Y-m-d');
    return ['work' => $work, 'vacation' => $vac, 'off' => $off, 'busy' => $busy,
            'next_free' => so_next_workday($sched, $login, max($today, $from)), 'today' => so_day_type($sched, $login, $today)];
}

/* ---------- ОТЗЫВЫ ---------- */

function so_reviews() {
    $r = so_read('reviews', []);
    return $r + ['seq' => 0, 'items' => []];
}

function so_reviews_update(callable $fn) {
    return so_tx('reviews', ['seq' => 0, 'items' => []], $fn);
}

function so_reviews_published() {
    $items = array_values(array_filter(so_reviews()['items'], function ($r) { return ($r['status'] ?? '') === 'published'; }));
    usort($items, function ($a, $b) { return $b['t'] <=> $a['t']; });
    return $items;
}

function so_review_stats($items) {
    $dist = [5 => 0, 4 => 0, 3 => 0, 2 => 0, 1 => 0];
    $sum = 0;
    foreach ($items as $r) { $dist[(int)$r['rating']] = ($dist[(int)$r['rating']] ?? 0) + 1; $sum += (int)$r['rating']; }
    $n = count($items);
    return ['count' => $n, 'avg' => $n ? round($sum / $n, 1) : 0, 'dist' => $dist];
}

/** Рейтинг кодера по опубликованным отзывам о его заказах и число сданных заказов. */
function so_coder_stats($login, $index = null, $reviews = null) {
    $index = $index ?? so_index();
    $reviews = $reviews ?? so_reviews_published();
    $done = 0;
    foreach ($index as $e) if (($e['coder'] ?? null) === $login && $e['status'] === 'done') $done++;
    $sum = 0; $n = 0;
    foreach ($reviews as $r) if (($r['coder'] ?? '') === $login) { $sum += (int)$r['rating']; $n++; }
    return ['done' => $done, 'rating' => $n ? round($sum / $n, 1) : null, 'reviews' => $n];
}

/** Добавить отзыв. Если пришёл из завершённого заказа — отметка «проверенный заказ». */
function so_review_add($name, $rating, $text, $order = null) {
    $cfg = so_config();
    $verified = $order && $order['status'] === 'done';
    $cat = so_catalog();
    return so_reviews_update(function (&$d) use ($name, $rating, $text, $order, $verified, $cfg, $cat) {
        $d['seq'] = (int)($d['seq'] ?? 0) + 1;
        $item = [
            'id'       => $d['seq'],
            'name'     => $name,
            'rating'   => max(1, min(5, (int)$rating)),
            'text'     => $text,
            't'        => time(),
            'status'   => ($verified && !empty($cfg['reviews_auto'])) ? 'published' : 'pending',
            'order_id' => $order ? (int)$order['id'] : null,
            'verified' => $verified,
            'coder'    => $order && !empty($order['coder']) ? $order['coder']['login'] : null,
            'type'     => $order ? ($cat['types'][$order['brief']['type'] ?? '']['name'] ?? null) : null,
            'reply'    => null,
            'ip'       => so_ip(),
        ];
        $d['items'][] = $item;
        return $item;
    });
}

/* ---------- PLATEGA ---------- */

function so_platega_http($cfg, $method, $path, $body = null) {
    $url = SO_PLATEGA_URL . $path;
    $headers = ['Content-Type: application/json', 'Accept: application/json',
                'X-MerchantId: ' . trim((string)$cfg['platega_id']), 'X-Secret: ' . trim((string)$cfg['platega_secret'])];
    $payload = $body === null ? null : json_encode($body, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    if (!function_exists('curl_init')) {
        $ctx = stream_context_create(['http' => ['method' => $method, 'header' => implode("\r\n", $headers), 'content' => (string)$payload,
                                                 'timeout' => 20, 'ignore_errors' => true]]);
        $raw = @file_get_contents($url, false, $ctx);
        $code = isset($http_response_header[0]) && preg_match('/\s(\d{3})/', $http_response_header[0], $m) ? (int)$m[1] : 0;
        return ['code' => $code, 'json' => json_decode((string)$raw, true), 'raw' => (string)$raw, 'error' => $raw === false ? 'нет связи с Platega' : ''];
    }
    $call = function ($verify) use ($url, $method, $headers, $payload) {
        $ch = curl_init($url);
        $opt = [CURLOPT_CUSTOMREQUEST => $method, CURLOPT_HTTPHEADER => $headers, CURLOPT_RETURNTRANSFER => true,
                CURLOPT_CONNECTTIMEOUT => 8, CURLOPT_TIMEOUT => 25, CURLOPT_SSL_VERIFYPEER => $verify, CURLOPT_SSL_VERIFYHOST => $verify ? 2 : 0];
        if ($payload !== null) $opt[CURLOPT_POSTFIELDS] = $payload;
        curl_setopt_array($ch, $opt);
        $raw = curl_exec($ch);
        $res = [$raw, (int)curl_getinfo($ch, CURLINFO_HTTP_CODE), curl_errno($ch), curl_error($ch)];
        curl_close($ch);
        return $res;
    };
    [$raw, $code, $errno, $err] = $call(true);
    if ($raw === false && in_array($errno, [35, 51, 58, 60, 77], true)) [$raw, $code, $errno, $err] = $call(false); // хостинг без корневых сертификатов
    return ['code' => $code, 'json' => json_decode((string)$raw, true), 'raw' => (string)$raw, 'error' => $raw === false ? 'нет связи с Platega: ' . $err : ''];
}

function so_platega_error($r) {
    if ($r['error'] !== '') return $r['error'];
    $j = is_array($r['json']) ? $r['json'] : [];
    $msg = $j['message'] ?? $j['error'] ?? $j['title'] ?? '';
    if (is_array($msg)) $msg = json_encode($msg, JSON_UNESCAPED_UNICODE);
    if ($r['code'] === 401 || $r['code'] === 403) return 'Platega не приняла ключи (код ' . $r['code'] . '). Проверьте Merchant ID и секрет.';
    return 'Platega ответила кодом ' . $r['code'] . ($msg !== '' ? ': ' . $msg : '');
}

/** Создать платёж. Возвращает ['ok', 'tx', 'url', 'error']. */
function so_platega_create($cfg, $amount, $desc, $return_url, $failed_url, $payload) {
    $method = (int)($cfg['platega_method'] ?? 2);
    $body = ['paymentDetails' => ['amount' => round((float)$amount, 2), 'currency' => 'RUB'], 'description' => $desc,
             'return' => $return_url, 'failedUrl' => $failed_url, 'payload' => $payload];
    if ($method > 0) {
        $r = so_platega_http($cfg, 'POST', '/transaction/process', $body + ['paymentMethod' => $method]);
    } else {
        $r = so_platega_http($cfg, 'POST', '/v2/transaction/process', $body);
        if (in_array($r['code'], [404, 405], true)) $r = so_platega_http($cfg, 'POST', '/transaction/process', $body + ['paymentMethod' => 2]);
    }
    $j = is_array($r['json']) ? $r['json'] : [];
    $tx  = $j['transactionId'] ?? $j['id'] ?? null;
    $url = $j['redirect'] ?? $j['url'] ?? $j['paymentUrl'] ?? null;
    if ($r['code'] >= 200 && $r['code'] < 300 && $tx && $url) return ['ok' => true, 'tx' => (string)$tx, 'url' => (string)$url, 'error' => ''];
    return ['ok' => false, 'tx' => null, 'url' => null, 'error' => so_platega_error($r)];
}

/** Статус платежа в Platega → paid | pending | cancelled | refunded | '' (не удалось узнать). */
function so_platega_status($cfg, $tx) {
    $r = so_platega_http($cfg, 'GET', '/transaction/' . rawurlencode((string)$tx));
    $j = is_array($r['json']) ? $r['json'] : [];
    $s = strtoupper((string)($j['status'] ?? ''));
    if ($r['code'] < 200 || $r['code'] >= 300 || $s === '') return ['status' => '', 'error' => so_platega_error($r)];
    $map = ['CONFIRMED' => 'paid', 'SUCCESS' => 'paid', 'PAID' => 'paid', 'PENDING' => 'pending', 'CREATED' => 'pending', 'PROCESSING' => 'pending',
            'CANCELED' => 'cancelled', 'CANCELLED' => 'cancelled', 'EXPIRED' => 'cancelled', 'FAILED' => 'cancelled', 'DECLINED' => 'cancelled',
            'CHARGEBACKED' => 'refunded', 'CHARGEBACK' => 'refunded', 'REFUNDED' => 'refunded'];
    return ['status' => $map[$s] ?? 'pending', 'error' => ''];
}

/**
 * Записать новый статус счёта (идемпотентно) и сообщить в чат.
 * $source — кто узнал: «Platega», «проверка», логин сотрудника.
 */
function so_invoice_apply($order_id, $inv_id, $status, $source) {
    return so_order_update($order_id, function (&$o) use ($inv_id, $status, $source) {
        $i = so_find_invoice($o, $inv_id);
        if ($i === null) return false;
        $inv = &$o['invoices'][$i];
        if ($inv['status'] === $status || ($inv['status'] === 'paid' && $status !== 'refunded')) return false;
        $inv['status'] = $status;
        if ($status === 'paid') {
            $inv['paid_at'] = time();
            $inv['paid_by'] = $source;
            so_msg_push($o, 'system', ['text' => '✅ Оплата ' . so_money($inv['amount']) . ' получена. Спасибо! Счёт «' . $inv['desc'] . '» закрыт.', 'kind' => 'system']);
        } elseif ($status === 'cancelled') {
            so_msg_push($o, 'system', ['text' => 'Счёт на ' . so_money($inv['amount']) . ' отменён.', 'kind' => 'system']);
        } elseif ($status === 'refunded') {
            so_msg_push($o, 'system', ['text' => 'Платёж ' . so_money($inv['amount']) . ' возвращён.', 'kind' => 'system']);
        }
        so_history($o, $source, 'Счёт #' . $inv['id'] . ' → ' . $status);
        unset($inv);
        so_bump($o);
        return true;
    });
}

/** Спросить у Platega статус счёта и применить его. Возвращает текст для показа. */
function so_invoice_check($order_id, $inv_id, $source) {
    $o = so_order_load($order_id);
    if (!$o) return ['ok' => false, 'msg' => 'Заказ не найден'];
    $i = so_find_invoice($o, $inv_id);
    if ($i === null) return ['ok' => false, 'msg' => 'Счёт не найден'];
    $inv = $o['invoices'][$i];
    if (empty($inv['tx'])) return ['ok' => true, 'msg' => 'Это счёт без Platega — отметьте оплату вручную.', 'status' => $inv['status']];
    if ($inv['status'] === 'paid') return ['ok' => true, 'msg' => 'Счёт уже оплачен.', 'status' => 'paid'];
    $cfg = so_config();
    if (!so_platega_ready($cfg)) return ['ok' => false, 'msg' => 'Platega не подключена.'];
    $st = so_platega_status($cfg, $inv['tx']);
    if ($st['status'] === '') return ['ok' => false, 'msg' => $st['error']];
    if ($st['status'] !== 'pending') so_invoice_apply($order_id, $inv_id, $st['status'], $source);
    $labels = ['paid' => '✅ Оплачено', 'pending' => '⏳ Ещё не оплачено', 'cancelled' => 'Платёж отменён', 'refunded' => 'Платёж возвращён'];
    return ['ok' => true, 'msg' => $labels[$st['status']] ?? $st['status'], 'status' => $st['status']];
}
