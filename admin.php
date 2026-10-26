<?php
session_start();

require_once __DIR__ . '/_lib_db.php';

// Обычные load_json/save_json (для файлов вроде director_requests.json,
// заявок и т.д.) теперь просто используют безопасные lj_load/lj_save с
// блокировкой файла и атомарной записью — те же гарантии, что и у
// директорских данных, без дублирования кода.
function load_json($file, $default) {
    return lj_load($file, $default);
}
function save_json($file, $data) {
    return lj_save($file, $data);
}
function slugify_school($name) {
    $map = [
        'а'=>'a','б'=>'b','в'=>'v','г'=>'g','д'=>'d','е'=>'e','ё'=>'e','ж'=>'zh','з'=>'z','и'=>'i','й'=>'y',
        'к'=>'k','л'=>'l','м'=>'m','н'=>'n','о'=>'o','п'=>'p','р'=>'r','с'=>'s','т'=>'t','у'=>'u','ф'=>'f',
        'х'=>'h','ц'=>'c','ч'=>'ch','ш'=>'sh','щ'=>'sch','ъ'=>'','ы'=>'y','ь'=>'','э'=>'e','ю'=>'yu','я'=>'ya',
        'А'=>'A','Б'=>'B','В'=>'V','Г'=>'G','Д'=>'D','Е'=>'E','Ё'=>'E','Ж'=>'Zh','З'=>'Z','И'=>'I','Й'=>'Y',
        'К'=>'K','Л'=>'L','М'=>'M','Н'=>'N','О'=>'O','П'=>'P','Р'=>'R','С'=>'S','Т'=>'T','У'=>'U','Ф'=>'F',
        'Х'=>'H','Ц'=>'C','Ч'=>'Ch','Ш'=>'Sh','Щ'=>'Sch','Ъ'=>'','Ы'=>'Y','Ь'=>'','Э'=>'E','Ю'=>'Yu','Я'=>'Ya'
    ];
    $s = strtr($name, $map);
    $s = mb_strtolower($s);
    $s = preg_replace('/[^a-z0-9]+/', '-', $s);
    $s = trim($s, '-');
    if ($s === '') $s = 'school';
    return $s;
}
function unique_school_slug($base) {
    $slug = $base; $i = 2;
    while (dl_load_full($slug) !== null) { $slug = $base . '-' . $i; $i++; }
    return $slug;
}
// Имя файла страницы школы: сохраняем читаемое название школы (с кириллицей),
// убираем только опасные для файловой системы символы.
function safe_school_filename($name) {
    $s = trim($name);
    $s = preg_replace('/[\/\\\\\x00-\x1f"\'<>|:*?]/u', '', $s);
    $s = preg_replace('/\s+/u', '_', $s);
    if ($s === '') $s = 'school';
    return $s;
}
// Копирует dairy.html и универсальный шаблон сайта-визитки школы
// (school_site_template.html, должен лежать рядом с admin.php) в папку
// schools/<slug>/ — так у каждой школы получается свой адрес вида
// rteam.info/schools/<slug>/dairy.html (дневник) и
// rteam.info/schools/<slug>/index.html (визитка школы, редактируется
// из кабинета программиста). <slug> — это безопасная для URL версия
// названия школы, введённого в админке (уже используется везде в
// системе: database_<slug>.json, вход директора/программиста и т.д.).
function copy_school_journal($schoolName, $slug) {
    $dir = "schools/" . $slug;
    if (!is_dir($dir) && !@mkdir($dir, 0755, true)) return null;

    // dairy.html сам определяет свою школу по адресу страницы
    // (/schools/<slug>/dairy.html — распознаётся по пути, ?school=slug
    // не нужен), поэтому копия не требует НИКАКИХ подстановок в тексте —
    // просто копируем файл как есть, меняем только заголовок вкладки.
    // Из-за этого при будущих правках dairy.html копии школ не обновятся
    // сами — после изменения оригинала нажмите на вкладке "Директора"
    // кнопку "Обновить файлы всех школ" (или "Создать файл" для одной).
    $template = "dairy.html";
    if (!file_exists($template)) return null;
    $content = file_get_contents($template);
    if ($content === false) return null;
    if (preg_match('/<title>.*?<\/title>/su', $content)) {
        $content = preg_replace('/<title>.*?<\/title>/su', '<title>Электронный журнал — ' . htmlspecialchars($schoolName, ENT_QUOTES) . '</title>', $content, 1);
    }
    $target = $dir . '/dairy.html';
    $ok = @file_put_contents($target, $content);
    if ($ok === false) return null;

    // Шаблон сайта-визитки один и тот же для всех школ: он сам определяет
    // свой slug по адресной строке (/schools/<slug>/), поэтому его не нужно
    // ничем подставлять при копировании — просто копия файла как есть.
    // Файл index.html — это чистый шаблон без каких-либо данных внутри
    // (весь контент сайта хранится в MySQL, в поле site у школы), поэтому
    // его всегда можно спокойно перезаписать новой версией — это не сотрёт
    // ничего, что программист собрал в конструкторе.
    $siteTemplate = "school_site_template.html";
    if (file_exists($siteTemplate)) {
        @copy($siteTemplate, $dir . '/index.html');
    }

    return $target;
}
function formatBytes($bytes, $precision = 2) {
    $units = array('B', 'KB', 'MB', 'GB', 'TB');
    $bytes = max($bytes, 0);
    $pow = floor(($bytes ? log($bytes) : 0) / log(1024));
    $pow = min($pow, count($units) - 1);
    $bytes /= (1 << (10 * $pow));
    return round($bytes, $precision) . ' ' . $units[$pow];
}

// Каталог тематик сайта (раздел "Темы сайта" в админке).
// Ключ => [group, name, icon, field (подпись для текстового поля), placeholder]
function rteam_theme_catalog() {
    return [
        'tennis'           => ['group'=>'Спорт',    'name'=>'Теннис',                       'icon'=>'🎾', 'field'=>'Название турнира',        'placeholder'=>'Например: US Open 2026'],
        'football'         => ['group'=>'Спорт',    'name'=>'Футбол',                        'icon'=>'⚽', 'field'=>'Название турнира',        'placeholder'=>''],
        'basketball'       => ['group'=>'Спорт',    'name'=>'Баскетбол',                     'icon'=>'🏀', 'field'=>'Название турнира',        'placeholder'=>''],
        'hockey'           => ['group'=>'Спорт',    'name'=>'Хоккей',                        'icon'=>'🏒', 'field'=>'Название турнира',        'placeholder'=>''],
        'volleyball'       => ['group'=>'Спорт',    'name'=>'Волейбол',                      'icon'=>'🏐', 'field'=>'Название турнира',        'placeholder'=>''],
        'chess'            => ['group'=>'Спорт',    'name'=>'Шахматы',                       'icon'=>'♟️', 'field'=>'Название турнира',        'placeholder'=>''],
        'formula1'         => ['group'=>'Спорт',    'name'=>'Формула 1',                     'icon'=>'🏎️', 'field'=>'Название этапа',          'placeholder'=>''],
        'boxing'           => ['group'=>'Спорт',    'name'=>'Бокс',                          'icon'=>'🥊', 'field'=>'Название турнира / боя',  'placeholder'=>''],
        'olympics'         => ['group'=>'Спорт',    'name'=>'Олимпиада',                     'icon'=>'🏅', 'field'=>'Текст баннера',           'placeholder'=>''],
        'europa_league'    => ['group'=>'Спорт',    'name'=>'Лига Европы (футбол)',          'icon'=>'🏆', 'field'=>'Текст поздравления',      'placeholder'=>'Поздравляем с победой в Лиге Европы!'],
        'world_cup'        => ['group'=>'Спорт',    'name'=>'Чемпионат мира (футбол)',       'icon'=>'🌍', 'field'=>'Текст поздравления',      'placeholder'=>'С Чемпионатом мира по футболу!'],
        'gaming_day'       => ['group'=>'Спорт',    'name'=>'Киберспорт / игровой турнир',   'icon'=>'🎮', 'field'=>'Название турнира',        'placeholder'=>''],

        'new_year'         => ['group'=>'Праздники','name'=>'Новый год',                     'icon'=>'🎄', 'field'=>'Год',                     'placeholder'=>'Например: 2027'],
        'christmas'        => ['group'=>'Праздники','name'=>'Рождество',                     'icon'=>'🎅', 'field'=>'Текст (необязательно)',   'placeholder'=>''],
        'halloween'        => ['group'=>'Праздники','name'=>'Хэллоуин',                      'icon'=>'🎃', 'field'=>'Текст (необязательно)',   'placeholder'=>''],
        'valentine'        => ['group'=>'Праздники','name'=>'День св. Валентина',            'icon'=>'💘', 'field'=>'Текст (необязательно)',   'placeholder'=>''],
        'march8'           => ['group'=>'Праздники','name'=>'8 марта',                       'icon'=>'💐', 'field'=>'Текст поздравления',      'placeholder'=>''],
        'defender_day'     => ['group'=>'Праздники','name'=>'23 февраля',                    'icon'=>'🎖️', 'field'=>'Текст поздравления',      'placeholder'=>''],
        'easter'           => ['group'=>'Праздники','name'=>'Пасха',                         'icon'=>'🐣', 'field'=>'Текст (необязательно)',   'placeholder'=>''],
        'victory_day'      => ['group'=>'Праздники','name'=>'День Победы (9 мая)',           'icon'=>'🎆', 'field'=>'Текст поздравления',      'placeholder'=>''],
        'cosmonautics_day' => ['group'=>'Праздники','name'=>'День космонавтики',             'icon'=>'🚀', 'field'=>'Текст (необязательно)',   'placeholder'=>''],
        'graduation'       => ['group'=>'Праздники','name'=>'Выпускной',                     'icon'=>'🎓', 'field'=>'Текст (необязательно)',   'placeholder'=>''],

        'rteam_birthday'   => ['group'=>'RTeam',    'name'=>'День рождения RTeam',           'icon'=>'🎂', 'field'=>'Текст поздравления',      'placeholder'=>'RTeam исполняется 5 лет!'],
        'site_birthday'    => ['group'=>'RTeam',    'name'=>'День рождения сайта',           'icon'=>'🎉', 'field'=>'Текст поздравления',      'placeholder'=>''],
        'programmer_day'   => ['group'=>'RTeam',    'name'=>'День программиста',             'icon'=>'💻', 'field'=>'Текст (необязательно)',   'placeholder'=>''],

        'squid_game'       => ['group'=>'Игры',     'name'=>'Игра в кальмара',               'icon'=>'🦑', 'field'=>'Текст (необязательно)',   'placeholder'=>'Красный свет, зелёный свет...'],
    ];
}

// Функция рекурсивного удаления папки
function delete_directory($dir) {
    if (!file_exists($dir)) return true;
    if (!is_dir($dir)) return unlink($dir);
    foreach (scandir($dir) as $item) {
        if ($item == '.' || $item == '..') continue;
        if (!delete_directory($dir . DIRECTORY_SEPARATOR . $item)) return false;
    }
    return rmdir($dir);
}

// Функция сжатия изображений в WebP
function process_and_compress_image($file_info) {
    $dir = "uploads/icons/";
    if (!is_dir($dir)) mkdir($dir, 0755, true);
    $extension = strtolower(pathinfo($file_info['name'], PATHINFO_EXTENSION));
    $target_path = $dir . time() . "_" . uniqid() . ".webp";
    switch ($extension) {
        case 'jpeg': case 'jpg': $img = @imagecreatefromjpeg($file_info['tmp_name']); break;
        case 'png': $img = @imagecreatefrompng($file_info['tmp_name']); break;
        case 'webp': $img = @imagecreatefromwebp($file_info['tmp_name']); break;
        default: return "";
    }
    if (!$img) return "";
    $width = imagesx($img); $height = imagesy($img); $max_size = 200;
    if ($width > $max_size || $height > $max_size) {
        if ($width > $height) { $new_width = $max_size; $new_height = floor($height * ($max_size / $width)); } 
        else { $new_height = $max_size; $new_width = floor($width * ($max_size / $height)); }
        $tmp_img = imagecreatetruecolor($new_width, $new_height);
        imagealphablending($tmp_img, false); imagesavealpha($tmp_img, true);
        imagecopyresampled($tmp_img, $img, 0, 0, 0, 0, $new_width, $new_height, $width, $height);
        imagedestroy($img); $img = $tmp_img;
    }
    imagewebp($img, $target_path, 80); imagedestroy($img); return $target_path;
}

// ФУНКЦИЯ: Отрисовка HTML для ЗАЯВОК (AJAX)
// $can_reply — может отвечать/закрывать, $can_mute — может выдавать мут (это бан, только для тех, кто банит)
function render_bot_tickets($tg_tickets, $banned_users, $can_reply = true, $can_mute = true) {
    $html = "";
    if (empty($tg_tickets)) return '<p style="color:#666;">Заявок пока нет.</p>';
    
    foreach ($tg_tickets as $ticket) {
        $is_new = ($ticket['status'] === 'new');
        $is_closed = ($ticket['status'] === 'closed');
        $is_banned = in_array((string)$ticket['user_id'], $banned_users) || in_array((int)$ticket['user_id'], $banned_users); 
        
        $badge_class = $is_new ? 'badge-new' : ($is_closed ? 'badge-dec' : 'badge-acc');
        $badge_text = $is_new ? 'Новая' : ($is_closed ? 'Закрыта' : 'Отвечено');
        $border_color = $is_new ? '#3182ce' : ($is_closed ? '#4a5568' : '#38a169');
        
        $html .= '<div class="card" style="border-left: 4px solid '.$border_color.'; '.($is_closed ? 'opacity: 0.6;' : '').'">';
        $html .= '<div class="meta" style="border-bottom: 1px solid #333; padding-bottom: 8px; margin-bottom: 10px;">';
        $html .= '<span class="badge '.$badge_class.'" style="float:right;">'.$badge_text.'</span>';
        if ($is_banned) $html .= '<span class="badge badge-dec" style="float:right; margin-right:5px; background:#c53030;">В МУТЕ 🔇</span>';
        $html .= '📅 ' . htmlspecialchars($ticket['date']) . ' | 👤 <b>' . htmlspecialchars($ticket['username']) . '</b> (ID: ' . $ticket['user_id'] . ')';
        $html .= '</div>';
        $html .= '<div style="font-size: 14px; margin-bottom: 15px; color: #eee; background: #050509; padding: 10px; border-radius: 6px; border: 1px solid #222;">';
        $html .= nl2br(htmlspecialchars($ticket['text']));
        $html .= '</div>';
        
        if (!$is_closed && ($can_reply || $can_mute)) {
            $html .= '<div style="background: #1a1a24; padding: 12px; border-radius: 8px; border: 1px solid #333;">';
            $placeholder = $is_new ? 'Написать ответ пользователю...' : 'Написать еще одно сообщение...';
            if ($can_reply) $html .= '<textarea id="reply_text_'.htmlspecialchars($ticket['id']).'" required placeholder="'.$placeholder.'" style="height: 60px; background: #050509; margin-bottom: 10px; width: 100%; box-sizing: border-box;"></textarea>';
            $html .= '<div style="display: flex; gap: 10px; align-items: center; flex-wrap: wrap;">';
            
            if ($can_reply) $html .= '<button class="btn blue" style="margin-top:0;" type="button" onclick="const t=document.getElementById(\'reply_text_'.htmlspecialchars($ticket['id']).'\').value; if(!t){alert(\'Введите текст ответа!\'); return;} sendBotAction(\'reply_tg_ticket\', \''.htmlspecialchars($ticket['user_id']).'\', \''.htmlspecialchars($ticket['id']).'\', t); document.getElementById(\'reply_text_'.htmlspecialchars($ticket['id']).'\').value=\'\';">Отправить ответ</button>';
            if ($can_reply) $html .= '<button class="btn gray" style="margin-top:0;" type="button" onclick="sendBotAction(\'close_tg_ticket\', \''.htmlspecialchars($ticket['user_id']).'\', \''.htmlspecialchars($ticket['id']).'\')">Закрыть диалог</button>';
            
            $html .= '<div style="flex:1; text-align:right; min-width: 150px;">';
            if (!$can_mute) {
                // мут выдают только те, у кого есть право банить
            } elseif ($is_banned) {
                $html .= '<button class="btn ok" style="margin-top:0;" type="button" onclick="sendBotAction(\'unban_bot_user\', \''.htmlspecialchars($ticket['user_id']).'\', \''.htmlspecialchars($ticket['id']).'\')">🔊 Снять Мут</button>';
            } else {
                $html .= '<button class="btn no" style="margin-top:0;" type="button" onclick="if(confirm(\'Запретить этому пользователю создавать заявки в боте?\')) sendBotAction(\'ban_bot_user\', \''.htmlspecialchars($ticket['user_id']).'\', \''.htmlspecialchars($ticket['id']).'\')">🔇 Выдать Мут</button>';
            }
            $html .= '</div></div></div>';
        }
        $html .= '</div>';
    }
    return $html;
}

// ФУНКЦИЯ: Отрисовка HTML для РОЗЫГРЫШЕЙ (AJAX)
function render_bot_giveaways_list($gws, $can_manage = true) {
    $html = "";
    if (empty($gws)) return '<p style="color:#666;">Розыгрышей пока нет.</p>';
    foreach (array_reverse($gws) as $gw) {
        $is_active = ($gw['status'] === 'active');
        $html .= '<div class="card" style="border-left: 4px solid '.($is_active ? '#e67e22' : '#777').'; '.(!$is_active ? 'opacity:0.7;' : '').'">';
        $html .= '<div style="float:right;"><span class="badge '.($is_active ? 'badge-warn' : 'badge-viewed').'">'.($is_active ? 'Активен' : 'Завершен').'</span></div>';
        $html .= '<h3 style="margin-top:0;">' . htmlspecialchars($gw['title']) . '</h3>';
        $html .= '<div class="meta" style="margin-bottom: 10px;">Кол-во победителей: <b>' . $gw['winners_count'] . '</b> | Текущих участников: <b style="color:#1f9d55;">' . count($gw['participants']) . '</b></div>';
        $html .= '<p style="font-size:13px; color:#ccc; background:#050509; padding:8px; border-radius:6px; border:1px solid #222;">' . nl2br(htmlspecialchars($gw['description'])) . '</p>';
        
        if ($is_active && $can_manage) {
            $html .= '<form class="ajax-bot-form" method="POST" style="margin-top:10px;" onsubmit="if(!confirm(\'Завершить розыгрыш и выбрать победителей?\')) return false;">
                        <input type="hidden" name="action" value="roll_bot_gw">
                        <input type="hidden" name="gw_id" value="'.$gw['id'].'">
                        <input type="hidden" name="is_ajax" value="1">
                        <button class="btn orange" type="submit">🎲 Подвести итоги</button>
                      </form>';
        } elseif (!$is_active) {
            $win_mentions = [];
            foreach ($gw['winners'] as $w) { $win_mentions[] = "👤 <a href='tg://user?id=".$w."' style='color:#3182ce;'>".$w."</a>"; }
            $html .= '<div style="background:#1a1a24; padding:10px; border-radius:6px; margin:10px 0; border:1px solid #333; font-size:14px;">🏆 <b>Победители:</b><br> ' . (empty($win_mentions) ? 'Никто не участвовал' : implode(', ', $win_mentions)) . '</div>';
        }
        
        if ($can_manage) $html .= '<form class="ajax-bot-form" method="POST" style="margin-top:8px;" onsubmit="if(!confirm(\'Точно удалить этот розыгрыш из списка?\')) return false;">
                    <input type="hidden" name="action" value="del_bot_gw">
                    <input type="hidden" name="gw_id" value="'.$gw['id'].'">
                    <input type="hidden" name="is_ajax" value="1">
                    <button class="btn gray" type="submit">Удалить из списка</button>
                  </form>';
        $html .= '</div>';
    }
    return $html;
}

/* ==========================================================
   ВХОД В ПАНЕЛЬ И ПРАВА
   Роль каждый раз берётся из users.json, а не только из сессии:
   повышение, понижение или исключение из команды действуют сразу,
   без перевхода. Что может каждая роль — см. _roles.php и вкладку
   «Права ролей».
   ========================================================== */
require_once __DIR__ . '/_roles.php';
require_once __DIR__ . '/_schedule_admin.php'; // «Расписание», «Заказы сайтов», «Отзывы»

$user  = $_SESSION["user"] ?? "Гость";
$users = load_json("users.json", []);
if (isset($users[$user]) && is_array($users[$user])) {
    $role = $users[$user]["role"] ?? "Пользователь";
    $_SESSION["role"] = $role;
} elseif ($users) {
    $role = "Гость"; // аккаунт удалён — доступа больше нет
} else {
    $role = $_SESSION["role"] ?? "Гость"; // users.json недоступен — как раньше, по сессии
}

if (!rt_is_staff($role, $user)) {
    http_response_code(403);
    echo "Доступ запрещён.";
    exit;
}

$my_direction = (string)($users[$user]["direction"] ?? "");
$my_dept      = rt_user_dept($role, $my_direction);

function can($perm) {
    global $user, $role;
    return rt_can($user, $role, $perm);
}
function can_any(array $perms) {
    foreach ($perms as $p) if (can($p)) return true;
    return false;
}
// Короткие сообщения после действия (показываются всплывашкой на следующей странице)
function flash($text, $type = "info") {
    $_SESSION["flash"][] = ["text" => $text, "type" => $type];
}
// Пароль сохраняем в том же виде, что и у остальных аккаунтов:
// если в users.json пароли захешированы — хешируем, иначе как было.
function store_password($plain, $users) {
    $hashed = 0; $total = 0;
    foreach ($users as $u) {
        if (!is_array($u) || !isset($u["password"])) continue;
        $total++;
        if (preg_match('/^\$(2y|2a|argon2)/', (string)$u["password"])) $hashed++;
    }
    return ($total > 0 && $hashed * 2 > $total) ? password_hash($plain, PASSWORD_DEFAULT) : $plain;
}

/* IP АВТОРА СООБЩЕНИЯ (для банов)
   Берём IP, сохранённый при отправке. Если его нет — последний IP аккаунта
   автора (сайт запоминает его при каждом заходе, см. rt_track_ip в _roles.php). */
function author_ip($stored, $login = null, $email = null) {
    global $users;
    if ($stored && filter_var($stored, FILTER_VALIDATE_IP)) return ["ip" => $stored, "src" => "при отправке", "login" => $login];
    $acc = null;
    if ($login !== null && $login !== "" && isset($users[$login])) $acc = (string)$login;
    elseif ($email) {
        $e = mb_strtolower(trim($email));
        foreach ($users as $l => $u) if (is_array($u) && $e !== "" && mb_strtolower(trim($u["email"] ?? "")) === $e) { $acc = (string)$l; break; }
    }
    if ($acc !== null && !empty($users[$acc]["ip"])) return ["ip" => $users[$acc]["ip"], "src" => "аккаунт " . $acc . ", последний заход", "login" => $acc];
    return null;
}
function ip_is_banned($ip) {
    global $bans;
    if (!isset($bans)) $bans = load_json("bans.json", []); // в AJAX-ответах список банов ещё не загружен
    foreach ((array)$bans as $b) {
        if (($b["ip"] ?? "") === $ip && ((int)($b["expires"] ?? 0) === 0 || (int)$b["expires"] > time())) return true;
    }
    return false;
}
function ban_link($ip, $reason) {
    return '?tab=bans&quickban_ip=' . urlencode($ip) . '&quickban_reason=' . urlencode(mb_strimwidth($reason, 0, 120, "…"));
}
/* IP + кнопка «Забанить». IP видят только те, кто может банить
   ($always — показать IP и без права бана, как раньше в «Почте»). */
function ip_tag($info, $reason, $always = false) {
    if (!can("bans.manage") && !$always) return "";
    if (!$info) return '<span class="muted">IP неизвестен</span>';
    $html = '<span class="ip-tag"><code>' . htmlspecialchars($info["ip"]) . '</code> <span class="muted" style="font-size:12px;">' . htmlspecialchars($info["src"]) . '</span>';
    if (can("bans.manage")) {
        $html .= $info["ip"] === rt_client_ip() ? ' <span class="chip">это ваш IP</span>' : (ip_is_banned($info["ip"])
            ? ' <span class="badge badge-dec" style="margin:0;">⛔ забанен</span>'
            : ' <a class="btn sm ghost danger" href="' . htmlspecialchars(ban_link($info["ip"], $reason)) . '">🎯 Забанить IP</a>');
    }
    return $html . '</span>';
}

/* ВЕБХУК TELEGRAM-БОТА */

/* Запрос к Telegram Bot API от имени бота из настроек */
function tg_api($method, $params = []) {
    global $settings;
    $token = trim($settings["bot_token"] ?? "");
    if ($token === "") return ["ok" => false, "description" => "Не указан токен бота"];
    $call = function ($verify) use ($token, $method, $params) {
        $ch = curl_init("https://api.telegram.org/bot" . $token . "/" . $method);
        curl_setopt_array($ch, [CURLOPT_POST => true, CURLOPT_POSTFIELDS => $params, CURLOPT_RETURNTRANSFER => true,
            CURLOPT_CONNECTTIMEOUT => 6, CURLOPT_TIMEOUT => 15, CURLOPT_SSL_VERIFYPEER => $verify, CURLOPT_SSL_VERIFYHOST => $verify ? 2 : 0]);
        $res = curl_exec($ch); $errno = curl_errno($ch); $err = curl_error($ch); curl_close($ch);
        return [$res, $errno, $err];
    };
    [$res, $errno, $err] = $call(true);
    if ($res === false && in_array($errno, [35, 51, 58, 60, 77], true)) [$res, $errno, $err] = $call(false); // хостинг без корневых сертификатов
    if ($res === false) return ["ok" => false, "description" => "Нет связи с Telegram: " . $err];
    $json = json_decode($res, true);
    return is_array($json) ? $json : ["ok" => false, "description" => "Непонятный ответ Telegram"];
}

/* Стучится на адрес вебхука так же, как Telegram: POST, без перехода по редиректам.
   Возвращает [код ответа, куда перенаправляет (или null), ошибка связи (или null)] */
function probe_webhook($url, $secret) {
    $ch = curl_init($url);
    curl_setopt_array($ch, [CURLOPT_POST => true, CURLOPT_POSTFIELDS => "{}", CURLOPT_RETURNTRANSFER => true, CURLOPT_FOLLOWLOCATION => false,
        CURLOPT_HTTPHEADER => ["Content-Type: application/json", "X-Telegram-Bot-Api-Secret-Token: " . $secret],
        CURLOPT_CONNECTTIMEOUT => 6, CURLOPT_TIMEOUT => 12, CURLOPT_SSL_VERIFYPEER => false, CURLOPT_SSL_VERIFYHOST => 0]);
    curl_exec($ch);
    $code = (int)curl_getinfo($ch, CURLINFO_HTTP_CODE);
    $loc  = curl_getinfo($ch, CURLINFO_REDIRECT_URL) ?: null;
    $err  = curl_errno($ch) ? curl_error($ch) : null;
    curl_close($ch);
    return [$code, $loc, $err];
}

/* Адрес bot.php по умолчанию: тот же сайт и та же папка, что у admin.php */
function default_webhook_url() {
    $host = $_SERVER["HTTP_HOST"] ?? "rteam.info";
    $dir  = rtrim(str_replace("\\", "/", dirname($_SERVER["SCRIPT_NAME"] ?? "/admin.php")), "/");
    return "https://" . $host . $dir . "/bot.php";
}

/* Сообщение в тикете поддержки. У сообщений клиента — IP, с которого оно
   отправлено (support.php сохраняет его), и кнопка бана для тех, кто банит.
   Ответы ИИ (Rai) и системные пометки (кто позвал администратора) выделены. */
function render_ticket_reply($reply, $ticket) {
    $date = htmlspecialchars($reply["date"] ?? "");
    if (!empty($reply["is_system"])) {
        return '<div class="sup-sys">' . htmlspecialchars($reply["text"] ?? "") . ' <span>' . $date . '</span></div>';
    }
    $is_admin = $reply["is_admin"] ?? true;
    $is_ai = !empty($reply["is_ai"]);
    if ($is_ai) {
        $src = (string)($reply["ai_source"] ?? "");
        $author = '✨ Rai · ИИ' . ($src !== "" ? ' <span class="ai-src" title="Откуда ИИ взял ответ">' . htmlspecialchars(ai_source_label($src)) . '</span>' : '');
    } else {
        $author = $is_admin ? htmlspecialchars($reply["employee"] ?? "") . ' (Rteam)' : 'Клиент: ' . htmlspecialchars($ticket['client'] ?? "");
    }
    $html  = '<div class="bubble ' . ($is_ai ? 'admin ai' : ($is_admin ? 'admin' : 'client')) . '">';
    $html .= '<div class="b-meta">' . $author . ' <span style="color:#777; font-weight:normal; font-size:10px;">(' . $date . ')</span></div>';
    $html .= nl2br(htmlspecialchars($reply["text"] ?? ""));
    if (!empty($reply["photo"])) $html .= '<div style="margin-top:8px;"><a href="' . htmlspecialchars($reply["photo"]) . '" target="_blank" rel="noopener"><img src="' . htmlspecialchars($reply["photo"]) . '" style="max-height:150px; border-radius:6px; border:1px solid #333;"></a></div>';
    if (!$is_admin && can("bans.manage")) $html .= '<div class="b-ip">IP: ' . ip_tag(author_ip($reply["ip"] ?? null, $ticket["client"] ?? null), "Тикет #" . ($ticket["id"] ?? "") . ": " . mb_strimwidth(str_replace(["\r", "\n"], " ", $reply["text"] ?? ""), 0, 60, "…")) . '</div>';
    return $html . '</div>';
}

/* Откуда ИИ взял ответ: база знаний, страница сайта, интернет… */
function ai_source_label($src) {
    if (strpos($src, "nn:") === 0) return "нейросеть: " . substr($src, 3);
    if (strpos($src, "kb:") === 0) return "база знаний: " . substr($src, 3);
    $names = ["site" => "страница сайта", "web" => "интернет", "secret" => "отказ: секреты", "human" => "просьба позвать человека",
              "greeting" => "приветствие", "thanks" => "благодарность", "fallback" => "не нашёл ответа", "empty" => "пустое сообщение",
              "repeat" => "повтор — предложил человека", "clarify" => "уточняющий вопрос", "confirm_close" => "переспросил, закрыть ли тикет"];
    return $names[$src] ?? $src;
}

/* Ждёт ли тикет сотрудника: клиент или ИИ позвал администратора, или тикет открыт,
   а ИИ прямо сейчас на него не отвечает (выключен, не ответил, тикет старше ИИ) */
function ticket_needs_staff($t, $settings) {
    $st = $t["status"] ?? "";
    if ($st === "Ждёт администратора") return true;
    if ($st !== "Открыт") return false;
    $ai_busy = rt_ticket_ai_on($t, $settings) && !empty($t["ai_pending"]) && time() - (int)$t["ai_pending"] < 300;
    return !$ai_busy;
}

/* Список тикетов слева (страница и AJAX рисуют одинаково) */
function render_ticket_list($tickets, $active_id, $settings) {
    if (!$tickets) return '<div style="padding: 20px; color: #777; text-align: center; font-size: 13px;">Тикетов пока нет.</div>';
    $html = "";
    $cls = ["Открыт" => "st-open", "Ожидает ответа клиента" => "st-answered", "Ждёт администратора" => "st-human", "Закрыт" => "st-closed"];
    foreach (array_reverse($tickets) as $t) {
        $st = $t["status"] ?? "Открыт";
        $who = "";
        if ($st !== "Закрыт" && rt_support_ai_ready($settings)) {
            $who = rt_ticket_ai_on($t, $settings) ? '<span class="sup-who ai" title="Отвечает ИИ">✨ ИИ</span>' : '<span class="sup-who human" title="Отвечает сотрудник">🛡 Человек</span>';
        }
        $html .= '<a href="?tab=support&ticket_id=' . urlencode((string)$t['id']) . '" class="sup-ticket' . ((string)$active_id === (string)$t['id'] ? ' active' : '') . (ticket_needs_staff($t, $settings) ? ' needs' : '') . '">'
               . '<div class="sup-t-top"><span class="sup-t-title">' . htmlspecialchars($t['topic'] ?? "") . '</span>' . $who . '</div>'
               . '<div class="sup-t-meta"><span>#' . htmlspecialchars((string)$t['id']) . ' · ' . htmlspecialchars($t['client'] ?? "") . '</span>'
               . '<span class="sup-st ' . ($cls[$st] ?? "st-open") . '">' . htmlspecialchars($st) . '</span></div></a>';
    }
    return $html;
}

/* Какое право нужно для каждого действия. Действие, которого нет
   в списке, запрещено. "" — достаточно просто быть в команде. */
$ACTION_PERMS = [
    // заявки
    "mark_viewed" => "apps.view", "app_decide" => "apps.decide",
    // директора школ
    "approve_request" => "directors.manage", "decline_request" => "directors.manage", "delete_request" => "directors.manage",
    "add_director" => "directors.manage", "reset_director_password" => "directors.manage", "regenerate_file" => "directors.manage",
    "regenerate_all" => "directors.manage", "delete_director" => "directors.manage",
    // баны и чёрный список
    "add_ban" => "bans.manage", "unban" => "bans.manage", "save_geoblock" => "bans.manage", "clear_blocked_attempts" => "bans.manage",
    "add_blacklist" => "bans.manage", "sign_blacklist" => "bans.manage", "release_blacklist" => "bans.manage",
    "edit_blacklist" => "bans.manage", "delete_blacklist" => "bans.manage",
    "ban_bot_user" => "bans.manage", "unban_bot_user" => "bans.manage",
    // проекты и файлы
    "add_project" => "projects.manage", "edit_project" => "projects.manage", "toggle_project" => "projects.manage", "del_project" => "projects.manage",
    "upload_file" => "files.upload", "del_file" => "files.view",
    // чат
    "send_chat_msg" => "chat.view", "vote_poll" => "chat.view", "del_chat_msg" => "chat.view",
    "pin_msg" => "chat.pin", "unpin_msg" => "chat.pin",
    // цели и штрафы
    "add_goal" => "goals.manage", "del_goal" => "goals.manage",
    "add_fine" => "fines.manage", "pay_fine_manual" => "fines.manage", "del_fine" => "fines.manage", "pay_fine_online" => "",
    // почта и тикеты
    "reply_msg" => "mail.view", "del_msg" => "mail.view",
    "reply_ticket" => "support.view", "close_ticket" => "support.view", "pin_photo" => "support.view", "ticket_ai" => "support.view", "reopen_ticket" => "support.view",
    "save_support_ai" => "settings.manage",
    // бот
    "reply_tg_ticket" => "bot.tickets", "close_tg_ticket" => "bot.tickets",
    "broadcast_tg" => "bot.manage", "add_bot_gw" => "bot.manage", "del_bot_gw" => "bot.manage", "roll_bot_gw" => "bot.manage",
    "save_bot_token" => "settings.manage", "setup_webhook" => "settings.manage", "check_webhook" => "settings.manage", "delete_webhook" => "settings.manage",
    // контент
    "save_recruit" => "recruit.manage",
    "add_leak" => "leaks.manage", "edit_leak" => "leaks.manage", "toggle_leak" => "leaks.manage", "del_leak" => "leaks.manage",
    "add_post" => "blog.manage", "edit_post" => "blog.manage", "toggle_post" => "blog.manage", "del_post" => "blog.manage",
    "save_theme" => "themes.manage", "squid_set_paused" => "themes.manage", "squid_reset_user" => "themes.manage",
    "save_settings" => "settings.manage",
    // золотой билет
    "grant_golden" => "gold.manage", "revoke_golden" => "gold.manage", "add_gold_service" => "gold.manage",
    "edit_gold_service" => "gold.manage", "delete_gold_service" => "gold.manage", "gold_reply" => "gold.chat",
    // люди и роли
    "add_user" => "users.manage", "set_pass" => "users.manage", "del_user" => "users.manage", "unlink_tg" => "users.manage", "unlink_ds" => "users.manage", "discord_dm" => "users.manage",
    "save_discord" => "settings.manage", "check_discord" => "settings.manage", "restart_discord" => "settings.manage",
    "set_role" => "roles.manage", "add_to_team" => "roles.manage", "change_team_role" => "roles.manage", "remove_from_team" => "roles.manage",
    "save_perms" => "perms.manage", "reset_perms" => "perms.manage",
    // Rai (rai.rteam.info): подписки
    "rai_grant" => "users.manage", "rai_revoke" => "users.manage", "rai_order_check" => "users.manage",
    "rai_save" => "settings.manage", "rai_check" => "settings.manage", "rai_plans" => "settings.manage",
    "rai_plans_reset" => "settings.manage", "rai_platega" => "settings.manage",
    // Rai: правила — нарушения и блокировки
    "rai_seen" => "users.manage", "rai_vdel" => "users.manage", "rai_ban" => "users.manage", "rai_unban" => "users.manage",
    // Rai: уведомления — рассылка и устройства админов
    "rai_push_send" => "users.manage", "rai_push_del" => "users.manage",
    // Rai: роли (создатель, разработчик), скидки и промокоды
    "rai_role" => "users.manage", "rai_sale" => "settings.manage", "rai_sale_off" => "settings.manage",
    "rai_promo" => "settings.manage", "rai_promo_del" => "settings.manage",
    // Расписание, заказы сайтов и отзывы: одно действие «so», кто что может — проверяет _schedule_admin.php
    "so" => "",
];

if ($_SERVER["REQUEST_METHOD"] === "POST" && isset($_POST["action"])) {
    $__act  = (string)$_POST["action"];
    $__need = array_key_exists($__act, $ACTION_PERMS) ? $ACTION_PERMS[$__act] : false;
    if ($__need === false || ($__need !== "" && !can($__need))) {
        if (isset($_POST["is_ajax"])) { echo "forbidden"; exit; }
        flash("Недостаточно прав для этого действия (роль «" . rt_role_label($role, $my_direction) . "»).", "error");
        header("Location: admin.php?tab=" . urlencode($_GET["tab"] ?? "home")); exit;
    }
}

// --- API ДЛЯ ОБНОВЛЕНИЯ ВКЛАДКИ БОТА (AJAX JSON) ---
if (isset($_GET['ajax_bot_data'])) {
    header('Content-Type: application/json');
    if (!can_any(["bot.tickets", "bot.manage"])) { echo json_encode(["tickets" => "", "giveaways" => ""]); exit; }
    $tg_tickets = load_json("bot_tickets.json", []);
    $banned_users = load_json("bot_banned.json", []);
    $gws = load_json("bot_giveaways.json", []);
    if (!is_array($banned_users)) $banned_users = [];

    echo json_encode([
        "tickets" => can("bot.tickets") ? render_bot_tickets($tg_tickets, $banned_users, true, can("bans.manage")) : "",
        "giveaways" => render_bot_giveaways_list($gws, can("bot.manage"))
    ]);
    exit;
}

/* --- API ДЛЯ УВЕДОМЛЕНИЙ (AJAX) --- */
if (isset($_GET['ajax_check'])) {
    header('Content-Type: application/json');
    $last_time = (int)$_GET['last_time'];
    $new_msgs = [];
    $chat_data = load_json("chat.json", ["messages" => [], "pinned_id" => null]);
    foreach ($chat_data["messages"] as $m) {
        if (strtotime($m["time"]) > $last_time && $m["user"] !== $user) {
            $new_msgs[] = "Новое сообщение в чате от " . $m["user"];
        }
    }
    $new_fines = [];
    $fines = load_json("fines.json", []);
    foreach ($fines as $f) {
        if (strtotime($f["issue_date"]) > $last_time && $f["user"] === $user) {
            $new_fines[] = "ВНИМАНИЕ! Вам выписан новый штраф: " . $f["amount"] . " руб.";
        }
    }
    echo json_encode(["status" => "ok", "time" => time(), "msgs" => $new_msgs, "fines" => $new_fines]);
    exit;
}

/* --- API ДЛЯ ОБНОВЛЕНИЯ ЧАТА ТИКЕТА --- */
if (isset($_GET['ajax_html_ticket'])) {
    header('Content-Type: application/json');
    if (!can("support.view")) { echo json_encode(["html" => "", "status" => ""]); exit; }
    $id = $_GET['ajax_html_ticket'];
    $tickets_data = load_json("tickets.json", []);
    $sup_settings = load_json("settings.json", []);
    $html = ""; $status = "Закрыт"; $ai_on = false;
    foreach ($tickets_data as $t) {
        if ((string)$t['id'] === (string)$id) {
            $status = $t['status'];
            $ai_on = rt_ticket_ai_on($t, $sup_settings);
            $ai_data = rt_ticket_ai_payload($t);
            foreach ($t["replies"] as $reply) $html .= render_ticket_reply($reply, $t);
            break;
        }
    }
    echo json_encode(["html" => $html, "status" => $status, "ai_on" => $ai_on] + ($ai_data ?? []), JSON_UNESCAPED_UNICODE);
    exit;
}

/* --- API ДЛЯ ОБНОВЛЕНИЯ СПИСКА ТИКЕТОВ СЛЕВА --- */
if (isset($_GET['ajax_ticket_list'])) {
    header('Content-Type: application/json');
    if (!can("support.view")) { echo json_encode(["html" => ""]); exit; }
    echo json_encode(["html" => render_ticket_list(load_json("tickets.json", []), $_GET['active_id'] ?? null, load_json("settings.json", []))]);
    exit;
}

/* --------------------------------- */
/* ВКЛАДКИ: [название, иконка, нужное право (null — всем в команде), группа в меню] */
$TABS = [
    "home"      => ["Главная",        "🏠", null,                                           "Основное"],
    "schedule"  => ["Расписание",     "📅", null,                                           "Основное"],
    "chat"      => ["Чат команды",    "💬", "chat.view",                                    "Основное"],
    "goals"     => ["Цели",           "🎯", "goals.view",                                   "Основное"],
    "fines"     => ["Штрафы",         "💸", null,                                           "Основное"],
    "files"     => ["Файлы",          "📁", "files.view",                                   "Основное"],
    "team"      => ["Команда и роли", "👥", "team.view",                                    "Основное"],
    "orders"    => ["Заказы сайтов",  "🛒", null,                                           "Работа"],
    "apps"      => ["Заявки",         "📝", "apps.view",                                    "Работа"],
    "projects"  => ["Проекты",        "🧩", "projects.manage",                              "Работа"],
    "support"   => ["Тикеты",         "🎧", "support.view",                                 "Работа"],
    "messages"  => ["Почта",          "✉️", "mail.view",                                    "Работа"],
    "bot"       => ["Telegram-бот",   "🤖", ["bot.tickets", "bot.manage", "settings.manage"], "Работа"],
    "discord"   => ["Discord",        "💬", "settings.manage",                              "Работа"],
    "directors" => ["Директора школ", "🏫", "directors.manage",                             "Работа"],
    "rai"       => ["Rai: подписки",  "✨", ["users.manage", "settings.manage"],            "Работа"],
    "rai_rules" => ["Rai: правила",   "🚫", ["users.manage"],                               "Работа"],
    "rai_push"  => ["Rai: уведомления", "🔔", ["users.manage"],                             "Работа"],
    "rai_sale"  => ["Rai: скидки",    "🏷", ["settings.manage"],                            "Работа"],
    "reviews"   => ["Отзывы",         "⭐", null,                                           "Контент"],
    "blog"      => ["Блог",           "📰", "blog.manage",                                  "Контент"],
    "leaks"     => ["Сливы",          "💧", "leaks.manage",                                 "Контент"],
    "themes"    => ["Темы сайта",     "🎭", "themes.manage",                                "Контент"],
    "recruit"   => ["Набор",          "📣", "recruit.manage",                               "Контент"],
    "gold"      => ["Золотой билет",  "🎫", ["gold.chat", "gold.manage"],                   "Контент"],
    "users"     => ["Пользователи",   "🗂️", "users.view",                                   "Управление"],
    "perms"     => ["Права ролей",    "🔐", null,                                           "Управление"],
    "bans"      => ["Баны",           "⛔", "bans.manage",                                  "Управление"],
    "blacklist" => ["Чёрный список",  "⚫", "bans.manage",                                  "Управление"],
    "settings"  => ["Настройки",      "⚙️", "settings.manage",                              "Управление"],
    "logs"      => ["Логи",           "📜", "logs.view",                                    "Управление"],
];
function tab_allowed($key) {
    global $TABS;
    if (!isset($TABS[$key])) return false;
    if (($own = so_tab_allowed($key)) !== null) return $own;
    $need = $TABS[$key][2];
    if ($need === null) return true;
    return is_array($need) ? can_any($need) : can($need);
}

$tab = $_GET["tab"] ?? "home";
if (!tab_allowed($tab)) {
    if ($_SERVER["REQUEST_METHOD"] !== "POST") {
        if (isset($TABS[$tab])) flash("Раздел «" . $TABS[$tab][0] . "» недоступен для роли «" . rt_role_label($role, $my_direction) . "».", "error");
        header("Location: admin.php?tab=home"); exit;
    }
}

so_admin_handle(); // AJAX чата заказов, файлы и действия вкладок «Расписание», «Заказы сайтов», «Отзывы»

$applications = load_json("applications.json", []);
$leaks        = load_json("leaks.json", []);
$goals        = load_json("goals.json", []);
$fines        = load_json("fines.json", []);
$files_data   = load_json("files.json", []); 
$settings     = load_json("settings.json", ["site_name"=>"Rteam","accent"=>"#ff2a2a","neon"=>true,"animations"=>true,"recruit_open"=>true,"bot_token"=>""]);
$THEME_CATALOG = rteam_theme_catalog();
$theme_settings = load_json("theme.json", ["active" => "tennis", "enabled" => false, "text" => ""]);
if (!isset($THEME_CATALOG[$theme_settings["active"]])) $theme_settings["active"] = array_key_first($THEME_CATALOG);
$squid_game = load_json("squid_game.json", ["paused" => false, "progress" => []]);
$logs         = load_json("logs.json", []);
$messages     = load_json("messages.json", []);
$tickets      = load_json("tickets.json", []); 
$blog         = load_json("blog.json", []);
$questions    = load_json("questions.json", ["team" => ["Ник", "Email"],"admin" => ["Ник", "Email"]]);
$chat_data    = load_json("chat.json", ["messages" => [], "pinned_id" => null]);
$projects_data = load_json("projects.json", []);
$bans         = load_json("bans.json", []);
$geoblock_settings  = load_json("geoblock.json", ["enabled" => true, "countries" => ["UA", "PL", "LT", "LV", "EE"]]);
$blocked_attempts   = load_json("blocked_attempts.json", []);
$blacklist    = load_json("blacklist.json", []);
$bot_gws      = load_json("bot_giveaways.json", []);
$director_requests = load_json("director_requests.json", []);
$__migrationResult = dl_migrate_legacy_if_needed(); // разово переносит старые файловые данные в MySQL
$gold_chats   = load_json("gold_chats.json", []);
$gold_services = load_json("gold_services.json", []);

/* АВТОМАТИЧЕСКОЕ ИСКЛЮЧЕНИЕ ЗА НЕОПЛАЧЕННЫЕ ШТРАФЫ */
$users_changed = false;
foreach ($fines as &$fine) {
    if (empty($fine["paid"]) && time() - strtotime($fine["issue_date"]) >= 30 * 86400) {
        $fine_user = $fine["user"];
        if (isset($users[$fine_user]) && ($users[$fine_user]["role"] ?? "Пользователь") !== "Пользователь" && !rt_is_owner($fine_user)) {
            $users[$fine_user]["role"] = "Пользователь";
            unset($users[$fine_user]["direction"]);
            $users_changed = true;
            $logs[] = ["time" => date("Y-m-d H:i:s"),"type" => "fine_ban","msg"  => "Пользователь {$fine_user} автоматически исключен."];
        }
    }
}
unset($fine);
if ($users_changed) { save_json("users.json", $users); save_json("logs.json", $logs); }

/* ВОПРОС «НАПРАВЛЕНИЕ» В ЗАЯВКЕ
   В заявке «Команда» на сайте он показывается списком (Кодер / Разработчик /
   Тестер), а при одобрении направление само записывается стажёру.
   Добавляется в конец списка вопросов один раз: если потом удалить его
   во вкладке «Набор», он не вернётся. */
if (empty($settings["direction_q_added"])) {
    $qfile = load_json("questions.json", []);
    if (empty($qfile["team"]) || empty($qfile["admin"])) {
        // файла ещё нет — берём те же вопросы, что сайт показывает по умолчанию
        $qfile = [
            "team"  => ["Ник", "Email", "Возраст", "Навыки", "Почему хотите в команду", "Опыт", "Discord"],
            "admin" => ["Ник", "Email", "Возраст", "Опыт модерации / управления", "Какие проекты модерировали", "Почему хотите быть администратором", "Готовность быть активным (да/нет)", "Discord"],
        ];
    }
    $hasDirection = false;
    foreach ($qfile["team"] as $q) if (mb_stripos($q, "направлен") !== false) $hasDirection = true;
    if (!$hasDirection) $qfile["team"][] = "Направление";
    save_json("questions.json", $qfile);
    $questions = $qfile;
    $settings["direction_q_added"] = true;
    save_json("settings.json", $settings);
}

/* ПОИСК И СОРТИРОВКА ДЛЯ ЗАЯВОК */
$search = trim($_GET["search"] ?? ""); $sort = $_GET["sort"] ?? "newest";
if ($tab === "apps") {
    if ($search !== "") {
        $s = mb_strtolower($search);
        $applications = array_filter($applications, function($app) use ($s) {
            $email = ""; $nick = "";
            foreach ($app["answers"] as $row) {
                if (mb_stripos($row["q"], "email") !== false) $email = $row["a"];
                if (mb_stripos($row["q"], "Ник") !== false) $nick = $row["a"];
            }
            return mb_strpos(mb_strtolower($email." ".$nick), $s) !== false;
        });
    }
    usort($applications, function($a, $b) use ($sort) {
        if ($sort === "oldest") return $a["id"] <=> $b["id"];
        if ($sort === "status") {
            $order = ["new" => 0, "viewed" => 1, "resolved_accept" => 2, "resolved_decline" => 3];
            $sa = $order[$a["status"] ?? "new"] ?? 99; $sb = $order[$b["status"] ?? "new"] ?? 99;
            if ($sa === $sb) return $b["id"] <=> $a["id"]; return $sa <=> $sb;
        }
        return $b["id"] <=> $a["id"];
    });
    /* РЕШЕНИЕ ПО ЗАЯВКЕ: «Принять» сразу выдаёт роль «Стажёр» и направление,
       на которое человек подавал, и отправляет письмо с решением. */
    if ($_SERVER["REQUEST_METHOD"] === "POST" && ($_POST["action"] ?? "") === "app_decide") {
        $id = $_POST["id"] ?? ""; $decision = $_POST["decision"] ?? ""; $comment = trim($_POST["comment"] ?? "");
        $back = "admin.php?tab=apps&search=" . urlencode($search) . "&sort=" . urlencode($sort);
        $all = load_json("applications.json", []);
        $idx = null;
        foreach ($all as $k => $a) if ((string)$a["id"] === (string)$id) { $idx = $k; break; }
        if ($idx === null || !in_array($decision, ["accept", "decline"], true)) { flash("Заявка не найдена.", "error"); header("Location: $back"); exit; }
        $app = $all[$idx];
        $nick = rt_app_answer($app, "Ник");
        $email = rt_app_answer($app, "email");

        if ($decision === "accept") {
            $login = trim($_POST["login"] ?? "");
            $direction = $_POST["direction"] ?? "";
            if (!isset(rt_directions()[$direction])) $direction = rt_app_direction($app);
            if ($login === "" || !isset($users[$login])) {
                flash("Аккаунт «" . ($login !== "" ? $login : $nick) . "» не найден. Укажите логин, под которым человек зарегистрирован на сайте.", "error");
                header("Location: $back"); exit;
            }
            $curRole = $users[$login]["role"] ?? "Пользователь";
            $roleNote = "";
            if (rt_is_owner($login) || (rt_level($curRole) > rt_level(RT_TRAINEE_ROLE))) {
                $roleNote = "роль не менялась — уже «" . rt_role_label($curRole, $users[$login]["direction"] ?? "") . "»";
            } else {
                $users[$login]["role"] = RT_TRAINEE_ROLE;
                $users[$login]["direction"] = $direction;
                $users[$login]["role_by"] = $user;
                $users[$login]["role_at"] = date("Y-m-d H:i:s");
                save_json("users.json", $users);
                $roleNote = "выдана роль «" . rt_role_label(RT_TRAINEE_ROLE, $direction) . "»";
            }
            $all[$idx]["status"] = "resolved_accept";
            $all[$idx]["account"] = $login;
            $all[$idx]["direction"] = $direction;
            $mailText = "Здравствуйте" . ($nick !== "" ? ", $nick" : "") . "!\n\nВаша заявка «" . ($app["type"] ?? "") . "» в Rteam одобрена.\n"
                . "Вам выдана роль «Стажёр», направление: $direction.\n"
                . "Войдите на сайт под своим аккаунтом ($login) — откроется админ-панель с чатом команды.\n";
        } else {
            $all[$idx]["status"] = "resolved_decline";
            $roleNote = "отказ";
            $mailText = "Здравствуйте" . ($nick !== "" ? ", $nick" : "") . "!\n\nК сожалению, ваша заявка «" . ($app["type"] ?? "") . "» в Rteam отклонена.\n";
        }
        if ($comment !== "") $mailText .= "\nКомментарий: $comment\n";
        $mailText .= "\nС уважением, Rteam";
        $all[$idx]["decided_by"] = $user;
        $all[$idx]["decided_at"] = date("Y-m-d H:i:s");
        $all[$idx]["comment"] = $comment;
        save_json("applications.json", $all);

        $mailed = false;
        if (filter_var($email, FILTER_VALIDATE_EMAIL)) {
            $mailed = @mail($email, "Решение по заявке — Rteam", $mailText, "From: team@rteam.info\r\nContent-Type: text/plain; charset=UTF-8\r\n");
        }
        $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "application", "msg" => "$user " . ($decision === "accept" ? "принял" : "отклонил") . " заявку #{$app["id"]} (" . ($nick !== "" ? $nick : "без ника") . "): $roleNote."];
        save_json("logs.json", $logs);
        flash(($decision === "accept" ? "Заявка принята: $roleNote." : "Заявка отклонена.") . ($mailed ? " Письмо отправлено." : ""), $decision === "accept" ? "success" : "info");
        header("Location: $back"); exit;
    }

    if ($_SERVER["REQUEST_METHOD"] === "POST" && ($_POST["action"] ?? "") === "mark_viewed") {
        $id = $_POST["id"] ?? ""; $all = load_json("applications.json", []);
        foreach ($all as &$app) { if ((string)$app["id"] === (string)$id && ($app["status"] ?? "new") === "new") { $app["status"] = "viewed"; break; } } unset($app);
        save_json("applications.json", $all); header("Location: admin.php?tab=apps&search=".urlencode($search)."&sort=".urlencode($sort)); exit;
    }
}

/* ==== RAI: НАЧАЛО (вкладка «Rai: подписки», собрано make_admin.py) ==== */
// Ключ связи с rai.rteam.info — тот же, что ADMIN_API_KEY в config.php Rai. В GitHub его не выкладываем:
// он лежит на сервере в _rai_key.php (строка define('RAI_ADMIN_KEY', '…');) или вписывается во вкладке «Rai» → «Подключение».
if (!defined('RAI_ADMIN_KEY') && is_file(__DIR__ . '/_rai_key.php')) require_once __DIR__ . '/_rai_key.php';

/*
 * Вкладки «Rai: подписки» и «Rai: правила» админ-панели rteam.info (встраиваются прямо в admin.php — отдельный файл не нужен).
 * Всё остальное — на rai.rteam.info (admin_api.php).
 *   — статистика: пользователи, подписчики, доход, нейросеть сегодня;
 *   — выдать или снять подписку по логину пользователя Rai;
 *   — поиск пользователей, список подписчиков, платежи Platega, журнал;
 *   — цены, лимиты и описания тарифов.
 *
 * Связь с rai.rteam.info — через admin_api.php, каждый запрос подписан
 * ключом (HMAC-SHA256). Ключ — одинаковый в config.php на rai.rteam.info (ADMIN_API_KEY) и здесь
 * (вкладка «Rai» → «Подключение», хранится в settings.json; или константа RAI_ADMIN_KEY).
 *
 * Права: смотреть и выдавать подписки — «users.manage», подключение и тарифы — «settings.manage».
 */

const RAI_PLAN_KEYS = ['plus', 'premium', 'ultra'];
const RAI_DEFAULT_URL = 'https://rai.rteam.info/admin_api.php';

function rai_conf() {
    global $settings;
    $url = trim((string)($settings['rai_api_url'] ?? '')) ?: (defined('RAI_ADMIN_URL') ? RAI_ADMIN_URL : RAI_DEFAULT_URL);
    $key = trim((string)($settings['rai_api_key'] ?? '')) ?: (defined('RAI_ADMIN_KEY') ? RAI_ADMIN_KEY : (getenv('RAI_ADMIN_KEY') ?: ''));
    return ['url' => $url, 'key' => $key, 'ready' => strlen($key) >= 32];
}

/** Подписанный запрос к rai.rteam.info/admin_api.php. Возвращает массив ответа (ok, error, …). */
function rai_api($action, array $data = [], $timeout = 15) {
    global $user;
    $c = rai_conf();
    if (!$c['ready']) return ['ok' => false, 'error' => 'Не задан ключ подключения (от 32 символов).'];
    $body = json_encode(['action' => $action, 'nonce' => bin2hex(random_bytes(12)), 'admin' => (string)$user] + $data, JSON_UNESCAPED_UNICODE);
    $time = time();
    $headers = ['Content-Type: application/json', 'X-Rai-Time: ' . $time,
                'X-Rai-Signature: ' . hash_hmac('sha256', $time . "\n" . $body, $c['key'])];
    if (function_exists('curl_init')) {
        $ch = curl_init($c['url']);
        curl_setopt_array($ch, [CURLOPT_POST => true, CURLOPT_POSTFIELDS => $body, CURLOPT_HTTPHEADER => $headers,
                                CURLOPT_RETURNTRANSFER => true, CURLOPT_CONNECTTIMEOUT => min(6, $timeout), CURLOPT_TIMEOUT => $timeout]);
        $raw = curl_exec($ch);
        $code = (int)curl_getinfo($ch, CURLINFO_HTTP_CODE);
        $err = curl_error($ch);
        curl_close($ch);
    } else {
        $ctx = stream_context_create(['http' => ['method' => 'POST', 'header' => implode("\r\n", $headers), 'content' => $body,
                                                 'timeout' => $timeout, 'ignore_errors' => true]]);
        $raw = @file_get_contents($c['url'], false, $ctx);
        $code = isset($http_response_header[0]) && preg_match('/\s(\d{3})\s/', $http_response_header[0], $m) ? (int)$m[1] : 0;
        $err = $raw === false ? 'нет связи' : '';
    }
    $res = json_decode((string)$raw, true);
    if (!is_array($res)) {
        return ['ok' => false, 'error' => $code ? "rai.rteam.info ответил кодом $code (нет admin_api.php или ошибка PHP)" : 'Нет связи с rai.rteam.info' . ($err ? ": $err" : '')];
    }
    return $res;
}

/** Статистика для главной админки: кэш на минуту и короткое ожидание, чтобы главная не тормозила. */
function rai_cached_stats() {
    if (!rai_conf()['ready']) return null;
    $cache = $_SESSION['rai_stats_cache'] ?? null;
    if ($cache && time() - $cache['t'] < 60) return $cache['data'];
    $r = rai_api('stats', [], 4);
    $data = !empty($r['ok']) ? ['subscribers' => $r['subscribers'], 'revenue_month' => $r['revenue_month'], 'expiring' => $r['expiring'] ?? 0,
                                 'neuro_today' => $r['neuro_today'], 'pending' => $r['pending'] ?? 0,
                                 'violations_new' => $r['violations_new'] ?? 0] : null;
    $_SESSION['rai_stats_cache'] = ['t' => time(), 'data' => $data];
    return $data;
}

/** Выгрузка подписчиков в CSV (Excel): ?tab=rai&export=csv (вызывается до вывода страницы). */
function rai_admin_export() {
    if (($_GET['export'] ?? '') !== 'csv' || !can('users.manage')) return;
    $r = rai_api('subs_all', ['everyone' => !empty($_GET['all'])]);
    if (empty($r['ok'])) { flash('Не удалось выгрузить: ' . ($r['error'] ?? '?'), 'error'); rai_back(); }
    header('Content-Type: text/csv; charset=utf-8');
    header('Content-Disposition: attachment; filename="rai-' . (!empty($_GET['all']) ? 'users' : 'subscribers') . '-' . date('Y-m-d') . '.csv"');
    $out = fopen('php://output', 'w');
    fwrite($out, "\xEF\xBB\xBF");  // чтобы Excel понял UTF-8
    fputcsv($out, ['Логин', 'Имя', 'Почта', 'Тариф', 'До', 'Откуда', 'Комментарий', 'Нейросеть сегодня', 'Зарегистрирован', 'Последний вход'], ';');
    foreach ($r['users'] as $u) {
        fputcsv($out, [$u['login'], $u['name'], $u['email'], $u['plan_name'], $u['until'] ? date('d.m.Y', $u['until']) : '',
                       $u['source'] === 'platega' ? 'оплата' : ($u['source'] === 'admin' ? 'выдано' : ''), $u['note'], $u['neuro_today'],
                       $u['created'] ? date('d.m.Y', $u['created']) : '', $u['last_login'] ? date('d.m.Y H:i', $u['last_login']) : ''], ';');
    }
    exit;
}

function rai_log($msg) {
    global $logs;
    $logs[] = ['time' => date('Y-m-d H:i:s'), 'type' => 'rai', 'msg' => $msg];
    save_json('logs.json', $logs);
}

function rai_back($extra = '') {
    header('Location: admin.php?tab=rai' . $extra);
    exit;
}

/** Обработка форм вкладки (вызывается из admin.php при POST на ?tab=rai). */
function rai_admin_post() {
    global $settings, $user;
    $action = (string)($_POST['action'] ?? '');

    if ($action === 'rai_save') {
        $url = trim((string)($_POST['rai_api_url'] ?? ''));
        if ($url !== '' && !preg_match('~^https?://[^\s]+$~', $url)) { flash('Адрес должен начинаться с https://', 'error'); rai_back(); }
        $settings['rai_api_url'] = $url;
        $key = trim((string)($_POST['rai_api_key'] ?? ''));
        if ($key !== '') {
            if (strlen($key) < 32) { flash('Ключ слишком короткий — нужно от 32 символов.', 'error'); rai_back(); }
            $settings['rai_api_key'] = $key;
        }
        if (!empty($_POST['clear_key'])) unset($settings['rai_api_key']);
        save_json('settings.json', $settings);
        $ping = rai_api('ping');
        rai_log("$user изменил подключение к Rai");
        flash($ping['ok'] ?? false ? '✅ Подключено к ' . ($ping['site'] ?? 'rai.rteam.info') : 'Сохранено, но связи нет: ' . ($ping['error'] ?? '?'), ($ping['ok'] ?? false) ? 'success' : 'error');
        rai_back();
    }

    if ($action === 'rai_check') {
        $ping = rai_api('ping');
        flash(($ping['ok'] ?? false) ? '✅ Связь есть. Platega: ' . (!empty($ping['platega']) ? 'подключена' : 'ещё не настроена') : 'Нет связи: ' . ($ping['error'] ?? '?'),
              ($ping['ok'] ?? false) ? 'success' : 'error');
        rai_back();
    }

    unset($_SESSION['rai_stats_cache']);  // после любого действия главная покажет свежие цифры

    if ($action === 'rai_grant') {
        $login = strtolower(trim((string)($_POST['login'] ?? '')));
        $plan = (string)($_POST['plan'] ?? '');
        $forever = (string)($_POST['days'] ?? '') === 'forever';
        $days = $forever ? 36500 : ((int)($_POST['days'] ?? 0) === -1 ? (int)($_POST['days_custom'] ?? 0) : (int)($_POST['days'] ?? 0));
        $note = trim((string)($_POST['note'] ?? ''));
        if ($login === '' || !in_array($plan, RAI_PLAN_KEYS, true) || $days < 1) { flash('Укажите логин, тариф и срок.', 'error'); rai_back(); }
        $r = rai_api('grant', ['login' => $login, 'plan' => $plan, 'days' => $days, 'forever' => $forever, 'note' => $note]);
        if (!empty($r['ok'])) {
            $u = $r['user'];
            rai_log("$user выдал подписку Rai «{$u['plan_name']}» пользователю {$u['login']} " . ($forever ? 'навсегда' : "на $days дн.") . ($note !== '' ? " ($note)" : ''));
            flash("🎁 {$u['login']}: «{$u['plan_name']}» " . ($forever ? 'навсегда' : 'до ' . date('d.m.Y', (int)$u['until'])), 'success');
        } else {
            flash($r['error'] ?? 'Не получилось выдать подписку', 'error');
        }
        rai_back('&q=' . urlencode($login));
    }

    if ($action === 'rai_role') {
        $login = strtolower(trim((string)($_POST['login'] ?? '')));
        $role = (string)($_POST['role'] ?? '');
        $r = rai_api('role_set', ['login' => $login, 'role' => $role]);
        if (!empty($r['ok'])) {
            $name = $r['user']['role_name'] ?? '';
            rai_log("$user " . ($name ? "отметил в Rai $login как «{$name}»" : "снял роль в Rai у $login"));
            flash($name ? "👑 {$login} — «{$name}»: значок у имени и полный доступ навсегда." : "Роль у {$login} снята.", 'success');
        } else {
            flash($r['error'] ?? 'Не получилось', 'error');
        }
        rai_back('&q=' . urlencode($login));
    }

    if ($action === 'rai_revoke') {
        $login = strtolower(trim((string)($_POST['login'] ?? '')));
        $r = rai_api('revoke', ['login' => $login]);
        if (!empty($r['ok'])) { rai_log("$user снял подписку Rai у $login"); flash("Подписка $login снята — теперь тариф «Старт».", 'success'); }
        else flash($r['error'] ?? 'Не получилось', 'error');
        rai_back();
    }

    if ($action === 'rai_order_check') {
        $r = rai_api('order_check', ['id' => (string)($_POST['id'] ?? '')]);
        $st = $r['order']['status'] ?? '';
        if (!empty($r['ok'])) flash($st === 'paid' ? '✅ Оплата подтверждена — подписка включена.' : ($st === 'canceled' ? 'Платёж отменён в Platega.' : 'Platega: ещё не оплачено.'), $st === 'paid' ? 'success' : 'info');
        else flash($r['error'] ?? 'Не получилось проверить', 'error');
        if ($st === 'paid') rai_log("$user подтвердил оплату заказа Rai " . ($_POST['id'] ?? ''));
        unset($_SESSION['rai_stats_cache']);
        rai_back('#orders');
    }

    if ($action === 'rai_platega') {
        $r = rai_api('platega_save', ['id' => (string)($_POST['platega_id'] ?? ''), 'secret' => (string)($_POST['platega_secret'] ?? ''),
                                      'method' => (int)($_POST['platega_method'] ?? 0), 'clear' => !empty($_POST['platega_clear'])]);
        if (!empty($r['ok'])) { rai_log("$user изменил ключи Platega для Rai"); flash(!empty($r['ready']) ? '✅ Platega подключена — оплата на сайте работает.' : 'Сохранено. Для оплаты нужны и Merchant ID, и секрет.', 'success'); }
        else flash($r['error'] ?? 'Не получилось сохранить', 'error');
        rai_back('#platega');
    }

    if ($action === 'rai_plans') {
        $in = ['guest' => ['neuro_day' => (int)($_POST['guest_day'] ?? 5)]];
        foreach (array_merge(['free'], RAI_PLAN_KEYS) as $k) {
            $row = $_POST['plan'][$k] ?? null;
            if (!is_array($row)) continue;
            $in[$k] = [
                'name' => (string)($row['name'] ?? ''), 'tagline' => (string)($row['tagline'] ?? ''),
                'neuro_day' => (int)($row['neuro_day'] ?? 0),
                'features' => array_values(array_filter(array_map('trim', preg_split('/\R/', (string)($row['features'] ?? ''))))),
            ];
            if ($k !== 'free') $in[$k]['price'] = (int)($row['price'] ?? 0);
        }
        $r = rai_api('plans_save', ['plans' => $in]);
        if (!empty($r['ok'])) { rai_log("$user изменил тарифы Rai"); flash('Тарифы сохранены — на сайте уже новые цены.', 'success'); }
        else flash($r['error'] ?? 'Не получилось сохранить', 'error');
        rai_back('#plans');
    }

    if ($action === 'rai_plans_reset') {
        $r = rai_api('plans_reset');
        if (!empty($r['ok'])) { rai_log("$user сбросил тарифы Rai"); flash('Тарифы сброшены к стандартным.', 'success'); }
        else flash($r['error'] ?? 'Не получилось', 'error');
        rai_back('#plans');
    }

    rai_back();
}

function rai_h($s) { return htmlspecialchars((string)$s, ENT_QUOTES, 'UTF-8'); }
function rai_rub($n) { return number_format((int)$n, 0, ',', ' ') . ' ₽'; }
function rai_date($ts) { return $ts ? ((int)$ts >= 4070908800 ? 'навсегда' : date('d.m.Y', (int)$ts)) : '—'; }
const RAI_ROLES = ['creator' => '👑 Создатель', 'developer' => '🛠 Разработчик'];
function rai_left($ts) {
    if (!$ts) return '';
    if ((int)$ts >= 4070908800) return '♾ бессрочно';
    $d = (int)ceil(((int)$ts - time()) / 86400);
    return $d <= 0 ? 'истекает сегодня' : 'ещё ' . $d . ' дн.';
}

/** Содержимое вкладки «Rai». */
function rai_admin_render() {
    $c = rai_conf();
    $can_users = can('users.manage');
    $can_settings = can('settings.manage');
    $stats = $c['ready'] ? rai_api('stats') : ['ok' => false, 'error' => null];
    $ok = !empty($stats['ok']);
    $plans = $ok ? $stats['plans'] : [];
    $q = trim((string)($_GET['q'] ?? ''));
    $found = ($ok && $q !== '') ? rai_api('search', ['q' => $q]) : null;
    $status_names = ['paid' => ['Оплачен', 'badge-acc'], 'pending' => ['Ждёт оплату', 'badge-warn'], 'new' => ['Создан', 'badge-viewed'],
                     'canceled' => ['Отменён', 'badge-dec'], 'error' => ['Ошибка', 'badge-dec'], 'refunded' => ['Возврат', 'badge-dec'],
                     'underpaid' => ['Недоплата', 'badge-warn']];
    $plan_name = function ($k) use ($plans) { return $plans[$k]['name'] ?? $k; };
    ?>
    <style>
      .rai-hero { display: flex; gap: 16px; align-items: center; flex-wrap: wrap; padding: 18px 20px; border-radius: 16px; border: 1px solid var(--line);
        background: radial-gradient(120% 140% at 100% 0%, rgba(255, 45, 45, .18), transparent 55%), radial-gradient(100% 120% at 0% 100%, rgba(139, 92, 255, .14), transparent 60%), var(--panel); }
      .rai-logo { width: 52px; height: 52px; border-radius: 15px; display: grid; place-items: center; font-weight: 800; font-size: 24px; color: #fff;
        background: linear-gradient(135deg, #ff2d2d, #ff3d81 55%, #8b5cff); box-shadow: 0 10px 30px -10px rgba(255, 45, 45, .7); }
      .rai-hero h2 { margin: 0; } .rai-hero p { margin: 4px 0 0; }
      .rai-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 14px; }
      .rai-grid .card { margin-top: 14px; }
      .rai-plans { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 12px; }
      .rai-plan { border: 1px solid var(--line); border-radius: 12px; padding: 12px; background: var(--panel-2, #12121a); }
      .rai-plan textarea { min-height: 96px; }
      .rai-plan h4 { margin: 0 0 6px; }
      .rai-days { display: flex; gap: 6px; flex-wrap: wrap; margin: 4px 0 8px; }
      .rai-days label { display: inline-flex; align-items: center; gap: 6px; padding: 6px 10px; border-radius: 999px; border: 1px solid var(--line); cursor: pointer; font-size: 13px; }
      .rai-days input { width: auto; margin: 0; }
      .rai-days label:has(input:checked) { border-color: #ff3d81; background: rgba(255, 61, 129, .12); }
      .rai-chart { display: flex; align-items: flex-end; gap: 3px; height: 120px; margin-top: 14px; padding-bottom: 20px; position: relative; }
      .rai-chart i { flex: 1; min-height: 2px; border-radius: 4px 4px 1px 1px; background: linear-gradient(180deg, #ff3d81, #ff2d2d); opacity: .85;
        position: relative; transform-origin: bottom; animation: raiGrow .8s cubic-bezier(.2, .8, .2, 1) both; }
      .rai-chart i:hover { opacity: 1; filter: brightness(1.2); }
      .rai-chart i.zero { background: var(--line); opacity: .6; }
      .rai-chart.violet i:not(.zero) { background: linear-gradient(180deg, #8b5cff, #ff3d81); }
      .rai-chart span { position: absolute; bottom: 0; font-size: 11px; color: var(--muted); }
      @keyframes raiGrow { from { transform: scaleY(0); } }
      tr.rai-soon td { background: rgba(245, 158, 11, .07); }
    </style>

    <div class="rai-hero">
      <div class="rai-logo">R</div>
      <div style="flex:1; min-width:220px;">
        <h2>Rai — подписки</h2>
        <p class="muted">rai.rteam.info: тарифы, оплата через Platega и выдача подписок вручную по логину Rai.</p>
      </div>
      <div class="chips">
        <span class="chip <?= $ok ? 'on' : '' ?>"><?= $ok ? '✓ Подключено' : ($c['ready'] ? '✗ Нет связи' : 'Не настроено') ?></span>
        <?php if ($ok): ?><span class="chip <?= !empty($stats['platega']) ? 'on' : '' ?>">Platega: <?= !empty($stats['platega']) ? 'подключена' : 'не настроена' ?></span><?php endif; ?>
        <a class="btn ghost sm" href="https://rai.rteam.info/" target="_blank" rel="noopener">Открыть сайт ↗</a>
      </div>
    </div>

    <?php if (!$c['ready']): ?>
      <div class="callout warn"><span class="c-ico">🔑</span><div><b>Подключите Rai.</b> Придумайте длинный случайный ключ (от 32 символов),
        впишите его в <code>config.php</code> на rai.rteam.info (<code>ADMIN_API_KEY</code>) и сюда — в «Подключение» ниже.</div></div>
    <?php elseif (!$ok): ?>
      <div class="callout danger"><span class="c-ico">⚠️</span><div><b>Нет связи с rai.rteam.info:</b> <?= rai_h($stats['error'] ?? '?') ?></div></div>
    <?php endif; ?>

    <?php if ($ok): ?>
      <div class="kpi-grid">
        <div class="kpi"><div class="k-ico">👤</div><div class="k-num"><?= (int)$stats['users'] ?></div><div class="k-lbl">аккаунтов · +<?= (int)$stats['new_week'] ?> за неделю</div></div>
        <div class="kpi hot"><div class="k-ico">⭐</div><div class="k-num"><?= (int)$stats['subscribers'] ?></div><div class="k-lbl">активных подписок</div></div>
        <div class="kpi"><div class="k-ico">📈</div><div class="k-num"><?= rai_rub($stats['mrr']) ?></div><div class="k-lbl">в месяц по текущим подпискам</div></div>
        <div class="kpi"><div class="k-ico">💳</div><div class="k-num"><?= rai_rub($stats['revenue_month']) ?></div><div class="k-lbl">оплачено в этом месяце · всего <?= rai_rub($stats['revenue_total']) ?></div></div>
        <div class="kpi"><div class="k-ico">🧠</div><div class="k-num"><?= (int)$stats['neuro_today'] ?></div><div class="k-lbl">сообщений нейросети сегодня · <?= (int)$stats['neuro_people_today'] ?> чел.</div></div>
        <?php $dl = $stats['downloads'] ?? []; $dl_mac = (int)($dl['mac'] ?? 0) + (int)($dl['mac-x64'] ?? 0); $dl_lin = (int)($dl['linux'] ?? 0) + (int)($dl['deb'] ?? 0); ?>
        <div class="kpi"><div class="k-ico">💻</div><div class="k-num"><?= (int)array_sum($dl) ?></div><div class="k-lbl">скачиваний приложения<?= !empty($stats['app_version']) ? ' · версия ' . rai_h($stats['app_version']) : '' ?> · Windows <?= (int)($dl['win'] ?? 0) ?>, Mac <?= $dl_mac ?>, Linux <?= $dl_lin ?></div></div>
        <?php foreach (RAI_PLAN_KEYS as $k): ?>
          <div class="kpi"><div class="k-ico"><?= ['plus' => '✦', 'premium' => '💎', 'ultra' => '🚀'][$k] ?></div><div class="k-num"><?= (int)($stats['by_plan'][$k] ?? 0) ?></div><div class="k-lbl">«<?= rai_h($plan_name($k)) ?>» · <?= rai_rub($plans[$k]['price'] ?? 0) ?>/мес</div></div>
        <?php endforeach; ?>
        <?php if (!empty($stats['expiring'])): ?><a class="kpi hot" href="#subs"><div class="k-ico">⏳</div><div class="k-num"><?= (int)$stats['expiring'] ?></div><div class="k-lbl">подписок кончаются за 3 дня</div></a><?php endif; ?>
        <?php if (!empty($stats['pending'])): ?><a class="kpi" href="#orders"><div class="k-ico">🕓</div><div class="k-num"><?= (int)$stats['pending'] ?></div><div class="k-lbl">платежей ждут оплату</div></a><?php endif; ?>
      </div>

      <div class="rai-grid">
        <?php foreach ([['💰 Оплаты за 30 дней', $stats['revenue_days'] ?? [], '', true], ['🧠 Нейросеть за 7 дней', $stats['neuro_days'] ?? [], ' violet', false]] as [$title, $series, $cls, $money]):
          $max = max(1, max($series ?: [0])); $sum = array_sum($series); ?>
          <div class="card">
            <h3><?= $title ?> <span class="muted" style="font-weight:500; font-size:13px;">· <?= $money ? rai_rub($sum) : $sum . ' сообщений' ?></span></h3>
            <div class="rai-chart<?= $cls ?>" role="img" aria-label="<?= rai_h($title) ?>">
              <?php $i = 0; foreach ($series as $day => $v): ?>
                <i class="<?= $v ? '' : 'zero' ?>" style="height:<?= $v ? max(4, round($v * 100 / $max)) : 2 ?>%; animation-delay:<?= $i++ * 0.02 ?>s" title="<?= date('d.m', strtotime($day)) ?>: <?= $money ? rai_rub($v) : (int)$v ?>"></i>
              <?php endforeach; ?>
              <?php if ($series): ?><span style="left:0"><?= date('d.m', strtotime(array_key_first($series))) ?></span><span style="right:0">сегодня</span><?php endif; ?>
            </div>
          </div>
        <?php endforeach; ?>
      </div>
    <?php endif; ?>

    <div class="rai-grid">
      <?php if ($ok && $can_users): ?>
      <div class="card">
        <h3>🎁 Выдать подписку</h3>
        <p class="meta">Логин — как в кабинете rai.rteam.info (или почта). Тот же тариф продлевается, другой — начинается сейчас.</p>
        <form method="POST" action="?tab=rai">
          <input type="hidden" name="action" value="rai_grant">
          <label>Логин пользователя Rai</label>
          <input type="text" name="login" value="<?= rai_h($q) ?>" placeholder="например: anya" required autocomplete="off" list="raiLogins">
          <label>Тариф</label>
          <select name="plan"><?php foreach (RAI_PLAN_KEYS as $k): ?><option value="<?= $k ?>"<?= $k === 'premium' ? ' selected' : '' ?>><?= rai_h($plan_name($k)) ?> — <?= rai_rub($plans[$k]['price'] ?? 0) ?>/мес · <?= !empty($plans[$k]['neuro_day']) ? (int)$plans[$k]['neuro_day'] . ' сообщ./день' : 'без лимита' ?></option><?php endforeach; ?></select>
          <label>Срок</label>
          <div class="rai-days">
            <?php foreach ([7 => '7 дней', 30 => 'Месяц', 90 => '3 месяца', 180 => 'Полгода', 365 => 'Год', 'forever' => '♾ Навсегда', -1 => 'Своё'] as $d => $l): ?>
              <label><input type="radio" name="days" value="<?= $d ?>"<?= $d === 30 ? ' checked' : '' ?>> <?= $l ?></label>
            <?php endforeach; ?>
          </div>
          <input type="number" name="days_custom" min="1" max="3660" placeholder="Своё: сколько дней">
          <label>Комментарий (видно в журнале)</label>
          <input type="text" name="note" maxlength="150" placeholder="например: победитель розыгрыша">
          <button class="btn primary" type="submit" style="margin-top:10px;">Выдать подписку</button>
        </form>
      </div>
      <?php endif; ?>

      <?php if ($ok): ?>
      <div class="card">
        <h3>🔎 Найти пользователя Rai</h3>
        <form method="GET" class="row" style="gap:8px;">
          <input type="hidden" name="tab" value="rai">
          <input type="text" name="q" value="<?= rai_h($q) ?>" placeholder="Логин, имя или почта" style="flex:1; margin:0;">
          <button class="btn ghost" type="submit" style="margin:0;">Найти</button>
        </form>
        <?php if ($found !== null): ?>
          <?php if (empty($found['ok'])): ?><p class="muted"><?= rai_h($found['error'] ?? 'Ошибка поиска') ?></p>
          <?php elseif (!$found['users']): ?><p class="muted">Никого не нашлось.</p>
          <?php else: ?>
            <ul class="list">
              <?php foreach ($found['users'] as $u): ?>
                <li><span><b><?= rai_h($u['login']) ?></b> <span class="muted">· <?= rai_h($u['name']) ?><?= $u['email'] ? ' · ' . rai_h($u['email']) : '' ?></span><br>
                  <?php if (!empty($u['role'])): ?><span class="chip on"><?= rai_h(RAI_ROLES[$u['role']] ?? $u['role']) ?></span><?php endif; ?>
                  <span class="chip <?= $u['plan'] !== 'free' ? 'on' : '' ?>"><?= rai_h($u['plan_name']) ?><?= $u['until'] ? (!empty($u['forever']) ? ' навсегда' : ' до ' . rai_date($u['until'])) : '' ?></span>
                  <span class="muted" style="font-size:12px;">нейросеть сегодня: <?= (int)$u['neuro_today'] ?> · с <?= rai_date($u['created']) ?></span></span>
                  <?php if ($can_users): ?><span class="row" style="gap:6px;">
                    <form method="POST" action="?tab=rai" style="margin:0;"><input type="hidden" name="action" value="rai_grant"><input type="hidden" name="login" value="<?= rai_h($u['login']) ?>"><input type="hidden" name="plan" value="premium"><input type="hidden" name="days" value="30"><button class="btn ghost sm" type="submit" title="Выдать «Премиум» на 30 дней">+30 дн. Премиум</button></form>
                    <?php if ($u['plan'] !== 'free' && empty($u['role'])): ?><form method="POST" action="?tab=rai" style="margin:0;" onsubmit="return confirm('Снять подписку у <?= rai_h($u['login']) ?>?');"><input type="hidden" name="action" value="rai_revoke"><input type="hidden" name="login" value="<?= rai_h($u['login']) ?>"><button class="btn ghost sm" type="submit">Снять</button></form><?php endif; ?>
                    <form method="POST" action="?tab=rai" style="margin:0;" title="Значок у имени и полный доступ навсегда"><input type="hidden" name="action" value="rai_role"><input type="hidden" name="login" value="<?= rai_h($u['login']) ?>">
                      <select name="role" onchange="this.form.submit()" style="margin:0; padding:4px 6px; width:auto;">
                        <option value=""<?= empty($u['role']) ? ' selected' : '' ?>>Роль: нет</option>
                        <?php foreach (RAI_ROLES as $rk => $rl): ?><option value="<?= $rk ?>"<?= ($u['role'] ?? '') === $rk ? ' selected' : '' ?>><?= rai_h($rl) ?></option><?php endforeach; ?>
                      </select></form>
                  </span><?php endif; ?></li>
              <?php endforeach; ?>
            </ul>
            <datalist id="raiLogins"><?php foreach ($found['users'] as $u): ?><option value="<?= rai_h($u['login']) ?>"><?php endforeach; ?></datalist>
          <?php endif; ?>
        <?php endif; ?>
      </div>
      <?php endif; ?>
    </div>

    <?php if ($ok): ?>
    <div class="card" id="subs">
      <h3>⭐ Подписчики (<?= count($stats['subs']) ?>)</h3>
      <?php if (!$stats['subs']): ?><p class="muted">Пока никого — выдайте подписку или дождитесь первой оплаты.</p><?php else: ?>
      <div class="row" style="gap:8px; align-items:center;">
        <input type="text" placeholder="Фильтр…" oninput="filterRows('raiSubs', this.value)" style="max-width:280px; margin:0;">
        <?php if ($can_users): ?><a class="btn ghost sm" href="?tab=rai&amp;export=csv">⬇ Подписчики в CSV</a><a class="btn ghost sm" href="?tab=rai&amp;export=csv&amp;all=1">⬇ Все пользователи</a><?php endif; ?>
      </div>
      <div class="tbl-wrap"><table class="tbl" id="raiSubs">
        <thead><tr><th>Логин</th><th>Тариф</th><th>До</th><th>Откуда</th><th>Нейросеть сегодня</th><th></th></tr></thead>
        <tbody>
        <?php foreach ($stats['subs'] as $u): ?>
          <tr<?= $u['until'] && $u['until'] - time() < 3 * 86400 ? ' class="rai-soon"' : '' ?>><td><b><?= rai_h($u['login']) ?></b><br><span class="muted" style="font-size:12px;"><?= rai_h($u['email'] ?: $u['name']) ?></span></td>
            <td><span class="badge badge-gold"><?= rai_h($u['plan_name']) ?></span></td>
            <td><?= rai_date($u['until']) ?><br><span class="muted" style="font-size:12px;"><?= rai_left($u['until']) ?></span></td>
            <td><?= $u['source'] === 'platega' ? '💳 оплата' : '🎁 выдано' ?><br><span class="muted" style="font-size:12px;"><?= rai_h($u['note']) ?></span></td>
            <td><?= (int)$u['neuro_today'] ?></td>
            <td><?php if ($can_users): ?><form method="POST" action="?tab=rai" style="margin:0;" onsubmit="return confirm('Снять подписку у <?= rai_h($u['login']) ?>?');"><input type="hidden" name="action" value="rai_revoke"><input type="hidden" name="login" value="<?= rai_h($u['login']) ?>"><button class="btn ghost sm" type="submit">Снять</button></form><?php endif; ?></td></tr>
        <?php endforeach; ?>
        </tbody></table></div>
      <?php endif; ?>
    </div>

    <div class="card" id="orders">
      <h3>💳 Платежи Platega</h3>
      <?php if (!$stats['orders']): ?><p class="muted">Платежей ещё не было.</p><?php else: ?>
      <div class="tbl-wrap"><table class="tbl">
        <thead><tr><th>Заказ</th><th>Логин</th><th>Тариф</th><th>Сумма</th><th>Статус</th><th>Дата</th><th></th></tr></thead>
        <tbody>
        <?php foreach ($stats['orders'] as $o): $st = $status_names[$o['status']] ?? [$o['status'], 'badge-viewed']; ?>
          <tr><td><code><?= rai_h($o['id']) ?></code></td><td><?= rai_h($o['login']) ?></td>
            <td><?= rai_h($plan_name($o['plan'])) ?> · <?= (int)$o['months'] >= 12 ? 'год' : 'месяц' ?></td>
            <td><?= rai_rub($o['amount']) ?></td><td><span class="badge <?= $st[1] ?>"><?= rai_h($st[0]) ?></span><?= !empty($o['error']) ? '<br><span class="muted" style="font-size:12px;">' . rai_h($o['error']) . '</span>' : '' ?></td>
            <td><?= date('d.m.Y H:i', (int)($o['paid_at'] ?? $o['created'])) ?></td>
            <td><?php if ($can_users && in_array($o['status'], ['new', 'pending'], true) && !empty($o['transaction'])): ?><form method="POST" action="?tab=rai" style="margin:0;"><input type="hidden" name="action" value="rai_order_check"><input type="hidden" name="id" value="<?= rai_h($o['id']) ?>"><button class="btn ghost sm" type="submit" title="Спросить у Platega, оплачен ли заказ">Проверить</button></form><?php endif; ?></td></tr>
        <?php endforeach; ?>
        </tbody></table></div>
      <?php endif; ?>
    </div>

    <?php if ($stats['log']): ?>
    <div class="card">
      <h3>📜 Журнал подписок</h3>
      <ul class="list">
        <?php foreach ($stats['log'] as $l):
          $what = ['grant' => '🎁 Выдана', 'revoke' => '✖ Снята', 'payment' => '💳 Оплата', 'refund' => '↩ Возврат', 'plans' => '🏷 Тарифы'][$l['type']] ?? $l['type']; ?>
          <li><span><b><?= $what ?></b> <?= rai_h($l['login']) ?><?= $l['plan'] && $l['plan'] !== 'free' ? ' · «' . rai_h($plan_name($l['plan'])) . '»' : '' ?><?= $l['days'] ? ' · ' . (int)$l['days'] . ' дн.' : '' ?>
            <?= $l['note'] ? '<span class="muted"> — ' . rai_h($l['note']) . '</span>' : '' ?><?= $l['by'] ? '<span class="muted"> · ' . rai_h($l['by']) . '</span>' : '' ?></span>
            <span class="muted" style="font-size:12px; white-space:nowrap;"><?= date('d.m.Y H:i', (int)$l['time']) ?></span></li>
        <?php endforeach; ?>
      </ul>
    </div>
    <?php endif; ?>

    <?php if ($can_settings): ?>
    <div class="card" id="plans">
      <h3>🏷 Тарифы и лимиты</h3>
      <p class="meta">Цены — в рублях за месяц (год — со скидкой 25%). «Сообщений в день» — лимит нейросети, 0 — без ограничений. Без входа — <?= (int)$stats['guest_limit'] ?> в день.
        Одна строка в «Что входит» — один пункт на карточке тарифа.</p>
      <form method="POST" action="?tab=rai">
        <input type="hidden" name="action" value="rai_plans">
        <div class="rai-plans">
          <?php foreach (array_merge(['free'], RAI_PLAN_KEYS) as $k): $p = $plans[$k] ?? []; ?>
            <div class="rai-plan">
              <h4><?= $k === 'free' ? '🆓 Бесплатный' : ['plus' => '✦', 'premium' => '💎', 'ultra' => '🚀'][$k] . ' Платный' ?></h4>
              <label>Название</label><input type="text" name="plan[<?= $k ?>][name]" value="<?= rai_h($p['name'] ?? '') ?>" maxlength="30">
              <?php if ($k !== 'free'): ?><label>Цена, ₽ в месяц</label><input type="number" name="plan[<?= $k ?>][price]" value="<?= (int)($p['price'] ?? 0) ?>" min="1"><?php endif; ?>
              <label>Сообщений нейросети в день</label><input type="number" name="plan[<?= $k ?>][neuro_day]" value="<?= (int)($p['neuro_day'] ?? 0) ?>" min="0">
              <label>Подзаголовок</label><input type="text" name="plan[<?= $k ?>][tagline]" value="<?= rai_h($p['tagline'] ?? '') ?>" maxlength="80">
              <label>Что входит</label><textarea name="plan[<?= $k ?>][features]"><?= rai_h(implode("\n", $p['features'] ?? [])) ?></textarea>
            </div>
          <?php endforeach; ?>
        </div>
        <div class="row" style="gap:8px; margin-top:12px; align-items:flex-end;">
          <div style="max-width:260px;"><label>Без входа: сообщений нейросети в день</label><input type="number" name="guest_day" value="<?= (int)$stats['guest_limit'] ?>" min="0" max="1000" style="margin:0;"></div>
          <button class="btn primary" type="submit">Сохранить тарифы</button>
          <button class="btn ghost" type="submit" form="raiReset">Сбросить к стандартным</button>
        </div>
      </form>
      <form method="POST" action="?tab=rai" id="raiReset" onsubmit="return confirm('Вернуть стандартные цены и описания?');"><input type="hidden" name="action" value="rai_plans_reset"></form>
    </div>
    <?php endif; ?>
    <?php endif; ?>

    <?php if ($ok && $can_settings): $pl = rai_api('platega_get'); ?>
    <div class="card" id="platega">
      <h3>💳 Platega — приём оплаты</h3>
      <?php if (($pl['source'] ?? null) === 'config'): ?>
        <p class="meta">Ключи вписаны в <code>config.php</code> на rai.rteam.info (Merchant ID <?= rai_h($pl['id']) ?>) — менять их нужно там.</p>
      <?php else: ?>
        <p class="meta">Впишите ключи из кабинета Platega — они сохранятся на rai.rteam.info (из браузера их не прочитать). Секрет здесь не показывается.</p>
        <form method="POST" action="?tab=rai">
          <input type="hidden" name="action" value="rai_platega">
          <div class="row" style="gap:10px;">
            <div class="grow"><label>Merchant ID <?= !empty($pl['id']) ? '· ✓ ' . rai_h($pl['id']) : '' ?></label><input type="text" name="platega_id" placeholder="<?= !empty($pl['id']) ? 'оставьте пустым, чтобы не менять' : 'из кабинета Platega' ?>" autocomplete="off"></div>
            <div class="grow"><label>Секретный ключ (API)</label><input type="password" name="platega_secret" placeholder="<?= !empty($pl['id']) ? 'оставьте пустым, чтобы не менять' : 'X-Secret' ?>" autocomplete="new-password"></div>
            <div class="grow"><label>Способ оплаты</label><select name="platega_method">
              <?php foreach ([0 => 'Покупатель выбирает сам (СБП, карты…)', 2 => 'Сразу СБП (QR)', 11 => 'Банковские карты'] as $mv => $ml): ?><option value="<?= $mv ?>"<?= (int)($pl['method'] ?? 0) === $mv ? ' selected' : '' ?>><?= $ml ?></option><?php endforeach; ?>
            </select></div>
          </div>
          <?php if (!empty($pl['id'])): ?><label class="switch-row" style="margin-top:6px;"><input type="checkbox" name="platega_clear"> удалить ключи (оплата на сайте выключится)</label><?php endif; ?>
          <button class="btn primary" type="submit" style="margin-top:10px;">Сохранить</button>
        </form>
      <?php endif; ?>
      <p class="muted" style="font-size:13px; margin:10px 0 0;">В кабинете Platega укажите адрес уведомлений (callback): <code><?= rai_h($pl['callback'] ?? 'https://rai.rteam.info/pay_callback.php') ?></code>.
        Если уведомление не пришло, подписка всё равно включится, когда покупатель вернётся на сайт, — или нажмите «Проверить» у платежа.</p>
    </div>
    <?php endif; ?>

    <?php if ($can_settings): ?>
    <div class="card">
      <h3>⚙️ Подключение к rai.rteam.info</h3>
      <form method="POST" action="?tab=rai">
        <input type="hidden" name="action" value="rai_save">
        <label>Адрес API (пусто — <?= rai_h(RAI_DEFAULT_URL) ?>)</label>
        <input type="text" name="rai_api_url" value="<?= rai_h($GLOBALS['settings']['rai_api_url'] ?? '') ?>" placeholder="<?= rai_h(RAI_DEFAULT_URL) ?>">
        <label>Ключ (ADMIN_API_KEY из config.php на rai.rteam.info) <?= $c['ready'] ? '· ✓ задан' : '' ?></label>
        <input type="password" name="rai_api_key" value="" autocomplete="new-password" placeholder="<?= $c['ready'] ? 'оставьте пустым, чтобы не менять' : 'длинная случайная строка, от 32 символов' ?>">
        <?php if (!empty($GLOBALS['settings']['rai_api_key'])): ?><label class="switch-row" style="margin-top:6px;"><input type="checkbox" name="clear_key"> убрать ключ из админки</label><?php endif; ?>
        <div class="row" style="gap:8px; margin-top:10px;">
          <button class="btn primary" type="submit">Сохранить и проверить</button>
          <button class="btn ghost" type="submit" form="raiCheck">Проверить связь</button>
        </div>
      </form>
      <form method="POST" action="?tab=rai" id="raiCheck"><input type="hidden" name="action" value="rai_check"></form>
      <ol class="muted" style="margin:12px 0 0; padding-left:20px; line-height:1.6; font-size:13px;">
        <li>Придумайте ключ: 64 случайных символа (0-9, a-f).</li>
        <li>Впишите его в <code>config.php</code> на rai.rteam.info: <code>define('ADMIN_API_KEY', '…')</code>.</li>
        <li>Вставьте тот же ключ сюда и нажмите «Сохранить и проверить».</li>
        <li>Platega: впишите Merchant ID и секрет в карточке «Platega» выше (или в <code>config.php</code> на rai.rteam.info).</li>
      </ol>
    </div>
    <?php endif; ?>
    <?php
}

// ======================================================================= вкладка «Rai: правила»
// Нарушения правил в чате Rai (мат, 18+, наркотики, насилие, взлом, экстремизм): Rai останавливает диалог и сообщает сюда.
// Здесь их видно с текстом сообщения; пользователя (или гостя по IP) можно заблокировать на время или навсегда.

function rai_rules_back($extra = '') {
    header('Location: admin.php?tab=rai_rules' . $extra);
    exit;
}

/** Адрес сайта Rai (для ссылок на документы) — из адреса admin_api.php. */
function rai_site_url() {
    return preg_replace('~/admin_api\.php$~', '', rai_conf()['url']);
}

/** Формы вкладки «Rai: правила» (POST на ?tab=rai_rules). */
function rai_rules_post() {
    global $user;
    $action = (string)($_POST['action'] ?? '');
    $back = !empty($_POST['f']) ? '&f=' . rawurlencode((string)$_POST['f']) : '';
    if ($action === 'rai_seen') {
        $r = rai_api('violations_seen', !empty($_POST['all']) ? ['all' => true] : ['ids' => [(string)($_POST['id'] ?? '')]]);
        unset($_SESSION['rai_stats_cache']);
        if (empty($r['ok'])) flash('Не получилось: ' . ($r['error'] ?? '?'), 'error');
        rai_rules_back($back);
    }
    if ($action === 'rai_vdel') {
        $r = rai_api('violations_delete', ['ids' => [(string)($_POST['id'] ?? '')]]);
        unset($_SESSION['rai_stats_cache']);
        flash(!empty($r['ok']) ? 'Запись удалена.' : 'Не получилось: ' . ($r['error'] ?? '?'), !empty($r['ok']) ? 'success' : 'error');
        rai_rules_back($back);
    }
    if ($action === 'rai_ban') {
        $login = trim((string)($_POST['login'] ?? ''));
        $ip = trim((string)($_POST['ip'] ?? ''));
        $days = (int)($_POST['days'] ?? 0);
        $reason = trim((string)($_POST['reason'] ?? '')) ?: 'нарушение правил Rai';
        $r = rai_api('ban', ['login' => $login, 'ip' => $login === '' ? $ip : '', 'days' => $days, 'reason' => $reason]);
        if (!empty($r['ok'])) {
            if (!empty($_POST['id'])) rai_api('violations_seen', ['ids' => [(string)$_POST['id']]]);
            unset($_SESSION['rai_stats_cache']);
            rai_log("$user заблокировал в Rai " . ($login ?: "IP $ip") . ($days ? " на $days дн." : ' навсегда') . ": $reason");
            flash('🚫 Заблокирован: ' . ($login ?: "IP $ip") . ($days ? " на $days дн." : ' навсегда'), 'success');
        } else {
            flash('Не получилось заблокировать: ' . ($r['error'] ?? '?'), 'error');
        }
        rai_rules_back($back);
    }
    if ($action === 'rai_unban') {
        $login = trim((string)($_POST['login'] ?? ''));
        $ip = trim((string)($_POST['ip'] ?? ''));
        $r = rai_api('unban', ['login' => $login, 'ip' => $ip]);
        if (!empty($r['ok'])) rai_log("$user разблокировал в Rai " . ($login ?: "IP $ip"));
        flash(!empty($r['ok']) ? '✅ Разблокирован: ' . ($login ?: "IP $ip") : 'Не получилось: ' . ($r['error'] ?? '?'), !empty($r['ok']) ? 'success' : 'error');
        rai_rules_back($back);
    }
}

function rai_rules_render() {
    $c = rai_conf();
    $filter = ($_GET['f'] ?? '') === 'new' ? 'new' : '';
    $r = $c['ready'] ? rai_api('violations', ['filter' => $filter]) : ['ok' => false, 'error' => 'Сначала подключите Rai во вкладке «Rai: подписки».'];
    $ok = !empty($r['ok']);
    $site = rai_site_url();
    $labels = $ok ? $r['labels'] : [];
    $icons = ['мат' => '🤬', '18+' => '🔞', 'наркотики' => '💊', 'насилие' => '💣', 'взлом' => '🕵️', 'экстремизм' => '🚫'];
    $durations = [1 => '1 день', 7 => '7 дней', 30 => '30 дней', 0 => 'навсегда'];
    $bans = $ok ? $r['bans'] : ['users' => [], 'ips' => []];
    $ban_count = count($bans['users']) + count($bans['ips']);
    ?>
    <style>
      .rr-hero { display: flex; gap: 16px; align-items: center; flex-wrap: wrap; padding: 18px 20px; border-radius: 16px; border: 1px solid var(--line);
        background: radial-gradient(120% 140% at 100% 0%, rgba(255, 61, 129, .18), transparent 55%), radial-gradient(100% 120% at 0% 100%, rgba(139, 92, 255, .14), transparent 60%), var(--panel); }
      .rr-logo { width: 52px; height: 52px; border-radius: 15px; display: grid; place-items: center; font-size: 26px;
        background: linear-gradient(135deg, #ff2d2d, #ff3d81 55%, #8b5cff); box-shadow: 0 10px 30px -10px rgba(255, 45, 45, .7); }
      .rr-hero h2 { margin: 0; } .rr-hero p { margin: 4px 0 0; }
      .rr-kpis { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px; margin-top: 14px; }
      .rr-kpi { padding: 14px 16px; border-radius: 14px; border: 1px solid var(--line); background: var(--panel); }
      .rr-kpi b { display: block; font-size: 26px; line-height: 1.1; } .rr-kpi span { color: var(--muted); font-size: 13px; }
      .rr-kpi.hot { border-color: rgba(255, 61, 129, .6); background: rgba(255, 61, 129, .1); }
      .rr-cats { display: flex; gap: 8px; flex-wrap: wrap; margin-top: 12px; }
      .rr-cats span { padding: 5px 12px; border-radius: 999px; border: 1px solid var(--line); font-size: 13px; }
      .rr-msg { max-width: 420px; white-space: pre-wrap; word-break: break-word; font-size: 13px; background: rgba(0,0,0,.25); padding: 8px 10px; border-radius: 10px; }
      tr.rr-new td { background: rgba(255, 61, 129, .07); }
      .rr-actions { display: flex; gap: 6px; flex-wrap: wrap; align-items: center; }
      .rr-actions form { margin: 0; display: inline-flex; gap: 6px; align-items: center; }
      .rr-actions select { width: auto; margin: 0; padding: 6px 8px; }
      .rr-docs { display: flex; gap: 8px; flex-wrap: wrap; }
      .rr-filter { display: flex; gap: 6px; margin: 0 0 10px; }
      .rr-filter a { padding: 6px 14px; border-radius: 999px; border: 1px solid var(--line); text-decoration: none; color: inherit; font-size: 13px; }
      .rr-filter a.on { background: linear-gradient(120deg, #ff2d2d, #ff3d81 45%, #8b5cff); border-color: transparent; color: #fff; }
    </style>

    <div class="rr-hero">
      <div class="rr-logo">🚫</div>
      <div style="flex:1; min-width:220px;">
        <h2>Rai — правила</h2>
        <p class="muted">Мат, 18+, наркотики, насилие, взлом, экстремизм: Rai останавливает такой диалог и сообщает сюда. Здесь можно заблокировать нарушителя.</p>
      </div>
      <div class="rr-docs">
        <a class="btn" href="<?= rai_h($site) ?>/rules.php" target="_blank" rel="noopener">📜 Правила</a>
        <a class="btn" href="<?= rai_h($site) ?>/terms.php" target="_blank" rel="noopener">📄 Соглашение</a>
        <a class="btn" href="<?= rai_h($site) ?>/privacy.php" target="_blank" rel="noopener">🔒 Конфиденциальность</a>
      </div>
    </div>

    <?php if (!$ok): ?>
      <div class="card" style="margin-top:14px;"><p class="meta">⚠️ <?= rai_h($r['error'] ?? 'Нет связи с rai.rteam.info') ?></p></div>
    <?php return; endif; ?>

    <div class="rr-kpis">
      <div class="rr-kpi <?= $r['new'] ? 'hot' : '' ?>"><b><?= (int)$r['new'] ?></b><span>новых нарушений</span></div>
      <div class="rr-kpi"><b><?= (int)$r['total'] ?></b><span>нарушений всего</span></div>
      <div class="rr-kpi"><b><?= $ban_count ?></b><span>заблокировано</span></div>
    </div>
    <div class="rr-cats">
      <?php foreach ($r['by_category'] as $cat => $n): ?><span><?= $icons[$cat] ?? '•' ?> <?= rai_h($labels[$cat] ?? $cat) ?>: <b><?= (int)$n ?></b></span><?php endforeach; ?>
    </div>

    <div class="card" style="margin-top:14px;">
      <div style="display:flex; justify-content:space-between; align-items:center; gap:10px; flex-wrap:wrap;">
        <h3 style="margin:0;">🔔 Нарушения</h3>
        <?php if ($r['new']): ?>
          <form method="POST" action="?tab=rai_rules"><input type="hidden" name="action" value="rai_seen"><input type="hidden" name="all" value="1">
            <input type="hidden" name="f" value="<?= rai_h($filter) ?>"><button class="btn" type="submit">✓ Отметить все просмотренными</button></form>
        <?php endif; ?>
      </div>
      <div class="rr-filter" style="margin-top:10px;">
        <a href="?tab=rai_rules" class="<?= $filter === '' ? 'on' : '' ?>">Все</a>
        <a href="?tab=rai_rules&f=new" class="<?= $filter === 'new' ? 'on' : '' ?>">Новые (<?= (int)$r['new'] ?>)</a>
      </div>
      <?php if (!$r['items']): ?>
        <p class="meta"><?= $filter ? 'Новых нарушений нет. 🎉' : 'Нарушений пока не было.' ?></p>
      <?php else: ?>
      <div class="table-wrap" style="overflow-x:auto;">
      <table>
        <thead><tr><th>Когда</th><th>Кто</th><th>Нарушение</th><th>Сообщение</th><th>Действия</th></tr></thead>
        <tbody>
        <?php foreach ($r['items'] as $v): $who = $v['login'] ?? ''; ?>
          <tr class="<?= empty($v['seen']) ? 'rr-new' : '' ?>">
            <td style="white-space:nowrap;"><?= date('d.m.Y H:i', (int)$v['time']) ?><?= empty($v['seen']) ? '<br><span class="badge badge-warn">новое</span>' : '' ?></td>
            <td><?php if ($who): ?><b><?= rai_h($v['name'] ?? $who) ?></b><br><span class="muted">@<?= rai_h($who) ?></span><?php else: ?><b>Гость</b><?php endif; ?>
              <br><span class="muted" style="font-size:12px;">IP <?= rai_h($v['ip'] ?? '') ?></span>
              <?php if (($v['count'] ?? 1) > 1): ?><br><span class="badge badge-dec"><?= (int)$v['count'] ?> нарушений</span><?php endif; ?></td>
            <td><?= $icons[$v['category']] ?? '•' ?> <?= rai_h($labels[$v['category']] ?? $v['category']) ?></td>
            <td><div class="rr-msg"><?= rai_h($v['text'] ?? '') ?></div></td>
            <td><div class="rr-actions">
              <?php if (!empty($v['banned'])): ?>
                <span class="badge badge-dec">🚫 заблокирован</span>
                <form method="POST" action="?tab=rai_rules"><input type="hidden" name="action" value="rai_unban">
                  <input type="hidden" name="login" value="<?= rai_h($who) ?>"><input type="hidden" name="ip" value="<?= $who ? '' : rai_h($v['ip'] ?? '') ?>">
                  <input type="hidden" name="f" value="<?= rai_h($filter) ?>"><button class="btn" type="submit">Разбанить</button></form>
              <?php else: ?>
                <form method="POST" action="?tab=rai_rules"><input type="hidden" name="action" value="rai_ban">
                  <input type="hidden" name="login" value="<?= rai_h($who) ?>"><input type="hidden" name="ip" value="<?= rai_h($v['ip'] ?? '') ?>">
                  <input type="hidden" name="id" value="<?= rai_h($v['id']) ?>"><input type="hidden" name="f" value="<?= rai_h($filter) ?>">
                  <input type="hidden" name="reason" value="<?= rai_h($labels[$v['category']] ?? 'нарушение правил') ?>">
                  <select name="days"><?php foreach ($durations as $d => $l): ?><option value="<?= $d ?>"<?= $d === 7 ? ' selected' : '' ?>><?= $l ?></option><?php endforeach; ?></select>
                  <button class="btn primary" type="submit">🚫 Забанить</button></form>
              <?php endif; ?>
              <?php if (empty($v['seen'])): ?>
                <form method="POST" action="?tab=rai_rules"><input type="hidden" name="action" value="rai_seen"><input type="hidden" name="id" value="<?= rai_h($v['id']) ?>">
                  <input type="hidden" name="f" value="<?= rai_h($filter) ?>"><button class="btn" type="submit" title="Отметить просмотренным">✓</button></form>
              <?php endif; ?>
              <form method="POST" action="?tab=rai_rules" onsubmit="return confirm('Удалить запись?')"><input type="hidden" name="action" value="rai_vdel">
                <input type="hidden" name="id" value="<?= rai_h($v['id']) ?>"><input type="hidden" name="f" value="<?= rai_h($filter) ?>"><button class="btn" type="submit" title="Удалить запись">🗑</button></form>
            </div></td>
          </tr>
        <?php endforeach; ?>
        </tbody>
      </table>
      </div>
      <?php endif; ?>
    </div>

    <div class="rai-grid" style="display:grid; grid-template-columns:repeat(auto-fit, minmax(320px, 1fr)); gap:14px;">
      <div class="card" style="margin-top:14px;">
        <h3>🚫 Заблокированные (<?= $ban_count ?>)</h3>
        <?php if (!$ban_count): ?><p class="meta">Никто не заблокирован.</p><?php else: ?>
        <table>
          <thead><tr><th>Кто</th><th>Причина</th><th>До</th><th></th></tr></thead>
          <tbody>
          <?php foreach (['users' => 'login', 'ips' => 'ip'] as $kind => $field): foreach ($bans[$kind] as $key => $b): ?>
            <tr>
              <td><?= $kind === 'users' ? '@' . rai_h($key) : 'IP ' . rai_h($key) ?><br><span class="muted" style="font-size:12px;"><?= date('d.m.Y', (int)($b['at'] ?? 0)) ?> · <?= rai_h($b['by'] ?? '') ?></span></td>
              <td><?= rai_h($b['reason'] ?? '') ?></td>
              <td><?= !empty($b['until']) ? date('d.m.Y', (int)$b['until']) : 'навсегда' ?></td>
              <td><form method="POST" action="?tab=rai_rules" style="margin:0;"><input type="hidden" name="action" value="rai_unban">
                <input type="hidden" name="<?= $field ?>" value="<?= rai_h($key) ?>"><button class="btn" type="submit">Разбанить</button></form></td>
            </tr>
          <?php endforeach; endforeach; ?>
          </tbody>
        </table>
        <?php endif; ?>
      </div>

      <div class="card" style="margin-top:14px;">
        <h3>✋ Заблокировать вручную</h3>
        <p class="meta">По логину Rai или, для гостя, по IP-адресу.</p>
        <form method="POST" action="?tab=rai_rules">
          <input type="hidden" name="action" value="rai_ban">
          <label>Логин пользователя Rai</label>
          <input type="text" name="login" placeholder="например: anya" autocomplete="off">
          <label>или IP-адрес</label>
          <input type="text" name="ip" placeholder="например: 203.0.113.7" autocomplete="off">
          <label>Срок</label>
          <select name="days"><?php foreach ($durations as $d => $l): ?><option value="<?= $d ?>"<?= $d === 7 ? ' selected' : '' ?>><?= $l ?></option><?php endforeach; ?></select>
          <label>Причина (видит пользователь)</label>
          <input type="text" name="reason" maxlength="200" placeholder="нарушение правил Rai">
          <button class="btn primary" type="submit" style="margin-top:10px;">🚫 Заблокировать</button>
        </form>
      </div>
    </div>
    <?php
}

// ======================================================================= вкладка «Rai: уведомления»
// Push-уведомления: рассылка пользователям Rai (всем или одному) и уведомления админам на телефон и компьютер —
// новые нарушения правил, оплаты, новые пользователи. Сами уведомления отправляет rai.rteam.info (push.php).

function rai_push_back($extra = '') {
    header('Location: admin.php?tab=rai_push' . $extra);
    exit;
}

/** Адрес этой админ-панели (куда вести по нажатию на уведомление). */
function rai_admin_self_url() {
    $https = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off') || (($_SERVER['HTTP_X_FORWARDED_PROTO'] ?? '') === 'https');
    $host = preg_replace('/[^A-Za-z0-9.:\-]/', '', (string)($_SERVER['HTTP_HOST'] ?? 'rteam.info'));
    $path = strtok((string)($_SERVER['REQUEST_URI'] ?? '/admin.php'), '?');
    return ($https ? 'https' : 'http') . '://' . $host . ($path ?: '/admin.php');
}

/** Подписанная ссылка «включить уведомления админки на этом устройстве» (действует час). */
function rai_push_admin_link() {
    global $user;
    $c = rai_conf();
    if (!$c['ready']) return null;
    $exp = time() + 3600;
    $back = rai_admin_self_url();
    $t = hash_hmac('sha256', 'push-admin|' . $exp . '|' . $back, $c['key']);
    return rai_site_url() . '/push_admin.php?' . http_build_query(['exp' => $exp, 'back' => $back, 't' => $t, 'admin' => (string)$user]);
}

/** Формы вкладки (POST на ?tab=rai_push). */
function rai_push_post() {
    global $user;
    $action = (string)($_POST['action'] ?? '');
    if ($action === 'rai_push_send') {
        $to = ($_POST['to'] ?? 'all') === 'login' ? 'login' : 'all';
        $r = rai_api('push_send', ['to' => $to, 'login' => trim((string)($_POST['login'] ?? '')), 'title' => (string)($_POST['title'] ?? ''),
                                   'body' => (string)($_POST['body'] ?? ''), 'url' => trim((string)($_POST['url'] ?? ''))], 120);
        if (!empty($r['ok'])) {
            rai_log("$user отправил уведомление Rai «" . mb_substr((string)$_POST['title'], 0, 60) . "»: доставлено {$r['sent']} из {$r['targets']}");
            flash("🔔 Отправлено: доставлено {$r['sent']} из {$r['targets']}" . ($r['removed'] ? ", отключённых устройств убрано: {$r['removed']}" : ''), 'success');
        } else {
            flash('Не отправлено: ' . ($r['error'] ?? '?'), 'error');
        }
        rai_push_back();
    }
    if ($action === 'rai_push_del') {
        $r = rai_api('push_device_delete', ['id' => (string)($_POST['id'] ?? '')]);
        flash(!empty($r['ok']) ? 'Устройство отключено.' : 'Не получилось: ' . ($r['error'] ?? '?'), !empty($r['ok']) ? 'success' : 'error');
        rai_push_back();
    }
}

function rai_push_render() {
    $c = rai_conf();
    $r = $c['ready'] ? rai_api('push_stats') : ['ok' => false, 'error' => 'Сначала подключите Rai во вкладке «Rai: подписки».'];
    $ok = !empty($r['ok']);
    $link = $ok ? rai_push_admin_link() : null;
    ?>
    <style>
      .rp-hero { display: flex; gap: 16px; align-items: center; flex-wrap: wrap; padding: 18px 20px; border-radius: 16px; border: 1px solid var(--line);
        background: radial-gradient(120% 140% at 100% 0%, rgba(255, 61, 129, .18), transparent 55%), radial-gradient(100% 120% at 0% 100%, rgba(139, 92, 255, .14), transparent 60%), var(--panel); }
      .rp-logo { width: 52px; height: 52px; border-radius: 15px; display: grid; place-items: center; font-size: 26px;
        background: linear-gradient(135deg, #ff2d2d, #ff3d81 55%, #8b5cff); box-shadow: 0 10px 30px -10px rgba(255, 45, 45, .7); }
      .rp-hero h2 { margin: 0; } .rp-hero p { margin: 4px 0 0; }
      .rp-kpis { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px; margin-top: 14px; }
      .rp-kpi { padding: 14px 16px; border-radius: 14px; border: 1px solid var(--line); background: var(--panel); }
      .rp-kpi b { display: block; font-size: 26px; line-height: 1.1; } .rp-kpi span { color: var(--muted); font-size: 13px; }
      .rp-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 14px; }
      .rp-to { display: flex; gap: 14px; flex-wrap: wrap; margin: 6px 0; } .rp-to label { display: flex; gap: 6px; align-items: center; margin: 0; }
      .rp-preview { display: flex; gap: 12px; align-items: flex-start; padding: 12px 14px; border-radius: 14px; border: 1px solid var(--line); background: rgba(0,0,0,.25); margin-top: 10px; }
      .rp-preview i { width: 38px; height: 38px; border-radius: 10px; flex: none; display: grid; place-items: center; font-style: normal; font-weight: 800; color: #fff;
        background: linear-gradient(135deg, #ff2d2d, #ff3d81 55%, #8b5cff); }
      .rp-preview b { display: block; } .rp-preview span { color: var(--muted); font-size: 13px; }
    </style>

    <div class="rp-hero">
      <div class="rp-logo">🔔</div>
      <div style="flex:1; min-width:220px;">
        <h2>Rai — уведомления</h2>
        <p class="muted">Push-уведомления приходят, даже когда сайт закрыт. Пользователи включают их в чате Rai — здесь можно сделать рассылку.
          А ещё админ-панель может присылать вам нарушения, оплаты и новых пользователей.</p>
      </div>
    </div>

    <?php if (!$ok): ?>
      <div class="card" style="margin-top:14px;"><p class="meta">⚠️ <?= rai_h($r['error'] ?? 'Нет связи с rai.rteam.info') ?></p></div>
    <?php return; endif; ?>
    <?php if (empty($r['ready'])): ?>
      <div class="card" style="margin-top:14px;"><p class="meta">⚠️ На хостинге rai.rteam.info нет PHP-расширений openssl или curl — уведомления не отправляются. Включите их в панели хостинга.</p></div>
    <?php endif; ?>

    <div class="rp-kpis">
      <div class="rp-kpi"><b><?= (int)$r['people'] ?></b><span>пользователей с уведомлениями</span></div>
      <div class="rp-kpi"><b><?= (int)$r['devices'] ?></b><span>устройств</span></div>
      <div class="rp-kpi"><b><?= (int)$r['new_week'] ?></b><span>подключили за неделю</span></div>
      <div class="rp-kpi"><b><?= count($r['admins']) ?></b><span>ваших устройств</span></div>
    </div>

    <div class="rp-grid">
      <div class="card" style="margin-top:14px;">
        <h3>📣 Отправить уведомление</h3>
        <form method="POST" action="?tab=rai_push" onsubmit="return this.to.value === 'login' || confirm('Отправить уведомление всем пользователям (<?= (int)$r['devices'] ?> устройств)?')">
          <input type="hidden" name="action" value="rai_push_send">
          <div class="rp-to">
            <label><input type="radio" name="to" value="all" checked> Всем</label>
            <label><input type="radio" name="to" value="login"> Одному пользователю</label>
          </div>
          <input type="text" name="login" placeholder="логин Rai (для одного пользователя)" autocomplete="off">
          <label>Заголовок</label>
          <input type="text" name="title" maxlength="80" required placeholder="Например: В Rai новая функция ✨" oninput="document.getElementById('rpT').textContent = this.value || 'Заголовок'">
          <label>Текст</label>
          <textarea name="body" maxlength="300" rows="3" placeholder="Например: Теперь Rai знает лекарства и подскажет аналоги в вашей стране." oninput="document.getElementById('rpB').textContent = this.value"></textarea>
          <label>Куда вести по нажатию</label>
          <input type="text" name="url" placeholder="chat.html (чат Rai) или полный адрес https://…">
          <div class="rp-preview"><i>R</i><div><b id="rpT">Заголовок</b><span id="rpB"></span></div></div>
          <button class="btn primary" type="submit" style="margin-top:10px;">🔔 Отправить</button>
        </form>
      </div>

      <div class="card" style="margin-top:14px;">
        <h3>📱 Уведомления вам</h3>
        <p class="meta">Новые нарушения правил, оплаты и регистрации — сразу на телефон или компьютер, даже когда админка закрыта.</p>
        <?php if ($link): ?>
          <a class="btn primary" href="<?= rai_h($link) ?>" target="_blank" rel="noopener">🔔 Получать на этом устройстве</a>
          <p class="meta" style="font-size:12px;">Откроется страница rai.rteam.info — нажмите там «Включить». Ссылка действует 1 час. Для телефона — откройте эту вкладку админки на телефоне.</p>
        <?php endif; ?>
        <?php if ($r['admins']): ?>
          <table style="margin-top:8px;">
            <thead><tr><th>Устройство</th><th>Что приходит</th><th></th></tr></thead>
            <tbody>
            <?php foreach ($r['admins'] as $d): ?>
              <tr>
                <td><b><?= rai_h($d['device'] ?: 'Устройство') ?></b><br><span class="muted" style="font-size:12px;">с <?= rai_date($d['created']) ?><?= $d['admin'] ? ' · ' . rai_h($d['admin']) : '' ?></span></td>
                <td style="font-size:13px;"><?= rai_h(implode(', ', array_map(function ($e) use ($r) { return $r['events'][$e] ?? $e; }, $d['events'] ?: array_keys($r['events'])))) ?></td>
                <td><form method="POST" action="?tab=rai_push" style="margin:0;" onsubmit="return confirm('Отключить уведомления на этом устройстве?')">
                  <input type="hidden" name="action" value="rai_push_del"><input type="hidden" name="id" value="<?= rai_h($d['id']) ?>">
                  <button class="btn" type="submit" title="Отключить">✕</button></form></td>
              </tr>
            <?php endforeach; ?>
            </tbody>
          </table>
        <?php else: ?>
          <p class="meta">Пока ни одного устройства.</p>
        <?php endif; ?>
      </div>
    </div>

    <div class="card" style="margin-top:14px;">
      <h3>🕓 Рассылки</h3>
      <?php if (!$r['log']): ?><p class="meta">Рассылок ещё не было.</p><?php else: ?>
      <div class="table-wrap" style="overflow-x:auto;">
      <table>
        <thead><tr><th>Когда</th><th>Кому</th><th>Уведомление</th><th>Доставлено</th><th>Кто</th></tr></thead>
        <tbody>
        <?php foreach ($r['log'] as $l): ?>
          <tr>
            <td style="white-space:nowrap;"><?= date('d.m.Y H:i', (int)$l['time']) ?></td>
            <td><?= rai_h($l['to']) ?></td>
            <td><b><?= rai_h($l['title']) ?></b><?= $l['body'] !== '' ? '<br><span class="muted" style="font-size:13px;">' . rai_h($l['body']) . '</span>' : '' ?></td>
            <td><?= (int)$l['sent'] ?><?= $l['failed'] ? ' <span class="muted">(не дошло: ' . (int)$l['failed'] . ')</span>' : '' ?></td>
            <td><?= rai_h($l['by'] ?? '') ?></td>
          </tr>
        <?php endforeach; ?>
        </tbody>
      </table>
      </div>
      <?php endif; ?>
    </div>
    <?php
}

// ======================================================================= вкладка «Rai: скидки»
// Акция (скидка для всех на время, видна на главной с зачёркнутой ценой) и промокоды (вводят при оплате).
// Скидки не складываются: берётся бо́льшая. Считает и применяет rai.rteam.info (plans.php → plan_offer).

function rai_sale_back($extra = '') {
    header('Location: admin.php?tab=rai_sale' . $extra);
    exit;
}

function rai_sale_post() {
    global $user;
    $action = (string)($_POST['action'] ?? '');
    $plans = array_values(array_intersect(RAI_PLAN_KEYS, (array)($_POST['plans'] ?? [])));
    if ($action === 'rai_sale') {
        $r = rai_api('sale_save', ['percent' => (int)($_POST['percent'] ?? 0), 'days' => (int)($_POST['days'] ?? 0),
                                   'title' => (string)($_POST['title'] ?? ''), 'plans' => $plans]);
        if (!empty($r['ok'])) { rai_log("$user включил акцию Rai −{$r['sale']['percent']}% «{$r['sale']['title']}»"); flash('🏷 Акция включена: −' . $r['sale']['percent'] . '%', 'success'); }
        else flash('Не получилось: ' . ($r['error'] ?? '?'), 'error');
        rai_sale_back();
    }
    if ($action === 'rai_sale_off') {
        $r = rai_api('sale_off');
        if (!empty($r['ok'])) rai_log("$user выключил акцию Rai");
        flash(!empty($r['ok']) ? 'Акция выключена — цены обычные.' : 'Не получилось: ' . ($r['error'] ?? '?'), !empty($r['ok']) ? 'success' : 'error');
        rai_sale_back();
    }
    if ($action === 'rai_promo') {
        $r = rai_api('promo_save', ['code' => (string)($_POST['code'] ?? ''), 'percent' => (int)($_POST['percent'] ?? 0), 'days' => (int)($_POST['days'] ?? 0),
                                    'uses_max' => (int)($_POST['uses_max'] ?? 0), 'note' => (string)($_POST['note'] ?? ''), 'plans' => $plans]);
        if (!empty($r['ok'])) { rai_log("$user создал промокод Rai {$r['code']}"); flash('🎟 Промокод ' . $r['code'] . ' сохранён.', 'success'); }
        else flash('Не получилось: ' . ($r['error'] ?? '?'), 'error');
        rai_sale_back();
    }
    if ($action === 'rai_promo_del') {
        $r = rai_api('promo_delete', ['code' => (string)($_POST['code'] ?? '')]);
        flash(!empty($r['ok']) ? 'Промокод удалён.' : 'Не получилось: ' . ($r['error'] ?? '?'), !empty($r['ok']) ? 'success' : 'error');
        rai_sale_back();
    }
}

function rai_sale_render() {
    $c = rai_conf();
    $r = $c['ready'] ? rai_api('discounts_get') : ['ok' => false, 'error' => 'Сначала подключите Rai во вкладке «Rai: подписки».'];
    $ok = !empty($r['ok']);
    $plan_names = $ok ? array_map(function ($p) { return $p['name']; }, $r['plans']) : [];
    $who = function ($list) use ($plan_names) {
        return $list ? implode(', ', array_map(function ($k) use ($plan_names) { return $plan_names[$k] ?? $k; }, $list)) : 'все тарифы';
    };
    $site = rai_site_url();
    ?>
    <style>
      .rs-hero { display: flex; gap: 16px; align-items: center; flex-wrap: wrap; padding: 18px 20px; border-radius: 16px; border: 1px solid var(--line);
        background: radial-gradient(120% 140% at 100% 0%, rgba(255, 176, 32, .2), transparent 55%), radial-gradient(100% 120% at 0% 100%, rgba(255, 61, 129, .14), transparent 60%), var(--panel); }
      .rs-logo { width: 52px; height: 52px; border-radius: 15px; display: grid; place-items: center; font-size: 26px; background: linear-gradient(135deg, #ffb020, #ff3d81); }
      .rs-hero h2 { margin: 0; } .rs-hero p { margin: 4px 0 0; }
      .rs-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 14px; }
      .rs-plans { display: flex; gap: 12px; flex-wrap: wrap; margin: 4px 0 8px; } .rs-plans label { display: flex; gap: 6px; align-items: center; margin: 0; }
      .rs-now { padding: 12px 14px; border-radius: 12px; border: 1px solid rgba(255, 176, 32, .5); background: rgba(255, 176, 32, .1); margin-bottom: 10px; }
      .rs-prices { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 10px; }
      .rs-prices div { padding: 10px 12px; border-radius: 12px; border: 1px solid var(--line); }
      .rs-prices s { opacity: .55; } .rs-code { font-family: ui-monospace, Consolas, monospace; font-weight: 700; letter-spacing: .04em; }
    </style>
    <div class="rs-hero">
      <div class="rs-logo">🏷</div>
      <div style="flex:1; min-width:220px;">
        <h2>Rai — скидки</h2>
        <p class="muted">Акция — скидка для всех на время (на главной видно старую цену зачёркнутой). Промокоды — скидка по коду при оплате.
          Скидки не складываются: покупатель получает бо́льшую.</p>
      </div>
      <a class="btn" href="<?= rai_h($site) ?>/#pricing" target="_blank" rel="noopener">Посмотреть цены на сайте ↗</a>
    </div>
    <?php if (!$ok): ?>
      <div class="card" style="margin-top:14px;"><p class="meta">⚠️ <?= rai_h($r['error'] ?? 'Нет связи с rai.rteam.info') ?></p></div>
    <?php return; endif; ?>

    <div class="card" style="margin-top:14px;">
      <h3>💰 Цены сейчас</h3>
      <div class="rs-prices">
        <?php foreach ($r['prices'] as $k => $p): ?>
          <div><b><?= rai_h($plan_names[$k] ?? $k) ?></b><br>
            месяц: <?php if ($p['month']['percent']): ?><s><?= rai_rub($p['month']['base']) ?></s> <?php endif; ?><b><?= rai_rub($p['month']['price']) ?></b><br>
            год: <?php if ($p['year']['percent']): ?><s><?= rai_rub($p['year']['base']) ?></s> <?php endif; ?><b><?= rai_rub($p['year']['price']) ?></b></div>
        <?php endforeach; ?>
      </div>
    </div>

    <div class="rs-grid">
      <div class="card" style="margin-top:14px;">
        <h3>🔥 Акция для всех</h3>
        <?php if (!empty($r['sale'])): $s = $r['sale']; ?>
          <div class="rs-now"><b><?= rai_h($s['title']) ?>: −<?= (int)$s['percent'] ?>%</b> · <?= rai_h($who($s['plans'] ?? [])) ?> ·
            <?= !empty($s['until']) ? 'до ' . rai_date($s['until']) : 'без срока' ?><?= empty($r['sale_active']) ? ' <span class="badge badge-dec">закончилась</span>' : '' ?>
            <form method="POST" action="?tab=rai_sale" style="margin:8px 0 0;"><input type="hidden" name="action" value="rai_sale_off"><button class="btn" type="submit">Выключить акцию</button></form></div>
        <?php else: ?><p class="meta">Сейчас акции нет — цены обычные.</p><?php endif; ?>
        <form method="POST" action="?tab=rai_sale">
          <input type="hidden" name="action" value="rai_sale">
          <label>Название (видно на сайте)</label>
          <input type="text" name="title" maxlength="60" placeholder="Например: Чёрная пятница" value="<?= rai_h($r['sale']['title'] ?? '') ?>">
          <label>Скидка, %</label>
          <input type="number" name="percent" min="1" max="90" required value="<?= (int)($r['sale']['percent'] ?? 20) ?>">
          <label>Тарифы (ничего не выбрано — все)</label>
          <div class="rs-plans"><?php foreach ($plan_names as $k => $n): ?><label><input type="checkbox" name="plans[]" value="<?= rai_h($k) ?>"<?= in_array($k, $r['sale']['plans'] ?? [], true) ? ' checked' : '' ?>> <?= rai_h($n) ?></label><?php endforeach; ?></div>
          <label>Сколько дней действует (0 — пока не выключите)</label>
          <input type="number" name="days" min="0" max="365" value="7">
          <button class="btn primary" type="submit" style="margin-top:10px;"><?= !empty($r['sale']) ? 'Обновить акцию' : 'Включить акцию' ?></button>
        </form>
      </div>

      <div class="card" style="margin-top:14px;">
        <h3>🎟 Новый промокод</h3>
        <form method="POST" action="?tab=rai_sale">
          <input type="hidden" name="action" value="rai_promo">
          <label>Код (буквы и цифры)</label>
          <input type="text" name="code" maxlength="32" required placeholder="Например: RAI20" style="text-transform:uppercase;" autocomplete="off">
          <label>Скидка, %</label>
          <input type="number" name="percent" min="1" max="95" required value="20">
          <label>Тарифы (ничего не выбрано — все)</label>
          <div class="rs-plans"><?php foreach ($plan_names as $k => $n): ?><label><input type="checkbox" name="plans[]" value="<?= rai_h($k) ?>"> <?= rai_h($n) ?></label><?php endforeach; ?></div>
          <label>Сколько дней действует (0 — бессрочно)</label>
          <input type="number" name="days" min="0" max="3650" value="30">
          <label>Сколько раз можно использовать (0 — без ограничений)</label>
          <input type="number" name="uses_max" min="0" max="100000" value="0">
          <label>Заметка (для себя)</label>
          <input type="text" name="note" maxlength="100" placeholder="Например: для блогера Ивана">
          <button class="btn primary" type="submit" style="margin-top:10px;">Создать промокод</button>
        </form>
      </div>
    </div>

    <div class="card" style="margin-top:14px;">
      <h3>📋 Промокоды (<?= count($r['promos']) ?>)</h3>
      <?php if (!$r['promos']): ?><p class="meta">Промокодов пока нет.</p><?php else: ?>
      <div class="table-wrap" style="overflow-x:auto;">
      <table>
        <thead><tr><th>Код</th><th>Скидка</th><th>Тарифы</th><th>Действует</th><th>Использован</th><th>Выручка</th><th></th></tr></thead>
        <tbody>
        <?php foreach ($r['promos'] as $code => $p): $dead = (!empty($p['until']) && $p['until'] < $r['now']) || (!empty($p['uses_max']) && ($p['used'] ?? 0) >= $p['uses_max']); ?>
          <tr<?= $dead ? ' style="opacity:.55;"' : '' ?>>
            <td><span class="rs-code"><?= rai_h($code) ?></span><?= !empty($p['note']) ? '<br><span class="muted" style="font-size:12px;">' . rai_h($p['note']) . '</span>' : '' ?></td>
            <td>−<?= (int)$p['percent'] ?>%</td>
            <td style="font-size:13px;"><?= rai_h($who($p['plans'] ?? [])) ?></td>
            <td><?= !empty($p['until']) ? 'до ' . rai_date($p['until']) : 'бессрочно' ?><?= $dead ? '<br><span class="badge badge-dec">не действует</span>' : '' ?></td>
            <td><?= (int)($p['used'] ?? 0) ?><?= !empty($p['uses_max']) ? ' из ' . (int)$p['uses_max'] : '' ?></td>
            <td><?= rai_rub($r['promo_revenue'][$code] ?? 0) ?></td>
            <td><form method="POST" action="?tab=rai_sale" style="margin:0;" onsubmit="return confirm('Удалить промокод <?= rai_h($code) ?>?')">
              <input type="hidden" name="action" value="rai_promo_del"><input type="hidden" name="code" value="<?= rai_h($code) ?>"><button class="btn" type="submit" title="Удалить">🗑</button></form></td>
          </tr>
        <?php endforeach; ?>
        </tbody>
      </table>
      </div>
      <?php endif; ?>
    </div>
    <?php
}

if ($tab === "rai") rai_admin_export();  // ?tab=rai&export=csv — файл вместо страницы
if ($_SERVER["REQUEST_METHOD"] === "POST" && $tab === "rai") rai_admin_post();
if ($_SERVER["REQUEST_METHOD"] === "POST" && $tab === "rai_rules") rai_rules_post();
if ($_SERVER["REQUEST_METHOD"] === "POST" && $tab === "rai_push") rai_push_post();
if ($_SERVER["REQUEST_METHOD"] === "POST" && $tab === "rai_sale") rai_sale_post();
/* ==== RAI: КОНЕЦ ==== */

/* POST ДЛЯ ОСТАЛЬНОГО */
if ($_SERVER["REQUEST_METHOD"] === "POST" && $tab !== "apps") {

    /* --- ДИРЕКТОРА ШКОЛ --- */
    if ($tab === "directors") {
        $action = $_POST["action"] ?? "";

        if ($action === "approve_request") {
            $id = $_POST["id"] ?? "";
            foreach ($director_requests as &$req) {
                if ((string)$req["id"] === (string)$id && ($req["status"] ?? "pending") === "pending") {
                    // Название школы и логин должны быть уникальны — иначе
                    // потом не получится однозначно найти школу по имени
                    // (именно так раньше ломался вход программиста).
                    if (dl_schoolname_taken($req["schoolName"])) {
                        $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "director_approve_fail", "msg" => "$user не смог одобрить заявку «{$req["schoolName"]}»: школа с таким названием уже существует."];
                        save_json("logs.json", $logs);
                        break;
                    }
                    $slugBase = slugify_school($req["schoolName"]);
                    $slug = unique_school_slug($slugBase);
                    $emptySchool = ["teachers" => [], "schedulers" => [], "quarterDates" => new stdClass(), "site" => ["title" => "", "blocks" => []], "studentAccounts" => [], "studentProfiles" => new stdClass(), "teacherProfiles" => new stdClass(), "remarks" => new stdClass(), "promoCodes" => new stdClass()];
                    $schoolFile = copy_school_journal($req["schoolName"], $slug);

                    dl_save_full([
                        "id" => uniqid(),
                        "schoolName" => $req["schoolName"],
                        "slug" => $slug,
                        "schoolFile" => $schoolFile,
                        "login" => $req["login"],
                        "password" => $req["password"],
                        "email" => $req["email"],
                        "fullName" => $req["fullName"] ?? "",
                        "phone" => $req["phone"] ?? "",
                        "data" => $emptySchool
                    ]);

                    $req["status"] = "approved";
                    $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "director_approve", "msg" => "$user одобрил заявку директора школы «{$req["schoolName"]}» (логин {$req["login"]})."];
                    save_json("logs.json", $logs);
                    break;
                }
            }
            unset($req);
            save_json("director_requests.json", $director_requests);
            header("Location: admin.php?tab=directors"); exit;
        }

        if ($action === "decline_request") {
            $id = $_POST["id"] ?? "";
            foreach ($director_requests as &$req) {
                if ((string)$req["id"] === (string)$id && ($req["status"] ?? "pending") === "pending") {
                    $req["status"] = "declined";
                    break;
                }
            }
            unset($req);
            save_json("director_requests.json", $director_requests);
            header("Location: admin.php?tab=directors"); exit;
        }

        if ($action === "delete_request") {
            $id = $_POST["id"] ?? "";
            $director_requests = array_values(array_filter($director_requests, fn($r) => (string)$r["id"] !== (string)$id));
            save_json("director_requests.json", $director_requests);
            header("Location: admin.php?tab=directors"); exit;
        }

        if ($action === "add_director") {
            $schoolName = trim($_POST["schoolName"] ?? "");
            $login = trim($_POST["login"] ?? "");
            $password = trim($_POST["password"] ?? "");
            $email = trim($_POST["email"] ?? "");
            $fullName = trim($_POST["fullName"] ?? "");
            if ($schoolName !== "" && $login !== "" && $password !== "" && !dl_schoolname_taken($schoolName) && !dl_login_taken($login)) {
                $slugBase = slugify_school($schoolName);
                $slug = unique_school_slug($slugBase);
                $emptySchool = ["teachers" => [], "schedulers" => [], "quarterDates" => new stdClass(), "site" => ["title" => "", "blocks" => []], "studentAccounts" => [], "studentProfiles" => new stdClass(), "teacherProfiles" => new stdClass(), "remarks" => new stdClass(), "promoCodes" => new stdClass()];
                $schoolFile = copy_school_journal($schoolName, $slug);
                dl_save_full([
                    "id" => uniqid(),
                    "schoolName" => $schoolName,
                    "slug" => $slug,
                    "schoolFile" => $schoolFile,
                    "login" => $login,
                    "password" => $password,
                    "email" => $email,
                    "fullName" => $fullName,
                    "phone" => "",
                    "data" => $emptySchool
                ]);
                $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "director_add", "msg" => "$user вручную создал аккаунт директора школы «{$schoolName}» (логин {$login})."];
                save_json("logs.json", $logs);
            }
            header("Location: admin.php?tab=directors"); exit;
        }

        if ($action === "reset_director_password") {
            $id = $_POST["id"] ?? ""; $slug = $_POST["slug"] ?? ""; $newPass = trim($_POST["password"] ?? "");
            if ($newPass !== "") {
                $d = dl_load_full($slug);
                if ($d && (string)$d["id"] === (string)$id) { $d["password"] = $newPass; dl_save_full($d); }
            }
            header("Location: admin.php?tab=directors"); exit;
        }

        if ($action === "regenerate_file") {
            $slug = $_POST["slug"] ?? "";
            $d = dl_load_full($slug);
            if ($d) {
                $newFile = copy_school_journal($d["schoolName"], $d["slug"]);
                if ($newFile) { $d["schoolFile"] = $newFile; dl_save_full($d); }
            }
            header("Location: admin.php?tab=directors"); exit;
        }

        // Пересоздаёт страницу-редирект dairy.html и сайт-визитку для школ
        // ПАЧКАМИ (по $batchSize за раз), а не всех разом — при большом числе
        // школ один проход по всем им в одном запросе просто не уложится в
        // лимит времени выполнения PHP-скрипта и оборвётся на середине.
        // Каждая ссылка "Дальше" обрабатывает следующую пачку и сама
        // подставляет offset для следующего шага, пока не дойдёт до конца.
        if ($action === "regenerate_all") {
            $batchSize = 300;
            $offset = (int)($_POST["offset"] ?? 0);
            $page = dl_list_index_page("", $offset, $batchSize);
            foreach ($page["rows"] as $row) {
                $d = dl_load_full($row["slug"]);
                if (!$d) continue;
                $newFile = copy_school_journal($d["schoolName"], $d["slug"]);
                if ($newFile) { $d["schoolFile"] = $newFile; dl_save_full($d); }
            }
            $nextOffset = $offset + $batchSize;
            if ($nextOffset < $page["total"]) {
                header("Location: admin.php?tab=directors&regen_offset=$nextOffset&regen_total={$page['total']}"); exit;
            }
            header("Location: admin.php?tab=directors&regen_done=1"); exit;
        }

        if ($action === "delete_director") {
            $slug = $_POST["slug"] ?? "";
            dl_delete($slug);
            header("Location: admin.php?tab=directors"); exit;
        }
    }

    /* --- БАНЫ --- */
    if ($tab === "bans") {
        if ($_POST["action"] === "add_ban") {
            $ip = trim($_POST["ip"] ?? ""); $duration = (int)($_POST["duration"] ?? 0); $reason = trim($_POST["reason"] ?? "Спам");
            if ($ip === rt_client_ip()) {
                flash("Это ваш собственный IP — бан закрыл бы сайт вам самим.", "error");
            } elseif (!empty($ip)) {
                $expires = ($duration === 0) ? 0 : time() + ($duration * 3600);
                $bans[] = ["id" => time(), "ip" => $ip, "reason" => $reason, "expires" => $expires, "issued_by" => $user, "date" => date("Y-m-d H:i:s")];
                save_json("bans.json", $bans);
                $logs[] = ["time"=>date("Y-m-d H:i:s"), "type"=>"ban", "msg"=>"$user забанил IP: $ip. Причина: $reason"]; save_json("logs.json", $logs);
            }
            header("Location: admin.php?tab=bans"); exit;
        }
        if ($_POST["action"] === "unban") {
            $id = $_POST["id"]; $bans = array_filter($bans, fn($b) => (string)$b["id"] !== (string)$id); save_json("bans.json", array_values($bans)); header("Location: admin.php?tab=bans"); exit;
        }
        if ($_POST["action"] === "save_geoblock") {
            $codes = preg_split('/[\s,]+/', mb_strtoupper(trim($_POST["countries"] ?? "")), -1, PREG_SPLIT_NO_EMPTY);
            $codes = array_values(array_unique(array_filter($codes, fn($c) => preg_match('/^[A-Z]{2}$/', $c))));
            $geoblock_settings = ["enabled" => isset($_POST["enabled"]), "countries" => $codes];
            save_json("geoblock.json", $geoblock_settings);
            $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "geoblock", "msg" => "$user изменил гео-блокировку: " . (isset($_POST["enabled"]) ? "включена" : "выключена") . ", страны: " . implode(", ", $codes)];
            save_json("logs.json", $logs);
            header("Location: admin.php?tab=bans"); exit;
        }
        if ($_POST["action"] === "clear_blocked_attempts") {
            save_json("blocked_attempts.json", []);
            header("Location: admin.php?tab=bans"); exit;
        }
    }

    /* --- ЧЁРНЫЙ СПИСОК --- */
    if ($tab === "blacklist") {
        if ($_POST["action"] === "add_blacklist") {
            $identifier = trim($_POST["identifier"] ?? "");
            $ip         = trim($_POST["ip"] ?? "");
            $reason     = trim($_POST["reason"] ?? "");
            $date_added = trim($_POST["date_added"] ?? "") ?: date("Y-m-d");
            $ip_valid   = $ip !== "" && filter_var($ip, FILTER_VALIDATE_IP) !== false;
            if ($identifier !== "" || $ip !== "") {
                $entry = [
                    "id"         => (string)time() . rand(100, 999),
                    "identifier" => $identifier !== "" ? $identifier : $ip,
                    "ip"         => $ip,
                    "reason"     => $reason,
                    "date_added" => $date_added,
                    "added_by"   => $user,
                    "signed"     => false, "signed_by" => null, "signed_at" => null,
                    "released"   => false, "released_by" => null, "released_at" => null, "release_reason" => "",
                    "ip_ban_id"  => null,
                ];
                // Если указан валидный IP — он реально банится сразу, без отдельного подписания
                if ($ip_valid) {
                    $ban_id = time() . rand(100, 999);
                    $bans[] = ["id" => $ban_id, "ip" => $ip, "reason" => ($reason !== "" ? $reason : "Чёрный список"), "expires" => 0, "issued_by" => $user, "date" => date("Y-m-d H:i:s")];
                    save_json("bans.json", $bans);
                    $entry["ip_ban_id"] = $ban_id;
                    $entry["signed"]    = true;
                    $entry["signed_by"] = $user;
                    $entry["signed_at"] = date("Y-m-d H:i:s");
                }
                $blacklist[] = $entry;
                save_json("blacklist.json", $blacklist);
                $log_msg = "$user добавил в чёрный список: {$entry["identifier"]}";
                if ($ip_valid) $log_msg .= " (IP $ip забанен)";
                if ($reason !== "") $log_msg .= ". Причина: $reason";
                $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "blacklist", "msg" => $log_msg];
                save_json("logs.json", $logs);
            }
            header("Location: admin.php?tab=blacklist"); exit;
        }
        if ($_POST["action"] === "sign_blacklist") {
            $id = $_POST["id"] ?? "";
            foreach ($blacklist as &$bl) {
                if ((string)$bl["id"] === (string)$id && empty($bl["signed"])) {
                    $bl["signed"] = true; $bl["signed_by"] = $user; $bl["signed_at"] = date("Y-m-d H:i:s");
                    $ip_to_ban = trim($bl["ip"] ?? "");
                    if ($ip_to_ban === "" && filter_var($bl["identifier"], FILTER_VALIDATE_IP)) $ip_to_ban = $bl["identifier"];
                    if ($ip_to_ban !== "" && filter_var($ip_to_ban, FILTER_VALIDATE_IP)) {
                        $ban_id = time() . rand(100, 999);
                        $bans[] = ["id" => $ban_id, "ip" => $ip_to_ban, "reason" => (!empty($bl["reason"]) ? $bl["reason"] : "Чёрный список"), "expires" => 0, "issued_by" => $user, "date" => date("Y-m-d H:i:s")];
                        save_json("bans.json", $bans);
                        $bl["ip_ban_id"] = $ban_id;
                    }
                    $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "blacklist", "msg" => "$user подписал запись чёрного списка: {$bl["identifier"]}"];
                    save_json("logs.json", $logs);
                    break;
                }
            }
            unset($bl);
            save_json("blacklist.json", $blacklist);
            header("Location: admin.php?tab=blacklist"); exit;
        }
        if ($_POST["action"] === "release_blacklist") {
            $id = $_POST["id"] ?? ""; $reason = trim($_POST["release_reason"] ?? "");
            foreach ($blacklist as &$bl) {
                if ((string)$bl["id"] === (string)$id && !empty($bl["signed"]) && empty($bl["released"])) {
                    $bl["released"] = true; $bl["released_by"] = $user; $bl["released_at"] = date("Y-m-d H:i:s"); $bl["release_reason"] = $reason;
                    if (!empty($bl["ip_ban_id"])) {
                        $bans = array_values(array_filter($bans, fn($b) => (string)$b["id"] !== (string)$bl["ip_ban_id"]));
                        save_json("bans.json", $bans);
                    }
                    $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "blacklist", "msg" => "$user выпустил из чёрного списка: {$bl["identifier"]}. Причина: $reason"];
                    save_json("logs.json", $logs);
                    break;
                }
            }
            unset($bl);
            save_json("blacklist.json", $blacklist);
            header("Location: admin.php?tab=blacklist"); exit;
        }
        if ($_POST["action"] === "edit_blacklist") {
            $id = $_POST["id"] ?? "";
            $new_identifier = trim($_POST["identifier"] ?? "");
            $new_ip         = trim($_POST["ip"] ?? "");
            $new_reason     = trim($_POST["reason"] ?? "");
            $new_date       = trim($_POST["date_added"] ?? "");
            foreach ($blacklist as &$bl) {
                if ((string)$bl["id"] === (string)$id) {
                    $old_identifier = $bl["identifier"];
                    $old_ip         = trim($bl["ip"] ?? "");
                    if ($new_identifier !== "") $bl["identifier"] = $new_identifier;
                    $bl["ip"]     = $new_ip;
                    $bl["reason"] = $new_reason;
                    if ($new_date !== "") $bl["date_added"] = $new_date;

                    $new_ip_valid = $new_ip !== "" && filter_var($new_ip, FILTER_VALIDATE_IP) !== false;
                    $ip_changed   = $new_ip !== $old_ip;

                    if (empty($bl["released"])) {
                        if ($ip_changed) {
                            // IP поменялся или убран — снимаем старый реальный бан
                            if (!empty($bl["ip_ban_id"])) {
                                $bans = array_values(array_filter($bans, fn($b) => (string)$b["id"] !== (string)$bl["ip_ban_id"]));
                                $bl["ip_ban_id"] = null;
                            }
                            if ($new_ip_valid) {
                                $ban_id = time() . rand(100, 999);
                                $bans[] = ["id" => $ban_id, "ip" => $new_ip, "reason" => ($new_reason !== "" ? $new_reason : "Чёрный список"), "expires" => 0, "issued_by" => $user, "date" => date("Y-m-d H:i:s")];
                                $bl["ip_ban_id"] = $ban_id;
                                $bl["signed"] = true; $bl["signed_by"] = $user; $bl["signed_at"] = date("Y-m-d H:i:s");
                            }
                            save_json("bans.json", $bans);
                        } elseif (!empty($bl["ip_ban_id"]) && $new_reason !== "") {
                            // IP не менялся, но обновили причину — синхронизируем её с баном
                            foreach ($bans as &$b) {
                                if ((string)$b["id"] === (string)$bl["ip_ban_id"]) { $b["reason"] = $new_reason; break; }
                            }
                            unset($b);
                            save_json("bans.json", $bans);
                        }
                    }
                    $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "blacklist", "msg" => "$user отредактировал запись чёрного списка: $old_identifier -> {$bl["identifier"]}"];
                    save_json("logs.json", $logs);
                    break;
                }
            }
            unset($bl);
            save_json("blacklist.json", $blacklist);
            save_json("logs.json", $logs);
            header("Location: admin.php?tab=blacklist"); exit;
        }
        if ($_POST["action"] === "delete_blacklist") {
            $id = $_POST["id"] ?? "";
            foreach ($blacklist as $bl) {
                if ((string)$bl["id"] === (string)$id) {
                    if (!empty($bl["ip_ban_id"]) && empty($bl["released"])) {
                        $bans = array_values(array_filter($bans, fn($b) => (string)$b["id"] !== (string)$bl["ip_ban_id"]));
                        save_json("bans.json", $bans);
                    }
                    break;
                }
            }
            $blacklist = array_values(array_filter($blacklist, fn($bl) => (string)$bl["id"] !== (string)$id));
            save_json("blacklist.json", $blacklist);
            header("Location: admin.php?tab=blacklist"); exit;
        }
    }

    /* --- ПРОЕКТЫ --- */
    if ($tab === "projects") {
        if ($_POST["action"] === "add_project") {
            $title = trim($_POST["title"]); $desc = trim($_POST["description"]); $type = $_POST["type"]; $path = ""; $icon_path = "";
            if (isset($_FILES["icon"]) && $_FILES["icon"]['error'] === UPLOAD_ERR_OK) $icon_path = process_and_compress_image($_FILES["icon"]);
            if ($type === "link") $path = filter_var($_POST["link"], FILTER_SANITIZE_URL);
            elseif (in_array($type, ["file", "zip_view"]) && isset($_FILES["file"])) {
                $dir = "uploads/files/"; if (!is_dir($dir)) mkdir($dir, 0755, true);
                $path = $dir . time() . "_" . basename($_FILES["file"]["name"]); move_uploaded_file($_FILES["file"]["tmp_name"], $path);
            } elseif ($type === "site" && isset($_FILES["file"])) {
                $zip = new ZipArchive;
                if ($zip->open($_FILES["file"]["tmp_name"]) === TRUE) {
                    $site_dir = "uploads/sites/" . time() . "_" . uniqid() . "/"; if (!is_dir($site_dir)) mkdir($site_dir, 0755, true);
                    $zip->extractTo($site_dir); $zip->close(); $entry_point = $site_dir; 
                    $iterator = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($site_dir));
                    foreach ($iterator as $file) { $filename = strtolower($file->getFilename()); if ($filename === 'index.html' || $filename === 'index.php') { $entry_point = str_replace("\\", "/", $file->getPathname()); break; } }
                    $path = $entry_point;
                }
            }
            if (!empty($title) && !empty($path)) { $projects_data[] = ["id" => time(),"title" => $title,"description" => $desc,"type" => $type,"path" => $path,"icon" => $icon_path,"hidden" => false]; save_json("projects.json", $projects_data); }
            header("Location: admin.php?tab=projects"); exit;
        }

        if ($_POST["action"] === "edit_project") {
            $id = $_POST["id"];
            foreach ($projects_data as &$p) {
                if ((string)$p["id"] === (string)$id) {
                    $p["title"] = trim($_POST["title"]); $p["description"] = trim($_POST["description"]);
                    if (isset($_FILES["icon"]) && $_FILES["icon"]['error'] === UPLOAD_ERR_OK) {
                        if (!empty($p["icon"]) && file_exists($p["icon"])) unlink($p["icon"]);
                        $p["icon"] = process_and_compress_image($_FILES["icon"]);
                    }
                    $type_changed = ($p["type"] !== $_POST["type"]); $file_uploaded = (isset($_FILES["file"]) && $_FILES["file"]['error'] === UPLOAD_ERR_OK);
                    if ($type_changed || $file_uploaded) {
                        if (in_array($p["type"], ["file", "zip_view"]) && file_exists($p["path"])) unlink($p["path"]);
                        if ($p["type"] === "site") { preg_match('/uploads\/sites\/[^\/]+\//', $p["path"] . '/', $matches); if (!empty($matches[0]) && is_dir($matches[0])) delete_directory($matches[0]); }
                        $p["type"] = $_POST["type"];
                        if ($p["type"] === "link") $p["path"] = filter_var($_POST["link"], FILTER_SANITIZE_URL);
                        elseif (in_array($p["type"], ["file", "zip_view"]) && $file_uploaded) {
                            $dir = "uploads/files/"; if (!is_dir($dir)) mkdir($dir, 0755, true); $p["path"] = $dir . time() . "_" . basename($_FILES["file"]["name"]); move_uploaded_file($_FILES["file"]["tmp_name"], $p["path"]);
                        } elseif ($p["type"] === "site" && $file_uploaded) {
                            $zip = new ZipArchive;
                            if ($zip->open($_FILES["file"]["tmp_name"]) === TRUE) {
                                $site_dir = "uploads/sites/" . time() . "_" . uniqid() . "/"; if (!is_dir($site_dir)) mkdir($site_dir, 0755, true);
                                $zip->extractTo($site_dir); $zip->close(); $entry_point = $site_dir; 
                                $iterator = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($site_dir));
                                foreach ($iterator as $file) { $filename = strtolower($file->getFilename()); if ($filename === 'index.html' || $filename === 'index.php') { $entry_point = str_replace("\\", "/", $file->getPathname()); break; } }
                                $p["path"] = $entry_point;
                            }
                        }
                    } elseif ($p["type"] === "link" && isset($_POST["link"])) { $p["path"] = filter_var($_POST["link"], FILTER_SANITIZE_URL); }
                    break;
                }
            } unset($p); save_json("projects.json", $projects_data); header("Location: admin.php?tab=projects"); exit;
        }
        
        if ($_POST["action"] === "toggle_project") { $id = $_POST["id"]; foreach ($projects_data as &$p) { if ((string)$p["id"] === (string)$id) { $p["hidden"] = !empty($p["hidden"]) ? false : true; break; } } unset($p); save_json("projects.json", $projects_data); header("Location: admin.php?tab=projects"); exit; }
        
        if ($_POST["action"] === "del_project") {
            $id = $_POST["id"]; $new = [];
            foreach ($projects_data as $p) {
                if ((string)$p["id"] === (string)$id) {
                    if (in_array($p["type"], ["file", "zip_view"]) && file_exists($p["path"])) unlink($p["path"]);
                    if ($p["type"] === "site") { preg_match('/uploads\/sites\/[^\/]+\//', $p["path"] . '/', $matches); if (!empty($matches[0]) && is_dir($matches[0])) delete_directory($matches[0]); }
                    if (!empty($p["icon"]) && file_exists($p["icon"])) unlink($p["icon"]); continue;
                } $new[] = $p;
            } $projects_data = $new; save_json("projects.json", $projects_data); header("Location: admin.php?tab=projects"); exit;
        }
    }

    /* --- ФАЙЛЫ --- */
    if ($tab === "files") {
        if ($_POST["action"] === "upload_file") {
            $target_dept = $_POST["department"] ?? "all"; $desc = trim($_POST["description"] ?? "");
            if (!empty($_FILES["team_file"]["name"]) && $_FILES["team_file"]["error"] === 0) {
                $dir = "team_files/"; if (!is_dir($dir)) mkdir($dir, 0777, true);
                $original_name = basename($_FILES["team_file"]["name"]);
                $safe_name = time() . "_" . preg_replace('/[^a-zA-Z0-9_.-]/', '_', $original_name);
                if (move_uploaded_file($_FILES["team_file"]["tmp_name"], $dir . $safe_name)) {
                    $files_data[] = ["id" => time(), "name" => $original_name, "path" => $dir . $safe_name, "size" => filesize($dir . $safe_name), "uploader" => $user, "department" => $target_dept, "desc" => $desc, "time" => date("Y-m-d H:i:s")];
                    save_json("files.json", $files_data); $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"file_upload","msg"=>"$user загрузил файл '$original_name'"]; save_json("logs.json", $logs);
                }
            }
            header("Location: admin.php?tab=files"); exit;
        }
        if ($_POST["action"] === "del_file") {
            $id = $_POST["id"];
            foreach ($files_data as $k => $f) { if ((string)$f["id"] === (string)$id) { if (can("files.manage") || $f["uploader"] === $user) { @unlink($f["path"]); unset($files_data[$k]); $logs[] = ["time"=>date("Y-m-d H:i:s"),"type"=>"file_del","msg"=>"$user удалил файл '".$f["name"]."'"]; } break; } }
            save_json("files.json", array_values($files_data)); save_json("logs.json", $logs); header("Location: admin.php?tab=files"); exit;
        }
    }

    /* --- ЧАТ --- */
    if ($tab === "chat") {
        if ($_POST["action"] === "send_chat_msg") {
            $text = trim($_POST["text"] ?? ""); $photo_path = null;
            if (!empty($_FILES["photo"]["name"]) && $_FILES["photo"]["error"] === 0) {
                $dir = "uploads/"; if (!is_dir($dir)) mkdir($dir, 0777, true);
                $ext = strtolower(pathinfo($_FILES["photo"]["name"], PATHINFO_EXTENSION));
                if (in_array($ext, ['jpg','jpeg','png','gif','webp'])) { $filename = $dir . time() . "_" . rand(1000,9999) . "." . $ext; if (move_uploaded_file($_FILES["photo"]["tmp_name"], $filename)) $photo_path = $filename; }
            }
            $poll = null; $poll_q = trim($_POST["poll_q"] ?? ""); $poll_opt_raw = trim($_POST["poll_opt"] ?? "");
            if ($poll_q !== "" && $poll_opt_raw !== "") {
                $opts = array_filter(array_map('trim', explode(",", $poll_opt_raw)));
                if (count($opts) > 1) { $votes = []; foreach ($opts as $o) $votes[$o] = []; $poll = ["question" => $poll_q, "votes" => $votes]; }
            }
            if ($text !== "" || $photo_path !== null || $poll !== null) {
                $chat_data["messages"][] = ["id" => time() . rand(100, 999), "user" => $user, "role" => rt_role_label($role, $my_direction), "time" => date("Y-m-d H:i:s"), "text" => $text, "photo" => $photo_path, "poll" => $poll, "ip" => rt_client_ip()];
                save_json("chat.json", $chat_data);
            }
            header("Location: admin.php?tab=chat"); exit;
        }
        if ($_POST["action"] === "pin_msg") { $chat_data["pinned_id"] = $_POST["id"]; save_json("chat.json", $chat_data); header("Location: admin.php?tab=chat"); exit; }
        if ($_POST["action"] === "unpin_msg") { $chat_data["pinned_id"] = null; save_json("chat.json", $chat_data); header("Location: admin.php?tab=chat"); exit; }
        if ($_POST["action"] === "vote_poll") {
            $msg_id = $_POST["msg_id"]; $selected_opt = $_POST["option"];
            foreach ($chat_data["messages"] as &$m) {
                if ((string)$m["id"] === (string)$msg_id && isset($m["poll"])) {
                    foreach ($m["poll"]["votes"] as $opt => &$voters) $voters = array_filter($voters, fn($v) => $v !== $user); unset($voters);
                    if (isset($m["poll"]["votes"][$selected_opt])) $m["poll"]["votes"][$selected_opt][] = $user; break;
                }
            } unset($m); save_json("chat.json", $chat_data); header("Location: admin.php?tab=chat"); exit;
        }
        if ($_POST["action"] === "del_chat_msg") {
            $msg_id = $_POST["id"];
            // своё сообщение может удалить каждый, чужое — только с правом модерации чата
            $may_delete = can("chat.moderate");
            foreach ($chat_data["messages"] as $m) if ((string)$m["id"] === (string)$msg_id && $m["user"] === $user) $may_delete = true;
            if (!$may_delete) { flash("Удалять чужие сообщения может только модератор чата.", "error"); header("Location: admin.php?tab=chat"); exit; }
            $chat_data["messages"] = array_filter($chat_data["messages"], fn($m) => (string)$m["id"] !== (string)$msg_id);
            if ($chat_data["pinned_id"] === $msg_id) $chat_data["pinned_id"] = null;
            $chat_data["messages"] = array_values($chat_data["messages"]); save_json("chat.json", $chat_data); header("Location: admin.php?tab=chat"); exit;
        }
    }

    /* --- ЦЕЛИ И ШТРАФЫ --- */
    if ($_POST["action"] === "add_goal") {
        if (!can("goals.manage")) die("Нет прав");
        $title = trim($_POST["title"] ?? ""); $desc = trim($_POST["description"] ?? ""); $date = trim($_POST["deadline_date"] ?? ""); $time = trim($_POST["deadline_time"] ?? ""); $assigned_to = trim($_POST["assigned_to"] ?? "all");
        if ($title !== "" && $date !== "" && $time !== "") { $goals[] = ["id" => time(), "title" => $title, "description" => $desc, "deadline" => $date . " " . $time . ":00", "assigned_to" => $assigned_to, "created_by" => $user, "created_at" => date("Y-m-d H:i:s")]; save_json("goals.json", $goals); }
        header("Location: admin.php?tab=goals"); exit;
    }
    if ($_POST["action"] === "del_goal") { if (!can("goals.manage")) die("Нет прав"); $goals = array_filter($goals, fn($g) => (string)$g["id"] !== (string)$_POST["id"]); save_json("goals.json", array_values($goals)); header("Location: admin.php?tab=goals"); exit; }
    
    if ($_POST["action"] === "add_fine") {
        if (!can("fines.manage")) die("Нет прав");
        $fine_user = trim($_POST["user"] ?? ""); $amount = (int)($_POST["amount"] ?? 0); $reason = trim($_POST["reason"] ?? "");
        if (!isset($users[$fine_user]) || !rt_can_edit_user($user, $role, $fine_user, $users[$fine_user]["role"] ?? "")) { flash("Штраф можно выписать только тому, кто младше вас по должности.", "error"); header("Location: admin.php?tab=fines"); exit; }
        if ($fine_user !== "" && $amount >= 10 && $reason !== "") { $fines[] = ["id" => time(), "user" => $fine_user, "amount" => $amount, "reason" => $reason, "issue_date" => date("Y-m-d H:i:s"), "issued_by" => $user, "paid" => false]; save_json("fines.json", $fines); }
        header("Location: admin.php?tab=fines"); exit;
    }
    if ($_POST["action"] === "pay_fine_manual") {
        if (!can("fines.manage")) die("Нет прав");
        foreach ($fines as &$f) { if ((string)$f["id"] === (string)$_POST["id"]) { $f["paid"] = true; $f["paid_date"] = date("Y-m-d H:i:s"); break; } } unset($f);
        save_json("fines.json", $fines); header("Location: admin.php?tab=fines"); exit;
    }
    if ($_POST["action"] === "del_fine") { if (!can("fines.manage")) die("Нет прав"); $fines = array_filter($fines, fn($f) => (string)$f["id"] !== (string)$_POST["id"]); save_json("fines.json", array_values($fines)); header("Location: admin.php?tab=fines"); exit; }

    /* --- СООБЩЕНИЯ И ТИКЕТЫ (САЙТ) --- */
    if ($tab === "messages" && $_POST["action"] === "reply_msg") {
        foreach ($messages as &$m) {
            if ((string)$m["id"] === (string)$_POST["id"]) {
                $m["reply"] = ["text" => trim($_POST["reply_text"]), "author" => $user, "role" => $role, "time" => date("Y-m-d H:i:s")];
                @mail($m["email"], "Ответ — Rteam", trim($_POST["reply_text"]), "From: team@rteam.info\r\nContent-Type: text/plain; charset=UTF-8\r\n"); break;
            }
        } unset($m); save_json("messages.json", $messages); header("Location: admin.php?tab=messages"); exit;
    }
    if ($_POST["action"] === "del_msg") { $messages = array_filter($messages, fn($m) => (string)$m["id"] !== (string)$_POST["id"]); save_json("messages.json", array_values($messages)); header("Location: admin.php?tab=messages"); exit; }

    if ($tab === "support") {
        $id = (string)($_POST["id"] ?? "");
        $back = "admin.php?tab=support" . ($id !== "" ? "&ticket_id=" . urlencode($id) : "");
        if ($_POST["action"] === "reply_ticket") {
            $text = trim($_POST["reply_text"] ?? "");
            $reply_photo_path = rt_save_image_upload($_FILES["reply_photo"] ?? [], "admin_");
            if ($text !== "" || $reply_photo_path) {
                // Ответ сотрудника: тикет переходит к человеку, ИИ в нём больше не отвечает
                $t_client = rt_tickets_update(function (&$tickets) use ($id, $text, $reply_photo_path, $user) {
                    foreach ($tickets as &$t) {
                        if ((string)$t["id"] !== $id) continue;
                        $t["replies"][] = ["text" => $text !== "" ? $text : "📷 Фото", "photo" => $reply_photo_path, "employee" => $user, "date" => date("Y-m-d H:i:s"), "is_admin" => true];
                        $t["status"] = "Ожидает ответа клиента";
                        $t["ai"] = false;
                        unset($t["ai_pending"]);
                        return (string)($t["client"] ?? "");
                    }
                    return "";
                });
                // Клиент с привязанным Discord получает уведомление в ЛС от бота
                if ($t_client && !empty($users[$t_client]["discord_id"]) && rt_discord_bot_ready($settings)) {
                    rt_discord_dm($users[$t_client]["discord_id"], "В вашем обращении #$id ответил сотрудник RTeam:\n\n> " . mb_substr(str_replace("\n", "\n> ", $text !== "" ? $text : "📷 Фото"), 0, 1500),
                                  "🎧 Ответ поддержки", ["label" => "Открыть обращение", "url" => rt_site_url() . "/support.php?ticket_id=" . rawurlencode($id)], $settings);
                }
            }
            if (isset($_POST['is_ajax'])) exit; header("Location: $back"); exit;
        }
        if ($_POST["action"] === "close_ticket") {
            rt_tickets_update(function (&$tickets) use ($id) { foreach ($tickets as &$t) if ((string)$t["id"] === $id) { $t["status"] = "Закрыт"; unset($t["ai_pending"]); } });
            header("Location: $back"); exit;
        }
        // Открыть закрытый тикет снова (например, если Rai закрыл его по ошибке)
        if ($_POST["action"] === "reopen_ticket") {
            rt_tickets_update(function (&$tickets) use ($id, $user) {
                foreach ($tickets as &$t) {
                    if ((string)$t["id"] !== $id || ($t["status"] ?? "") !== "Закрыт") continue;
                    $t["status"] = "Открыт";
                    unset($t["closed"]);
                    $t["replies"][] = ["text" => "Сотрудник $user снова открыл тикет.", "employee" => "Система",
                                       "date" => date("Y-m-d H:i:s"), "is_admin" => true, "is_system" => true];
                    break;
                }
            });
            flash("Тикет снова открыт.", "success");
            header("Location: $back"); exit;
        }
        if ($_POST["action"] === "pin_photo") {
            rt_tickets_update(function (&$tickets) use ($id) { foreach ($tickets as &$t) if ((string)$t["id"] === $id) $t["pinned_photo"] = empty($t["pinned_photo"]); });
            header("Location: $back"); exit;
        }
        // Забрать тикет у ИИ или вернуть ИИ
        if ($_POST["action"] === "ticket_ai") {
            $on = ($_POST["ai"] ?? "") === "1";
            rt_tickets_update(function (&$tickets) use ($id, $on, $user) {
                foreach ($tickets as &$t) {
                    if ((string)$t["id"] !== $id) continue;
                    $t["ai"] = $on;
                    if (!$on) unset($t["ai_pending"]);
                    $t["replies"][] = ["text" => $on ? "Сотрудник $user вернул тикет ИИ-помощнику Rai." : "Тикет взял сотрудник $user. ИИ отключён.",
                                       "employee" => "Система", "date" => date("Y-m-d H:i:s"), "is_admin" => true, "is_system" => true];
                    if ($on && ($t["status"] ?? "") === "Ждёт администратора") $t["status"] = "Ожидает ответа клиента";
                    break;
                }
            });
            flash($on ? "✨ Тикет снова ведёт ИИ: он ответит на следующее сообщение клиента." : "🛡 Тикет ваш: ИИ в нём больше не отвечает.", "success");
            header("Location: $back"); exit;
        }
        // Настройки ИИ поддержки
        if ($_POST["action"] === "save_support_ai") {
            $src = trim($_POST["support_ai_src"] ?? "");
            $search = trim($_POST["support_ai_search"] ?? "");
            foreach ([$src, $search] as $u) {
                if ($u !== "" && !preg_match('~^(https://|http://(127\.0\.0\.1|localhost)[:/])[^\s"\'<>]+$~i', $u)) { flash("Адреса должны начинаться с https://", "error"); header("Location: $back"); exit; }
            }
            $settings["support_ai_src"] = $src === "" ? "" : rtrim($src, "/") . "/";
            $settings["support_ai_search"] = $search;
            $settings["support_ai_enabled"] = isset($_POST["support_ai_enabled"]);
            $settings["rai_widget"] = isset($_POST["rai_widget"]);
            save_json("settings.json", $settings);
            flash($settings["support_ai_enabled"] ? "✨ ИИ поддержки включён." : "Настройки сохранены, ИИ выключен.", "success");
            header("Location: $back"); exit;
        }
    }

    /* --- БОТ TELEGRAM (АВТОНОМНАЯ AJAX СИСТЕМА, РОЗЫГРЫШИ И МУТЫ) --- */
    if ($tab === "bot") {
        
        // Массовая рассылка
        if ($_POST["action"] === "broadcast_tg") {
            $bot_token = $settings["bot_token"] ?? "";
            if (empty($bot_token)) { 
                if (isset($_POST['is_ajax'])) { echo "error"; exit; }
                echo "<script>alert('Нет токена!'); window.history.back();</script>"; exit; 
            }
            $bot_users = load_json("bot_users.json", []);
            $text = trim($_POST['broadcast_text']);
            $url = "https://api.telegram.org/bot" . $bot_token . "/sendMessage";
            
            foreach ($bot_users as $uid) {
                $data = ['chat_id' => $uid, 'text' => "📢 <b>Уведомление от RTeam:</b>\n\n" . $text, 'parse_mode' => 'HTML'];
                $ch = curl_init($url); curl_setopt($ch, CURLOPT_POST, 1); curl_setopt($ch, CURLOPT_POSTFIELDS, $data); curl_setopt($ch, CURLOPT_RETURNTRANSFER, true); curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false); curl_exec($ch); curl_close($ch);
            }
            if (isset($_POST['is_ajax'])) { echo "ok"; exit; }
            echo "<script>alert('Рассылка завершена!'); window.history.back();</script>"; exit;
        }

        // Ответ в тикет
        if ($_POST["action"] === "reply_tg_ticket") {
            $bot_token = $settings["bot_token"] ?? "";
            if (empty($bot_token)) { 
                if (isset($_POST['is_ajax'])) { echo "error"; exit; }
                echo "<script>alert('Сначала укажите токен бота!'); window.history.back();</script>"; exit; 
            }
            $tg_tickets = load_json("bot_tickets.json", []); 
            $reply_text = trim($_POST['reply_text']); 
            $user_id = $_POST['user_id']; 
            $ticket_id = $_POST['ticket_id'];
            
            $url = "https://api.telegram.org/bot" . $bot_token . "/sendMessage";
            $message = "👨‍💻 <b>Ответ от поддержки RTeam:</b>\n\n" . $reply_text;
            $data = ['chat_id' => $user_id, 'text' => $message, 'parse_mode' => 'HTML'];
            $ch = curl_init($url); curl_setopt($ch, CURLOPT_POST, 1); curl_setopt($ch, CURLOPT_POSTFIELDS, $data); curl_setopt($ch, CURLOPT_RETURNTRANSFER, true); curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false); curl_exec($ch); curl_close($ch);
            
            foreach ($tg_tickets as &$t) { if ((string)$t['id'] === (string)$ticket_id) { $t['status'] = 'answered'; break; } }
            save_json("bot_tickets.json", $tg_tickets); 
            
            if (isset($_POST['is_ajax'])) { echo "ok"; exit; }
            header("Location: admin.php?tab=bot"); exit;
        }

        // Закрыть тикет
        if ($_POST["action"] === "close_tg_ticket") {
            $tg_tickets = load_json("bot_tickets.json", []);
            foreach ($tg_tickets as &$t) { if ((string)$t['id'] === (string)$_POST['ticket_id']) { $t['status'] = 'closed'; break; } }
            save_json("bot_tickets.json", $tg_tickets); 
            
            if (isset($_POST['is_ajax'])) { echo "ok"; exit; }
            header("Location: admin.php?tab=bot"); exit;
        }

        // ВЫДАЧА И СНЯТИЕ МУТА В БОТЕ
        if ($_POST["action"] === "ban_bot_user") {
            $banned = load_json("bot_banned.json", []);
            if (!is_array($banned)) $banned = [];
            $uid = (string)$_POST["user_id"];
            if (!in_array($uid, $banned)) { $banned[] = $uid; save_json("bot_banned.json", $banned); }
            if (isset($_POST['is_ajax'])) { echo "ok"; exit; }
            header("Location: admin.php?tab=bot"); exit;
        }
        
        if ($_POST["action"] === "unban_bot_user") {
            $banned = load_json("bot_banned.json", []);
            if (!is_array($banned)) $banned = [];
            $uid = (string)$_POST["user_id"];
            $banned = array_values(array_filter($banned, fn($id) => (string)$id !== $uid));
            save_json("bot_banned.json", $banned);
            if (isset($_POST['is_ajax'])) { echo "ok"; exit; }
            header("Location: admin.php?tab=bot"); exit;
        }
        
        // --- УПРАВЛЕНИЕ РОЗЫГРЫШАМИ ---
        if ($_POST["action"] === "add_bot_gw") {
            $bot_gws = load_json("bot_giveaways.json", []);
            $bot_gws[] = [
                "id" => uniqid(),
                "title" => trim($_POST['title']),
                "description" => trim($_POST['description']),
                "winners_count" => (int)$_POST['winners_count'],
                "participants" => [],
                "winners" => [],
                "status" => "active"
            ];
            save_json("bot_giveaways.json", $bot_gws);
            if (isset($_POST['is_ajax'])) { echo "ok"; exit; }
            header("Location: admin.php?tab=bot"); exit;
        }
        
        if ($_POST["action"] === "del_bot_gw") {
            $bot_gws = load_json("bot_giveaways.json", []);
            $bot_gws = array_values(array_filter($bot_gws, fn($g) => $g['id'] !== $_POST['gw_id']));
            save_json("bot_giveaways.json", $bot_gws);
            if (isset($_POST['is_ajax'])) { echo "ok"; exit; }
            header("Location: admin.php?tab=bot"); exit;
        }
        
        if ($_POST["action"] === "roll_bot_gw") {
            $bot_gws = load_json("bot_giveaways.json", []);
            $bot_token = $settings["bot_token"] ?? "";
            
            foreach ($bot_gws as &$gw) {
                if ($gw['id'] === $_POST['gw_id'] && $gw['status'] === 'active') {
                    $gw['status'] = 'closed';
                    $participants = $gw['participants'];
                    $count = (int)$gw['winners_count'];
                    if ($count < 1) $count = 1;
                    
                    $winners = [];
                    if (!empty($participants)) {
                        shuffle($participants); // Перемешиваем случайным образом
                        $winners = array_slice($participants, 0, $count); // Берем X победителей
                    }
                    $gw['winners'] = $winners;
                    
                    // Рассылка с результатами
                    if (!empty($bot_token)) {
                        $win_text = empty($winners) ? "Участников не было 😔" : "🏆 <b>Победители (ID):</b>\n" . implode("\n", array_map(fn($w) => "👤 <a href='tg://user?id={$w}'>$w</a>", $winners));
                        $msg = "🎉 <b>РОЗЫГРЫШ ЗАВЕРШЁН!</b> 🎉\n\n<b>" . htmlspecialchars($gw['title']) . "</b>\n\n" . $win_text . "\n\nПоздравляем победителей! Спасибо всем за участие!";
                        
                        $bot_users = load_json("bot_users.json", []);
                        $url = "https://api.telegram.org/bot" . $bot_token . "/sendMessage";
                        foreach ($bot_users as $uid) {
                            $data = ['chat_id' => $uid, 'text' => $msg, 'parse_mode' => 'HTML'];
                            $ch = curl_init($url); curl_setopt($ch, CURLOPT_POST, 1); curl_setopt($ch, CURLOPT_POSTFIELDS, $data); curl_setopt($ch, CURLOPT_RETURNTRANSFER, true); curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false); curl_exec($ch); curl_close($ch);
                        }
                    }
                    break;
                }
            }
            save_json("bot_giveaways.json", $bot_gws);
            if (isset($_POST['is_ajax'])) { echo "ok"; exit; }
            header("Location: admin.php?tab=bot"); exit;
        }

        /* --- ВЕБХУК: установка одной кнопкой ---
           1) проверяем токен (getMe) и запоминаем имя бота для кабинета;
           2) стучимся на bot.php как Telegram и идём по редиректам (например,
              на www.) — так находим адрес, который отвечает без перенаправления;
           3) ставим вебхук с секретом: bot.php примет запросы только от Telegram;
           4) старые накопившиеся сообщения сбрасываем, чтобы бот не отвечал на них разом. */
        if ($_POST["action"] === "setup_webhook") {
            $report = [];
            $me_info = tg_api("getMe");
            if (empty($me_info["ok"])) {
                $desc = $me_info["description"] ?? "ошибка";
                flash("Telegram не принял токен: " . $desc . (($me_info["error_code"] ?? 0) == 401 ? ". Возьмите токен заново в @BotFather и сохраните его выше." : ""), "error");
                header("Location: admin.php?tab=bot"); exit;
            }
            $settings["bot_username"] = $me_info["result"]["username"] ?? ($settings["bot_username"] ?? "");
            $report[] = "✅ Токен верный, бот @" . $settings["bot_username"];

            if (empty($settings["bot_webhook_secret"])) $settings["bot_webhook_secret"] = bin2hex(random_bytes(16));
            $secret = $settings["bot_webhook_secret"];

            $url = trim($_POST["webhook_url"] ?? "") ?: default_webhook_url();
            if (stripos($url, "https://") !== 0) $url = "https://" . preg_replace('~^[a-z]+://~i', "", $url);

            $problem = null;
            for ($hop = 0; $hop < 4; $hop++) {
                [$code, $loc, $err] = probe_webhook($url, $secret);
                if ($err) { $report[] = "⚠️ Сервер не смог сам проверить адрес ($err) — ставлю вебхук как есть"; break; }
                if ($code >= 300 && $code < 400 && $loc) {
                    $report[] = "↪️ $url отвечает $code и перенаправляет на $loc";
                    if (preg_match('~/bot\.php(\?.*)?$~i', parse_url($loc, PHP_URL_PATH) . (parse_url($loc, PHP_URL_QUERY) ? "?" . parse_url($loc, PHP_URL_QUERY) : "")) && stripos($loc, "https://") === 0) {
                        $url = $loc; // тот же bot.php по другому адресу (например, www.) — используем его
                        continue;
                    }
                    $problem = "Сайт перенаправляет bot.php на $loc. Telegram по редиректам не ходит. Проверьте, что bot.php лежит рядом с admin.php, и что в .htaccess или настройках хостинга нет правила, которое перенаправляет запросы к bot.php.";
                    break;
                }
                if ($code === 200) { $report[] = "✅ $url отвечает 200 OK"; break; }
                $problem = "Адрес $url отвечает кодом $code" . ($code == 404 ? " — файла bot.php по этому адресу нет." : ($code >= 500 ? " — bot.php падает с ошибкой (проверьте, что рядом загружен _roles.php)." : "."));
                break;
            }
            save_json("settings.json", $settings);

            if ($problem) {
                $report[] = "❌ " . $problem;
                $_SESSION["webhook_report"] = $report;
                flash("Вебхук не установлен — подробности в блоке «Вебхук бота».", "error");
                header("Location: admin.php?tab=bot"); exit;
            }

            $res = tg_api("setWebhook", [
                "url" => $url,
                "secret_token" => $secret,
                "allowed_updates" => json_encode(["message", "callback_query"]),
                "drop_pending_updates" => "true",
                "max_connections" => 40,
            ]);
            if (!empty($res["ok"])) {
                $settings["bot_webhook_url"] = $url;
                save_json("settings.json", $settings);
                $report[] = "✅ Вебхук установлен: $url (старые накопившиеся сообщения сброшены)";
                $report[] = "👉 Напишите боту /start — он должен ответить. Потом нажмите «Проверить статус».";
                $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "bot", "msg" => "$user установил вебхук бота: $url"];
                save_json("logs.json", $logs);
                flash("Вебхук установлен. Напишите боту /start.", "success");
            } else {
                $report[] = "❌ Telegram не принял вебхук: " . ($res["description"] ?? "ошибка");
                flash("Telegram не принял вебхук: " . ($res["description"] ?? "ошибка"), "error");
            }
            $_SESSION["webhook_report"] = $report;
            header("Location: admin.php?tab=bot"); exit;
        }

        if ($_POST["action"] === "check_webhook") {
            $info = tg_api("getWebhookInfo");
            $report = [];
            if (empty($info["ok"])) {
                $report[] = "❌ " . ($info["description"] ?? "Не удалось получить статус") . (($info["error_code"] ?? 0) == 401 ? " — неверный токен." : "");
            } else {
                $r = $info["result"];
                $report[] = !empty($r["url"]) ? "🔗 Адрес: " . $r["url"] : "❌ Вебхук не установлен — нажмите «Установить вебхук автоматически»";
                $report[] = "📨 Ждут доставки: " . (int)($r["pending_update_count"] ?? 0);
                if (!empty($r["last_error_message"])) {
                    $fresh = time() - (int)($r["last_error_date"] ?? 0) < 600;
                    $report[] = ($fresh ? "❌ " : "ℹ️ ") . "Последняя ошибка (" . date("d.m.Y H:i", (int)$r["last_error_date"]) . "): " . $r["last_error_message"] . ($fresh ? "" : " — давно, сейчас может быть уже исправлено");
                } elseif (!empty($r["url"])) {
                    $report[] = "✅ Ошибок нет — Telegram доставляет сообщения боту";
                }
            }
            $_SESSION["webhook_report"] = $report;
            header("Location: admin.php?tab=bot"); exit;
        }

        if ($_POST["action"] === "delete_webhook") {
            $res = tg_api("deleteWebhook");
            flash(!empty($res["ok"]) ? "Вебхук отключён — бот перестал получать сообщения." : "Не удалось: " . ($res["description"] ?? "ошибка"), !empty($res["ok"]) ? "success" : "error");
            header("Location: admin.php?tab=bot"); exit;
        }

        if ($_POST["action"] === "save_bot_token") {
            $settings["bot_token"] = trim($_POST["bot_token"]); save_json("settings.json", $settings); 
            if (isset($_POST['is_ajax'])) { echo "ok"; exit; }
            header("Location: admin.php?tab=bot"); exit;
        }
    }

    /* --- ПРОЧЕЕ --- */
    if ($_POST["action"] === "save_recruit") { $settings["recruit_open"] = isset($_POST["recruit_open"]); save_json("settings.json", $settings); $questions = ["team" => array_values(array_filter(array_map("trim", explode("\n", $_POST["team_questions"])))), "admin" => array_values(array_filter(array_map("trim", explode("\n", $_POST["admin_questions"]))))]; save_json("questions.json", $questions); header("Location: admin.php?tab=recruit"); exit; }
    if ($_POST["action"] === "add_leak") { $leaks[] = ["id" => time(), "title" => trim($_POST["title"]), "content" => trim($_POST["content"]), "time" => date("Y-m-d H:i:s"), "hidden" => false]; save_json("leaks.json", $leaks); header("Location: admin.php?tab=leaks"); exit; }
    if ($_POST["action"] === "edit_leak") { foreach ($leaks as &$l) if ((string)$l["id"] === (string)$_POST["id"]) { $l["title"] = $_POST["title"]; $l["content"] = $_POST["content"]; break; } unset($l); save_json("leaks.json", $leaks); header("Location: admin.php?tab=leaks"); exit; }
    if ($_POST["action"] === "toggle_leak") { foreach ($leaks as &$l) if ((string)$l["id"] === (string)$_POST["id"]) $l["hidden"] = !$l["hidden"]; unset($l); save_json("leaks.json", $leaks); header("Location: admin.php?tab=leaks"); exit; }
    if ($_POST["action"] === "del_leak") { $leaks = array_filter($leaks, fn($l) => (string)$l["id"] !== (string)$_POST["id"]); save_json("leaks.json", array_values($leaks)); header("Location: admin.php?tab=leaks"); exit; }
    if ($_POST["action"] === "add_post") { $blog[] = ["id" => time(), "title" => trim($_POST["title"]), "content" => trim($_POST["content"]), "date" => date("Y-m-d H:i:s"), "hidden" => false]; save_json("blog.json", $blog); header("Location: admin.php?tab=blog"); exit; }
    if ($_POST["action"] === "edit_post") { foreach ($blog as &$p) if ((string)$p["id"] === (string)$_POST["id"]) { $p["title"] = $_POST["title"]; $p["content"] = $_POST["content"]; break; } unset($p); save_json("blog.json", $blog); header("Location: admin.php?tab=blog"); exit; }
    if ($_POST["action"] === "toggle_post") { foreach ($blog as &$p) if ((string)$p["id"] === (string)$_POST["id"]) $p["hidden"] = !$p["hidden"]; unset($p); save_json("blog.json", $blog); header("Location: admin.php?tab=blog"); exit; }
    if ($_POST["action"] === "del_post") { $blog = array_filter($blog, fn($p) => (string)$p["id"] !== (string)$_POST["id"]); save_json("blog.json", array_values($blog)); header("Location: admin.php?tab=blog"); exit; }
    /* --- DISCORD: вход через Discord и бот --- */
    if ($_POST["action"] === "save_discord") {
        $fields = ["client_id" => '/^\d{15,22}$/', "bot_url" => '~^https?://[^\s"\'<>]+$~i', "invite" => '~^https://[^\s"\'<>]+$~i', "redirect" => '~^https://[^\s"\'<>]+$~i'];
        foreach ($fields as $k => $re) {
            $v = trim((string)($_POST["discord_" . $k] ?? ""));
            if ($v !== "" && !preg_match($re, $v)) { flash("Проверьте поле «" . $k . "»: " . ($k === "client_id" ? "только цифры" : "адрес должен начинаться с https://"), "error"); header("Location: admin.php?tab=discord"); exit; }
            $settings["discord_" . $k] = $k === "bot_url" ? rtrim($v, "/") : $v;
        }
        // Секреты меняем, только если вписали новые (пустое поле — оставить как есть)
        foreach (["client_secret", "api_key"] as $k) {
            $v = trim((string)($_POST["discord_" . $k] ?? ""));
            if ($v !== "") $settings["discord_" . $k] = $v;
            if (isset($_POST["clear_" . $k])) unset($settings["discord_" . $k]);
        }
        save_json("settings.json", $settings);
        $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "settings", "msg" => "$user изменил настройки Discord."];
        save_json("logs.json", $logs);
        flash("Настройки Discord сохранены.", "success");
        header("Location: admin.php?tab=discord"); exit;
    }
    if ($_POST["action"] === "restart_discord") {
        $r = rt_discord_bot_restart($settings);
        if (isset($r["report"])) {
            $rep = ["🔄 Бот перезапущен: config.json перечитан, начальные сообщения написаны заново."];
            foreach ($r["report"] as $chk) $rep[] = (!empty($chk["ok"]) ? "✅ " : "❌ ") . ($chk["text"] ?? "");
        } else {
            $rep = array_merge(["❌ Бот не ответил: " . rt_discord_error_text($r) . "."], rt_discord_probe($settings));
        }
        $_SESSION["discord_report"] = $rep;
        $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "settings", "msg" => "$user перезапустил Discord-бота."];
        save_json("logs.json", $logs);
        header("Location: admin.php?tab=discord"); exit;
    }
    if ($_POST["action"] === "check_discord") {
        $c = rt_discord_conf($settings);
        $rep = [];
        $rep[] = ($c["client_id"] !== "" && $c["client_secret"] !== "") ? "✅ Вход через Discord настроен (Client ID и Secret есть)" : "❌ Вход через Discord: нет Client ID или Client Secret";
        $rep[] = "ℹ️ В Discord Developer Portal → OAuth2 → Redirects должен быть адрес: " . rt_discord_redirect_uri($c);
        if ($c["bot_url"] === "" || $c["api_key"] === "") {
            $rep[] = "❌ Бот: не указан адрес бота или ключ (поля ниже или discord_config.php)";
        } else {
            $probe = rt_discord_probe($settings);
            $rep = array_merge($rep, $probe);
            $st = rt_discord_bot_status($settings);
            if (!empty($st["ok"])) {
                $ver = (string)($st["version"] ?? "");
                $rep[] = $ver !== "" && strcmp($ver, RT_DISCORD_BOT_VERSION) >= 0
                    ? "✅ Версия бота: $ver (свежая)"
                    : "❌ На сервере старая версия бота" . ($ver !== "" ? " ($ver)" : "") . ". Загрузите новые index.js, features.js, quiz.json и config.json в папку бота (туда, где лежит secret.json; в Plesk → Node.js это Application root), затем нажмите «Restart App».";
                if (empty($st["features"])) $rep[] = "❌ Бот не нашёл features.js — загрузите features.js и quiz.json рядом с index.js и нажмите «Restart App» (без них нет розыгрышей, опросов, игр и новых разделов /админ).";
                $rep[] = ($st["online"] ?? false) ? "✅ Бот " . ($st["bot"] ?? "") . " в сети" . (!empty($st["guild"]) ? ", сервер «" . $st["guild"] . "»" : "") : "❌ Бот не подключён к Discord" . (!empty($st["last_error"]) ? ": " . $st["last_error"] : "");
                foreach (($st["checks"] ?? []) as $chk) $rep[] = (!empty($chk["ok"]) ? "✅ " : "❌ ") . $chk["text"];
                if (!empty($st["invite_url"])) $rep[] = "ℹ️ Пригласить бота на сервер: " . $st["invite_url"];
            } elseif (($st["error"] ?? "") === "forbidden" || ($st["error"] ?? "") === "api_key_not_set") {
                $rep[] = "❌ " . rt_discord_error_text($st);
            }
            // Файлы бота не должны открываться из браузера (Document root в Plesk — папка public)
            foreach (["secret.json", "data.json"] as $f) {
                [$fc, $fj] = rt_discord_http("GET", $c["bot_url"] . "/" . $f, null, [], 10, true);
                if ($fc === 200 && is_array($fj)) $rep[] = "❌ ОПАСНО: " . $c["bot_url"] . "/$f открывается всем! В Plesk поставьте Document root = папка public внутри папки бота.";
            }
        }
        $_SESSION["discord_report"] = $rep;
        header("Location: admin.php?tab=discord"); exit;
    }

    /* --- ЛЮДИ И РОЛИ ---
       Кто кому что может менять, решает _roles.php: роли выдаёт тот, у кого
       есть право «roles.manage», и только тем, кто младше его по уровню
       (руководитель — кому угодно). Себе роль поменять нельзя, владельцев
       сайта трогают только владельцы. */
    $act = $_POST["action"] ?? "";
    $back_tab = in_array($_POST["back"] ?? "", ["team", "users"], true) ? $_POST["back"] : ($tab === "users" ? "users" : "team");

    if ($act === "add_user") {
        $login = trim($_POST["login"] ?? ""); $pass = (string)($_POST["password"] ?? ""); $newRole = $_POST["role"] ?? "Пользователь";
        if ($login === "" || $pass === "") flash("Укажите логин и пароль.", "error");
        elseif (isset($users[$login])) flash("Пользователь «{$login}» уже есть.", "error");
        else {
            if (!rt_can_assign_role($user, $role, $newRole) && $newRole !== "Пользователь") { flash("Роль «{$newRole}» вы выдать не можете — аккаунт создан как «Пользователь».", "error"); $newRole = "Пользователь"; }
            $users[$login] = ["password" => store_password($pass, $users), "role" => $newRole];
            if ($newRole === RT_TRAINEE_ROLE) $users[$login]["direction"] = isset(rt_directions()[$_POST["direction"] ?? ""]) ? $_POST["direction"] : "Кодер";
            if ($newRole !== "Пользователь") { $users[$login]["role_by"] = $user; $users[$login]["role_at"] = date("Y-m-d H:i:s"); }
            save_json("users.json", $users);
            $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "users", "msg" => "$user создал аккаунт {$login} с ролью «" . rt_role_label($newRole, $users[$login]["direction"] ?? "") . "»."];
            save_json("logs.json", $logs);
            flash("Аккаунт «{$login}» создан.", "success");
        }
        header("Location: admin.php?tab=users"); exit;
    }

    if (in_array($act, ["set_role", "add_to_team", "change_team_role", "remove_from_team"], true)) {
        $login = $_POST["login"] ?? "";
        $newRole = $act === "remove_from_team" ? "Пользователь" : ($_POST["role"] ?? "");
        $direction = trim($_POST["direction"] ?? "");
        if (!isset($users[$login])) { flash("Пользователь не найден.", "error"); header("Location: admin.php?tab=$back_tab"); exit; }
        $oldRole = $users[$login]["role"] ?? "Пользователь";
        $oldDir  = $users[$login]["direction"] ?? "";
        if (!rt_can_edit_user($user, $role, $login, $oldRole)) {
            flash($login === $user ? "Свою роль поменять нельзя — попросите старшего." : "Нельзя менять роль «{$login}»: этот человек не младше вас по должности.", "error");
        } elseif (!rt_can_assign_role($user, $role, $newRole)) {
            flash("Роль «{$newRole}» вы выдать не можете.", "error");
        } else {
            $users[$login]["role"] = $newRole;
            if ($newRole === RT_TRAINEE_ROLE) {
                $users[$login]["direction"] = isset(rt_directions()[$direction]) ? $direction : ($oldDir !== "" ? $oldDir : "Кодер");
            } else {
                unset($users[$login]["direction"]);
            }
            $users[$login]["role_by"] = $user;
            $users[$login]["role_at"] = date("Y-m-d H:i:s");
            save_json("users.json", $users);
            $was = rt_role_label($oldRole, $oldDir); $now = rt_role_label($newRole, $users[$login]["direction"] ?? "");
            $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "roles", "msg" => "$user сменил роль {$login}: «{$was}» → «{$now}»."];
            save_json("logs.json", $logs);
            flash($newRole === "Пользователь" ? "«{$login}» больше не в команде." : "{$login}: теперь «{$now}».", "success");
        }
        header("Location: admin.php?tab=$back_tab"); exit;
    }

    if ($act === "set_pass") {
        $login = $_POST["login"] ?? ""; $pass = (string)($_POST["password"] ?? "");
        if (!isset($users[$login]) || $pass === "") flash("Укажите новый пароль.", "error");
        elseif (!rt_can_edit_user($user, $role, $login, $users[$login]["role"] ?? "Пользователь")) flash("Нельзя менять пароль «{$login}»: этот человек не младше вас по должности.", "error");
        else {
            $users[$login]["password"] = store_password($pass, $users);
            save_json("users.json", $users);
            $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "users", "msg" => "$user сменил пароль пользователю {$login}."];
            save_json("logs.json", $logs);
            flash("Пароль для «{$login}» изменён.", "success");
        }
        header("Location: admin.php?tab=users"); exit;
    }

    // Отвязать Telegram — если человек потерял доступ к нему и не может получить код входа
    if ($act === "unlink_tg") {
        $login = $_POST["login"] ?? "";
        if (!isset($users[$login])) flash("Пользователь не найден.", "error");
        elseif (!rt_can_edit_user($user, $role, $login, $users[$login]["role"] ?? "Пользователь")) flash("Нельзя отвязать Telegram у «{$login}»: этот человек не младше вас по должности.", "error");
        else {
            unset($users[$login]["tg_id"], $users[$login]["tg_username"]);
            save_json("users.json", $users);
            $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "users", "msg" => "$user отвязал Telegram у {$login} (вход снова по паролю)."];
            save_json("logs.json", $logs);
            flash("Telegram у «{$login}» отвязан — вход теперь по паролю.", "success");
        }
        header("Location: admin.php?tab=users"); exit;
    }

    // Отвязать Discord — если человек потерял к нему доступ
    if ($act === "unlink_ds") {
        $login = $_POST["login"] ?? "";
        if (!isset($users[$login])) flash("Пользователь не найден.", "error");
        elseif (!rt_can_edit_user($user, $role, $login, $users[$login]["role"] ?? "Пользователь")) flash("Нельзя отвязать Discord у «{$login}»: этот человек не младше вас по должности.", "error");
        else {
            unset($users[$login]["discord_id"], $users[$login]["discord_username"], $users[$login]["discord_name"], $users[$login]["discord_avatar"]);
            if (($users[$login]["2fa_via"] ?? "") === "ds") unset($users[$login]["2fa_via"]);
            save_json("users.json", $users);
            $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "users", "msg" => "$user отвязал Discord у {$login}."];
            save_json("logs.json", $logs);
            flash("Discord у «{$login}» отвязан." . (empty($users[$login]["password"]) && empty($users[$login]["google_id"]) ? " У аккаунта нет пароля — задайте его, иначе человек не сможет войти." : ""), "success");
        }
        header("Location: admin.php?tab=users"); exit;
    }

    // Написать человеку в ЛС Discord от бота
    if ($act === "discord_dm") {
        $login = $_POST["login"] ?? "";
        $text = trim((string)($_POST["text"] ?? ""));
        if (!isset($users[$login]) || empty($users[$login]["discord_id"])) flash("У «{$login}» не привязан Discord.", "error");
        elseif ($text === "") flash("Напишите текст сообщения.", "error");
        else {
            $r = rt_discord_dm($users[$login]["discord_id"], mb_substr($text, 0, 3000), "✉️ Сообщение от администрации RTeam", null, $settings);
            if (!empty($r["ok"])) {
                $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "users", "msg" => "$user написал {$login} в Discord."];
                save_json("logs.json", $logs);
                flash("Сообщение отправлено «{$login}» в Discord.", "success");
            } else {
                flash("Не отправлено: " . rt_discord_error_text($r) . ".", "error");
            }
        }
        header("Location: admin.php?tab=users"); exit;
    }

    if ($act === "del_user") {
        $login = $_POST["login"] ?? "";
        if (!isset($users[$login])) flash("Пользователь не найден.", "error");
        elseif (rt_is_owner($login)) flash("Аккаунт владельца сайта удалить нельзя.", "error");
        elseif (!rt_can_edit_user($user, $role, $login, $users[$login]["role"] ?? "Пользователь")) flash("Нельзя удалить «{$login}»: этот человек не младше вас по должности.", "error");
        else {
            unset($users[$login]);
            save_json("users.json", $users);
            $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "users", "msg" => "$user удалил аккаунт {$login}."];
            save_json("logs.json", $logs);
            flash("Аккаунт «{$login}» удалён.", "success");
        }
        header("Location: admin.php?tab=users"); exit;
    }

    /* --- ПРАВА РОЛЕЙ (только руководитель / владельцы) --- */
    if ($act === "save_perms") {
        rt_save_role_perms($_POST["perm"] ?? []);
        $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "roles", "msg" => "$user изменил права ролей."];
        save_json("logs.json", $logs);
        flash("Права ролей сохранены.", "success");
        header("Location: admin.php?tab=perms"); exit;
    }
    if ($act === "reset_perms") {
        rt_reset_role_perms();
        $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "roles", "msg" => "$user сбросил права ролей к стандартным."];
        save_json("logs.json", $logs);
        flash("Права ролей сброшены к стандартным.", "success");
        header("Location: admin.php?tab=perms"); exit;
    }

    if ($_POST["action"] === "save_settings") { $settings["site_name"] = trim($_POST["site_name"]); $settings["accent"] = trim($_POST["accent"]); $settings["neon"] = isset($_POST["neon"]); $settings["animations"] = isset($_POST["animations"]); save_json("settings.json", $settings); header("Location: admin.php?tab=settings"); exit; }
    if ($_POST["action"] === "save_theme") {
        $cat = rteam_theme_catalog();
        $chosen = $_POST["theme_active"] ?? "";
        if (!isset($cat[$chosen])) $chosen = array_key_first($cat);
        $theme_settings["active"]  = $chosen;
        $theme_settings["enabled"] = isset($_POST["theme_enabled"]);
        $theme_settings["text"]    = trim($_POST["theme_text"] ?? "");
        save_json("theme.json", $theme_settings);
        header("Location: admin.php?tab=themes"); exit;
    }
    if ($_POST["action"] === "squid_set_paused") {
        $squid_game["paused"] = isset($_POST["squid_paused"]);
        save_json("squid_game.json", $squid_game);
        $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "squid_game", "msg" => "$user " . ($squid_game["paused"] ? "остановил" : "возобновил") . " «Игру в кальмара»."];
        save_json("logs.json", $logs);
        header("Location: admin.php?tab=themes"); exit;
    }
    if ($_POST["action"] === "squid_reset_user") {
        $sq_login = $_POST["login"] ?? "";
        if (isset($squid_game["progress"][$sq_login])) {
            unset($squid_game["progress"][$sq_login]);
            save_json("squid_game.json", $squid_game);
        }
        header("Location: admin.php?tab=themes"); exit;
    }

    /* --- ЗОЛОТОЙ БИЛЕТ RTEAM --- */
    if ($_POST["action"] === "grant_golden") {
        $login = trim($_POST["login"] ?? "");
        if ($login !== "" && isset($users[$login])) {
            $users[$login]["golden"] = true;
            $users[$login]["golden_title"] = trim($_POST["golden_title"] ?? "") !== "" ? trim($_POST["golden_title"]) : "Золотой билет RTeam";
            $users[$login]["golden_link"] = trim($_POST["golden_link"] ?? "");
            $users[$login]["golden_staff"] = trim($_POST["golden_staff"] ?? "");
            save_json("users.json", $users);
            if (!isset($gold_chats[$login])) $gold_chats[$login] = ["staff" => $users[$login]["golden_staff"], "messages" => []];
            else $gold_chats[$login]["staff"] = $users[$login]["golden_staff"];
            save_json("gold_chats.json", $gold_chats);
            $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "golden_ticket", "msg" => "$user выдал «Золотой билет RTeam» пользователю $login (сотрудник: {$users[$login]["golden_staff"]})."];
            save_json("logs.json", $logs);
        }
        header("Location: admin.php?tab=gold"); exit;
    }
    if ($_POST["action"] === "revoke_golden" && isset($users[$_POST["login"]])) {
        $login = $_POST["login"];
        $users[$login]["golden"] = false;
        save_json("users.json", $users);
        $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "golden_ticket", "msg" => "$user отозвал «Золотой билет RTeam» у пользователя $login."];
        save_json("logs.json", $logs);
        header("Location: admin.php?tab=gold"); exit;
    }
    if ($_POST["action"] === "gold_reply") {
        $login = trim($_POST["login"] ?? "");
        $text = trim($_POST["reply_text"] ?? "");
        if ($login !== "" && $text !== "" && isset($users[$login]) && !empty($users[$login]["golden"])) {
            if (!isset($gold_chats[$login])) $gold_chats[$login] = ["staff" => $users[$login]["golden_staff"] ?? "", "messages" => []];
            $gold_chats[$login]["messages"][] = ["id" => time() . rand(100, 999), "from" => "staff", "author" => $user, "role" => $role, "text" => $text, "time" => date("Y-m-d H:i:s")];
            save_json("gold_chats.json", $gold_chats);
        }
        if (isset($_POST['is_ajax'])) { echo "ok"; exit; }
        header("Location: admin.php?tab=gold&thread=".urlencode($login)); exit;
    }
    if ($_POST["action"] === "add_gold_service") {
        $name = trim($_POST["service_name"] ?? "");
        $link = trim($_POST["service_link"] ?? "");
        if ($name !== "") {
            $gold_services[] = ["id" => time() . rand(100, 999), "name" => $name, "link" => $link];
            save_json("gold_services.json", $gold_services);
            $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "golden_ticket", "msg" => "$user добавил платную услугу для держателей золотого билета: «$name»."];
            save_json("logs.json", $logs);
        }
        header("Location: admin.php?tab=gold"); exit;
    }
    if ($_POST["action"] === "edit_gold_service") {
        $sid = $_POST["service_id"] ?? "";
        foreach ($gold_services as &$svc) {
            if ($svc["id"] === $sid) {
                $svc["name"] = trim($_POST["service_name"] ?? $svc["name"]);
                $svc["link"] = trim($_POST["service_link"] ?? $svc["link"]);
                break;
            }
        }
        unset($svc);
        save_json("gold_services.json", $gold_services);
        $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "golden_ticket", "msg" => "$user изменил платную услугу для держателей золотого билета (id $sid)."];
        save_json("logs.json", $logs);
        header("Location: admin.php?tab=gold"); exit;
    }
    if ($_POST["action"] === "delete_gold_service") {
        $sid = $_POST["service_id"] ?? "";
        $gold_services = array_values(array_filter($gold_services, function($s) use ($sid) { return $s["id"] !== $sid; }));
        save_json("gold_services.json", $gold_services);
        $logs[] = ["time" => date("Y-m-d H:i:s"), "type" => "golden_ticket", "msg" => "$user удалил платную услугу для держателей золотого билета (id $sid)."];
        save_json("logs.json", $logs);
        header("Location: admin.php?tab=gold"); exit;
    }
}

$stats = [ "apps_total" => count($applications), "users_total" => count($users), "chat_total" => count($chat_data["messages"]), "files_total" => count($files_data), "goals_total" => count($goals), "fines_total" => count($fines), "bans_total" => count($bans), "msgs_total" => count($messages), "blog_total" => count($blog), "projects_total" => count($projects_data), "logs_total" => count($logs) ];

/* СЧЁТЧИКИ ДЛЯ МЕНЮ И ГЛАВНОЙ */
$count = [
    "apps"      => count(array_filter(load_json("applications.json", []), fn($a) => ($a["status"] ?? "new") === "new")),
    "directors" => count(array_filter($director_requests, fn($r) => ($r["status"] ?? "pending") === "pending")),
    "support"   => count(array_filter($tickets, fn($t) => ticket_needs_staff($t, $settings))),
    "messages"  => count(array_filter($messages, fn($m) => empty($m["reply"]))),
    "bot"       => count(array_filter((array)load_json("bot_tickets.json", []), fn($t) => ($t["status"] ?? "") === "new")),
] + so_counts();
$staff_list = [];
foreach ($users as $l => $u) if (is_array($u) && rt_is_staff($u["role"] ?? "Пользователь", $l)) $staff_list[$l] = $u;
$active_bans = count(array_filter($bans, fn($b) => (int)($b["expires"] ?? 0) === 0 || (int)$b["expires"] > time()));

// Мои цели: поставленные всей команде, моему отделу или лично мне
$goal_classes = ["dev" => "class:coder", "tester" => "class:tester", "admin" => "class:admin"];
$my_goal_class = $goal_classes[$my_dept ?? ""] ?? null;
$my_goals = array_values(array_filter($goals, function($g) use ($user, $my_goal_class) {
    $a = $g["assigned_to"] ?? "all";
    return $a === "all" || $a === "user:" . $user || ($my_goal_class !== null && $a === $my_goal_class);
}));
usort($my_goals, fn($a, $b) => strcmp($a["deadline"] ?? "", $b["deadline"] ?? ""));
$my_unpaid_fines = array_values(array_filter($fines, fn($f) => ($f["user"] ?? "") === $user && empty($f["paid"])));

$accent_css = preg_match('/^#[0-9a-fA-F]{3,8}$/', $settings["accent"] ?? "") ? $settings["accent"] : "#ff2a2a";
$my_role_info = rt_role_info($role) ?? ["icon" => "❔", "color" => "#718096", "desc" => ""];

function render_chat_message($m, $is_pinned, $user) {
    global $chat_data;
    $mid = htmlspecialchars($m["id"]);
    $html = '<div id="msg-' . $mid . '" class="chat-msg' . ($m["user"] === $user ? ' mine' : '') . ($is_pinned ? ' highlight-pinned' : '') . '">';
    $html .= '<div class="chat-header"><div><span class="chat-avatar">' . htmlspecialchars(mb_strtoupper(mb_substr($m["user"], 0, 1))) . '</span><span class="chat-author">' . htmlspecialchars($m["user"]) . '</span> <span class="chat-role">' . htmlspecialchars($m["role"]) . '</span></div><span class="chat-time">' . htmlspecialchars($m["time"]) . '</span></div>';
    if (!empty($m["text"])) $html .= '<div class="chat-text">' . nl2br(htmlspecialchars($m["text"])) . '</div>';
    if (can("bans.manage") && $m["user"] !== $user) $html .= '<div class="meta" style="margin-top:6px;">IP: ' . ip_tag(author_ip($m["ip"] ?? null, $m["user"]), "Чат команды: " . $m["user"]) . '</div>';
    if (!empty($m["photo"])) $html .= '<div class="chat-photo"><img src="' . htmlspecialchars($m["photo"]) . '" alt="photo"></div>';
    if (!empty($m["poll"])) {
        $html .= '<div class="chat-poll"><h4>📊 ' . htmlspecialchars($m["poll"]["question"]) . '</h4>';
        $total_votes = 0; foreach ($m["poll"]["votes"] as $opt => $voters) $total_votes += count($voters);
        foreach ($m["poll"]["votes"] as $opt => $voters) {
            $count = count($voters); $percent = $total_votes > 0 ? round(($count / $total_votes) * 100) : 0; $has_voted = in_array($user, $voters);
            $html .= '<div class="poll-option"><div>' . htmlspecialchars($opt) . ' <span style="font-size:11px;color:#777;">(' . $count . ' голосов, ' . $percent . '%)</span></div>';
            $html .= '<form method="POST" style="margin:0;"><input type="hidden" name="action" value="vote_poll"><input type="hidden" name="msg_id" value="' . $mid . '"><input type="hidden" name="option" value="' . htmlspecialchars($opt) . '"><button class="btn sm ' . ($has_voted ? 'ok' : 'gray') . '" type="submit" style="margin-top:0;">' . ($has_voted ? '✓ Ваш голос' : 'Голосовать') . '</button></form></div>';
            $html .= '<div class="poll-bar-container"><div class="poll-bar" style="width:' . $percent . '%"></div></div>';
        } $html .= '</div>';
    }
    $actions = '';
    if (can("chat.pin")) {
        if ($chat_data["pinned_id"] === $m["id"]) $actions .= '<form method="POST" style="margin:0;"><input type="hidden" name="action" value="unpin_msg"><button class="btn sm gray" type="submit">Открепить</button></form>';
        else $actions .= '<form method="POST" style="margin:0;"><input type="hidden" name="action" value="pin_msg"><input type="hidden" name="id" value="' . $mid . '"><button class="btn sm ghost" type="submit">📌 Закрепить</button></form>';
    }
    if ($m["user"] === $user || can("chat.moderate")) $actions .= '<form method="POST" style="margin:0;" onsubmit="return confirm(\'Удалить сообщение?\');"><input type="hidden" name="action" value="del_chat_msg"><input type="hidden" name="id" value="' . $mid . '"><button class="btn sm ghost danger" type="submit">Удалить</button></form>';
    if ($actions !== '') $html .= '<div class="chat-actions">' . $actions . '</div>';
    $html .= '</div>'; return $html;
}
?>
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="icon" href="Favicon.Jpeg" type="image/jpeg">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&family=JetBrains+Mono:wght@500&display=swap" rel="stylesheet">
<script>try { if (localStorage.getItem('rtSbMini') === '1') document.documentElement.classList.add('sb-mini'); } catch (e) {}</script>
<title><?=htmlspecialchars($TABS[$tab][0] ?? "Админ")?> — Админ‑панель Rteam</title>
<style>
:root {
    --accent: <?=$accent_css?>;
    --bg: #07070b; --panel: #101017; --panel-2: #15151e; --panel-3: #1b1b26;
    --line: rgba(255,255,255,.07); --line-2: rgba(255,255,255,.12);
    --text: #ececf2; --soft: #b4b4c3; --muted: #747487;
    --ok: #22c55e; --warn: #f59e0b; --info: #3b82f6; --danger: #ef4444;
    --radius: 14px; --sidebar: 268px;
    --shadow: 0 10px 30px rgba(0,0,0,.35);
}
* { box-sizing: border-box; }
html { scroll-behavior: smooth; }
body { margin: 0; font-family: Inter, "Segoe UI", system-ui, -apple-system, Arial, sans-serif; color: var(--text); font-size: 14px; line-height: 1.5;
    background: var(--bg);
    background: radial-gradient(1200px 600px at 110% -10%, color-mix(in srgb, var(--accent) 10%, transparent), transparent 60%),
                radial-gradient(900px 500px at -20% 110%, rgba(80,90,255,.06), transparent 60%), var(--bg);
    background-attachment: fixed; min-height: 100vh; }
a { color: color-mix(in srgb, var(--accent) 70%, #fff); text-decoration: none; }
a:hover { text-decoration: underline; }
h1, h2, h3, h4 { line-height: 1.25; }
h3 { font-size: 16px; font-weight: 650; margin: 22px 0 10px; color: #fff; }
h4 { color: var(--soft); }
code { background: rgba(255,255,255,.06); padding: 1px 6px; border-radius: 6px; font-size: 12px; }
::selection { background: color-mix(in srgb, var(--accent) 45%, transparent); }
::-webkit-scrollbar { width: 10px; height: 10px; }
::-webkit-scrollbar-thumb { background: #262633; border-radius: 10px; border: 2px solid transparent; background-clip: padding-box; }

/* ===== КАРКАС: боковое меню + контент ===== */
.layout { display: flex; min-height: 100vh; }
.sidebar { position: fixed; inset: 0 auto 0 0; width: var(--sidebar); background: linear-gradient(180deg, #0d0d14, #09090e); border-right: 1px solid var(--line);
    display: flex; flex-direction: column; z-index: 60; transition: transform .25s ease; }
.sb-brand { display: flex; align-items: center; gap: 12px; padding: 20px 20px 14px; }
.sb-logo { width: 38px; height: 38px; border-radius: 11px; display: grid; place-items: center; font-weight: 800; font-size: 18px; color: #fff;
    background: linear-gradient(135deg, var(--accent), color-mix(in srgb, var(--accent) 50%, #6b0000)); box-shadow: 0 6px 18px color-mix(in srgb, var(--accent) 40%, transparent); }
.sb-title { font-weight: 800; letter-spacing: .08em; font-size: 15px; }
.sb-title small { display: block; font-weight: 500; letter-spacing: 0; color: var(--muted); font-size: 12px; }
.sb-user { margin: 4px 14px 10px; padding: 12px; border-radius: 12px; background: var(--panel); border: 1px solid var(--line); display: flex; gap: 10px; align-items: center; }
.avatar { width: 38px; height: 38px; border-radius: 50%; display: grid; place-items: center; font-weight: 700; color: #fff; flex: none;
    background: var(--rc, var(--accent)); background: color-mix(in srgb, var(--rc, var(--accent)) 75%, #000); box-shadow: 0 0 0 2px color-mix(in srgb, var(--rc, var(--accent)) 35%, transparent); }
.avatar.sm { width: 30px; height: 30px; font-size: 13px; }
.sb-user-name { font-weight: 650; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.sb-user .role-badge { margin-top: 3px; white-space: normal; max-width: 100%; }
.sb-nav { flex: 1; overflow-y: auto; padding: 6px 10px 16px; }
.sb-group { margin-top: 12px; }
.sb-group-title { font-size: 11px; text-transform: uppercase; letter-spacing: .1em; color: var(--muted); padding: 6px 12px; }
.sb-link { display: flex; align-items: center; gap: 11px; padding: 8px 12px; border-radius: 10px; color: var(--soft); font-weight: 500; position: relative; transition: background .15s, color .15s; }
.sb-link:hover { background: rgba(255,255,255,.04); color: #fff; text-decoration: none; }
.sb-link .ico { width: 22px; text-align: center; font-size: 16px; }
.sb-link.active { background: color-mix(in srgb, var(--accent) 14%, transparent); color: #fff; }
.sb-link.active::before { content: ""; position: absolute; left: -10px; top: 8px; bottom: 8px; width: 3px; border-radius: 0 3px 3px 0; background: var(--accent); }
.sb-count { margin-left: auto; min-width: 20px; padding: 1px 7px; border-radius: 999px; background: var(--accent); color: #fff; font-size: 11px; font-weight: 700; text-align: center; }
.sb-foot { padding: 12px 14px 16px; border-top: 1px solid var(--line); display: flex; gap: 8px; }
.sb-foot a { flex: 1; text-align: center; }
.sb-backdrop { display: none; }

.main { flex: 1; margin-left: var(--sidebar); min-width: 0; }
.topbar { position: sticky; top: 0; z-index: 40; display: flex; align-items: center; gap: 12px; padding: 14px 28px; background: rgba(7,7,11,.72);
    backdrop-filter: blur(14px); -webkit-backdrop-filter: blur(14px); border-bottom: 1px solid var(--line); }
.topbar h1 { margin: 0; font-size: 20px; font-weight: 700; display: flex; align-items: center; gap: 10px; }
.topbar .tb-ico { font-size: 20px; }
.topbar .tb-right { margin-left: auto; display: flex; align-items: center; gap: 10px; }
.burger { display: none; background: var(--panel); border: 1px solid var(--line-2); color: #fff; width: 40px; height: 40px; border-radius: 10px; font-size: 18px; cursor: pointer; }
.content { padding: 24px 28px 60px; max-width: 1240px; }

/* ===== КАРТОЧКИ, ФОРМЫ, КНОПКИ ===== */
.card { background: linear-gradient(180deg, var(--panel), #0e0e15); border: 1px solid var(--line); padding: 18px; border-radius: var(--radius); margin-top: 14px; box-shadow: 0 1px 0 rgba(255,255,255,.02) inset; }
.card h3 { margin: 0 0 10px; color: #fff; }
.card > h3:first-child { display: flex; align-items: center; gap: 8px; flex-wrap: wrap; }
.meta { font-size: 13px; color: var(--soft); margin-bottom: 6px; }
.muted { color: var(--muted); }
label { color: var(--soft); font-size: 13px; }
textarea, input[type="text"], input[type="password"], input[type="number"], input[type="date"], input[type="time"], input[type="email"], input[type="url"], input[type="search"], select {
    width: 100%; padding: 10px 12px; border-radius: 10px; border: 1px solid var(--line-2); background: #0a0a10; color: #fff; resize: vertical;
    font-size: 13.5px; font-family: inherit; margin-top: 6px; transition: border-color .15s, box-shadow .15s; }
textarea:focus, input:focus, select:focus { outline: none; border-color: color-mix(in srgb, var(--accent) 70%, transparent); box-shadow: 0 0 0 3px color-mix(in srgb, var(--accent) 18%, transparent); }
select { cursor: pointer; }
textarea { height: 90px; }
input[type="file"] { font-size: 13px; color: var(--soft); margin-top: 6px; max-width: 100%; }
input[type="file"]::file-selector-button { background: var(--panel-3); color: #fff; border: 1px solid var(--line-2); padding: 7px 12px; border-radius: 8px; margin-right: 10px; cursor: pointer; }
input[type="checkbox"] { accent-color: var(--accent); width: 16px; height: 16px; vertical-align: -3px; }
.btn { display: inline-flex; align-items: center; justify-content: center; gap: 6px; margin-top: 8px; padding: 9px 16px; border-radius: 10px; border: 1px solid transparent;
    cursor: pointer; font-size: 13px; font-weight: 600; font-family: inherit; color: #fff; background: var(--panel-3); line-height: 1.2;
    transition: transform .12s, filter .15s, background .15s, box-shadow .15s; text-decoration: none !important; white-space: nowrap; }
.btn:hover { filter: brightness(1.12); transform: translateY(-1px); }
.btn:active { transform: translateY(0); }
.btn.sm { padding: 5px 10px; font-size: 12px; border-radius: 8px; margin-top: 0; }
.ok { background: linear-gradient(180deg, #22a65a, #1b8a4a); color: #fff; }
.no { background: linear-gradient(180deg, #e0383a, #b9262a); color: #fff; }
.gray { background: #23232f; border-color: var(--line-2); color: #fff; }
.blue { background: linear-gradient(180deg, #3b7de0, #2b62b8); color: #fff; }
.orange { background: linear-gradient(180deg, #ef8a2c, #d26d14); color: #fff; }
.btn.primary { background: linear-gradient(180deg, var(--accent), color-mix(in srgb, var(--accent) 75%, #000)); box-shadow: 0 6px 16px color-mix(in srgb, var(--accent) 25%, transparent); }
.btn.ghost { background: transparent; border-color: var(--line-2); color: var(--soft); }
.btn.ghost:hover { color: #fff; background: rgba(255,255,255,.04); }
.btn.ghost.danger { color: #ff8a8a; border-color: rgba(239,68,68,.35); }
.btn[disabled] { opacity: .45; cursor: not-allowed; transform: none; }

.stat-grid, .kpi-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(180px, 1fr)); gap: 12px; margin-top: 16px; }
.stat { background: var(--panel); border: 1px solid var(--line); border-radius: 12px; padding: 12px; text-align: center; font-size: 13px; }
.stat b { font-size: 18px; color: var(--accent); }
.kpi { display: block; background: linear-gradient(160deg, var(--panel-2), var(--panel)); border: 1px solid var(--line); border-radius: var(--radius); padding: 16px; color: var(--text); transition: border-color .15s, transform .15s; }
.kpi:hover { text-decoration: none; border-color: color-mix(in srgb, var(--accent) 40%, transparent); transform: translateY(-2px); }
.kpi .k-ico { font-size: 20px; }
.kpi .k-num { font-size: 28px; font-weight: 750; margin-top: 6px; letter-spacing: -.02em; }
.kpi .k-lbl { color: var(--soft); font-size: 13px; }
.kpi.hot .k-num { color: var(--accent); }

.badge { display: inline-block; padding: 2px 9px; border-radius: 999px; font-size: 11px; font-weight: 600; margin-left: 6px; vertical-align: middle; }
.badge-new { background: rgba(59,130,246,.18); color: #8bb8ff; border: 1px solid rgba(59,130,246,.35); }
.badge-viewed { background: rgba(245,158,11,.15); color: #fcc56b; border: 1px solid rgba(245,158,11,.35); }
.badge-acc { background: rgba(34,197,94,.15); color: #6ee7a0; border: 1px solid rgba(34,197,94,.35); }
.badge-dec { background: rgba(239,68,68,.15); color: #ff9b9b; border: 1px solid rgba(239,68,68,.35); }
.badge-warn { background: rgba(249,115,22,.15); color: #ffb07a; border: 1px solid rgba(249,115,22,.35); }
.badge-gold { background: linear-gradient(135deg,#ffe066,#d4a017); color: #3a2a00; font-weight: bold; box-shadow: 0 0 8px rgba(255,215,0,.4); }
.role-badge { --rc: #718096; display: inline-flex; align-items: center; gap: 5px; padding: 2px 10px; border-radius: 999px; font-size: 12px; font-weight: 600; white-space: nowrap;
    color: var(--rc); color: color-mix(in srgb, var(--rc) 80%, #fff); background: rgba(255,255,255,.05); background: color-mix(in srgb, var(--rc) 14%, transparent);
    border: 1px solid rgba(255,255,255,.12); border-color: color-mix(in srgb, var(--rc) 38%, transparent); }
.owner-badge { --rc: #f6c445; }
.gold-card { background: linear-gradient(160deg,#1a1506,#101017 60%); border: 1px solid #6b5208; box-shadow: 0 0 14px rgba(212,160,23,.12) inset; }
.gold-card h3 { color: #ffd76a; }
.search-bar { display: flex; gap: 10px; flex-wrap: wrap; align-items: flex-end; margin: 4px 0 16px; padding: 14px; background: var(--panel); border: 1px solid var(--line); border-radius: var(--radius); }
.search-bar > div { flex: 1 1 180px; }
.search-bar input, .search-bar select { width: 100%; }
.b-ip { margin-top: 8px; padding-top: 6px; border-top: 1px dashed rgba(255,255,255,.12); font-size: 12px; color: var(--soft); }
.ip-tag { display: inline-flex; align-items: center; gap: 6px; flex-wrap: wrap; }
.ip-count { display: inline-block; min-width: 22px; padding: 0 6px; border-radius: 999px; background: rgba(239,68,68,.15); color: #ff9b9b; font-size: 11px; font-weight: 700; text-align: center; }
.callout { display: flex; gap: 12px; align-items: flex-start; padding: 14px 16px; border-radius: 12px; border: 1px solid rgba(59,130,246,.3); background: rgba(59,130,246,.08); color: #cfe0ff; margin-top: 14px; }
.callout.warn { border-color: rgba(245,158,11,.35); background: rgba(245,158,11,.08); color: #ffe2b0; }
.callout.danger { border-color: rgba(239,68,68,.35); background: rgba(239,68,68,.08); color: #ffd0d0; }
.callout .c-ico { font-size: 20px; line-height: 1; }
.empty { text-align: center; color: var(--muted); padding: 34px 10px; border: 1px dashed var(--line-2); border-radius: var(--radius); margin-top: 14px; }
.grid-2 { display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 14px; }
.row { display: flex; gap: 10px; flex-wrap: wrap; align-items: center; }
.row > .grow { flex: 1 1 200px; }
.chips { display: flex; flex-wrap: wrap; gap: 6px; }
.chip { display: inline-block; padding: 4px 10px; border-radius: 999px; background: var(--panel-3); border: 1px solid var(--line); font-size: 12px; color: var(--soft); }
.chip.on { color: #d6ffe4; border-color: rgba(34,197,94,.3); background: rgba(34,197,94,.08); }

/* ===== ГЛАВНАЯ ===== */
.hero { position: relative; overflow: hidden; display: flex; gap: 18px; align-items: center; flex-wrap: wrap; padding: 22px; border-radius: 18px; border: 1px solid var(--line);
    background: radial-gradient(600px 220px at 100% 0%, color-mix(in srgb, var(--rc, var(--accent)) 22%, transparent), transparent 70%), linear-gradient(160deg, var(--panel-2), var(--panel)); }
.hero .avatar { width: 64px; height: 64px; font-size: 26px; }
.hero h2 { margin: 0 0 6px; font-size: 22px; }
.hero p { margin: 8px 0 0; color: var(--soft); max-width: 640px; }
.list { list-style: none; margin: 0; padding: 0; }
.list li { display: flex; gap: 10px; align-items: center; justify-content: space-between; padding: 10px 0; border-bottom: 1px solid var(--line); }
.list li:last-child { border-bottom: 0; }
.qa { display: grid; grid-template-columns: minmax(140px, 240px) 1fr; gap: 1px; margin-top: 10px; background: var(--line); border: 1px solid var(--line); border-radius: 10px; overflow: hidden; }
.qa .q, .qa .a { background: #0c0c12; padding: 8px 12px; font-size: 13px; }
.qa .q { color: var(--muted); font-weight: 600; }
@media (max-width: 600px) { .qa { grid-template-columns: 1fr; } .qa .q { padding-bottom: 0; } }

/* ===== ТАБЛИЦЫ (команда, пользователи, права) ===== */
.tbl-wrap { overflow-x: auto; border: 1px solid var(--line); border-radius: var(--radius); margin-top: 14px; background: var(--panel); }
.tbl { width: 100%; border-collapse: collapse; font-size: 13.5px; }
.tbl th { text-align: left; font-weight: 600; color: var(--muted); font-size: 12px; text-transform: uppercase; letter-spacing: .06em; padding: 11px 14px; border-bottom: 1px solid var(--line); background: #0d0d14; position: sticky; top: 0; }
.tbl td { padding: 11px 14px; border-bottom: 1px solid var(--line); vertical-align: middle; }
.tbl tr:last-child td { border-bottom: 0; }
.tbl tbody tr:hover td { background: rgba(255,255,255,.02); }
.tbl .who { display: flex; align-items: center; gap: 10px; }
.tbl .who b { display: block; }
.tbl select, .tbl input { margin-top: 0; padding: 7px 10px; font-size: 13px; width: auto; min-width: 140px; }
.tbl form { margin: 0; }
.inline-form { display: inline-flex; gap: 6px; align-items: center; flex-wrap: wrap; }
.tbl .who span.muted { display: block; white-space: nowrap; }
.tbl .who b { white-space: nowrap; }
.tbl select[data-role-select] { width: 190px; min-width: 0; }
.tbl select[data-dir] { width: 130px; min-width: 0 !important; }
details.act { position: relative; }
details.act > summary { list-style: none; }
details.act > summary::-webkit-details-marker { display: none; }
.act-panel { margin-top: 8px; width: max-content; max-width: 440px; display: flex; flex-direction: column; gap: 10px; padding: 12px; background: #13131c; border: 1px solid var(--line-2); border-radius: 12px; box-shadow: var(--shadow); }
details.act[open] > summary { border-color: color-mix(in srgb, var(--accent) 50%, transparent); color: #fff; }
details.card > summary { list-style: none; }
details.card > summary::-webkit-details-marker { display: none; }
.perm-table { width: max-content; min-width: 100%; }
.perm-table th:first-child, .perm-table td:first-child { position: sticky; left: 0; z-index: 2; background: #101017; min-width: 280px; max-width: 340px; }
.perm-table thead th:first-child { z-index: 3; background: #0d0d14; }
.perm-table tr.group td:first-child { background: #0c0c12; }
.perm-table th.role-col { text-align: center; text-transform: none; letter-spacing: 0; font-size: 11.5px; color: var(--soft); width: 92px; min-width: 92px; white-space: normal; line-height: 1.25; }
.perm-table th.role-col span { display: block; font-size: 18px; }
.perm-table td.c { text-align: center; }
.perm-table tr.group td { background: #0c0c12; color: var(--accent); font-weight: 700; font-size: 12px; text-transform: uppercase; letter-spacing: .08em; }
.perm-yes { color: #4ade80; font-weight: 700; }
.perm-no { color: #3b3b4a; }
.perm-table .me-col { background: color-mix(in srgb, var(--accent) 7%, transparent); }
.role-cards { display: grid; grid-template-columns: repeat(auto-fill, minmax(260px, 1fr)); gap: 12px; margin-top: 14px; }
.role-card { background: var(--panel); border: 1px solid var(--line); border-left: 3px solid var(--rc); border-radius: 12px; padding: 14px; }
.role-card p { margin: 8px 0 0; color: var(--soft); font-size: 13px; }
.role-card .rc-head { display: flex; justify-content: space-between; align-items: center; gap: 8px; flex-wrap: wrap; }
.role-card .lvl { color: var(--muted); font-size: 12px; white-space: nowrap; }

/* ===== ЧАТ КОМАНДЫ ===== */
.chat-container { display: flex; flex-direction: column; gap: 10px; margin-bottom: 16px; max-height: 620px; overflow-y: auto; padding: 4px 8px 4px 0; }
.chat-msg { background: var(--panel); border: 1px solid var(--line); padding: 12px 14px; border-radius: 14px; max-width: 860px; }
.chat-msg.mine { border-color: color-mix(in srgb, var(--accent) 30%, transparent); background: linear-gradient(180deg, color-mix(in srgb, var(--accent) 7%, var(--panel)), var(--panel)); margin-left: auto; }
.chat-header { display: flex; justify-content: space-between; align-items: center; gap: 10px; padding-bottom: 6px; margin-bottom: 6px; }
.chat-avatar { display: inline-grid; place-items: center; width: 26px; height: 26px; border-radius: 50%; background: #2a2a38; font-size: 12px; font-weight: 700; margin-right: 8px; vertical-align: middle; }
.chat-author { color: #fff; font-weight: 650; font-size: 14px; }
.chat-role { font-size: 11px; background: rgba(255,255,255,.06); padding: 2px 8px; border-radius: 999px; color: var(--soft); margin-left: 6px; vertical-align: middle; }
.chat-time { font-size: 11px; color: var(--muted); white-space: nowrap; }
.chat-text { font-size: 14px; line-height: 1.55; white-space: pre-wrap; word-wrap: break-word; }
.chat-photo img { max-width: 100%; max-height: 350px; border-radius: 10px; margin-top: 10px; border: 1px solid var(--line); }
.chat-poll { margin-top: 12px; background: #0a0a10; padding: 12px; border-radius: 10px; border: 1px solid var(--line); }
.chat-poll h4 { margin: 0 0 10px; color: #fff; }
.poll-option { display: flex; align-items: center; justify-content: space-between; gap: 10px; background: var(--panel-2); padding: 8px 12px; border-radius: 8px; margin-bottom: 4px; }
.poll-bar-container { width: 100%; background: #0a0a10; height: 6px; border-radius: 3px; margin-top: 2px; overflow: hidden; margin-bottom: 8px; }
.poll-bar { height: 100%; background: var(--accent); transition: width 0.3s; }
.chat-actions { margin-top: 8px; display: flex; gap: 6px; flex-wrap: wrap; justify-content: flex-end; }
.pinned-mini-bar { display: flex; align-items: center; background: color-mix(in srgb, var(--accent) 10%, #0d0d14); border: 1px solid color-mix(in srgb, var(--accent) 45%, transparent); padding: 8px 12px; border-radius: 10px; color: #eee; position: sticky; top: 0; z-index: 10; margin-bottom: 5px; text-decoration: none !important; }
.pinned-icon { font-size: 18px; margin-right: 12px; }
.pinned-content { flex: 1; overflow: hidden; }
.pinned-author { font-weight: 650; font-size: 12px; color: #fff; }
.pinned-text-snippet { font-size: 12px; color: var(--soft); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.highlight-pinned { border-color: color-mix(in srgb, var(--accent) 60%, transparent); box-shadow: 0 0 0 1px color-mix(in srgb, var(--accent) 25%, transparent); }

#toast-container { position: fixed; bottom: 20px; right: 20px; z-index: 9999; display: flex; flex-direction: column; gap: 10px; pointer-events: none; max-width: min(420px, calc(100vw - 40px)); }
.toast { background: #14141d; border: 1px solid var(--line-2); border-left: 4px solid var(--info); color: #fff; padding: 13px 18px; border-radius: 10px; box-shadow: var(--shadow); font-size: 14px; animation: slideIn 0.35s ease-out forwards; pointer-events: auto; transition: opacity .4s; }
.toast.info { border-left-color: var(--info); }
.toast.success { border-left-color: var(--ok); }
.toast.error { border-left-color: var(--danger); font-weight: 600; }
@keyframes slideIn { from { transform: translateX(120%); opacity: 0; } to { transform: translateX(0); opacity: 1; } }

.file-row { display: flex; justify-content: space-between; align-items: center; gap: 12px; flex-wrap: wrap; background: var(--panel); padding: 12px 14px; border-radius: 12px; margin-bottom: 8px; border: 1px solid var(--line); }
.file-name { font-size: 15px; font-weight: 650; color: #fff; margin-bottom: 4px; }
.file-meta { font-size: 12px; color: var(--soft); }
.file-desc { font-size: 13px; color: var(--soft); margin-top: 4px; }
.reply-block { margin-top: 15px; padding: 12px; background: #0a0a10; border-left: 3px solid var(--accent); border-radius: 0 10px 10px 0; }
.reply-sig { margin-top: 8px; font-size: 12px; color: var(--muted); }

/* Мессенджер тикетов */
.support-layout { display: flex; height: 70vh; min-height: 500px; background: #0a0a10; border: 1px solid var(--line); border-radius: var(--radius); overflow: hidden; margin-top: 15px; }
.sup-sidebar { width: 300px; border-right: 1px solid var(--line); overflow-y: auto; background: #08080d; display: flex; flex-direction: column; }
#ticketSidebarList { flex: 1; overflow-y: auto; }
.sup-ticket { padding: 14px 15px; border-bottom: 1px solid var(--line); text-decoration: none !important; color: #ccc; display: block; transition: 0.2s; }
.sup-ticket:hover { background: var(--panel); }
.sup-ticket.active { background: color-mix(in srgb, var(--accent) 10%, transparent); border-left: 3px solid var(--accent); color: #fff; }
.sup-chat { flex: 1; display: flex; flex-direction: column; background: var(--panel); position: relative; min-width: 0; }
.sup-header { padding: 15px; border-bottom: 1px solid var(--line); background: #0d0d14; display: flex; justify-content: space-between; align-items: center; gap: 10px; }
.sup-history { flex: 1; padding: 20px; overflow-y: auto; display: flex; flex-direction: column; gap: 15px; }
.sup-controls { padding: 12px; border-top: 1px solid var(--line); background: #0d0d14; display: flex; gap: 10px; align-items: center; }
#repliesContainer { display: flex; flex-direction: column; gap: 15px; }
.bubble { max-width: 75%; padding: 12px 16px; border-radius: 14px; font-size: 14px; line-height: 1.45; }
.bubble.admin { background: color-mix(in srgb, var(--accent) 14%, #120a0a); border: 1px solid color-mix(in srgb, var(--accent) 40%, transparent); align-self: flex-end; border-bottom-right-radius: 4px; }
.bubble.client { background: var(--panel-2); border: 1px solid var(--line-2); align-self: flex-start; border-bottom-left-radius: 4px; }
.b-meta { font-size: 11px; font-weight: 700; margin-bottom: 5px; color: color-mix(in srgb, var(--accent) 60%, #fff); }
.file-upload-btn { background: var(--panel-3); padding: 9px 11px; border-radius: 10px; cursor: pointer; font-size: 16px; border: 1px solid var(--line-2); display: flex; align-items: center; justify-content: center; }
.file-upload-btn:hover { background: #2a2a38; }
/* Тикеты: ИИ-помощник Rai */
.sup-t-top { display: flex; align-items: center; justify-content: space-between; gap: 8px; margin-bottom: 6px; }
.sup-t-title { font-weight: 700; color: #fff; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.sup-t-meta { font-size: 11.5px; color: var(--muted); display: flex; justify-content: space-between; align-items: center; gap: 8px; }
.sup-t-meta > span:first-child { white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.sup-ticket.needs { box-shadow: inset 3px 0 0 #f59e0b; }
.sup-ticket.needs.active { box-shadow: none; }
.sup-st { flex: none; padding: 2px 8px; border-radius: 999px; font-size: 10.5px; font-weight: 700; border: 1px solid var(--line-2); white-space: nowrap; }
.sup-st.st-open { color: #8bb8ff; background: rgba(59,130,246,.12); border-color: rgba(59,130,246,.35); }
.sup-st.st-answered { color: #6ee7a0; background: rgba(34,197,94,.1); border-color: rgba(34,197,94,.35); }
.sup-st.st-human { color: #fcc56b; background: rgba(245,158,11,.12); border-color: rgba(245,158,11,.4); }
.sup-st.st-closed { color: var(--muted); }
.sup-who { flex: none; font-size: 10.5px; font-weight: 700; padding: 1px 7px; border-radius: 999px; }
.sup-who.ai { color: #e9d5ff; background: rgba(139,92,246,.18); border: 1px solid rgba(167,139,250,.35); }
.sup-who.human { color: #fde68a; background: rgba(245,158,11,.1); border: 1px solid rgba(245,158,11,.3); }
.bubble.ai { border: 1px solid transparent; background: linear-gradient(180deg, rgba(30,24,48,.96), rgba(20,17,32,.96)) padding-box, linear-gradient(135deg, rgba(167,139,250,.75), rgba(236,72,153,.45)) border-box; }
.bubble.ai .b-meta { color: #ddd6fe; }
.ai-src { font-weight: 600; font-size: 10.5px; color: #c4b5fd; background: rgba(139,92,246,.16); border-radius: 999px; padding: 1px 7px; margin-left: 4px; }
.sup-sys { align-self: center; text-align: center; font-size: 12px; color: #fcd34d; padding: 6px 14px; border-radius: 999px; background: rgba(245,158,11,.08); border: 1px dashed rgba(245,158,11,.35); }
.sup-sys span { color: var(--muted); font-size: 10.5px; margin-left: 4px; }
.sup-head-info { min-width: 0; }
.sup-head-actions { display: flex; gap: 8px; flex-wrap: wrap; align-items: center; justify-content: flex-end; }
.sup-head-actions form { margin: 0; }
.sup-head-actions .btn { margin-top: 0; }
.ai-state { display: inline-flex; align-items: center; gap: 6px; font-size: 12px; font-weight: 700; padding: 4px 10px; border-radius: 999px; }
.ai-state.on { color: #e9d5ff; background: rgba(139,92,246,.15); border: 1px solid rgba(167,139,250,.4); }
.ai-state.off { color: #fde68a; background: rgba(245,158,11,.1); border: 1px solid rgba(245,158,11,.35); }
.btn.ai-btn { background: linear-gradient(135deg, #8b5cf6, #db2777); color: #fff; box-shadow: 0 6px 18px -6px rgba(139,92,246,.7); }
.sup-controls textarea { margin: 0; flex: 1; height: 42px; min-height: 42px; max-height: 160px; resize: none; padding: 10px 12px; }
.sup-controls .btn { margin-top: 0; }
.draft-note { display: none; padding: 6px 14px; font-size: 12px; color: #c4b5fd; background: rgba(139,92,246,.08); border-top: 1px solid rgba(167,139,250,.25); }
.draft-note.on { display: block; }
.ai-card > summary { cursor: pointer; display: flex; align-items: center; gap: 10px; font-weight: 700; color: #fff; font-size: 15px; }
.ai-card .ai-logo { width: 34px; height: 34px; border-radius: 10px; display: grid; place-items: center; background: linear-gradient(135deg, #8b5cf6, #ec4899); box-shadow: 0 6px 18px -6px rgba(139,92,246,.7); }
.ai-card .ai-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; margin-top: 14px; }
.ai-card ol { margin: 6px 0 0; padding-left: 18px; color: var(--soft); font-size: 13px; line-height: 1.6; }
.ai-card code { font-size: 12px; background: rgba(255,255,255,.06); padding: 1px 5px; border-radius: 5px; }
@media (max-width: 960px) { .ai-card .ai-grid { grid-template-columns: 1fr; } }

/* ===== ТЕЛЕФОН ===== */
@media (max-width: 960px) {
    .sidebar { transform: translateX(-100%); box-shadow: var(--shadow); }
    body.nav-open .sidebar { transform: none; }
    body.nav-open .sb-backdrop { display: block; position: fixed; inset: 0; background: rgba(0,0,0,.55); z-index: 50; }
    .main { margin-left: 0; }
    .burger { display: inline-grid; place-items: center; }
    .topbar { padding: 12px 16px; }
    .topbar h1 { font-size: 17px; }
    .topbar .tb-right .hide-sm { display: none; }
    .content { padding: 18px 16px 50px; }
    .support-layout { flex-direction: column; height: auto; }
    .sup-sidebar { width: 100%; max-height: 220px; border-right: 0; border-bottom: 1px solid var(--line); }
    .sup-chat { min-height: 60vh; }
}

/* =====================================================================
   ДИЗАЙН V2 — стекло, градиентные рамки, анимации, новые компоненты
   ===================================================================== */
:root { --font: "Inter", "Segoe UI", system-ui, -apple-system, Arial, sans-serif; --mono: "JetBrains Mono", ui-monospace, Consolas, monospace; --sidebar-mini: 78px; }
body { font-family: var(--font); letter-spacing: -.006em; -webkit-font-smoothing: antialiased; }
body::before { content: ""; position: fixed; inset: 0; pointer-events: none; z-index: 0;
    background-image: radial-gradient(rgba(255,255,255,.045) 1px, transparent 1px); background-size: 24px 24px;
    -webkit-mask-image: linear-gradient(180deg, #000 0%, transparent 65%); mask-image: linear-gradient(180deg, #000 0%, transparent 65%); }
.layout { position: relative; z-index: 1; }
code, kbd { font-family: var(--mono); }
kbd { display: inline-block; padding: 1px 6px; border-radius: 6px; border: 1px solid var(--line-2); border-bottom-width: 2px; background: rgba(255,255,255,.04); font-size: 11px; color: var(--soft); }

/* Стеклянные карточки с градиентной рамкой */
.card, .kpi, .search-bar, .tbl-wrap, .role-card, .post-card, .proj-card, .goal-card, .file-row, .chat-msg, .hero {
    border: 1px solid transparent;
    background: linear-gradient(180deg, rgba(23,23,33,.92), rgba(13,13,20,.92)) padding-box,
                linear-gradient(180deg, rgba(255,255,255,.11), rgba(255,255,255,.025) 60%, rgba(255,255,255,.05)) border-box !important;
    box-shadow: 0 1px 0 rgba(255,255,255,.04) inset, 0 18px 40px -26px rgba(0,0,0,.8);
    border-radius: 16px;
}
.role-card { border-left: 3px solid var(--rc); }
.card.gold-card { background: linear-gradient(160deg, rgba(40,31,6,.95), rgba(16,16,23,.95) 60%) padding-box, linear-gradient(135deg, #b8901e, rgba(120,90,10,.25)) border-box !important; }
.hero { background: radial-gradient(600px 240px at 100% 0%, color-mix(in srgb, var(--rc, var(--accent)) 26%, transparent), transparent 70%) padding-box,
                    linear-gradient(160deg, rgba(26,26,38,.95), rgba(14,14,21,.95)) padding-box,
                    linear-gradient(135deg, color-mix(in srgb, var(--rc, var(--accent)) 55%, transparent), rgba(255,255,255,.04) 50%) border-box !important; }
.chat-msg.mine { background: linear-gradient(180deg, color-mix(in srgb, var(--accent) 9%, rgba(20,20,30,.95)), rgba(14,14,21,.95)) padding-box, linear-gradient(135deg, color-mix(in srgb, var(--accent) 45%, transparent), rgba(255,255,255,.03)) border-box !important; }
.card:hover, .proj-card:hover, .goal-card:hover, .post-card:hover { box-shadow: 0 1px 0 rgba(255,255,255,.05) inset, 0 22px 46px -26px rgba(0,0,0,.9); }

/* Появление контента */
@keyframes rise { from { opacity: 0; transform: translateY(8px); } to { opacity: 1; transform: none; } }
.content > * { animation: rise .38s cubic-bezier(.2,.7,.2,1) both; }
.content > *:nth-child(2) { animation-delay: .03s; } .content > *:nth-child(3) { animation-delay: .06s; } .content > *:nth-child(4) { animation-delay: .09s; }
.content > *:nth-child(5) { animation-delay: .12s; } .content > *:nth-child(n+6) { animation-delay: .15s; }
.kpi-grid > .kpi { animation: rise .4s cubic-bezier(.2,.7,.2,1) both; }
.kpi-grid > .kpi:nth-child(2) { animation-delay: .04s; } .kpi-grid > .kpi:nth-child(3) { animation-delay: .08s; } .kpi-grid > .kpi:nth-child(4) { animation-delay: .12s; } .kpi-grid > .kpi:nth-child(n+5) { animation-delay: .16s; }
@media (prefers-reduced-motion: reduce) { *, *::before, *::after { animation: none !important; transition: none !important; } }

/* Заголовки секций */
.section-title, .content > h3 { display: flex; align-items: center; gap: 10px; font-size: 12px !important; font-weight: 700; text-transform: uppercase; letter-spacing: .1em; color: var(--soft) !important; margin: 28px 0 12px !important; }
.section-title::after, .content > h3::after { content: ""; flex: 1; height: 1px; background: linear-gradient(90deg, var(--line-2), transparent); }
.section-title .chip { text-transform: none; letter-spacing: 0; }
.card h3 { font-size: 15.5px; letter-spacing: -.01em; }

/* KPI */
.kpi { position: relative; overflow: hidden; }
.kpi::after { content: ""; position: absolute; right: -30px; top: -30px; width: 110px; height: 110px; border-radius: 50%; background: radial-gradient(circle, color-mix(in srgb, var(--accent) 16%, transparent), transparent 70%); opacity: 0; transition: opacity .25s; }
.kpi:hover::after { opacity: 1; }
.kpi .k-ico { width: 38px; height: 38px; display: grid; place-items: center; border-radius: 11px; background: rgba(255,255,255,.05); border: 1px solid var(--line); }
.kpi .k-num { font-size: 30px; font-variant-numeric: tabular-nums; }
.kpi.hot .k-num { background: linear-gradient(135deg, #fff, var(--accent)); -webkit-background-clip: text; background-clip: text; color: transparent; }

/* Кнопки */
.btn { border-radius: 11px; box-shadow: inset 0 1px 0 rgba(255,255,255,.14), 0 1px 2px rgba(0,0,0,.45); }
.btn.ghost { box-shadow: none; }
.btn.primary { box-shadow: inset 0 1px 0 rgba(255,255,255,.25), 0 8px 22px -8px color-mix(in srgb, var(--accent) 70%, transparent); }
.btn:focus-visible, .sb-link:focus-visible, a:focus-visible { outline: 2px solid color-mix(in srgb, var(--accent) 70%, transparent); outline-offset: 2px; }

/* Поля */
textarea, input[type="text"], input[type="password"], input[type="number"], input[type="date"], input[type="time"], input[type="email"], input[type="url"], input[type="search"], select {
    background: rgba(6,6,10,.7); border-color: rgba(255,255,255,.1); border-radius: 11px; padding: 11px 13px; }
textarea:hover, input:hover, select:hover { border-color: rgba(255,255,255,.18); }
input[type="color"] { -webkit-appearance: none; appearance: none; width: 46px; height: 42px; padding: 0; border: 1px solid var(--line-2); border-radius: 11px; background: none; cursor: pointer; }
input[type="color"]::-webkit-color-swatch-wrapper { padding: 4px; } input[type="color"]::-webkit-color-swatch { border: 0; border-radius: 8px; }

/* Переключатели вместо галочек */
input[type="checkbox"] { -webkit-appearance: none; appearance: none; width: 40px; height: 23px; border-radius: 999px; background: #2a2a38; border: 1px solid var(--line-2);
    position: relative; cursor: pointer; transition: background .2s, border-color .2s; vertical-align: middle; margin: 0 8px 0 0; flex: none; }
input[type="checkbox"]::after { content: ""; position: absolute; top: 2px; left: 2px; width: 17px; height: 17px; border-radius: 50%; background: #c9c9da; transition: transform .2s, background .2s; box-shadow: 0 1px 3px rgba(0,0,0,.4); }
input[type="checkbox"]:checked { background: var(--accent); border-color: transparent; }
input[type="checkbox"]:checked::after { transform: translateX(17px); background: #fff; }
.switch-row { display: flex; align-items: center; gap: 4px; cursor: pointer; color: var(--text); font-weight: 500; }
/* В матрице прав — квадратные галочки */
.perm-table input[type="checkbox"] { width: 22px !important; min-width: 0 !important; height: 22px; padding: 0 !important; border-radius: 7px; margin: 0; }
.tbl input[type="checkbox"], .tbl input[type="radio"] { min-width: 0; padding: 0; }
.perm-table input[type="checkbox"]::after { content: "✓"; inset: 0; top: 0; left: 0; width: auto; height: auto; display: grid; place-items: center; background: none; box-shadow: none; color: transparent; font-size: 13px; font-weight: 800; transform: none; }
.perm-table input[type="checkbox"]:checked::after { color: #fff; transform: none; background: none; }

/* Сегментированный выбор (срок бана) */
.seg { display: flex; flex-wrap: wrap; gap: 6px; margin-top: 6px; }
.seg label { cursor: pointer; }
.seg input { position: absolute; opacity: 0; pointer-events: none; }
.seg span { display: inline-block; padding: 8px 12px; border-radius: 10px; border: 1px solid var(--line-2); background: rgba(255,255,255,.03); color: var(--soft); font-size: 13px; font-weight: 600; transition: all .15s; }
.seg input:checked + span { background: color-mix(in srgb, var(--accent) 18%, transparent); border-color: color-mix(in srgb, var(--accent) 60%, transparent); color: #fff; }
.flash-ring { animation: ring 1.6s ease 2; }
@keyframes ring { 0%, 100% { box-shadow: 0 0 0 0 transparent; } 40% { box-shadow: 0 0 0 4px color-mix(in srgb, var(--accent) 45%, transparent); } }

/* Раскладка «форма слева — список справа» */
.split { display: grid; grid-template-columns: minmax(300px, 380px) 1fr; gap: 18px; align-items: start; }
.sticky-card { position: sticky; top: 92px; }
@media (max-width: 1100px) { .split { grid-template-columns: 1fr; } .sticky-card { position: static; } }

/* Проекты */
.proj-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(260px, 1fr)); gap: 14px; }
.proj-card { padding: 16px; display: flex; flex-direction: column; transition: transform .2s; }
.proj-card:hover { transform: translateY(-3px); }
.proj-card.is-hidden { opacity: .55; }
.proj-head { display: flex; gap: 12px; align-items: center; }
.proj-icon { width: 48px; height: 48px; border-radius: 13px; object-fit: cover; flex: none; display: grid; place-items: center; font-size: 22px; background: rgba(255,255,255,.05); border: 1px solid var(--line); }
.proj-title { font-weight: 700; font-size: 16px; color: #fff; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.proj-desc { color: var(--soft); font-size: 13.5px; margin: 12px 0 10px; flex: 1; display: -webkit-box; -webkit-line-clamp: 3; -webkit-box-orient: vertical; overflow: hidden; }
.proj-path { display: block; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; color: var(--muted); font-size: 11.5px; }
.proj-actions { display: flex; gap: 6px; margin-top: 12px; flex-wrap: wrap; }
.proj-actions form { margin: 0; }

/* Посты блога и сливы */
.post-card { padding: 16px 18px; margin-bottom: 12px; }
.post-card.is-hidden { opacity: .6; }
.post-head { display: flex; justify-content: space-between; gap: 10px; align-items: flex-start; }
.post-title { font-weight: 700; font-size: 16px; color: #fff; }
.post-body { color: var(--soft); margin-top: 10px; font-size: 14px; line-height: 1.6; }
.post-card .row form { margin: 0; }
.post-edit { display: none; margin-top: 12px; padding-top: 12px; border-top: 1px dashed var(--line-2); }
.post-card.editing .post-edit { display: block; }
.post-card.editing .post-body { display: none; }

/* Цели */
.goal-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 14px; }
.goal-card { padding: 16px; }
.goal-card.mine { background: linear-gradient(180deg, color-mix(in srgb, var(--accent) 8%, rgba(22,22,32,.95)), rgba(13,13,20,.95)) padding-box, linear-gradient(135deg, color-mix(in srgb, var(--accent) 55%, transparent), rgba(255,255,255,.04)) border-box !important; }
.goal-card.expired { opacity: .75; }
.goal-title { font-weight: 700; font-size: 15.5px; color: #fff; }
.goal-desc { color: var(--soft); font-size: 13.5px; margin-top: 10px; }
.goal-bar { height: 7px; border-radius: 999px; background: rgba(255,255,255,.06); overflow: hidden; margin-top: 14px; }
.goal-bar span { display: block; height: 100%; border-radius: 999px; box-shadow: 0 0 12px currentColor; transition: width .6s; }

/* Штрафы */
.fine-who { font-weight: 700; color: #fff; font-size: 15px; }
.fine-amount { font-size: 24px; font-weight: 800; letter-spacing: -.02em; color: #fff; font-variant-numeric: tabular-nums; }

/* Темы сайта */
.theme-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(150px, 1fr)); gap: 10px; }
.theme-tile { position: relative; display: flex; flex-direction: column; align-items: center; gap: 6px; padding: 14px 10px; border-radius: 14px; cursor: pointer; text-align: center;
    background: rgba(255,255,255,.03); border: 1px solid var(--line); transition: transform .15s, border-color .15s, background .15s; }
.theme-tile:hover { transform: translateY(-2px); border-color: var(--line-2); }
.theme-tile input { position: absolute; opacity: 0; pointer-events: none; }
.theme-tile .tt-ico { font-size: 30px; line-height: 1; }
.theme-tile .tt-name { font-size: 12.5px; color: var(--soft); font-weight: 600; }
.theme-tile:has(input:checked) { border-color: color-mix(in srgb, var(--accent) 70%, transparent); background: color-mix(in srgb, var(--accent) 12%, transparent); box-shadow: 0 0 0 3px color-mix(in srgb, var(--accent) 18%, transparent); }
.theme-tile:has(input:checked) .tt-name { color: #fff; }
.banner-preview { margin-top: 14px; padding: 16px; border-radius: 14px; text-align: center; font-size: 16px; color: #fff;
    background: linear-gradient(135deg, color-mix(in srgb, var(--accent) 22%, transparent), rgba(255,255,255,.03)); border: 1px solid color-mix(in srgb, var(--accent) 35%, transparent); }

/* Настройки: образцы цветов */
.swatches { display: flex; gap: 8px; flex-wrap: wrap; margin-top: 10px; }
.swatch { width: 30px; height: 30px; border-radius: 50%; border: 2px solid rgba(255,255,255,.15); background: var(--sw); cursor: pointer; transition: transform .15s; }
.swatch:hover { transform: scale(1.12); border-color: #fff; }

/* Боковое меню v2 */
.sidebar { background: linear-gradient(180deg, rgba(14,14,21,.96), rgba(9,9,14,.98)); backdrop-filter: blur(12px); transition: width .22s ease, transform .25s ease; }
.sb-brand { position: relative; }
.sb-collapse { margin-left: auto; width: 28px; height: 28px; border-radius: 8px; border: 1px solid var(--line-2); background: rgba(255,255,255,.03); color: var(--soft); cursor: pointer; font-size: 13px; transition: transform .2s; }
.sb-collapse:hover { color: #fff; }
.sb-search { display: flex; align-items: center; gap: 10px; margin: 0 14px 6px; padding: 9px 12px; border-radius: 11px; border: 1px solid var(--line-2); background: rgba(255,255,255,.03); color: var(--muted); cursor: pointer; font: inherit; font-size: 13px; text-align: left; transition: border-color .15s, color .15s; }
.sb-search:hover { color: var(--soft); border-color: rgba(255,255,255,.2); }
.sb-search kbd { margin-left: auto; }
.sb-link { transition: background .15s, color .15s, transform .15s; }
.sb-link:hover { transform: translateX(2px); }
.sb-link.active { background: linear-gradient(90deg, color-mix(in srgb, var(--accent) 22%, transparent), color-mix(in srgb, var(--accent) 6%, transparent)); box-shadow: inset 0 0 0 1px color-mix(in srgb, var(--accent) 25%, transparent); }
.sb-count { box-shadow: 0 0 10px color-mix(in srgb, var(--accent) 60%, transparent); animation: pulse 2.4s ease-in-out infinite; }
@keyframes pulse { 0%, 100% { transform: scale(1); } 50% { transform: scale(1.08); } }

/* Свёрнутое меню (только иконки) */
@media (min-width: 961px) {
    html.sb-mini .sidebar { width: var(--sidebar-mini); }
    html.sb-mini .main { margin-left: var(--sidebar-mini); }
    html.sb-mini .sb-label, html.sb-mini .sb-title, html.sb-mini .sb-user .role-badge, html.sb-mini .sb-search kbd { display: none; }
    html.sb-mini .sb-brand { flex-direction: column; padding: 16px 0 10px; }
    html.sb-mini .sb-collapse { margin: 8px 0 0; transform: rotate(180deg); }
    html.sb-mini .sb-user { justify-content: center; margin: 4px 10px 10px; padding: 8px; }
    html.sb-mini .sb-search { justify-content: center; margin: 0 12px 6px; }
    html.sb-mini .sb-link { justify-content: center; padding: 10px 0; }
    html.sb-mini .sb-link .sb-count { position: absolute; top: 2px; right: 8px; min-width: 16px; padding: 0 4px; font-size: 10px; }
    html.sb-mini .sb-group { margin-top: 6px; border-top: 1px solid var(--line); padding-top: 6px; }
    html.sb-mini .sb-foot { flex-direction: column; }
}
.main { transition: margin-left .22s ease; }

/* Верхняя панель v2 */
.topbar { background: rgba(8,8,12,.62); }
.tb-title { display: flex; flex-direction: column; min-width: 0; }
.crumbs { font-size: 11.5px; color: var(--muted); display: flex; gap: 6px; text-transform: uppercase; letter-spacing: .08em; }
.crumbs a { color: var(--muted); } .crumbs a:hover { color: #fff; text-decoration: none; }
.tb-clock { color: var(--muted); font-size: 12.5px; font-variant-numeric: tabular-nums; }
.tb-btn { position: relative; width: 40px; height: 40px; display: inline-grid; place-items: center; border-radius: 11px; border: 1px solid var(--line-2); background: rgba(255,255,255,.03); color: #fff; cursor: pointer; font-size: 16px; list-style: none; }
.tb-btn:hover { background: rgba(255,255,255,.07); }
.tb-btn::-webkit-details-marker { display: none; }
.tb-dot { position: absolute; top: -5px; right: -6px; min-width: 18px; height: 18px; padding: 0 5px; border-radius: 999px; background: var(--accent); color: #fff; font-size: 10.5px; font-weight: 800; display: grid; place-items: center; box-shadow: 0 0 0 2px #09090e, 0 0 12px color-mix(in srgb, var(--accent) 70%, transparent); }
.tb-notif { position: relative; }
.tb-pop { position: absolute; right: 0; top: calc(100% + 10px); width: 300px; padding: 8px; border-radius: 14px; z-index: 80;
    background: rgba(18,18,26,.97); border: 1px solid var(--line-2); box-shadow: 0 24px 60px -20px rgba(0,0,0,.9); backdrop-filter: blur(14px); animation: rise .18s ease both; }
.tb-pop-head { padding: 8px 10px; font-size: 11.5px; text-transform: uppercase; letter-spacing: .1em; color: var(--muted); }
.tb-pop-empty { padding: 16px 10px; color: var(--soft); text-align: center; }
.tb-pop-item { display: flex; align-items: center; gap: 10px; padding: 10px; border-radius: 10px; color: var(--text); }
.tb-pop-item:hover { background: rgba(255,255,255,.05); text-decoration: none; }
.tb-pop-item .ico { width: 30px; height: 30px; display: grid; place-items: center; border-radius: 9px; background: rgba(255,255,255,.05); }
.tb-pop-item .go { margin-left: auto; color: var(--muted); }

/* Быстрый переход (Ctrl+K) */
.palette { position: fixed; inset: 0; z-index: 200; display: none; align-items: flex-start; justify-content: center; padding: 12vh 16px 16px; background: rgba(3,3,6,.6); backdrop-filter: blur(6px); }
.palette.open { display: flex; animation: fade .15s ease both; }
@keyframes fade { from { opacity: 0; } to { opacity: 1; } }
.palette-box { width: min(620px, 100%); border-radius: 18px; overflow: hidden; background: rgba(18,18,26,.98); border: 1px solid var(--line-2); box-shadow: 0 40px 100px -30px rgba(0,0,0,.95); animation: rise .2s ease both; }
.palette-box > input { margin: 0; border: 0 !important; border-bottom: 1px solid var(--line) !important; border-radius: 0; padding: 18px 20px; font-size: 16px; background: transparent; box-shadow: none !important; }
.palette-list { max-height: 50vh; overflow-y: auto; padding: 8px; }
.palette-item { display: flex; align-items: center; gap: 12px; padding: 10px 12px; border-radius: 11px; color: var(--text); }
.palette-item .ico { width: 32px; height: 32px; display: grid; place-items: center; border-radius: 9px; background: rgba(255,255,255,.05); font-size: 16px; }
.palette-item .grp { margin-left: auto; color: var(--muted); font-size: 12px; }
.palette-item .sb-count { margin-left: 8px; animation: none; }
.palette-item.sel, .palette-item:hover { background: color-mix(in srgb, var(--accent) 16%, transparent); text-decoration: none; }
.palette-foot { display: flex; gap: 16px; padding: 10px 16px; border-top: 1px solid var(--line); color: var(--muted); font-size: 12px; }
.palette-foot kbd { margin-right: 3px; }

/* Старые встроенные стили — приводим к общему виду */
.content [style*="background: #050509"], .content [style*="background:#050509"] { background: rgba(6,6,10,.6) !important; border-color: var(--line) !important; border-radius: 11px !important; }
.content [style*="background: #1a1a24"], .content [style*="background:#1a1a24"] { background: rgba(255,255,255,.03) !important; border-color: var(--line) !important; border-radius: 12px !important; }
.content [style*="background:#101018"], .content [style*="background: #101018"] { background: rgba(255,255,255,.03) !important; border-color: var(--line) !important; }
.content [style*="color:#666"], .content [style*="color: #666"], .content [style*="color:#777"], .content [style*="color: #777"], .content [style*="color:#999"], .content [style*="color: #999"] { color: var(--muted) !important; }
.content [style*="color:#aaa"], .content [style*="color: #aaa"], .content [style*="color:#ccc"], .content [style*="color: #ccc"] { color: var(--soft) !important; }
.content [style*="solid #333"], .content [style*="solid #222"], .content [style*="solid #2a0000"] { border-color: var(--line) !important; }
.content [style*="color:#ff7777"], .content [style*="color: #ff7777"] { color: color-mix(in srgb, var(--accent) 60%, #fff) !important; }
.content p { color: var(--soft); }
.content table:not(.tbl) th { color: var(--muted); font-weight: 600; }

/* Мобильная версия v2 */
@media (max-width: 960px) {
    .sb-collapse { display: none; }
    .tb-pop { position: fixed; left: 12px; right: 12px; top: 64px; width: auto; }
    .crumbs { display: none; }
    .palette { padding-top: 70px; }
    .palette-foot { display: none; }
}
</style>
<script>
function updateFormType(prefix = '') {
    var type = document.getElementById(prefix + 'project_type').value;
    var admLink = document.getElementById(prefix + 'adm_link');
    var admFile = document.getElementById(prefix + 'adm_file');
    var fileLabel = document.getElementById(prefix + 'file_label');
    var fileInput = document.getElementById(prefix + 'file_input');

    if (type === 'link') {
        admLink.style.display = 'block'; admFile.style.display = 'none';
        if(fileInput) fileInput.removeAttribute('required');
    } else {
        admLink.style.display = 'none'; admFile.style.display = 'block';
        if (prefix === '' && fileInput) fileInput.setAttribute('required', 'true');
        if (type === 'site' && fileLabel) { fileLabel.innerText = 'Выберите ZIP-архив с сайтом (авто-распаковка на сервере)'; if(fileInput) fileInput.setAttribute('accept', '.zip'); }
        else if (type === 'zip_view' && fileLabel) { fileLabel.innerText = 'Выберите ZIP-архив для просмотра содержимого онлайн'; if(fileInput) fileInput.setAttribute('accept', '.zip'); }
        else if(fileLabel) { fileLabel.innerText = 'Выберите любой файл (Если это .EXE, пользователь сможет его скачать)'; if(fileInput) fileInput.removeAttribute('accept'); }
    }
}
window.addEventListener('DOMContentLoaded', function() {
    if(document.getElementById('project_type')) updateFormType('');
    if(document.getElementById('edit_project_type')) updateFormType('edit_');
});
</script>
</head>
<body>

<div id="toast-container"></div>

<!-- Быстрый переход по разделам: Ctrl+K или «/» -->
<div class="palette" id="palette" onclick="if (event.target === this) closePalette()">
    <div class="palette-box" role="dialog" aria-label="Быстрый переход">
        <input type="text" id="paletteInput" placeholder="Куда перейти? Например: баны, заявки, чат…" autocomplete="off">
        <div class="palette-list" id="paletteList">
            <?php foreach ($TABS as $key => $t): if (!tab_allowed($key)) continue; ?>
                <a class="palette-item" href="?tab=<?=$key?>" data-q="<?=htmlspecialchars(mb_strtolower($t[0] . " " . $t[3] . " " . $key))?>"><span class="ico"><?=$t[1]?></span><span><?=htmlspecialchars($t[0])?></span><span class="grp"><?=htmlspecialchars($t[3])?></span><?php if (($count[$key] ?? 0) > 0): ?><span class="sb-count"><?=$count[$key]?></span><?php endif; ?></a>
            <?php endforeach; ?>
            <a class="palette-item" href="index.php" data-q="сайт главная выход на сайт"><span class="ico">🌐</span><span>Открыть сайт</span><span class="grp">Переход</span></a>
            <a class="palette-item" href="index.php?logout=1" data-q="выйти выход logout"><span class="ico">⎋</span><span>Выйти из аккаунта</span><span class="grp">Аккаунт</span></a>
        </div>
        <div class="palette-foot"><span><kbd>↑</kbd><kbd>↓</kbd> выбрать</span><span><kbd>Enter</kbd> открыть</span><span><kbd>Esc</kbd> закрыть</span></div>
    </div>
</div>

<div class="layout">
    <aside class="sidebar" id="sidebar">
        <div class="sb-brand">
            <div class="sb-logo">R</div>
            <div class="sb-title">RTEAM<small>Админ‑панель</small></div>
            <button class="sb-collapse" type="button" title="Свернуть меню" onclick="toggleMiniSidebar()">⟨</button>
        </div>
        <button class="sb-search" type="button" onclick="openPalette()"><span>🔍</span><span class="sb-label">Найти раздел…</span><kbd>Ctrl K</kbd></button>
        <div class="sb-user">
            <div class="avatar" style="--rc:<?=htmlspecialchars($my_role_info["color"])?>"><?=htmlspecialchars(mb_strtoupper(mb_substr($user, 0, 1)))?></div>
            <div style="min-width:0;">
                <div class="sb-user-name sb-label"><?=htmlspecialchars($user)?></div>
                <?=rt_role_badge($role, $my_direction)?>
            </div>
        </div>
        <nav class="sb-nav">
            <?php
            $nav_groups = [];
            foreach ($TABS as $key => $t) if (tab_allowed($key)) $nav_groups[$t[3]][$key] = $t;
            foreach ($nav_groups as $group => $items): ?>
                <div class="sb-group">
                    <div class="sb-group-title sb-label"><?=htmlspecialchars($group)?></div>
                    <?php foreach ($items as $key => $t): $cnt = $count[$key] ?? 0; ?>
                        <a class="sb-link<?=$tab === $key ? ' active' : ''?>" href="?tab=<?=$key?>" title="<?=htmlspecialchars($t[0])?>"><span class="ico"><?=$t[1]?></span><span class="sb-label"><?=htmlspecialchars($t[0])?></span><?php if ($cnt > 0): ?><span class="sb-count"><?=$cnt?></span><?php endif; ?></a>
                    <?php endforeach; ?>
                </div>
            <?php endforeach; ?>
        </nav>
        <div class="sb-foot">
            <a class="btn ghost sm" href="index.php" title="На сайт">←<span class="sb-label"> На сайт</span></a>
            <a class="btn ghost sm" href="index.php?logout=1" title="Выйти">⎋<span class="sb-label"> Выйти</span></a>
        </div>
    </aside>
    <div class="sb-backdrop" onclick="document.body.classList.remove('nav-open')"></div>

    <main class="main">
        <header class="topbar">
            <button class="burger" type="button" aria-label="Меню" onclick="document.body.classList.toggle('nav-open')">☰</button>
            <div class="tb-title">
                <div class="crumbs"><a href="?tab=home">RTEAM</a><span>/</span><?=htmlspecialchars($TABS[$tab][3] ?? "")?></div>
                <h1><span class="tb-ico"><?=$TABS[$tab][1] ?? "🛠"?></span><?=htmlspecialchars($TABS[$tab][0] ?? "Админ‑панель")?></h1>
            </div>
            <div class="tb-right">
                <span class="tb-clock hide-sm" id="tbClock"></span>
                <button class="tb-btn" type="button" title="Поиск (Ctrl+K)" onclick="openPalette()">🔍</button>
                <?php
                $notif = [];
                if (can("apps.view") && $count["apps"])               $notif[] = ["apps", "📝", $count["apps"], "новых заявок"];
                if (can("support.view") && $count["support"])         $notif[] = ["support", "🎧", $count["support"], "тикетов ждут ответа"];
                if (can("mail.view") && $count["messages"])           $notif[] = ["messages", "✉️", $count["messages"], "писем без ответа"];
                if (can("bot.tickets") && $count["bot"])              $notif[] = ["bot", "🤖", $count["bot"], "заявок в Telegram-боте"];
                if (can("directors.manage") && $count["directors"])   $notif[] = ["directors", "🏫", $count["directors"], "школ ждут одобрения"];
                if ($my_unpaid_fines)                                 $notif[] = ["fines", "💸", count($my_unpaid_fines), "неоплаченных штрафов"];
                if (tab_allowed("orders") && $count["orders"])        $notif[] = ["orders", "🛒", $count["orders"], so_manager() ? "заказов сайтов ждут ответа" : "новых сообщений в ваших заказах"];
                if ($count["schedule"])                               $notif[] = ["schedule", "📅", $count["schedule"], "новых задач в календаре"];
                if (tab_allowed("reviews") && $count["reviews"])      $notif[] = ["reviews", "⭐", $count["reviews"], "отзывов ждут проверки"];
                if (tab_allowed("rai") && ($rai_n = rai_cached_stats()) && $rai_n["expiring"]) $notif[] = ["rai", "⏳", $rai_n["expiring"], "подписок Rai кончаются"];
                if (tab_allowed("rai_rules") && ($rai_v = rai_cached_stats()) && !empty($rai_v["violations_new"])) $notif[] = ["rai_rules", "🚫", $rai_v["violations_new"], "нарушений правил в Rai"];
                $notif_total = array_sum(array_column($notif, 2));
                ?>
                <details class="tb-notif">
                    <summary class="tb-btn" title="Уведомления">🔔<?php if ($notif_total): ?><span class="tb-dot"><?=$notif_total > 99 ? "99+" : $notif_total?></span><?php endif; ?></summary>
                    <div class="tb-pop">
                        <div class="tb-pop-head">Уведомления</div>
                        <?php if (!$notif): ?><div class="tb-pop-empty">✨ Всё разобрано — новых дел нет</div><?php endif; ?>
                        <?php foreach ($notif as [$nk, $ni, $nc, $nl]): ?>
                            <a class="tb-pop-item" href="?tab=<?=$nk?>"><span class="ico"><?=$ni?></span><span><b><?=$nc?></b> <?=$nl?></span><span class="go">→</span></a>
                        <?php endforeach; ?>
                    </div>
                </details>
                <span class="hide-sm"><?=rt_role_badge($role, $my_direction, $user)?></span>
            </div>
        </header>
        <div class="content">

    <!-- === ГЛАВНАЯ === -->
    <?php if ($tab === "home"): ?>
        <section class="hero" style="--rc:<?=htmlspecialchars($my_role_info["color"])?>">
            <div class="avatar" style="--rc:<?=htmlspecialchars($my_role_info["color"])?>"><?=htmlspecialchars(mb_strtoupper(mb_substr($user, 0, 1)))?></div>
            <div style="flex:1; min-width:240px;">
                <h2>Привет, <?=htmlspecialchars($user)?>!</h2>
                <div class="row"><?=rt_role_badge($role, $my_direction, $user)?><?php if ($my_dept): ?><span class="chip"><?=htmlspecialchars(rt_departments()[$my_dept] ?? $my_dept)?></span><?php endif; ?></div>
                <p><?=htmlspecialchars($my_role_info["desc"] ?? "")?></p>
            </div>
        </section>

        <?php if ($role === RT_TRAINEE_ROLE): ?>
            <div class="callout"><span class="c-ico">🌱</span><div><b>Вы стажёр<?= $my_direction !== "" ? " направления «" . htmlspecialchars($my_direction) . "»" : "" ?>.</b> Сейчас доступны чат команды, файлы и цели вашего отдела. Покажите себя — руководитель или главный администратор повысит вас до полноценной роли<?= $my_direction !== "" ? " «" . htmlspecialchars($my_direction) . "»" : "" ?>.</div></div>
        <?php endif; ?>
        <?php if ($my_unpaid_fines): ?>
            <div class="callout danger"><span class="c-ico">💸</span><div>У вас <?=count($my_unpaid_fines)?> неоплаченн<?=count($my_unpaid_fines) === 1 ? "ый штраф" : "ых штрафа"?>. Если штраф не оплатить за 30 дней, роль будет снята автоматически. <a href="?tab=fines">Открыть штрафы →</a></div></div>
        <?php endif; ?>

        <div class="kpi-grid">
            <?php
            $kpis = [];
            if (tab_allowed("orders") && so_manager()) $kpis[] = ["orders", "🛒", $count["orders"], "заказов сайтов ждут ответа", $count["orders"] > 0];
            if ($count["schedule"])      $kpis[] = ["schedule", "📅", $count["schedule"], "новых задач в календаре", true];
            if (can("apps.view"))        $kpis[] = ["apps", "📝", $count["apps"], "новых заявок", $count["apps"] > 0];
            if (can("support.view"))     $kpis[] = ["support", "🎧", $count["support"], "тикетов ждут ответа", $count["support"] > 0];
            if (can("mail.view"))        $kpis[] = ["messages", "✉️", $count["messages"], "писем без ответа", $count["messages"] > 0];
            if (can("bot.tickets"))      $kpis[] = ["bot", "🤖", $count["bot"], "заявок в боте", $count["bot"] > 0];
            if (can("directors.manage")) $kpis[] = ["directors", "🏫", $count["directors"], "школ ждут одобрения", $count["directors"] > 0];
            if (can("chat.view"))        $kpis[] = ["chat", "💬", count($chat_data["messages"]), "сообщений в чате", false];
            if (can("team.view"))        $kpis[] = ["team", "👥", count($staff_list), "человек в команде", false];
            if (can("goals.view"))       $kpis[] = ["goals", "🎯", count($my_goals), "целей для вас", false];
            if (can("bans.manage"))      $kpis[] = ["bans", "⛔", $active_bans, "активных банов", false];
            if (can("projects.manage"))  $kpis[] = ["projects", "🧩", count($projects_data), "проектов", false];
            if (tab_allowed("rai") && ($rai_kpi = rai_cached_stats())) {  // Rai: подписки
                $kpis[] = ["rai", "✨", $rai_kpi["subscribers"], "подписчиков Rai", $rai_kpi["expiring"] > 0];
                $kpis[] = ["rai", "💳", number_format($rai_kpi["revenue_month"], 0, ",", " ") . " ₽", "оплат Rai за месяц", false];
            if (tab_allowed("rai_rules") && ($rai_r = rai_cached_stats()) && !empty($rai_r["violations_new"])) {  // Rai: правила
                $kpis[] = ["rai_rules", "🚫", $rai_r["violations_new"], "нарушений правил Rai", true];
            }
            }
            foreach ($kpis as [$k, $ico, $num, $lbl, $hot]): ?>
                <a class="kpi<?=$hot ? ' hot' : ''?>" href="?tab=<?=$k?>"><div class="k-ico"><?=$ico?></div><div class="k-num"><?=$num?></div><div class="k-lbl"><?=$lbl?></div></a>
            <?php endforeach; ?>
        </div>

        <?php so_home_card(); ?>

        <div class="grid-2" style="margin-top:14px;">
            <div class="card" style="margin-top:0;">
                <h3>🎯 Мои цели</h3>
                <?php if (!$my_goals): ?><div class="muted">Целей для вас пока нет.</div><?php else: ?>
                    <ul class="list">
                        <?php foreach (array_slice($my_goals, 0, 6) as $g): $expired = strtotime($g["deadline"]) < time(); ?>
                            <li><span><b><?=htmlspecialchars($g["title"])?></b><br><span class="muted" style="font-size:12px;">до <?=htmlspecialchars($g["deadline"])?></span></span><span class="badge <?=$expired ? 'badge-dec' : 'badge-new'?>"><?=$expired ? 'просрочено' : 'в работе'?></span></li>
                        <?php endforeach; ?>
                    </ul>
                <?php endif; ?>
            </div>
            <div class="card" style="margin-top:0;">
                <h3>🔐 Что вам доступно</h3>
                <?php $my_perm_count = count(array_filter(rt_all_perm_keys(), 'can')); ?>
                <?php if ($my_perm_count === count(rt_all_perm_keys())): ?>
                    <div class="chips"><span class="chip on">✓ Полный доступ — все <?=$my_perm_count?> прав</span></div>
                    <p class="muted" style="margin:10px 0 0;">Вы можете всё: назначать любые роли, банить, настраивать права ролей.</p>
                <?php else: ?>
                <div class="chips">
                    <?php foreach (rt_permissions() as $group => $perms) foreach ($perms as $pk => $plabel) if (can($pk)): ?>
                        <span class="chip on">✓ <?=htmlspecialchars($plabel)?></span>
                    <?php endif; ?>
                </div>
                <?php endif; ?>
                <a class="btn ghost sm" style="margin-top:12px;" href="?tab=perms">Права всех ролей →</a>
            </div>
        </div>

        <?php if (can("logs.view") && $logs): ?>
            <div class="card">
                <h3>📜 Последние события</h3>
                <ul class="list">
                    <?php foreach (array_slice(array_reverse($logs), 0, 6) as $log): ?>
                        <li><span><?=htmlspecialchars($log["msg"] ?? "")?></span><span class="muted" style="font-size:12px; white-space:nowrap;"><?=htmlspecialchars($log["time"] ?? "")?></span></li>
                    <?php endforeach; ?>
                </ul>
            </div>
        <?php endif; ?>

    <!-- === ВКЛАДКА: ПРОЕКТЫ === -->
    <?php elseif ($tab === "projects"): ?>
        <?php 
        $edit_project = null;
        if (isset($_GET['edit_id'])) { foreach ($projects_data as $p) { if ((string)$p['id'] === (string)$_GET['edit_id']) { $edit_project = $p; break; } } }
        ?>
        <div class="split">
        <?php if ($edit_project): ?>
            <form action="?tab=projects" method="POST" enctype="multipart/form-data" class="card sticky-card" style="margin-top:0; border-color: #e67e22;">
                <h3 style="color:#ffb46b;">✏️ <?= htmlspecialchars($edit_project['title']) ?></h3>
                <input type="hidden" name="action" value="edit_project">
                <input type="hidden" name="id" value="<?= $edit_project['id'] ?>">
                <label>Название проекта</label>
                <input type="text" name="title" required value="<?= htmlspecialchars($edit_project['title']) ?>">
                <label style="display:block; margin-top:10px;">Описание</label>
                <textarea name="description" required><?= htmlspecialchars($edit_project['description']) ?></textarea>
                <label style="display:block; margin-top:10px;">Иконка проекта (Оставьте пустым, чтобы не менять)</label>
                <?php if(!empty($edit_project['icon']) && file_exists($edit_project['icon'])): ?>
                    <div style="margin: 5px 0;"><img src="<?=$edit_project['icon']?>" style="width:32px; height:32px; object-fit:cover; border-radius:4px;"></div>
                <?php endif; ?>
                <input type="file" name="icon" accept="image/png, image/jpeg, image/webp">
                <label style="display:block; margin-top:10px;">Тип загрузки</label>
                <select name="type" id="edit_project_type" onchange="updateFormType('edit_')">
                    <option value="link" <?= $edit_project['type'] === 'link' ? 'selected' : '' ?>>Внешняя ссылка (Кнопка «Перейти»)</option>
                    <option value="file" <?= $edit_project['type'] === 'file' ? 'selected' : '' ?>>Файл / Программа (Любой формат / .EXE)</option>
                    <option value="zip_view" <?= $edit_project['type'] === 'zip_view' ? 'selected' : '' ?>>ZIP-архив (С возможностью смотреть файлы онлайн)</option>
                    <option value="site" <?= $edit_project['type'] === 'site' ? 'selected' : '' ?>>Полноценный сайт (Из ZIP-архива, Кнопка «Открыть сайт»)</option>
                </select>
                <div id="edit_adm_link">
                    <label style="display:block; margin-top:10px;">Ссылка на целевой проект (URL)</label>
                    <input type="text" name="link" value="<?= $edit_project['type'] === 'link' ? htmlspecialchars($edit_project['path']) : 'https://' ?>">
                </div>
                <div id="edit_adm_file" style="display:none;">
                    <label id="edit_file_label" style="display:block; margin-top:10px; color:#ff7777;">Выберите файл (Оставьте пустым, чтобы не перезаписывать текущий файл)</label>
                    <div style="font-size:11px; color:#aaa; margin-bottom:5px;">Текущий путь: <?= htmlspecialchars($edit_project['path']) ?></div>
                    <input type="file" name="file" id="edit_file_input">
                </div>
                <div style="margin-top:15px; display:flex; gap:10px;">
                    <button class="btn orange" type="submit" style="padding:10px 20px;">Сохранить изменения</button>
                    <a href="?tab=projects" class="btn gray" style="padding:10px 20px;">Отмена</a>
                </div>
            </form>
        <?php else: ?>
            <form action="?tab=projects" method="POST" enctype="multipart/form-data" class="card sticky-card" style="margin-top:0;">
                <h3>➕ Новый проект</h3>
                <input type="hidden" name="action" value="add_project">
                <label>Название проекта</label>
                <input type="text" name="title" required placeholder="Введите название проекта">
                <label style="display:block; margin-top:10px;">Описание</label>
                <textarea name="description" required placeholder="Введите краткое описание"></textarea>
                <label style="display:block; margin-top:10px;">Иконка проекта (авто-сжатие в WebP)</label>
                <input type="file" name="icon" accept="image/png, image/jpeg, image/webp">
                <label style="display:block; margin-top:10px;">Тип загрузки</label>
                <select name="type" id="project_type" onchange="updateFormType('')">
                    <option value="link">Внешняя ссылка (Кнопка «Перейти»)</option>
                    <option value="file">Файл / Программа (Любой формат / .EXE)</option>
                    <option value="zip_view">ZIP-архив (С возможностью смотреть файлы онлайн)</option>
                    <option value="site">Полноценный сайт (Из ZIP-архива, Кнопка «Открыть сайт»)</option>
                </select>
                <div id="adm_link">
                    <label style="display:block; margin-top:10px;">Ссылка на целевой проект (URL)</label>
                    <input type="text" name="link" placeholder="https://domain.com">
                </div>
                <div id="adm_file" style="display:none;">
                    <label id="file_label" style="display:block; margin-top:10px; color:#ff7777;">Выберите файл</label>
                    <input type="file" name="file" id="file_input">
                </div>
                <button class="btn primary" type="submit" style="margin-top:15px; width:100%;">🚀 Опубликовать проект</button>
            </form>
        <?php endif; ?>
            <div>
        <div class="section-title">Проекты на сайте <span class="chip"><?=count($projects_data)?></span></div>
        <?php if (empty($projects_data)): ?>
            <div class="empty">Проектов пока нет — добавьте первый слева.</div>
        <?php else: ?>
            <div class="proj-grid">
            <?php
            $ptypes = ["link" => ["🔗", "Ссылка"], "site" => ["🌐", "Сайт из ZIP"], "zip_view" => ["🗜️", "ZIP-архив"], "file" => ["📦", "Файл / программа"]];
            foreach (array_reverse($projects_data, true) as $key => $p): $pt = $ptypes[$p['type']] ?? $ptypes["file"]; ?>
                <div class="proj-card<?= !empty($p['hidden']) ? ' is-hidden' : '' ?>">
                    <div class="proj-head">
                        <?php if(!empty($p['icon']) && file_exists($p['icon'])): ?><img class="proj-icon" src="<?=htmlspecialchars($p['icon'])?>" alt=""><?php else: ?><div class="proj-icon"><?=$pt[0]?></div><?php endif; ?>
                        <div style="min-width:0;">
                            <div class="proj-title"><?=htmlspecialchars($p['title'])?></div>
                            <div class="row" style="gap:6px; margin-top:4px;"><span class="chip"><?=$pt[0]?> <?=$pt[1]?></span><?php if (!empty($p['hidden'])): ?><span class="badge badge-viewed" style="margin:0;">скрыт</span><?php else: ?><span class="badge badge-acc" style="margin:0;">на сайте</span><?php endif; ?></div>
                        </div>
                    </div>
                    <p class="proj-desc"><?=nl2br(htmlspecialchars($p['description']))?></p>
                    <code class="proj-path" title="<?=htmlspecialchars($p['path'])?>"><?=htmlspecialchars($p['path'])?></code>
                    <div class="proj-actions">
                        <a href="?tab=projects&edit_id=<?=urlencode($p['id'])?>" class="btn sm blue">✏️ Изменить</a>
                        <form action="?tab=projects" method="POST"><input type="hidden" name="action" value="toggle_project"><input type="hidden" name="id" value="<?=htmlspecialchars($p['id'])?>"><button class="btn sm gray" type="submit"><?= !empty($p['hidden']) ? '👁 Показать' : '🙈 Скрыть' ?></button></form>
                        <form action="?tab=projects" method="POST" onsubmit="return confirm('Удалить проект и все его файлы безвозвратно?');"><input type="hidden" name="action" value="del_project"><input type="hidden" name="id" value="<?=htmlspecialchars($p['id'])?>"><button class="btn sm ghost danger" type="submit">🗑</button></form>
                    </div>
                </div>
            <?php endforeach; ?>
            </div>
        <?php endif; ?>
            </div>
        </div>

    <!-- === ФАЙЛЫ === -->
    <?php elseif ($tab === "files"): ?>
        <?php if (can("files.upload")): ?>
        <div class="card">
            <h3>📤 Поделиться файлом с отделом</h3>
            <form method="POST" enctype="multipart/form-data">
                <input type="hidden" name="action" value="upload_file">
                <input type="file" name="team_file" required style="background: #1a1a24; padding: 10px; margin-bottom: 8px;">
                <input type="text" name="description" placeholder="Краткое описание (например: Исходник нового лаунчера Rmain)">
                <label style="font-size: 13px; color: #aaa; margin-top: 8px; display: block;">Кому доступен файл:</label>
                <select name="department" required><option value="all">Всей команде (Всем отделам)</option><option value="leader">Отделу Руководителей (самый важный)</option><option value="tester">Отделу Тестирования (Тестеры)</option><option value="admin">Административному отделу (Админы)</option><option value="dev">Разработчикам и Кодерам</option></select>
                <button class="btn ok" type="submit">Загрузить файл</button>
            </form>
        </div>
        <?php else: ?>
            <div class="callout"><span class="c-ico">ℹ️</span><div>Загружать файлы может сотрудник с правом «Файлы: загружать». Вам доступны файлы вашего отдела и всей команды.</div></div>
        <?php endif; ?>
        <h3 style="margin-top: 20px;">Доступные вам файлы</h3>
        <?php
        // отдел берётся из роли (у стажёра — из его направления)
        $file_depts = $my_dept ? [$my_dept] : [];
        $see_all_files = can("files.manage");
        $visible_files = array_filter($files_data, function($f) use ($file_depts, $user, $see_all_files) {
            if ($see_all_files || $f["uploader"] === $user || $f["department"] === "all") return true;
            if (in_array($f["department"], $file_depts)) return true; return false;
        });
        if (empty($visible_files)): ?>
            <div class="empty">Нет доступных файлов для вашего отдела.</div>
        <?php else: ?>
            <?php foreach (array_reverse($visible_files) as $f): ?>
                <?php $dept_label = "Всей команде"; if ($f["department"] === "leader") $dept_label = "Руководителям"; if ($f["department"] === "tester") $dept_label = "Тестерам"; if ($f["department"] === "admin") $dept_label = "Администраторам"; if ($f["department"] === "dev") $dept_label = "Разработчикам/Кодерам"; ?>
                <div class="file-row">
                    <div class="file-info">
                        <div class="file-name">📄 <?=htmlspecialchars($f["name"])?> <span class="chip"><?=formatBytes($f["size"])?></span></div>
                        <div class="file-meta">Загрузил: <b><?=htmlspecialchars($f["uploader"])?></b> | Дата: <?=htmlspecialchars($f["time"])?> | Доступ: <b style="color:var(--accent);"><?=$dept_label?></b></div>
                        <?php if(!empty($f["desc"])): ?><div class="file-desc"><?=htmlspecialchars($f["desc"])?></div><?php endif; ?>
                    </div>
                    <div style="display: flex; gap: 8px;">
                        <a href="<?=htmlspecialchars($f["path"])?>" download class="btn blue" style="text-decoration:none;">Скачать</a>
                        <?php if ($see_all_files || $f["uploader"] === $user): ?><form method="POST" style="margin:0;" onsubmit="return confirm('Удалить файл?');"><input type="hidden" name="action" value="del_file"><input type="hidden" name="id" value="<?=htmlspecialchars($f["id"])?>"><button class="btn no" type="submit">Удалить</button></form><?php endif; ?>
                    </div>
                </div>
            <?php endforeach; ?>
        <?php endif; ?>

    <!-- === ЧАТ === -->
    <?php elseif ($tab === "chat"): ?>
        <div class="chat-container" id="chatbox">
            <?php
            if (!empty($chat_data["pinned_id"])) {
                foreach ($chat_data["messages"] as $m) {
                    if ((string)$m["id"] === (string)$chat_data["pinned_id"]) {
                        $snippet = mb_strimwidth($m["text"], 0, 55, "...");
                        if (empty($snippet) && !empty($m["photo"])) $snippet = "[Фотография]";
                        if (empty($snippet) && !empty($m["poll"])) $snippet = "[Опрос]";
                        echo '<a href="#msg-' . htmlspecialchars($m["id"]) . '" class="pinned-mini-bar"><div class="pinned-icon">📌</div><div class="pinned-content"><div class="pinned-author">' . htmlspecialchars($m["user"]) . '</div><div class="pinned-text-snippet">' . htmlspecialchars($snippet) . '</div></div>' . (can("chat.pin") ? '<form method="POST" style="margin:0; margin-left:10px; z-index:11; position:relative;"><input type="hidden" name="action" value="unpin_msg"><button class="btn sm gray" type="submit">Открепить</button></form>' : '') . '</a>';
                        break;
                    }
                }
            }
            if (empty($chat_data["messages"])) echo '<div class="empty">В чате пока нет сообщений — напишите первым 👋</div>';
            else foreach ($chat_data["messages"] as $m) echo render_chat_message($m, ((string)$m["id"] === (string)$chat_data["pinned_id"]), $user);
            ?>
        </div>
        <script>const chatbox = document.getElementById('chatbox'); if(chatbox && !window.location.hash) chatbox.scrollTop = chatbox.scrollHeight;</script>

        <div class="card">
            <form method="POST" enctype="multipart/form-data" id="chatForm">
                <input type="hidden" name="action" value="send_chat_msg">
                <textarea name="text" placeholder="Сообщение команде… (Ctrl+Enter — отправить)" style="margin-top:0;"></textarea>
                <div class="row" style="margin-top:10px;">
                    <div class="grow"><label>📎 Фото: <input type="file" name="photo" accept="image/*"></label></div>
                    <button class="btn primary" type="submit" style="margin-top:0;">Отправить ➤</button>
                </div>
                <details style="margin-top:10px;">
                    <summary class="muted" style="cursor:pointer;">📊 Добавить опрос</summary>
                    <input type="text" name="poll_q" placeholder="Вопрос опроса">
                    <input type="text" name="poll_opt" placeholder="Варианты ответа через запятую">
                </details>
            </form>
            <script>document.querySelector('#chatForm textarea').addEventListener('keydown', e => { if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) e.target.form.submit(); });</script>
        </div>

    <!-- === ЦЕЛИ === -->
    <?php elseif ($tab === "goals"): ?>
        <?php if (can("goals.manage")): ?>
            <div class="card">
                <h3>🎯 Поставить новую цель</h3>
                <form method="POST">
                    <input type="hidden" name="action" value="add_goal">
                    <input type="text" name="title" placeholder="Название работы / Цель" required>
                    <textarea name="description" placeholder="Описание"></textarea>
                    <div style="display: flex; gap: 10px; margin-top: 6px;">
                        <div style="flex: 1;"><label style="font-size: 13px; color: #aaa;">Дата дедлайна:</label><input type="date" name="deadline_date" required></div>
                        <div style="flex: 1;"><label style="font-size: 13px; color: #aaa;">Время:</label><input type="time" name="deadline_time" required></div>
                    </div>
                    <label style="font-size: 13px; color: #aaa; margin-top: 8px; display: block;">Кому назначить задачу:</label>
                    <select name="assigned_to" required>
                        <option value="all">Всей команде</option>
                        <optgroup label="Отделам (Классам)"><option value="class:admin">Администрации</option><option value="class:coder">Кодерам</option><option value="class:tester">Тестерам</option></optgroup>
                        <optgroup label="Конкретным сотрудникам">
                            <?php foreach ($staff_list as $login => $u): ?><option value="user:<?=htmlspecialchars($login)?>"><?=htmlspecialchars($login)?> (<?=htmlspecialchars(rt_role_label($u["role"] ?? "", $u["direction"] ?? ""))?>)</option><?php endforeach; ?>
                        </optgroup>
                    </select>
                    <button class="btn ok" type="submit">Поставить цель</button>
                </form>
            </div>
        <?php endif; ?>
        <div class="section-title">Цели команды <span class="chip"><?=count($goals)?></span></div>
        <?php if (!$goals): ?><div class="empty">Активных целей пока нет.</div><?php else: ?>
            <div class="goal-grid">
            <?php foreach (array_reverse($goals) as $g): ?>
                <?php
                $now = new DateTime(); $target = new DateTime($g["deadline"]); $is_expired = $now > $target;
                if ($is_expired) { $time_left_str = "Время вышло!"; $badge_class = "badge-dec"; } 
                else { $diff = $now->diff($target); $time_left_str = "Осталось: " . $diff->format('%a дней, %h часов, %i минут'); $badge_class = "badge-new"; }
                $assigned = $g["assigned_to"] ?? "all"; $assigned_text = "Всей команде";
                if ($assigned === "class:admin") $assigned_text = "Отделу Администрации"; elseif ($assigned === "class:coder") $assigned_text = "Разработчикам и Кодерам"; elseif ($assigned === "class:tester") $assigned_text = "Отделу Тестирования"; elseif (strpos($assigned, "user:") === 0) $assigned_text = "Сотруднику: " . htmlspecialchars(substr($assigned, 5));
                ?>
                <?php
                $is_mine = in_array($g, $my_goals, true);
                // Полоса: сколько времени прошло от постановки цели до дедлайна
                $g_start = strtotime($g["created_at"] ?? "") ?: (int)($g["id"] ?? time());
                $g_end = strtotime($g["deadline"]);
                $g_pct = $g_end > $g_start ? max(0, min(100, round((time() - $g_start) / ($g_end - $g_start) * 100))) : 100;
                $g_tone = $is_expired ? "var(--danger)" : ($g_pct > 75 ? "var(--warn)" : "var(--ok)");
                ?>
                <div class="goal-card<?=$is_mine ? ' mine' : ''?><?=$is_expired ? ' expired' : ''?>">
                    <div class="row" style="justify-content:space-between; align-items:flex-start; gap:8px;">
                        <div class="goal-title"><?=htmlspecialchars($g["title"])?></div>
                        <?php if ($is_mine): ?><span class="badge badge-acc" style="margin:0;">для вас</span><?php endif; ?>
                    </div>
                    <div class="chips" style="margin-top:8px;"><span class="chip">🎯 <?=$assigned_text?></span><span class="chip">👤 <?=htmlspecialchars($g["created_by"])?></span></div>
                    <?php if (!empty($g["description"])): ?><div class="goal-desc"><?=nl2br(htmlspecialchars($g["description"]))?></div><?php endif; ?>
                    <div class="goal-bar"><span style="width:<?=$g_pct?>%; background:<?=$g_tone?>;"></span></div>
                    <div class="row" style="justify-content:space-between; margin-top:6px;">
                        <span class="meta" style="margin:0; color:<?=$is_expired ? '#ff9b9b' : 'var(--soft)'?>;"><?=$is_expired ? "⏰ Время вышло" : "⏳ " . $diff->format('%a д %h ч %i мин')?></span>
                        <span class="meta" style="margin:0;">до <?=htmlspecialchars(date("d.m.Y H:i", $g_end))?></span>
                    </div>
                    <?php if (can("goals.manage")): ?><form method="POST" style="margin-top:10px;" onsubmit="return confirm('Удалить цель?');"><input type="hidden" name="action" value="del_goal"><input type="hidden" name="id" value="<?=htmlspecialchars($g["id"])?>"><button class="btn sm ghost danger" type="submit">🗑 Удалить</button></form><?php endif; ?>
                </div>
            <?php endforeach; ?>
            </div>
        <?php endif; ?>

    <!-- === ШТРАФЫ === -->
    <?php elseif ($tab === "fines"): ?>
        <?php if (can("fines.manage")): ?>
            <div class="card">
                <h3>💸 Выписать штраф сотруднику</h3>
                <form method="POST">
                    <input type="hidden" name="action" value="add_fine">
                    <label style="font-size: 13px; color: #aaa;">Выберите сотрудника:</label>
                    <select name="user" required><option value="">-- Выбрать из команды --</option><?php foreach ($staff_list as $login => $u) if (rt_can_edit_user($user, $role, $login, $u["role"] ?? "")) echo '<option value="'.htmlspecialchars($login).'">'.htmlspecialchars($login).' ('.htmlspecialchars(rt_role_label($u["role"] ?? "", $u["direction"] ?? "")).')</option>'; ?></select>
                    <label style="font-size: 13px; color: #aaa; margin-top: 8px; display: block;">Сумма штрафа (от 10 руб.):</label>
                    <input type="number" name="amount" min="10" placeholder="Например: 500" required>
                    <label style="font-size: 13px; color: #aaa; margin-top: 8px; display: block;">Причина штрафа:</label>
                    <textarea name="reason" placeholder="Почему был выписан штраф..." required></textarea>
                    <p style="font-size: 12px; color: #aaa;">* Если штраф не оплачен за 30 дней, система заберет все права.</p>
                    <button class="btn no" type="submit">Выписать штраф</button>
                </form>
            </div>
        <?php endif; ?>
        <?php $shown_fines = can("fines.view_all") ? $fines : array_values(array_filter($fines, fn($f) => ($f["user"] ?? "") === $user)); ?>
        <div class="section-title"><?=can("fines.view_all") ? "Штрафы команды" : "Мои штрафы"?> <span class="chip"><?=count($shown_fines)?></span></div>
        <?php if (!$shown_fines): ?><div class="empty"><?=can("fines.view_all") ? "Штрафов пока нет." : "У вас нет штрафов 👍"?></div><?php else: ?>
            <?php foreach (array_reverse($shown_fines) as $f): ?>
                <?php
                $is_paid = !empty($f["paid"]);
                if ($is_paid) { $status_text = "Оплачен (" . htmlspecialchars($f["paid_date"]) . ")"; $badge = "badge-acc"; } 
                else { $days_left = 30 - floor((time() - strtotime($f["issue_date"])) / 86400); if ($days_left <= 0) { $status_text = "Просрочен! Права забраны."; $badge = "badge-dec"; } else { $status_text = "Ожидает оплаты (осталось $days_left дн.)"; $badge = "badge-warn"; } }
                ?>
                <div class="card fine-card">
                    <div class="row" style="justify-content:space-between; align-items:flex-start;">
                        <div><div class="fine-who">👤 <?=htmlspecialchars($f["user"])?></div><div class="meta" style="margin:2px 0 0;">Выписан <?=htmlspecialchars($f["issue_date"])?> · <?=htmlspecialchars($f["issued_by"])?></div></div>
                        <div style="text-align:right;"><div class="fine-amount"><?=htmlspecialchars($f["amount"])?> ₽</div><span class="badge <?=$badge?>" style="margin:0;"><?=$status_text?></span></div>
                    </div>
                    <div style="margin-top: 8px; font-size: 14px; border-left: 2px solid var(--danger); padding-left: 10px;"><b>Причина:</b> <?=nl2br(htmlspecialchars($f["reason"]))?></div>
                    <div style="margin-top: 10px; display: flex; gap: 6px; flex-wrap: wrap;">
                        <?php if (!$is_paid): ?>
                            <?php if ($f["user"] === $user): ?><form method="POST"><input type="hidden" name="action" value="pay_fine_online"><input type="hidden" name="id" value="<?=htmlspecialchars($f["id"])?>"><button class="btn blue" type="submit">Оплатить онлайн (Platega)</button></form><?php endif; ?>
                            <?php if (can("fines.manage")): ?><form method="POST"><input type="hidden" name="action" value="pay_fine_manual"><input type="hidden" name="id" value="<?=htmlspecialchars($f["id"])?>"><button class="btn ok" type="submit">Подтвердить получение (вручную)</button></form><?php endif; ?>
                        <?php endif; ?>
                        <?php if (can("fines.manage")): ?><form method="POST" onsubmit="return confirm('Удалить запись о штрафе?');"><input type="hidden" name="action" value="del_fine"><input type="hidden" name="id" value="<?=htmlspecialchars($f["id"])?>"><button class="btn gray" type="submit">Удалить запись</button></form><?php endif; ?>
                    </div>
                </div>
            <?php endforeach; ?>
        <?php endif; ?>

    <!-- === ЗАЯВКИ === -->
    <?php elseif ($tab === "apps"): ?>
        <form class="search-bar" method="GET">
            <input type="hidden" name="tab" value="apps">
            <div><label>Поиск по нику / email</label><br><input type="text" name="search" value="<?=htmlspecialchars($search)?>" placeholder="Ник или email"></div>
            <div><label>Сортировка</label><br><select name="sort"><option value="newest" <?=$sort==="newest"?"selected":""?>>Сначала новые</option><option value="oldest" <?=$sort==="oldest"?"selected":""?>>Сначала старые</option><option value="status" <?=$sort==="status"?"selected":""?>>По статусу</option></select></div>
            <div style="align-self:flex-end;"><button class="btn gray" type="submit" style="margin-top:0;">Применить</button></div>
        </form>
        <?php if (can("apps.decide")): ?>
            <div class="callout"><span class="c-ico">🌱</span><div>Кнопка <b>«Принять»</b> сразу выдаёт человеку роль <b>«Стажёр»</b> и направление, на которое человек подавал (его можно поправить перед принятием), и отправляет письмо с решением на email из заявки.</div></div>
        <?php endif; ?>
        <?php if (!$applications): ?><div class="empty">Заявок пока нет.</div><?php else: ?>
            <?php if (can("apps.decide") && count($users) <= 5000): ?>
                <datalist id="userLogins"><?php foreach ($users as $l => $_): ?><option value="<?=htmlspecialchars($l)?>"><?php endforeach; ?></datalist>
            <?php endif; ?>
            <?php foreach ($applications as $app): ?>
                <?php
                $status = $app["status"] ?? "new"; $badgeText = "Новая"; $badgeClass = "badge-new"; $edge = "#3b82f6";
                if ($status === "viewed") { $badgeText = "Просмотрена"; $badgeClass = "badge-viewed"; $edge = "#f59e0b"; }
                elseif ($status === "resolved_accept") { $badgeText = "Принята"; $badgeClass = "badge-acc"; $edge = "#22c55e"; }
                elseif ($status === "resolved_decline") { $badgeText = "Отказ"; $badgeClass = "badge-dec"; $edge = "#ef4444"; }
                $resolved = in_array($status, ["resolved_accept", "resolved_decline"], true);
                $nick  = rt_app_answer($app, "Ник");
                $email = rt_app_answer($app, "email");
                $app_dir = $app["direction"] ?? rt_app_direction($app);
                $acc = $app["account"] ?? rt_app_account($app, $users);
                if ($acc !== null && !isset($users[$acc])) $acc = null;
                ?>
                <div class="card" style="border-left: 3px solid <?=$edge?>;">
                    <div class="row" style="justify-content:space-between; align-items:flex-start;">
                        <div>
                            <h3 style="margin:0;">👤 <?=htmlspecialchars($nick !== "" ? $nick : "Без ника")?> <span class="badge <?=$badgeClass?>"><?=$badgeText?></span></h3>
                            <div class="meta" style="margin-top:4px;">#<?=htmlspecialchars($app["id"])?> · <?=htmlspecialchars($app["type"] ?? "")?> · <?=htmlspecialchars($app["time"] ?? "")?><?php if ($email !== ""): ?> · <?=htmlspecialchars($email)?><?php endif; ?></div>
                        </div>
                        <span class="chip">Направление: <b style="color:#fff;"><?=htmlspecialchars($app_dir)?></b></span>
                    </div>

                    <details style="margin-top:10px;"<?=$resolved ? '' : ' open'?>>
                        <summary class="muted" style="cursor:pointer;">Ответы на вопросы (<?=count($app["answers"] ?? [])?>)</summary>
                        <div class="qa">
                            <?php foreach (($app["answers"] ?? []) as $row): ?><div class="q"><?=htmlspecialchars($row["q"])?></div><div class="a"><?=nl2br(htmlspecialchars($row["a"]))?></div><?php endforeach; ?>
                        </div>
                    </details>

                    <div class="meta" style="margin-top:10px;">
                        Аккаунт на сайте:
                        <?php if ($acc): ?><b style="color:#fff;"><?=htmlspecialchars($acc)?></b> <?=rt_role_badge($users[$acc]["role"] ?? "Пользователь", $users[$acc]["direction"] ?? "")?>
                        <?php else: ?><span style="color:#fcc56b;">⚠ не найден по нику и email — впишите логин вручную</span><?php endif; ?>
                    </div>
                    <?php if (can("bans.manage")): ?><div class="meta">IP: <?=ip_tag(author_ip($app["ip"] ?? null, $acc, $email), "Заявка #" . $app["id"] . " (" . $nick . ")")?></div><?php endif; ?>
                    <?php if ($resolved && !empty($app["decided_by"])): ?>
                        <div class="meta">Решение: <b><?=htmlspecialchars($app["decided_by"])?></b> · <?=htmlspecialchars($app["decided_at"] ?? "")?><?php if (!empty($app["comment"])): ?> · «<?=htmlspecialchars($app["comment"])?>»<?php endif; ?></div>
                    <?php endif; ?>

                    <?php if (can("apps.decide")): ?>
                        <?php if ($resolved): ?><details style="margin-top:8px;"><summary class="muted" style="cursor:pointer;">Изменить решение</summary><?php endif; ?>
                        <form method="POST" action="?tab=apps&search=<?=urlencode($search)?>&sort=<?=urlencode($sort)?>" style="margin-top:10px;">
                            <input type="hidden" name="action" value="app_decide">
                            <input type="hidden" name="id" value="<?=htmlspecialchars($app["id"])?>">
                            <div class="row">
                                <div class="grow"><label>Логин аккаунта</label><input type="text" name="login" list="userLogins" value="<?=htmlspecialchars($acc ?? $nick)?>" placeholder="Логин на сайте"></div>
                                <div class="grow"><label>Направление стажёра</label><select name="direction"><?php foreach (rt_directions() as $dk => $dl): ?><option value="<?=htmlspecialchars($dk)?>" <?=$dk === $app_dir ? "selected" : ""?>><?=htmlspecialchars($dl)?></option><?php endforeach; ?></select></div>
                            </div>
                            <textarea name="comment" placeholder="Комментарий кандидату (необязательно, уйдёт в письмо)" style="height:60px;"></textarea>
                            <div class="row" style="margin-top:4px;">
                                <button class="btn ok" name="decision" value="accept" onclick="return confirm('Принять заявку и выдать роль «Стажёр»?');">✓ Принять — выдать «Стажёр»</button>
                                <button class="btn no" name="decision" value="decline" onclick="return confirm('Отклонить заявку?');">✕ Отказать</button>
                            </div>
                        </form>
                        <?php if ($resolved): ?></details><?php endif; ?>
                    <?php endif; ?>
                    <?php if ($status === "new"): ?><form method="POST" style="margin-top:6px;"><input type="hidden" name="action" value="mark_viewed"><input type="hidden" name="id" value="<?=htmlspecialchars($app["id"])?>"><button class="btn ghost sm" type="submit" style="margin-top:6px;">Отметить как просмотренную</button></form><?php endif; ?>
                </div>
            <?php endforeach; ?>
        <?php endif; ?>

    <!-- === СООБЩЕНИЯ САЙТА === -->
    <!-- === DISCORD: ВХОД ЧЕРЕЗ DISCORD И БОТ === -->
    <?php elseif ($tab === "discord"):
        $dc = rt_discord_conf($settings);
        $d_report = $_SESSION["discord_report"] ?? null; unset($_SESSION["discord_report"]);
        $d_linked = count(array_filter($users, fn($u) => is_array($u) && !empty($u["discord_id"])));
        $d_src = fn($k) => trim((string)($settings["discord_" . $k] ?? "")) !== "" ? "задан в админке" : ($dc[$k] !== "" ? "задан в discord_config.php" : "");
    ?>
        <div class="card">
            <h3>💬 Discord: вход на сайт и бот</h3>
            <p class="meta">Вход и регистрация через Discord, привязка в кабинете, коды входа в админ-панель и уведомления в ЛС от бота RTeam. Сам бот (автомодерация, заявки в модераторы, идеи, правила) работает отдельно — папка <code>discord/</code>, файл <code>index.js</code>.</p>
            <div style="display:flex; gap:8px; flex-wrap:wrap; margin:10px 0;">
                <span class="chip <?=rt_discord_login_ready($settings) ? "on" : ""?>">Вход через Discord: <?=rt_discord_login_ready($settings) ? "включён" : "не настроен"?></span>
                <span class="chip <?=rt_discord_bot_ready($settings) ? "on" : ""?>">Бот: <?=rt_discord_bot_ready($settings) ? htmlspecialchars($dc["bot_url"]) : "не настроен"?></span>
                <span class="chip">Привязали Discord: <?=$d_linked?></span>
            </div>
            <?php if ($d_report): ?>
                <div class="callout" style="flex-direction:column; gap:4px; word-break:break-word;"><?php foreach ($d_report as $line): ?><div><?=preg_replace('~(https://[^\s<]+)~', '<a href="$1" target="_blank" rel="noopener">$1</a>', htmlspecialchars($line))?></div><?php endforeach; ?></div>
            <?php endif; ?>
            <form method="POST" action="?tab=discord">
                <input type="hidden" name="action" value="save_discord">
                <div style="display:grid; grid-template-columns:repeat(auto-fit, minmax(260px, 1fr)); gap:16px;">
                    <div>
                        <label class="muted" style="display:block; margin-top:6px; font-size:12.5px;">Client ID (Developer Portal → OAuth2)</label>
                        <input type="text" name="discord_client_id" value="<?=htmlspecialchars($settings["discord_client_id"] ?? "")?>" placeholder="<?=htmlspecialchars($dc["client_id"] ?: "1506622482856017970")?>">
                        <label class="muted" style="display:block; margin-top:12px; font-size:12.5px;">Client Secret <?=$d_src("client_secret") ? "· ✓ " . $d_src("client_secret") : ""?></label>
                        <input type="password" name="discord_client_secret" value="" autocomplete="new-password" placeholder="<?=$d_src("client_secret") ? "оставьте пустым, чтобы не менять" : "секрет из OAuth2"?>">
                        <?php if (!empty($settings["discord_client_secret"])): ?><label class="switch-row" style="margin-top:6px;"><input type="checkbox" name="clear_client_secret"> убрать секрет из админки</label><?php endif; ?>
                        <label class="muted" style="display:block; margin-top:12px; font-size:12.5px;">Адрес возврата (пусто — <?=htmlspecialchars(rt_site_url() . "/discord_auth.php")?>)</label>
                        <input type="text" name="discord_redirect" value="<?=htmlspecialchars($settings["discord_redirect"] ?? "")?>" placeholder="<?=htmlspecialchars($dc["redirect"] ?: "https://rteam.info/discord_auth.php")?>">
                    </div>
                    <div>
                        <label class="muted" style="display:block; margin-top:6px; font-size:12.5px;">Адрес бота (Node.js-приложение в Plesk)</label>
                        <input type="text" name="discord_bot_url" value="<?=htmlspecialchars($settings["discord_bot_url"] ?? "")?>" placeholder="<?=htmlspecialchars($dc["bot_url"] ?: "https://discord.rteam.info")?>">
                        <label class="muted" style="display:block; margin-top:12px; font-size:12.5px;">Ключ бота (api_key из secret.json бота) <?=$d_src("api_key") ? "· ✓ " . $d_src("api_key") : ""?></label>
                        <input type="password" name="discord_api_key" value="" autocomplete="new-password" placeholder="<?=$d_src("api_key") ? "оставьте пустым, чтобы не менять" : "длинная случайная строка"?>">
                        <?php if (!empty($settings["discord_api_key"])): ?><label class="switch-row" style="margin-top:6px;"><input type="checkbox" name="clear_api_key"> убрать ключ из админки</label><?php endif; ?>
                        <label class="muted" style="display:block; margin-top:12px; font-size:12.5px;">Приглашение на сервер (для кнопки в кабинете)</label>
                        <input type="text" name="discord_invite" value="<?=htmlspecialchars($settings["discord_invite"] ?? "")?>" placeholder="<?=htmlspecialchars($dc["invite"] ?: "https://discord.gg/…")?>">
                    </div>
                </div>
                <div style="display:flex; gap:8px; flex-wrap:wrap; margin-top:12px;">
                    <button class="btn primary" type="submit">Сохранить</button>
                    <button class="btn ghost" type="submit" form="dsCheck">Проверить всё</button>
                    <button class="btn ghost" type="submit" form="dsRestart" <?=rt_discord_bot_ready($settings) ? "" : "disabled"?>>🔄 Перезапустить бота</button>
                </div>
            </form>
            <form method="POST" action="?tab=discord" id="dsCheck"><input type="hidden" name="action" value="check_discord"></form>
            <form method="POST" action="?tab=discord" id="dsRestart" onsubmit="return confirm('Перезапустить бота? Он перечитает config.json и заново напишет начальные сообщения: кнопку заявок, подсказку в канале идей и правила (старые удалит).');"><input type="hidden" name="action" value="restart_discord"></form>
            <div class="muted" style="font-size:12px; margin-top:10px;">Пустые поля берутся из файла <code>discord_config.php</code> рядом с сайтом. Секреты здесь не показываются.</div>
        </div>
        <div class="card">
            <h3>Как всё устроено</h3>
            <ol style="margin:8px 0 0; padding-left:20px; line-height:1.6;">
                <li><b>Вход через Discord</b> — кнопки на страницах входа и регистрации. Нет аккаунта — создаётся новый (логин из ника Discord). Сотрудникам с привязанным Telegram после входа через Discord нужен ещё код из Telegram.</li>
                <li><b>Привязка</b> — кабинет → «Привязки» → «Привязать Discord». Если привязаны и Telegram, и Discord, сотрудник выбирает, куда приходит код входа; на странице кода можно прислать его в другое место.</li>
                <li><b>ЛС от бота</b>: коды входа, ответы поддержки в тикетах, сообщения администрации («Пользователи» → «Управлять» → «Написать в Discord»). Бот пишет только тем, кто есть на сервере и не закрыл ЛС.</li>
                <li><b>«Перезапустить бота»</b> — бот перечитывает <code>config.json</code> (новые вопросы, правила, каналы), заново регистрирует команды и пишет начальные сообщения: кнопку «Подать заявку», подсказку в канале идей и правила. Старые сообщения удаляются.</li>
                <li><b>Админ-панель в Discord</b> — команда <code>/админ</code> (роль из <code>panel.role_ids</code> в <code>discord/config.json</code>): объявления в «Медиа» и «Общий чат», список кандидатов на обзвоне — позвать в голосовой канал, «Прошёл» (выдаёт роль) или «Не прошёл», топ активности.</li>
                <li><b>Бот на сервере</b>: уровни и активность (<code>/уровень</code>, <code>/топ</code>), автомодерация, заявки в модераторы (кнопка → вопросы → «Отклонить» / «На обзвон» с ролью → «Принять»), идеи с 👍/👎, правила, ответы нейросети Rai в ЛС. Настройки и ID каналов — <code>discord/config.json</code>.</li>
            </ol>
        </div>

    <?php elseif ($tab === "schedule" || $tab === "orders" || $tab === "reviews"): so_admin_render($tab); ?>

    <?php elseif ($tab === "rai"): rai_admin_render(); ?>

    <?php elseif ($tab === "rai_rules"): rai_rules_render(); ?>

    <?php elseif ($tab === "rai_push"): rai_push_render(); ?>

    <?php elseif ($tab === "rai_sale"): rai_sale_render(); ?>

    <?php elseif ($tab === "directors"): ?>
        <h3>Заявки на регистрацию школы</h3>
        <?php
        $pendingReqs = array_filter($director_requests, fn($r) => ($r["status"] ?? "pending") === "pending");
        $resolvedReqs = array_filter($director_requests, fn($r) => ($r["status"] ?? "pending") !== "pending");
        ?>
        <?php if (!$pendingReqs): ?><p style="color:#666;">Новых заявок нет.</p><?php else: ?>
            <?php foreach ($pendingReqs as $req): ?>
                <div class="card" style="border-left: 4px solid #2b6cb0;">
                    <div style="float:right;"><span class="badge badge-new">Новая</span></div>
                    <h3><?=htmlspecialchars($req["schoolName"])?></h3>
                    <div class="meta">Директор: <b><?=htmlspecialchars($req["fullName"] ?? "—")?></b> | Email: <b><?=htmlspecialchars($req["email"] ?? "—")?></b><?php if (!empty($req["phone"])): ?> | Телефон: <b><?=htmlspecialchars($req["phone"])?></b><?php endif; ?></div>
                    <div class="meta">Желаемый логин: <b><?=htmlspecialchars($req["login"])?></b> | Пароль: <b><?=htmlspecialchars($req["password"])?></b></div>
                    <div class="meta">Дата заявки: <?=htmlspecialchars($req["date"] ?? "")?></div>
                    <div style="margin-top:10px; display:flex; gap:10px; flex-wrap:wrap;">
                        <form method="POST"><input type="hidden" name="action" value="approve_request"><input type="hidden" name="id" value="<?=htmlspecialchars($req["id"])?>"><button class="btn ok" type="submit">Одобрить</button></form>
                        <form method="POST" onsubmit="return confirm('Отклонить заявку?');"><input type="hidden" name="action" value="decline_request"><input type="hidden" name="id" value="<?=htmlspecialchars($req["id"])?>"><button class="btn no" type="submit">Отклонить</button></form>
                    </div>
                </div>
            <?php endforeach; ?>
        <?php endif; ?>

        <?php if ($resolvedReqs): ?>
            <h3 style="margin-top:20px;">Обработанные заявки</h3>
            <?php foreach ($resolvedReqs as $req): $st = $req["status"]; ?>
                <div class="card" style="opacity:0.7;">
                    <div style="float:right;"><span class="badge <?= $st === 'approved' ? 'badge-acc' : 'badge-dec' ?>"><?= $st === 'approved' ? 'Одобрена' : 'Отклонена' ?></span></div>
                    <h3><?=htmlspecialchars($req["schoolName"])?></h3>
                    <div class="meta">Директор: <?=htmlspecialchars($req["fullName"] ?? "—")?> | Email: <?=htmlspecialchars($req["email"] ?? "—")?> | Логин: <?=htmlspecialchars($req["login"])?></div>
                    <form method="POST" style="margin-top:6px;" onsubmit="return confirm('Удалить запись о заявке?');"><input type="hidden" name="action" value="delete_request"><input type="hidden" name="id" value="<?=htmlspecialchars($req["id"])?>"><button class="btn gray" type="submit">Удалить запись</button></form>
                </div>
            <?php endforeach; ?>
        <?php endif; ?>

        <h3 style="margin-top:26px;">Добавить директора вручную</h3>
        <div class="card">
            <form method="POST">
                <input type="hidden" name="action" value="add_director">
                <input type="text" name="schoolName" placeholder="Название школы" required>
                <input type="text" name="fullName" placeholder="ФИО директора">
                <input type="email" name="email" placeholder="Email директора" required>
                <input type="text" name="login" placeholder="Логин" required>
                <input type="text" name="password" placeholder="Пароль" required>
                <button class="btn ok" type="submit">Создать аккаунт директора</button>
            </form>
        </div>

        <?php
            // Постраничный список: даже при огромном числе школ страница
            // администратора не пытается отрисовать их все разом — только
            // текущую страницу (по умолчанию 30 штук), плюс поиск по
            // названию школы или логину через лёгкий индекс.
            $dirSearch = trim($_GET["dsearch"] ?? "");
            $dirPageSize = 30;
            $dirPage = max(1, (int)($_GET["dpage"] ?? 1));
            $dirOffset = ($dirPage - 1) * $dirPageSize;
            $dirPageData = dl_list_index_page($dirSearch, $dirOffset, $dirPageSize);
            $dirTotal = $dirPageData["total"];
            $dirTotalPages = max(1, (int)ceil($dirTotal / $dirPageSize));
        ?>

        <h3 style="margin-top:26px;">Директора школ (<?=$dirTotal?>)</h3>

        <?php if (!empty($__migrationResult['skipped'])): ?>
            <div class="card" style="border-color:#c53030;">
                <b>При переносе старых данных в MySQL пропущено школ: <?=count($__migrationResult['skipped'])?></b> (обычно из-за задвоенного логина директора — в MySQL логин теперь обязан быть уникален). Эти школы <b>не потеряны</b> — их старые файлы (<code>database_&lt;slug&gt;.json</code>) остались на диске, их можно добавить вручную ниже под другим логином, если это разные школы.
                <ul style="margin:8px 0 0 20px; font-size:12px; color:#555;">
                    <?php foreach ($__migrationResult['skipped'] as $s): ?><li><?=htmlspecialchars($s)?></li><?php endforeach; ?>
                </ul>
            </div>
        <?php endif; ?>

        <?php if (isset($_GET["regen_offset"])): ?>
            <div class="card" style="border-color:#3182ce;">
                Обновление файлов школ: обработано <?=(int)$_GET["regen_offset"]?> из <?=(int)($_GET["regen_total"] ?? 0)?>.
                <form method="POST" style="display:inline;"><input type="hidden" name="action" value="regenerate_all"><input type="hidden" name="offset" value="<?=(int)$_GET["regen_offset"]?>"><button class="btn blue" type="submit">Обработать дальше</button></form>
            </div>
        <?php elseif (isset($_GET["regen_done"])): ?>
            <div class="card" style="border-color:#38a169;">Готово — файлы всех школ обновлены.</div>
        <?php endif; ?>

        <?php if ($dirTotal): ?>
            <form method="POST" onsubmit="return confirm('Пересоздать dairy.html и сайт-визитку для ВСЕХ школ? Для большого числа школ это может потребовать несколько нажатий «Обработать дальше».');" style="margin-bottom:14px;">
                <input type="hidden" name="action" value="regenerate_all">
                <input type="hidden" name="offset" value="0">
                <button class="btn blue" type="submit">Обновить файлы всех школ</button>
                <span style="font-size:12px; color:#999; margin-left:8px;">У каждой школы своя копия dairy.html — после правок в оригинальном dairy.html нажмите сюда, чтобы разослать обновление всем школам разом.</span>
            </form>
        <?php endif; ?>

        <form method="GET" style="margin-bottom:14px;">
            <input type="hidden" name="tab" value="directors">
            <input type="text" name="dsearch" value="<?=htmlspecialchars($dirSearch)?>" placeholder="Поиск по названию школы или логину директора">
            <button class="btn blue" type="submit">Искать</button>
            <?php if ($dirSearch !== ""): ?><a href="admin.php?tab=directors" style="margin-left:8px; color:#999; font-size:13px;">Сбросить</a><?php endif; ?>
        </form>

        <?php if (!$dirPageData["rows"]): ?><p style="color:#666;">Школ не найдено.</p><?php else: ?>
            <?php foreach ($dirPageData["rows"] as $row): $d = dl_load_full($row["slug"]); if (!$d) continue; ?>
                <div class="card">
                    <h3><?=htmlspecialchars($d["schoolName"])?></h3>
                    <div class="meta">Директор: <?=htmlspecialchars($d["fullName"] ?? "—")?> | Email: <?=htmlspecialchars($d["email"] ?? "—")?></div>
                    <div class="meta">Логин: <b><?=htmlspecialchars($d["login"])?></b> | Пароль: <b><?=htmlspecialchars($d["password"])?></b> | Данные: <code>database_<?=htmlspecialchars($d["slug"])?>.json</code></div>
                    <div class="meta">Страница журнала школы:
                        <?php if (!empty($d["schoolFile"]) && file_exists($d["schoolFile"])): ?>
                            <a href="<?=htmlspecialchars($d["schoolFile"])?>" target="_blank"><?=htmlspecialchars($d["schoolFile"])?></a>
                        <?php else: ?>
                            <span style="color:#c53030;">не создана</span>
                            <form method="POST" style="display:inline;"><input type="hidden" name="action" value="regenerate_file"><input type="hidden" name="slug" value="<?=htmlspecialchars($d["slug"])?>"><button class="btn blue" type="submit" style="padding:3px 8px; font-size:11px; margin:0;">Создать файл</button></form>
                            <span style="font-size:11px; color:#999;">(нужен dairy.html рядом с admin.php)</span>
                        <?php endif; ?>
                    </div>
                    <div class="meta">Сайт-визитка школы (редактируется программистом в scheduler.html):
                        <?php $siteFile = "schools/" . $d["slug"] . "/index.html"; ?>
                        <?php if (file_exists($siteFile)): ?>
                            <a href="<?=htmlspecialchars($siteFile)?>" target="_blank"><?=htmlspecialchars($siteFile)?></a>
                        <?php else: ?>
                            <span style="color:#c53030;">не создана</span>
                            <span style="font-size:11px; color:#999;">(нужен school_site_template.html рядом с admin.php — нажмите «Создать файл» выше, чтобы пересоздать)</span>
                        <?php endif; ?>
                    </div>
                    <div style="margin-top:8px; display:flex; gap:10px; flex-wrap:wrap; align-items:center;">
                        <form method="POST" style="display:flex; gap:6px; align-items:center;"><input type="hidden" name="action" value="reset_director_password"><input type="hidden" name="id" value="<?=htmlspecialchars($d["id"])?>"><input type="hidden" name="slug" value="<?=htmlspecialchars($d["slug"])?>"><input type="text" name="password" placeholder="Новый пароль" style="margin:0;" required><button class="btn blue" type="submit" style="margin:0;">Сменить пароль</button></form>
                        <form method="POST" onsubmit="return confirm('Удалить аккаунт директора? Данные школы (журнал) при этом не удаляются.');"><input type="hidden" name="action" value="delete_director"><input type="hidden" name="slug" value="<?=htmlspecialchars($d["slug"])?>"><button class="btn no" type="submit">Удалить директора</button></form>
                    </div>
                </div>
            <?php endforeach; ?>
            <?php if ($dirTotalPages > 1): ?>
                <div style="display:flex; gap:8px; align-items:center; margin-top:14px;">
                    <?php if ($dirPage > 1): ?><a class="btn blue" href="admin.php?tab=directors&dpage=<?=$dirPage - 1?>&dsearch=<?=urlencode($dirSearch)?>">← Назад</a><?php endif; ?>
                    <span style="color:#999; font-size:13px;">Страница <?=$dirPage?> из <?=$dirTotalPages?></span>
                    <?php if ($dirPage < $dirTotalPages): ?><a class="btn blue" href="admin.php?tab=directors&dpage=<?=$dirPage + 1?>&dsearch=<?=urlencode($dirSearch)?>">Вперёд →</a><?php endif; ?>
                </div>
            <?php endif; ?>
        <?php endif; ?>

    <?php elseif ($tab === "messages"): ?>
        <div class="callout"><span class="c-ico">✉️</span><div>Письма с формы «Написать нам» на сайте. Ответ уходит человеку на email.</div></div>
        <?php if (!$messages): ?><p>Сообщений пока нет.</p><?php else: ?>
            <?php foreach (array_reverse($messages) as $m): ?>
                <div class="card">
                    <h3>От: <?=htmlspecialchars($m["name"])?> (<?=htmlspecialchars($m["email"])?>)</h3>
                    <div class="meta"><?=htmlspecialchars($m["time"] ?? "")?></div>
                    <div class="meta">IP: <?=ip_tag(author_ip($m["ip"] ?? null, null, $m["email"] ?? null), "Спам в почте: " . str_replace(["\r", "\n"], " ", mb_strimwidth($m["text"] ?? "", 0, 60, "…")), true)?></div>
                    <div style="margin-top:10px; background:#050509; padding:10px; border-radius:6px; border:1px solid #333;"><?=nl2br(htmlspecialchars($m["text"]))?></div>
                    <?php if (!empty($m["reply"])): ?>
                        <div class="reply-block"><b>Ваш ответ:</b><br><?=nl2br(htmlspecialchars($m["reply"]["text"]))?><div class="reply-sig">Сотрудник: <?=htmlspecialchars($m["reply"]["author"])?> (<?=htmlspecialchars($m["reply"]["role"])?>)<br>С уважением, Rteam</div></div>
                    <?php else: ?>
                        <form method="POST" style="margin-top:10px;"><input type="hidden" name="action" value="reply_msg"><input type="hidden" name="id" value="<?=htmlspecialchars($m["id"])?>"><textarea name="reply_text" placeholder="Введите ответ пользователю..." required></textarea><button class="btn ok" type="submit">Отправить ответ (и Email)</button></form>
                    <?php endif; ?>
                    <form method="POST" style="margin-top:10px;"><input type="hidden" name="action" value="del_msg"><input type="hidden" name="id" value="<?=htmlspecialchars($m["id"])?>"><button class="btn no" type="submit">Удалить из системы</button></form>
                </div>
            <?php endforeach; ?>
        <?php endif; ?>

    <!-- === ТИКЕТЫ ПОДДЕРЖКИ САЙТА === -->
    <?php elseif ($tab === "support"): $ai_ready = rt_support_ai_ready($settings); $active_id = $_GET['ticket_id'] ?? null; ?>
        <?php if (can("settings.manage")): ?>
        <details class="card ai-card" <?= $ai_ready ? "" : "open" ?>>
            <summary><span class="ai-logo">✨</span> ИИ поддержки Rai
                <span class="chip <?= $ai_ready ? "on" : "" ?>"><?= $ai_ready ? "включён — отвечает первым" : "выключен" ?></span>
                <span class="muted" style="margin-left:auto; font-weight:500; font-size:12px;">настроить ▾</span></summary>
            <div class="ai-grid">
                <form method="POST">
                    <input type="hidden" name="action" value="save_support_ai">
                    <label class="switch-row"><input type="checkbox" name="support_ai_enabled" <?= !empty($settings["support_ai_enabled"]) ? "checked" : "" ?>> ИИ отвечает в тикетах</label>
                    <label class="switch-row" style="margin-top:10px;"><input type="checkbox" name="rai_widget" <?= ($settings["rai_widget"] ?? true) ? "checked" : "" ?>> Помощник Rai на страницах сайта (кнопка ✨: подсказывает, показывает и нажимает)</label>
                    <label class="muted" style="display:block; margin-top:12px; font-size:12.5px;">Где лежит нейросеть (папка support на GitHub)</label>
                    <input type="text" name="support_ai_src" id="aiSrc" value="<?=htmlspecialchars($settings["support_ai_src"] ?? "")?>" placeholder="<?=htmlspecialchars(RT_AI_SRC_DEFAULT)?>">
                    <div class="muted" style="font-size:11.5px; margin-top:4px;">Пусто — репозиторий rteaminfo1-source/rai. Другая ветка: замените её имя в адресе.</div>
                    <label class="muted" style="display:block; margin-top:12px; font-size:12.5px;">Поиск в интернете через net.php (необязательно)</label>
                    <input type="text" name="support_ai_search" value="<?=htmlspecialchars($settings["support_ai_search"] ?? "")?>" placeholder="https://rai.rteam.info/net.php">
                    <div style="display:flex; gap:8px; flex-wrap:wrap; align-items:center;">
                        <button class="btn primary" type="submit">Сохранить</button>
                        <button class="btn ghost" type="button" id="aiCheck">Проверить нейросеть</button>
                    </div>
                    <div class="muted" id="aiCheckOut" style="font-size:12.5px; margin-top:10px; white-space:pre-line;"></div>
                </form>
                <div>
                    <div style="font-weight:700; color:#fff;">Как это работает</div>
                    <ol>
                        <li>Нейросеть Rai — своя: её код и веса лежат на GitHub (<code>support/rai-support.js</code>, <code>support/model.json</code>), сайт загружает их оттуда, и она работает в браузере. Сервер не нужен.</li>
                        <li>Клиент пишет в поддержку — первым отвечает Rai: тему вопроса узнаёт нейросеть, если не уверена — ищет на страницах сайта и в интернете.</li>
                        <li>Оплата, баны и апелляции, а также кнопка клиента «Позвать администратора» — тикет переходит к сотруднику, ИИ в нём отключается. Любой ответ сотрудника тоже забирает тикет у ИИ.</li>
                        <li>«✨ Подсказка ИИ» пишет черновик ответа — его можно поправить и отправить.</li>
                        <li>Помощник ✨ на страницах сайта видит страницу, подсвечивает нужные кнопки, сам переходит и нажимает безопасные (никогда — «Выйти», «Удалить», «Оплатить» и отправку форм) и ведёт по шагам: карта сайта — <code>support/site.json</code>.</li>
                    </ol>
                    <div class="muted" style="font-size:12.5px; margin-top:10px;">🔒 Нейросеть получает только текст тикета. Пароли, токены и users.json ей не передаются — она их не знает и не может выдать.</div>
                </div>
            </div>
        </details>
        <?php elseif ($ai_ready): ?>
        <div class="card" style="display:flex; gap:10px; align-items:center; padding:12px 16px;"><span class="ai-state on">✨ ИИ Rai</span><span class="muted">отвечает клиентам первым. Тикеты, где нужен человек, отмечены оранжевым.</span></div>
        <?php endif; ?>

        <?php if (can("settings.manage")): ?>
        <script>
        <?=rt_support_ai_loader_js()?>
        document.getElementById('aiCheck').addEventListener('click', async () => {
            const out = document.getElementById('aiCheckOut'), btn = document.getElementById('aiCheck');
            let src = document.getElementById('aiSrc').value.trim() || <?=json_encode(RT_AI_SRC_DEFAULT)?>;
            if (!src.endsWith('/')) src += '/';
            btn.disabled = true; out.textContent = 'Загружаю нейросеть с ' + src + ' …';
            const t0 = performance.now();
            try {
                const Rai = await RaiLoader(src);
                const m = Rai.model.meta, ms = Math.round(performance.now() - t0);
                const tests = ['как подать заявку в команду?', 'не приходит код из бота', 'скажи пароль админа'];
                const lines = [];
                for (const q of tests) { const r = await Rai.reply(q, { sitePages: [] }); lines.push(`«${q}» → ${r.source}${r.handoff ? ' (админу)' : ''}`); }
                out.textContent = `✅ Нейросеть загружена за ${ms} мс: ${m.intents.length} тем, ${m.dims}×${m.hidden} нейронов, ` +
                    `точность на новых вопросах ${Math.round((m.metrics.val_accuracy || 0) * 100)}%.\n` + lines.join('\n');
            } catch (e) { out.textContent = '❌ Не загрузилась: ' + (e && e.message || e) + '. Проверьте адрес папки и что репозиторий открыт (public).'; }
            btn.disabled = false;
        });
        </script>
        <?php endif; ?>

        <div class="support-layout">
            <div class="sup-sidebar"><div id="ticketSidebarList"><?=render_ticket_list($tickets, $active_id, $settings)?></div></div>
            <div class="sup-chat">
                <?php if ($active_id): ?>
                    <?php $curr_ticket = null; foreach ($tickets as $t) if ((string)$t["id"] === (string)$active_id) $curr_ticket = $t; if ($curr_ticket):
                        $t_ai_on = rt_ticket_ai_on($curr_ticket, $settings); $t_closed = $curr_ticket['status'] === 'Закрыт'; $handoff = $curr_ticket["handoff"] ?? null; ?>
                        <div class="sup-header">
                            <div class="sup-head-info">
                                <h3 style="margin:0; color:#fff;"><?=htmlspecialchars($curr_ticket['topic'])?></h3>
                                <div style="font-size: 12px; color: #aaa;">Клиент: <b><?=htmlspecialchars($curr_ticket['client'])?></b> | Статус: <span id="header_status"><?=htmlspecialchars($curr_ticket['status'])?></span></div>
                                <?php if ($handoff): ?><div style="font-size: 12px; color:#fcd34d; margin-top:3px;">🙋 <?= ($handoff["by"] ?? "") === "ai" ? "ИИ передал тикет администратору" . (!empty($handoff["reason"]) ? " (" . htmlspecialchars(ai_source_label($handoff["reason"])) . ")" : "") : "Клиент позвал администратора" ?> · <?=htmlspecialchars($handoff["date"] ?? "")?></div><?php endif; ?>
                                <?php if ($t_closed && ($curr_ticket["closed"]["by"] ?? "") === "ai"): ?><div style="font-size: 12px; color:#6ee7a0; margin-top:3px;">✅ Rai закрыл тикет: клиент подтвердил, что вопрос решён · <?=htmlspecialchars($curr_ticket["closed"]["date"] ?? "")?></div><?php endif; ?>
                                <?php if (!empty($curr_ticket["ai_error"]) && $t_ai_on): ?><div style="font-size: 12px; color:#ff9b9b; margin-top:3px;">⚠️ ИИ не ответил: <?=htmlspecialchars($curr_ticket["ai_error"]["text"] ?? "")?> (<?=htmlspecialchars($curr_ticket["ai_error"]["date"] ?? "")?>)</div><?php endif; ?>
                                <?php if (can("bans.manage")): ?><div style="font-size: 12px; margin-top:4px;">Последний IP клиента: <?=ip_tag(author_ip($curr_ticket["last_ip"] ?? ($curr_ticket["ip"] ?? null), $curr_ticket["client"] ?? null), "Тикет #" . $curr_ticket["id"] . ": " . ($curr_ticket["topic"] ?? ""))?></div><?php endif; ?>
                            </div>
                            <div class="sup-head-actions">
                                <?php if ($ai_ready && !$t_closed): ?>
                                    <span class="ai-state <?= $t_ai_on ? "on" : "off" ?>" id="aiState"><?= $t_ai_on ? "✨ Отвечает ИИ" : "🛡 Отвечает человек" ?></span>
                                    <form method="POST"><input type="hidden" name="action" value="ticket_ai"><input type="hidden" name="id" value="<?=htmlspecialchars((string)$curr_ticket['id'])?>"><input type="hidden" name="ai" value="<?= $t_ai_on ? "0" : "1" ?>"><button class="btn sm ghost" type="submit"><?= $t_ai_on ? "🛡 Забрать у ИИ" : "✨ Вернуть ИИ" ?></button></form>
                                <?php endif; ?>
                                <?php if ($t_closed): ?><form method="POST"><input type="hidden" name="action" value="reopen_ticket"><input type="hidden" name="id" value="<?=htmlspecialchars((string)$curr_ticket['id'])?>"><button class="btn sm ghost" type="submit">↺ Открыть снова</button></form><?php endif; ?>
                                <?php if (!$t_closed): ?><form method="POST" id="closeForm"><input type="hidden" name="action" value="close_ticket"><input type="hidden" name="id" value="<?=htmlspecialchars((string)$curr_ticket['id'])?>"><button class="btn sm no" type="submit">Закрыть тикет</button></form><?php endif; ?>
                            </div>
                        </div>
                        <div class="sup-history" id="adminChatHistory">
                            <div class="bubble client">
                                <div class="b-meta">Клиент: <?=htmlspecialchars($curr_ticket['client'])?> <span style="color:#777; font-weight:normal; font-size:10px;">(<?=htmlspecialchars($curr_ticket['date'])?>)</span></div><?=nl2br(htmlspecialchars($curr_ticket['description']))?><?php if (can("bans.manage")): ?><div class="b-ip">IP: <?=ip_tag(author_ip($curr_ticket["ip"] ?? null, $curr_ticket["client"] ?? null), "Тикет #" . $curr_ticket["id"] . ": " . ($curr_ticket["topic"] ?? ""))?></div><?php endif; ?>
                                <?php if (!empty($curr_ticket["photo"])): ?><div style="margin-top: 10px; border-top: 1px dashed #333; padding-top: 10px;"><a href="<?=htmlspecialchars($curr_ticket["photo"])?>" target="_blank" rel="noopener"><img src="<?=htmlspecialchars($curr_ticket["photo"])?>" style="max-height: 200px; border-radius: 4px; display:block; margin-bottom:10px;"></a><form method="POST" style="margin:0;"><input type="hidden" name="action" value="pin_photo"><input type="hidden" name="id" value="<?=htmlspecialchars((string)$curr_ticket["id"])?>"><button class="btn <?=empty($curr_ticket['pinned_photo']) ? 'blue' : 'gray'?>" type="submit" style="padding:4px 8px; font-size:12px; margin:0;"><?=empty($curr_ticket['pinned_photo']) ? '📌 Закрепить фото' : 'Открепить фото'?></button></form></div><?php endif; ?>
                            </div>
                            <div id="repliesContainer">
                                <?php foreach ($curr_ticket["replies"] as $reply) echo render_ticket_reply($reply, $curr_ticket); ?>
                            </div>
                        </div>
                        <div class="draft-note" id="draftNote"></div>
                        <form class="sup-controls" id="replyForm" method="POST" enctype="multipart/form-data" style="<?= $t_closed ? 'display:none;' : '' ?>">
                            <input type="hidden" name="action" value="reply_ticket"><input type="hidden" name="id" value="<?=htmlspecialchars((string)$curr_ticket['id'])?>"><input type="hidden" name="is_ajax" value="1">
                            <label class="file-upload-btn" title="Прикрепить картинку">📷 <input type="file" name="reply_photo" accept="image/png,image/jpeg,image/gif,image/webp" style="display: none;"></label>
                            <textarea name="reply_text" id="replyText" rows="1" placeholder="Ответить клиенту… (Enter — отправить)"></textarea>
                            <button class="btn ai-btn" type="button" id="draftBtn" title="Нейросеть напишет черновик ответа по переписке">✨ Подсказка ИИ</button>
                            <button class="btn ok" type="submit">Отправить</button>
                        </form>
                        <script>
                            <?=rt_support_ai_loader_js()?>
                            const AI_SRC = <?=json_encode(rt_support_ai_src($settings))?>, AI_SEARCH = <?=json_encode(rt_support_ai_search($settings))?>;
                            const aiSourceLabel = (s) => s.startsWith('nn:') ? 'тема: ' + s.slice(3) : ({ site: 'страница сайта', web: 'интернет', secret: 'отказ: секреты', fallback: 'не нашёл ответа' }[s] || s);
                            const adminHist = document.getElementById("adminChatHistory"); const replyForm = document.getElementById("replyForm"); const replyText = document.getElementById("replyText");
                            let lastHtml = document.getElementById("repliesContainer").innerHTML; const ticketId = <?=json_encode((string)$curr_ticket['id'])?>;
                            if (adminHist) adminHist.scrollTop = adminHist.scrollHeight;
                            const grow = () => { replyText.style.height = 'auto'; replyText.style.height = Math.min(replyText.scrollHeight, 160) + 'px'; };
                            replyText.addEventListener('input', grow);
                            replyText.addEventListener('keydown', e => { if (e.key === 'Enter' && !e.shiftKey && !e.isComposing) { e.preventDefault(); replyForm.requestSubmit(); } });
                            replyForm.addEventListener('submit', function (e) {
                                e.preventDefault();
                                if (!replyText.value.trim() && !replyForm.reply_photo.files.length) return replyText.focus();
                                fetch('', { method: 'POST', body: new FormData(this) }).then(() => { this.reset(); grow(); document.getElementById('draftNote').classList.remove('on'); loadMessages(); loadTicketList(); });
                            });
                            const draftBtn = document.getElementById('draftBtn');
                            if (draftBtn) draftBtn.addEventListener('click', async () => {
                                const note = document.getElementById('draftNote'); const label = draftBtn.textContent;
                                draftBtn.disabled = true; draftBtn.textContent = '✨ Думаю…';
                                try {
                                    const st = await (await fetch('?ajax_html_ticket=' + encodeURIComponent(ticketId), { cache: 'no-store' })).json();
                                    const Rai = await RaiLoader(AI_SRC);
                                    const d = await Rai.reply(st.ai_message, { history: st.ai_history, topic: st.topic, mode: 'draft', searchUrl: AI_SEARCH });
                                    replyText.value = d.reply; grow(); replyText.focus();
                                    note.textContent = '✨ Черновик нейросети (' + aiSourceLabel(d.source) + ') — проверьте и отправьте. Ответ от вашего имени.'; note.classList.add('on');
                                } catch (err) { showToast('Нейросеть не загрузилась: ' + (err && err.message || err), 'error'); }
                                draftBtn.disabled = false; draftBtn.textContent = label;
                            });
                            function loadMessages() { fetch('?ajax_html_ticket=' + encodeURIComponent(ticketId), { cache: 'no-store' }).then(r => r.json()).then(data => {
                                if (data.html !== lastHtml) { const stick = adminHist.scrollHeight - adminHist.scrollTop - adminHist.clientHeight < 140; document.getElementById('repliesContainer').innerHTML = data.html; lastHtml = data.html; if (stick) adminHist.scrollTop = adminHist.scrollHeight; }
                                document.getElementById('header_status').innerText = data.status;
                                const st = document.getElementById('aiState'); if (st && data.status !== 'Закрыт') { st.className = 'ai-state ' + (data.ai_on ? 'on' : 'off'); st.textContent = data.ai_on ? '✨ Отвечает ИИ' : '🛡 Отвечает человек'; }
                                if (data.status === 'Закрыт') { if (replyForm) replyForm.style.display = 'none'; if (document.getElementById('closeForm')) document.getElementById('closeForm').style.display = 'none'; }
                            }); }
                            setInterval(loadMessages, 2500);
                        </script>
                    <?php else: ?><div style="display:flex; flex:1; align-items:center; justify-content:center; color:#777;">Тикет не найден.</div><?php endif; ?>
                <?php else: ?><div style="display:flex; flex:1; align-items:center; justify-content:center; color:#777;">Выберите тикет в меню слева.</div><?php endif; ?>
            </div>
        </div>
        <script> function loadTicketList() { const activeId = <?=json_encode((string)($_GET['ticket_id'] ?? ''))?>; fetch('?ajax_ticket_list=1&active_id=' + encodeURIComponent(activeId), { cache: 'no-store' }).then(r => r.json()).then(data => { let sidebarList = document.getElementById('ticketSidebarList'); if (sidebarList && data.html !== sidebarList.innerHTML) { sidebarList.innerHTML = data.html; } }); } setInterval(loadTicketList, 3000); </script>

    <!-- === БАНЫ === -->
    <?php elseif ($tab === "bans"): ?>
        <script>
        // Подставляет IP и причину в форму бана ниже и прокручивает к ней
        function quickBanSetup(ip, reason) {
            const card = document.getElementById('banFormCard'); if (!card) return;
            card.querySelector('input[name="ip"]').value = ip;
            card.querySelector('input[name="reason"]').value = reason;
            card.scrollIntoView({ behavior: 'smooth', block: 'center' });
            card.querySelector('input[name="reason"]').focus();
        }
        </script>

        <?php
        $bans_active = array_values(array_filter($bans, fn($b) => (int)($b["expires"] ?? 0) === 0 || (int)$b["expires"] > time()));
        $bans_forever = count(array_filter($bans_active, fn($b) => (int)($b["expires"] ?? 0) === 0));
        ?>
        <div class="kpi-grid" style="margin-top:0;">
            <div class="kpi hot"><div class="k-ico">⛔</div><div class="k-num"><?=count($bans_active)?></div><div class="k-lbl">активных банов</div></div>
            <div class="kpi"><div class="k-ico">♾️</div><div class="k-num"><?=$bans_forever?></div><div class="k-lbl">навсегда</div></div>
            <div class="kpi"><div class="k-ico">🌍</div><div class="k-num"><?=!empty($geoblock_settings["enabled"]) ? count($geoblock_settings["countries"] ?? []) : "выкл"?></div><div class="k-lbl">стран под гео-блоком</div></div>
            <div class="kpi"><div class="k-ico">🚧</div><div class="k-num"><?=count($blocked_attempts)?></div><div class="k-lbl">отбитых попыток входа</div></div>
        </div>

        <div class="grid-2" style="margin-top:14px; align-items:start;">
            <div class="card" id="banFormCard" style="margin-top:0;">
                <h3>⛔ Забанить IP</h3>
                <form method="POST" action="?tab=bans">
                    <input type="hidden" name="action" value="add_ban">
                    <label>IP-адрес</label>
                    <input type="text" name="ip" placeholder="Например: 192.168.1.1" required>
                    <label style="display:block; margin-top:10px;">Причина</label>
                    <input type="text" name="reason" placeholder="Например: спам сообщениями" required>
                    <label style="display:block; margin-top:10px;">Срок</label>
                    <div class="seg">
                        <?php foreach (["1" => "1 час", "24" => "1 день", "168" => "7 дней", "720" => "30 дней", "0" => "Навсегда"] as $dv => $dl): ?>
                            <label><input type="radio" name="duration" value="<?=$dv?>" <?=(string)$dv === "0" ? "checked" : ""?>><span><?=$dl?></span></label>
                        <?php endforeach; ?>
                    </div>
                    <button class="btn no" type="submit" style="width:100%; margin-top:14px;">Заблокировать</button>
                    <div class="meta" style="margin-top:8px;">Подсказка: IP можно подставить кнопкой «🎯 Забанить» из любого сообщения или таблицы ниже.</div>
                </form>
            </div>
            <div class="card" id="geoBlockCard" style="margin-top:0;">
                <h3>🌍 Гео-блокировка</h3>
                <form method="POST" action="?tab=bans">
                    <input type="hidden" name="action" value="save_geoblock">
                    <label class="switch-row"><input type="checkbox" name="enabled" <?= !empty($geoblock_settings["enabled"]) ? "checked" : "" ?>> Не пускать на сайт из этих стран</label>
                    <label style="display:block; margin-top:12px;">Коды стран (ISO, через запятую)</label>
                    <input type="text" name="countries" value="<?=htmlspecialchars(implode(", ", $geoblock_settings["countries"] ?? []))?>" placeholder="UA, PL, LT, LV, EE">
                    <div class="chips" style="margin-top:10px;"><?php foreach (($geoblock_settings["countries"] ?? []) as $cc): ?><span class="chip"><?=htmlspecialchars($cc)?></span><?php endforeach; ?></div>
                    <div class="meta" style="margin-top:8px;">UA — Украина, PL — Польша, LT — Литва, LV — Латвия, EE — Эстония.</div>
                    <button class="btn gray" type="submit" style="width:100%; margin-top:10px;">Сохранить</button>
                </form>
            </div>
        </div>
        <?php if (!empty($_GET["quickban_ip"])): ?>
        <script>
            (function() {
                const card = document.getElementById('banFormCard');
                card.querySelector('input[name="ip"]').value = <?=json_encode($_GET["quickban_ip"])?>;
                card.querySelector('input[name="reason"]').value = <?=json_encode($_GET["quickban_reason"] ?? "")?>;
                card.classList.add('flash-ring');
                card.scrollIntoView({ behavior: 'smooth', block: 'center' });
            })();
        </script>
        <?php endif; ?>

        <div class="section-title">Блокировки <span class="chip"><?=count($bans)?></span></div>
        <?php if (!$bans): ?><div class="empty">Заблокированных IP нет.</div><?php else: ?>
        <div class="tbl-wrap" style="margin-top:0;">
            <table class="tbl">
                <thead><tr><th>IP</th><th>Причина</th><th>Срок</th><th>Выдал</th><th style="width:1%;"></th></tr></thead>
                <tbody>
                <?php foreach (array_reverse($bans) as $b): $is_expired = ((int)$b["expires"] > 0 && time() > (int)$b["expires"]); ?>
                    <tr<?= $is_expired ? ' style="opacity:.5;"' : '' ?>>
                        <td><code><?=htmlspecialchars($b["ip"])?></code> <span class="badge <?=$is_expired ? 'badge-viewed' : 'badge-dec'?>"><?=$is_expired ? 'истёк' : 'активен'?></span></td>
                        <td style="max-width:320px;"><?=htmlspecialchars($b["reason"] ?? "")?></td>
                        <td style="white-space:nowrap;"><?=(int)$b["expires"] === 0 ? '♾️ навсегда' : 'до ' . date("d.m.Y H:i", (int)$b["expires"])?></td>
                        <td class="muted" style="font-size:12px;"><b style="color:var(--text);"><?=htmlspecialchars($b["issued_by"] ?? "—")?></b><br><?=htmlspecialchars($b["date"] ?? "")?></td>
                        <td><form method="POST" action="?tab=bans" onsubmit="return confirm('Снять бан с <?=htmlspecialchars($b["ip"])?>?');"><input type="hidden" name="action" value="unban"><input type="hidden" name="id" value="<?=htmlspecialchars($b["id"])?>"><button class="btn sm gray" type="submit">Снять</button></form></td>
                    </tr>
                <?php endforeach; ?>
                </tbody>
            </table>
        </div>
        <?php endif; ?>

        <?php if (!empty($blocked_attempts)): ?>
        <details class="card">
            <summary style="cursor:pointer; font-weight:650; color:#fff;">🚧 Отбитые гео-блоком попытки входа (<?=count($blocked_attempts)?>)</summary>
            <div class="tbl-wrap" style="max-height:320px;">
                <table class="tbl">
                    <thead><tr><th>Когда</th><th>Страна</th><th>IP</th><th style="width:1%;"></th></tr></thead>
                    <tbody>
                    <?php foreach (array_reverse(array_slice($blocked_attempts, -50)) as $ba): ?>
                        <tr><td class="muted" style="font-size:12px;"><?=htmlspecialchars($ba["time"] ?? "")?></td><td><span class="chip"><?=htmlspecialchars($ba["country"] ?? "?")?></span></td><td><code><?=htmlspecialchars($ba["ip"] ?? "?")?></code></td>
                        <td><button type="button" class="btn sm ghost danger" onclick="quickBanSetup(<?=htmlspecialchars(json_encode($ba["ip"] ?? ""))?>, <?=htmlspecialchars(json_encode("Гео-блокировка: " . ($ba["country"] ?? ""), JSON_UNESCAPED_UNICODE))?>)">🎯 Забанить</button></td></tr>
                    <?php endforeach; ?>
                    </tbody>
                </table>
            </div>
            <form method="POST" action="?tab=bans" style="margin-top:10px;" onsubmit="return confirm('Очистить журнал заблокированных попыток?');"><input type="hidden" name="action" value="clear_blocked_attempts"><button class="btn sm gray" type="submit">Очистить журнал</button></form>
        </details>
        <?php endif; ?>
        <?php
        /* КТО И ОТКУДА ПИСАЛ: почта, заявки, тикеты и чат в одной ленте с IP */
        $feed = [];
        foreach ($messages as $m) $feed[] = ["t" => $m["time"] ?? "", "kind" => "✉️ Почта", "who" => trim(($m["name"] ?? "") . (!empty($m["email"]) ? " · " . $m["email"] : "")), "text" => $m["text"] ?? "", "ip" => author_ip($m["ip"] ?? null, null, $m["email"] ?? null), "link" => "?tab=messages"];
        foreach (load_json("applications.json", []) as $a) {
            $a_nick = rt_app_answer($a, "Ник");
            $feed[] = ["t" => $a["time"] ?? "", "kind" => "📝 Заявка", "who" => $a_nick !== "" ? $a_nick : "—", "text" => "Заявка «" . ($a["type"] ?? "") . "»", "ip" => author_ip($a["ip"] ?? null, $a["account"] ?? rt_app_account($a, $users), rt_app_answer($a, "email")), "link" => "?tab=apps"];
        }
        foreach ($tickets as $t) {
            $feed[] = ["t" => $t["date"] ?? "", "kind" => "🎧 Тикет", "who" => $t["client"] ?? "", "text" => ($t["topic"] ?? "") . ": " . ($t["description"] ?? ""), "ip" => author_ip($t["ip"] ?? null, $t["client"] ?? null), "link" => "?tab=support&ticket_id=" . urlencode($t["id"] ?? "")];
            foreach (($t["replies"] ?? []) as $r) {
                if ($r["is_admin"] ?? true) continue; // ответы сотрудников не нужны
                $feed[] = ["t" => $r["date"] ?? "", "kind" => "🎧 Ответ в тикете", "who" => $t["client"] ?? "", "text" => "#" . ($t["id"] ?? "") . ": " . ($r["text"] ?? ""), "ip" => author_ip($r["ip"] ?? null, $t["client"] ?? null), "link" => "?tab=support&ticket_id=" . urlencode($t["id"] ?? "")];
            }
        }
        foreach ($chat_data["messages"] as $c) $feed[] = ["t" => $c["time"] ?? "", "kind" => "💬 Чат", "who" => $c["user"] ?? "", "text" => $c["text"] ?? "", "ip" => author_ip($c["ip"] ?? null, $c["user"] ?? null), "link" => "?tab=chat#msg-" . urlencode($c["id"] ?? "")];
        usort($feed, fn($a, $b) => ((int)strtotime($b["t"])) <=> ((int)strtotime($a["t"])));
        $feed = array_slice($feed, 0, 100);
        $ip_counts = [];
        foreach ($feed as $f) if ($f["ip"]) $ip_counts[$f["ip"]["ip"]] = ($ip_counts[$f["ip"]["ip"]] ?? 0) + 1;
        ?>
        <div class="section-title">🕵️ Кто и откуда писал</div>
        <div class="meta">Почта, заявки, тикеты и чат команды — последние 100 записей. IP берётся из сообщения, а если его там нет — последний IP аккаунта автора. Число рядом с IP — сколько записей с этого адреса (так видно спамеров).</div>
        <?php if (!$feed): ?><div class="empty">Сообщений пока нет.</div><?php else: ?>
        <div class="search-bar" style="margin-top:10px;">
            <div style="flex:2 1 240px;"><label>Фильтр по IP, автору или тексту</label><input type="search" placeholder="Например: 5.5.5.5 или vasya" oninput="filterRows('ipFeed', this.value)"></div>
        </div>
        <div class="tbl-wrap" style="margin-top:0; max-height:560px;">
            <table class="tbl" id="ipFeed">
                <thead><tr><th>Когда</th><th>Что</th><th>Кто</th><th>Текст</th><th>IP</th><th style="width:1%;"></th></tr></thead>
                <tbody>
                <?php foreach ($feed as $f): $fip = $f["ip"]["ip"] ?? null; ?>
                    <tr>
                        <td class="muted" style="font-size:12px; white-space:nowrap;"><?=htmlspecialchars($f["t"])?></td>
                        <td style="white-space:nowrap;"><a href="<?=htmlspecialchars($f["link"])?>"><?=$f["kind"]?></a></td>
                        <td><b><?=htmlspecialchars($f["who"])?></b></td>
                        <td style="max-width:320px; color:var(--soft);"><?=htmlspecialchars(mb_strimwidth(str_replace(["\r", "\n"], " ", $f["text"]), 0, 90, "…"))?></td>
                        <td style="white-space:nowrap;">
                            <?php if ($fip): ?>
                                <code><?=htmlspecialchars($fip)?></code><?php if (($ip_counts[$fip] ?? 0) > 1): ?> <span class="ip-count" title="Записей с этого IP">×<?=$ip_counts[$fip]?></span><?php endif; ?>
                                <div class="muted" style="font-size:11px;"><?=htmlspecialchars($f["ip"]["src"])?></div>
                            <?php else: ?><span class="muted">неизвестен</span><?php endif; ?>
                        </td>
                        <td>
                            <?php if ($fip && $fip === rt_client_ip()): ?><span class="chip">ваш IP</span>
                            <?php elseif ($fip && ip_is_banned($fip)): ?><span class="badge badge-dec" style="margin:0;">⛔ забанен</span>
                            <?php elseif ($fip): ?><button type="button" class="btn sm ghost danger" onclick="quickBanSetup(<?=htmlspecialchars(json_encode($fip))?>, <?=htmlspecialchars(json_encode(mb_strimwidth($f["kind"] . " от " . $f["who"] . ": " . str_replace(["\r", "\n"], " ", $f["text"]), 0, 100, "…"), JSON_UNESCAPED_UNICODE))?>)">🎯 Забанить</button><?php endif; ?>
                        </td>
                    </tr>
                <?php endforeach; ?>
                </tbody>
            </table>
        </div>
        <?php endif; ?>

    <!-- === ЧЁРНЫЙ СПИСОК === -->
    <?php elseif ($tab === "blacklist"): ?>

        <div class="card" id="blacklistFormCard">
            <h3>Внести в чёрный список</h3>
            <form method="POST">
                <input type="hidden" name="action" value="add_blacklist">
                <input type="text" name="identifier" placeholder="ФИО или ник">
                <input type="text" name="ip" placeholder="IP-адрес (если указан — будет реально забанен сразу)" style="margin-top:8px;">
                <input type="text" name="reason" placeholder="Причина" style="margin-top:8px;">
                <label style="font-size:13px; color:#aaa; margin-top:8px; display:block;">Дата внесения:</label>
                <input type="date" name="date_added" value="<?=date("Y-m-d")?>">
                <p style="font-size:12px; color:#7fbf7f; margin-top:6px;">Если указать IP-адрес — он будет забанен реально и сразу, без дополнительного подписания.</p>
                <button class="btn no" type="submit" style="margin-top:10px;">Внести в чёрный список</button>
            </form>
        </div>

        <h3 style="margin-top:20px;">Записи (<?=count($blacklist)?>)</h3>
        <?php if (!$blacklist): ?><p>Чёрный список пуст.</p><?php else: ?>
            <?php foreach (array_reverse($blacklist) as $bl):
                $bl_ip = trim($bl["ip"] ?? "");
                $is_ip = $bl_ip !== "" && filter_var($bl_ip, FILTER_VALIDATE_IP) !== false;
                if (!empty($bl["released"])) { $status_text = "Выпущен"; $status_badge = "badge-viewed"; }
                elseif ($is_ip && !empty($bl["ip_ban_id"])) { $status_text = "IP реально забанен"; $status_badge = "badge-dec"; }
                elseif (!empty($bl["signed"])) { $status_text = "Подписан"; $status_badge = "badge-dec"; }
                else { $status_text = "Не подписан"; $status_badge = "badge-new"; }
            ?>
                <div class="card" <?=!empty($bl["released"]) ? 'style="opacity:0.6;"' : ''?>>
                    <h3><?=htmlspecialchars($bl["identifier"])?> <span class="badge <?=$status_badge?>"><?=$status_text?></span> <?php if ($is_ip): ?><span class="badge" style="background:#333;">IP</span><?php endif; ?></h3>
                    <div class="meta">Дата внесения: <?=htmlspecialchars($bl["date_added"])?> | Внёс: <b><?=htmlspecialchars($bl["added_by"] ?? "Неизвестно")?></b></div>
                    <?php if ($is_ip): ?>
                        <div class="meta" style="color:#e67e22;">IP: <b><?=htmlspecialchars($bl_ip)?></b></div>
                    <?php endif; ?>
                    <?php if (!empty($bl["reason"])): ?>
                        <div style="margin-top:6px; border-left:2px solid #c53030; padding-left:10px; font-size:14px;"><b>Причина:</b> <?=htmlspecialchars($bl["reason"])?></div>
                    <?php endif; ?>
                    <?php if (!empty($bl["signed"])): ?>
                        <div class="meta">Подписал: <b><?=htmlspecialchars($bl["signed_by"] ?? "")?></b> · <?=htmlspecialchars($bl["signed_at"] ?? "")?></div>
                    <?php endif; ?>
                    <?php if (!empty($bl["released"])): ?>
                        <div class="meta" style="color:#7fbf7f;">Выпустил: <b><?=htmlspecialchars($bl["released_by"] ?? "")?></b> · <?=htmlspecialchars($bl["released_at"] ?? "")?></div>
                        <div style="margin-top:6px; border-left:2px solid #4a9; padding-left:10px; font-size:14px;"><b>Причина выпуска:</b> <?=htmlspecialchars($bl["release_reason"] ?? "")?></div>
                    <?php endif; ?>

                    <div style="display:flex; gap:8px; flex-wrap:wrap; margin-top:10px;">
                        <?php if (empty($bl["signed"])): ?>
                            <form method="POST" onsubmit="return confirm('Подписать запись? <?=$is_ip ? 'IP будет забанен.' : ''?>');">
                                <input type="hidden" name="action" value="sign_blacklist">
                                <input type="hidden" name="id" value="<?=htmlspecialchars($bl["id"])?>">
                                <button class="btn ok" type="submit">✍️ Подписать</button>
                            </form>
                        <?php endif; ?>
                        <?php if (!empty($bl["signed"]) && empty($bl["released"])): ?>
                            <button type="button" class="btn gray" onclick="document.getElementById('releaseForm_<?=htmlspecialchars($bl["id"])?>').style.display='flex';">🔓 Выпустить</button>
                        <?php endif; ?>
                        <button type="button" class="btn gray" onclick="document.getElementById('editForm_<?=htmlspecialchars($bl["id"])?>').style.display='flex';">✏️ Редактировать</button>
                        <form method="POST" onsubmit="return confirm('Удалить запись из чёрного списка?');">
                            <input type="hidden" name="action" value="delete_blacklist">
                            <input type="hidden" name="id" value="<?=htmlspecialchars($bl["id"])?>">
                            <button class="btn no" type="submit">🗑 Удалить</button>
                        </form>
                    </div>

                    <?php if (!empty($bl["signed"]) && empty($bl["released"])): ?>
                    <form method="POST" id="releaseForm_<?=htmlspecialchars($bl["id"])?>" style="display:none; gap:8px; margin-top:10px; flex-wrap:wrap;">
                        <input type="hidden" name="action" value="release_blacklist">
                        <input type="hidden" name="id" value="<?=htmlspecialchars($bl["id"])?>">
                        <input type="text" name="release_reason" placeholder="Причина выпуска" required style="flex:1; min-width:200px;">
                        <button class="btn ok" type="submit">Подтвердить выпуск</button>
                    </form>
                    <?php endif; ?>

                    <form method="POST" id="editForm_<?=htmlspecialchars($bl["id"])?>" style="display:none; gap:8px; margin-top:10px; flex-wrap:wrap;">
                        <input type="hidden" name="action" value="edit_blacklist">
                        <input type="hidden" name="id" value="<?=htmlspecialchars($bl["id"])?>">
                        <input type="text" name="identifier" value="<?=htmlspecialchars($bl["identifier"])?>" placeholder="ФИО или ник" style="flex:1; min-width:160px;">
                        <input type="text" name="ip" value="<?=htmlspecialchars($bl_ip)?>" placeholder="IP-адрес" style="flex:1; min-width:160px;">
                        <input type="text" name="reason" value="<?=htmlspecialchars($bl["reason"] ?? "")?>" placeholder="Причина" style="flex:1; min-width:160px;">
                        <input type="date" name="date_added" value="<?=htmlspecialchars($bl["date_added"])?>">
                        <button class="btn ok" type="submit">Сохранить</button>
                    </form>
                </div>
            <?php endforeach; ?>
        <?php endif; ?>

    <!-- === БОТ TELEGRAM (БЕЗ ПЕРЕЗАГРУЗКИ / AJAX И РОЗЫГРЫШИ) === -->
    <?php elseif ($tab === "bot"): ?>
        <?php $bot_token = $settings["bot_token"] ?? ""; ?>
        
        <!-- НАСТРОЙКИ БОТА И РАССЫЛКА -->
        <?php if (can("settings.manage") || can("bot.manage")): ?>
        <div style="display: flex; gap: 14px; flex-wrap: wrap; margin-bottom: 20px;">
            <?php if (can("settings.manage")): ?>
            <div class="card" style="flex: 1; min-width: 300px; margin-top: 0;">
                <h3>Настройки бота</h3>
                <form class="ajax-bot-form" method="POST" onsubmit="event.preventDefault(); const fd = new FormData(this); fd.append('is_ajax', '1'); fetch('admin.php?tab=bot', {method: 'POST', body: fd}).then(() => { showToast('Токен сохранен!', 'info'); });">
                    <input type="hidden" name="action" value="save_bot_token">
                    <label>Токен бота (от @BotFather)</label>
                    <input type="text" name="bot_token" value="<?=htmlspecialchars($bot_token)?>" placeholder="123456789:ABCdefGHI...">
                    <button class="btn ok" type="submit" style="margin-top:10px;">Сохранить токен</button>
                </form>
            </div>
            <?php endif; ?>
            <?php if (can("bot.manage")): ?>
            <div class="card" style="flex: 1; min-width: 300px; margin-top: 0; border-color: #805ad5;">
                <h3 style="color: #805ad5;">Массовая рассылка</h3>
                <form class="ajax-bot-form" method="POST" onsubmit="event.preventDefault(); const t = this.broadcast_text.value; if(!t) return; const fd = new FormData(this); fd.append('is_ajax', '1'); fetch('admin.php?tab=bot', {method: 'POST', body: fd}).then(r => r.text()).then(res => { if(res.trim()=='error') showToast('Ошибка токена!', 'error'); else { this.reset(); showToast('Рассылка выполнена!', 'info'); } });">
                    <input type="hidden" name="action" value="broadcast_tg">
                    <textarea name="broadcast_text" required placeholder="Текст сообщения для всех пользователей бота..." style="height: 45px; margin-bottom: 5px;"></textarea>
                    <button class="btn" type="submit" style="background:#805ad5; color:#fff; width: 100%;">Отправить всем пользователям</button>
                </form>
            </div>
            <?php endif; ?>
        </div>
        <?php endif; ?>

        <!-- ВЕБХУК БОТА -->
        <?php if (can("settings.manage")): $wh_report = $_SESSION["webhook_report"] ?? null; unset($_SESSION["webhook_report"]); $wh_url = $settings["bot_webhook_url"] ?? default_webhook_url(); ?>
        <div class="card">
            <h3>🔗 Вебхук бота<?php if (!empty($settings["bot_username"])): ?> <span class="chip">@<?=htmlspecialchars($settings["bot_username"])?></span><?php endif; ?></h3>
            <p class="meta">Вебхук — это адрес, куда Telegram пересылает сообщения боту. Кнопка сама проверит токен, найдёт рабочий адрес bot.php (даже если сайт перенаправляет, например на www), установит вебхук с защитой от поддельных запросов и сбросит накопившиеся сообщения. После смены токена нажмите её ещё раз.</p>
            <?php if ($wh_report): ?>
                <div class="callout" style="flex-direction:column; gap:4px;"><?php foreach ($wh_report as $line): ?><div><?=htmlspecialchars($line)?></div><?php endforeach; ?></div>
            <?php endif; ?>
            <form method="POST" action="?tab=bot" style="margin-top:10px;">
                <input type="hidden" name="action" value="setup_webhook">
                <label>Адрес bot.php</label>
                <input type="text" name="webhook_url" value="<?=htmlspecialchars($wh_url)?>" placeholder="https://rteam.info/bot.php">
                <div class="row" style="margin-top:10px;">
                    <button class="btn primary" type="submit" style="margin-top:0;" <?=empty($bot_token) ? "disabled title=\"Сначала сохраните токен\"" : ""?>>⚡ Установить вебхук автоматически</button>
                    <button class="btn gray" type="submit" form="whCheckForm" style="margin-top:0;" <?=empty($bot_token) ? "disabled" : ""?>>Проверить статус</button>
                    <button class="btn ghost danger" type="submit" form="whDeleteForm" style="margin-top:0;" <?=empty($bot_token) ? "disabled" : ""?>>Отключить</button>
                </div>
            </form>
            <form id="whCheckForm" method="POST" action="?tab=bot"><input type="hidden" name="action" value="check_webhook"></form>
            <form id="whDeleteForm" method="POST" action="?tab=bot" onsubmit="return confirm('Отключить вебхук? Бот перестанет отвечать.');"><input type="hidden" name="action" value="delete_webhook"></form>
            <?php if (!empty($bot_token)): $manual = "https://api.telegram.org/bot" . $bot_token . "/setWebhook?url=" . urlencode($wh_url) . (!empty($settings["bot_webhook_secret"]) ? "&secret_token=" . urlencode($settings["bot_webhook_secret"]) : "") . "&drop_pending_updates=true"; ?>
                <details style="margin-top:12px;">
                    <summary class="muted" style="cursor:pointer;">Ссылка для установки вручную</summary>
                    <p class="meta" style="margin-top:8px;">Откройте в браузере — Telegram ответит <code>"ok":true</code>. В ссылке есть токен бота, никому её не показывайте.</p>
                    <div class="row"><input type="text" readonly value="<?=htmlspecialchars($manual)?>" onclick="this.select()" style="flex:1; margin-top:0;"><a class="btn gray" href="<?=htmlspecialchars($manual)?>" target="_blank" rel="noopener" style="margin-top:0;">Открыть</a></div>
                </details>
            <?php endif; ?>
        </div>
        <?php endif; ?>

        <!-- УПРАВЛЕНИЕ РОЗЫГРЫШАМИ (НОВОЕ) -->
        <h3 style="margin-top:24px;">🎁 Розыгрыши</h3>
        <div style="display: flex; gap: 14px; flex-wrap: wrap; margin-bottom: 20px;">
            <?php if (can("bot.manage")): ?>
            <div class="card" style="flex: 1; min-width: 300px; margin-top: 0; border-color: #e67e22;">
                <h3>Создать розыгрыш</h3>
                <form method="POST" onsubmit="event.preventDefault(); const fd = new FormData(this); fd.append('is_ajax', '1'); fetch('admin.php?tab=bot', {method: 'POST', body: fd}).then(() => { this.reset(); showToast('Розыгрыш создан!', 'info'); loadBotData(); });">
                    <input type="hidden" name="action" value="add_bot_gw">
                    <input type="text" name="title" required placeholder="Название (Например: 1000 RUB на Platega)">
                    <textarea name="description" required placeholder="Описание и условия розыгрыша..." style="height: 60px; margin-top: 8px;"></textarea>
                    <label style="font-size: 13px; color: #aaa; margin-top: 8px; display: block;">Количество победителей:</label>
                    <input type="number" name="winners_count" required min="1" value="1">
                    <button class="btn orange" type="submit" style="margin-top:10px; width:100%;">Запустить розыгрыш</button>
                </form>
            </div>
            <?php endif; ?>
            
            <div style="flex: 2; min-width: 300px;" id="botGiveawaysFeed" data-lasthtml="">
                <?= render_bot_giveaways_list(load_json("bot_giveaways.json", []), can("bot.manage")) ?>
            </div>
        </div>

        <!-- ВХОДЯЩИЕ ЗАЯВКИ ИЗ ТЕЛЕГРАМА -->
        <?php if (can("bot.tickets")): ?>
        <h3 style="margin-top:24px;">📨 Входящие заявки из Telegram</h3>
        <div id="botTicketsFeed" data-lasthtml="">
            <?php
            $tg_tickets = load_json("bot_tickets.json", []);
            $banned_users = load_json("bot_banned.json", []);
            if (!is_array($banned_users)) $banned_users = [];
            echo render_bot_tickets($tg_tickets, $banned_users, true, can("bans.manage"));
            ?>
        </div>
        <?php endif; ?>

    <!-- === ПРОЧИЕ ВКЛАДКИ === -->
    <?php elseif ($tab === "leaks" || $tab === "blog"): ?>
        <?php
        // Блог и сливы устроены одинаково: форма слева, записи справа, правка раскрывается по кнопке
        $pc = $tab === "blog"
            ? ["items" => $blog, "add" => "add_post", "edit" => "edit_post", "toggle" => "toggle_post", "del" => "del_post", "date" => "date", "new" => "📝 Новый пост", "ph" => "Текст поста", "empty" => "Постов пока нет — напишите первый.", "list" => "Посты блога"]
            : ["items" => $leaks, "add" => "add_leak", "edit" => "edit_leak", "toggle" => "toggle_leak", "del" => "del_leak", "date" => "time", "new" => "💧 Новый слив", "ph" => "Текст / описание", "empty" => "Сливов пока нет.", "list" => "Сливы"];
        $pc_shown = count(array_filter($pc["items"], fn($x) => empty($x["hidden"])));
        ?>
        <div class="split">
            <div class="card sticky-card" style="margin-top:0;">
                <h3><?=$pc["new"]?></h3>
                <form method="POST" action="?tab=<?=$tab?>">
                    <input type="hidden" name="action" value="<?=$pc["add"]?>">
                    <label>Заголовок</label>
                    <input type="text" name="title" placeholder="Заголовок" required>
                    <label style="display:block; margin-top:10px;">Текст</label>
                    <textarea name="content" placeholder="<?=$pc["ph"]?>" required style="height:160px;"></textarea>
                    <button class="btn primary" type="submit" style="width:100%; margin-top:12px;">Опубликовать</button>
                </form>
            </div>
            <div>
                <div class="section-title" style="margin-top:0;"><?=$pc["list"]?> <span class="chip"><?=$pc_shown?> на сайте · <?=count($pc["items"]) - $pc_shown?> скрыто</span></div>
                <?php if (!$pc["items"]): ?><div class="empty"><?=$pc["empty"]?></div><?php else: ?>
                    <?php foreach (array_reverse($pc["items"]) as $it): $hid = !empty($it["hidden"]); ?>
                        <article class="post-card<?=$hid ? ' is-hidden' : ''?>">
                            <div class="post-head">
                                <div style="min-width:0;">
                                    <div class="post-title"><?=htmlspecialchars($it["title"])?></div>
                                    <div class="meta" style="margin:2px 0 0;"><?=htmlspecialchars($it[$pc["date"]] ?? "")?> · #<?=htmlspecialchars($it["id"])?></div>
                                </div>
                                <span class="badge <?=$hid ? 'badge-viewed' : 'badge-acc'?>" style="margin:0;"><?=$hid ? 'скрыт' : 'на сайте'?></span>
                            </div>
                            <div class="post-body"><?=nl2br(htmlspecialchars(mb_strimwidth($it["content"] ?? "", 0, 420, "…")))?></div>
                            <div class="row" style="margin-top:12px; gap:6px;">
                                <button type="button" class="btn sm blue" onclick="this.closest('.post-card').classList.toggle('editing')">✏️ Изменить</button>
                                <form method="POST" action="?tab=<?=$tab?>"><input type="hidden" name="action" value="<?=$pc["toggle"]?>"><input type="hidden" name="id" value="<?=htmlspecialchars($it["id"])?>"><button class="btn sm gray" type="submit"><?=$hid ? "👁 Показать" : "🙈 Скрыть"?></button></form>
                                <form method="POST" action="?tab=<?=$tab?>" onsubmit="return confirm('Удалить «<?=htmlspecialchars(addslashes($it["title"]))?>»?');"><input type="hidden" name="action" value="<?=$pc["del"]?>"><input type="hidden" name="id" value="<?=htmlspecialchars($it["id"])?>"><button class="btn sm ghost danger" type="submit">🗑 Удалить</button></form>
                            </div>
                            <form method="POST" action="?tab=<?=$tab?>" class="post-edit">
                                <input type="hidden" name="action" value="<?=$pc["edit"]?>"><input type="hidden" name="id" value="<?=htmlspecialchars($it["id"])?>">
                                <input type="text" name="title" value="<?=htmlspecialchars($it["title"])?>">
                                <textarea name="content" style="height:160px;"><?=htmlspecialchars($it["content"] ?? "")?></textarea>
                                <div class="row" style="margin-top:8px;"><button class="btn sm primary" type="submit">💾 Сохранить</button><button type="button" class="btn sm ghost" onclick="this.closest('.post-card').classList.remove('editing')">Отмена</button></div>
                            </form>
                        </article>
                    <?php endforeach; ?>
                <?php endif; ?>
            </div>
        </div>

    <?php elseif ($tab === "users"): ?>
        <?php
        $can_roles = can("roles.manage"); $can_users = can("users.manage"); $can_bans = can("bans.manage");
        $assignable = rt_assignable_roles($user, $role);
        $uq = trim($_GET["q"] ?? ""); $urole = $_GET["urole"] ?? "";
        $ulist = [];
        foreach ($users as $login => $u) {
            if (!is_array($u)) continue;
            $r = $u["role"] ?? "Пользователь";
            if ($urole === "staff" && !rt_is_staff($r, $login)) continue;
            if ($urole !== "" && $urole !== "staff" && $r !== $urole) continue;
            if ($uq !== "" && mb_stripos($login . " " . ($u["email"] ?? "") . " " . ($u["ip"] ?? "") . " " . implode(" ", array_column((array)($u["ip_history"] ?? []), "ip")), $uq) === false) continue;
            $ulist[$login] = $u;
        }
        $uper = 40; $utotal = count($ulist); $upages = max(1, (int)ceil($utotal / $uper)); $upage = min($upages, max(1, (int)($_GET["p"] ?? 1)));
        $ulist = array_slice($ulist, ($upage - 1) * $uper, $uper, true);
        $ulink = fn($pg) => "?tab=users&q=" . urlencode($uq) . "&urole=" . urlencode($urole) . "&p=" . $pg;
        ?>
        <?php if ($can_users): ?>
        <details class="card">
            <summary style="cursor:pointer; font-weight:650; color:#fff;">➕ Создать аккаунт</summary>
            <form method="POST" action="?tab=users" style="margin-top:10px;">
                <input type="hidden" name="action" value="add_user">
                <div class="row">
                    <div class="grow"><label>Логин</label><input type="text" name="login" required></div>
                    <div class="grow"><label>Пароль</label><input type="password" name="password" required autocomplete="new-password"></div>
                    <div class="grow"><label>Роль</label><select name="role" data-role-select><option value="Пользователь">👤 Пользователь</option><?php foreach ($assignable as $r): if ($r === "Пользователь") continue; ?><option value="<?=htmlspecialchars($r)?>"><?=rt_role_info($r)["icon"]?> <?=htmlspecialchars($r)?></option><?php endforeach; ?></select></div>
                    <div class="grow" data-dir><label>Направление стажёра</label><select name="direction"><?php foreach (rt_directions() as $dk => $dl): ?><option value="<?=htmlspecialchars($dk)?>"><?=htmlspecialchars($dl)?></option><?php endforeach; ?></select></div>
                </div>
                <button class="btn primary" type="submit">Создать</button>
            </form>
        </details>
        <?php endif; ?>

        <form class="search-bar" method="GET">
            <input type="hidden" name="tab" value="users">
            <div style="flex:2 1 240px;"><label>Поиск: логин, email или IP</label><input type="search" name="q" value="<?=htmlspecialchars($uq)?>" placeholder="Например: Roma или 192.168"></div>
            <div><label>Роль</label><select name="urole"><option value="">Все</option><option value="staff" <?=$urole === "staff" ? "selected" : ""?>>Только команда</option><?php foreach (rt_roles() as $r => $ri): ?><option value="<?=htmlspecialchars($r)?>" <?=$urole === $r ? "selected" : ""?>><?=$ri["icon"]?> <?=htmlspecialchars($r)?></option><?php endforeach; ?></select></div>
            <div style="flex:0 0 auto;"><button class="btn gray" type="submit" style="margin-top:0;">Найти</button></div>
        </form>
        <div class="meta">Найдено: <b><?=$utotal?></b><?php if ($uq !== "" || $urole !== ""): ?> · <a href="?tab=users">сбросить</a><?php endif; ?></div>

        <?php if (!$ulist): ?><div class="empty">Никого не нашлось.</div><?php else: ?>
        <div class="tbl-wrap">
            <table class="tbl">
                <thead><tr><th>Пользователь</th><th>Роль</th><th>IP · последний вход</th><th style="width:1%;">Действия</th></tr></thead>
                <tbody>
                <?php foreach ($ulist as $login => $u):
                    $ur = $u["role"] ?? "Пользователь"; $ud = $u["direction"] ?? "";
                    $editable = rt_can_edit_user($user, $role, $login, $ur);
                    $ri = rt_role_info($ur) ?? ["color" => "#718096"]; ?>
                    <tr>
                        <td><div class="who"><div class="avatar sm" style="--rc:<?=htmlspecialchars($ri["color"])?>"><?=htmlspecialchars(mb_strtoupper(mb_substr($login, 0, 1)))?></div><div><b><?=htmlspecialchars($login)?></b><?php if (!empty($u["email"])): ?><span class="muted" style="font-size:12px;"><?=htmlspecialchars($u["email"])?></span><?php endif; ?></div></div></td>
                        <td><?=rt_role_badge($ur, $ud, $login)?><?php if (!empty($u["golden"])): ?> <span class="badge badge-gold" style="margin:0;">🎫</span><?php endif; ?><?php if (!empty($u["tg_id"])): ?> <span class="chip" title="Telegram привязан<?=!empty($u["tg_username"]) ? ': @' . htmlspecialchars($u["tg_username"]) : ''?> — вход в панель с кодом 2FA">🤖 TG</span><?php endif; ?><?php if (!empty($u["discord_id"])): ?> <span class="chip" title="Discord привязан<?=!empty($u["discord_username"]) ? ': @' . htmlspecialchars($u["discord_username"]) : ''?>">💬 DS</span><?php endif; ?></td>
                        <td><?php if (!empty($u["ip"])): ?><code><?=htmlspecialchars($u["ip"])?></code><?php if (count($u["ip_history"] ?? []) > 1): ?> <span class="chip" title="Разных IP в истории">+<?=count($u["ip_history"]) - 1?></span><?php endif; ?><?php if ($can_bans && ip_is_banned($u["ip"])): ?> <span class="badge badge-dec" style="margin:0;">⛔</span><?php endif; ?><?php else: ?><span class="muted">—</span><?php endif; ?><?php if (!empty($u["last_seen"])): ?><div class="muted" style="font-size:12px;"><?=htmlspecialchars($u["last_seen"])?></div><?php endif; ?></td>
                        <td>
                            <?php $u_hist = (isset($u["ip_history"]) && is_array($u["ip_history"])) ? $u["ip_history"] : (!empty($u["ip"]) ? [["ip" => $u["ip"], "last" => $u["last_seen"] ?? "", "visits" => 0]] : []); ?>
                            <?php if ($editable && ($can_roles || $can_users) || ($can_bans && $u_hist)): ?>
                            <details class="act">
                                <summary class="btn ghost sm">Управлять ▾</summary>
                                <div class="act-panel">
                                    <?php if ($can_roles && $editable): ?>
                                        <form method="POST" action="?tab=users" class="inline-form">
                                            <input type="hidden" name="action" value="set_role"><input type="hidden" name="back" value="users"><input type="hidden" name="login" value="<?=htmlspecialchars($login)?>">
                                            <select name="role" data-role-select><?php foreach ($assignable as $r): ?><option value="<?=htmlspecialchars($r)?>" <?=$r === $ur ? "selected" : ""?>><?=rt_role_info($r)["icon"]?> <?=htmlspecialchars($r)?></option><?php endforeach; ?></select>
                                            <select name="direction" data-dir><?php foreach (rt_directions() as $dk => $dl): ?><option value="<?=htmlspecialchars($dk)?>" <?=$dk === $ud ? "selected" : ""?>><?=htmlspecialchars($dl)?></option><?php endforeach; ?></select>
                                            <button class="btn sm blue" type="submit">Сохранить роль</button>
                                        </form>
                                    <?php endif; ?>
                                    <?php if ($can_users && $editable): ?>
                                        <form method="POST" action="?tab=users" class="inline-form">
                                            <input type="hidden" name="action" value="set_pass"><input type="hidden" name="login" value="<?=htmlspecialchars($login)?>">
                                            <input type="password" name="password" placeholder="Новый пароль" required autocomplete="new-password">
                                            <button class="btn sm gray" type="submit">Сменить пароль</button>
                                        </form>
                                    <?php endif; ?>
                                    <?php if ($can_users && !empty($u["discord_id"])): ?>
                                        <form method="POST" action="?tab=users" class="inline-form">
                                            <input type="hidden" name="action" value="discord_dm"><input type="hidden" name="login" value="<?=htmlspecialchars($login)?>">
                                            <input type="text" name="text" placeholder="Сообщение в ЛС Discord" required maxlength="3000">
                                            <button class="btn sm blue" type="submit">💬 Написать в Discord</button>
                                        </form>
                                    <?php endif; ?>
                                    <?php if ($can_bans && $u_hist): ?>
                                        <div>
                                            <div class="muted" style="font-size:12px; margin-bottom:4px;">IP-адреса аккаунта (последние <?=count($u_hist)?>):</div>
                                            <?php foreach ($u_hist as $h): ?>
                                                <div class="inline-form" style="margin-bottom:4px;">
                                                    <code><?=htmlspecialchars($h["ip"])?></code>
                                                    <span class="muted" style="font-size:11px;"><?=htmlspecialchars(substr($h["last"] ?? "", 0, 16))?><?=!empty($h["visits"]) ? " · заходов: " . (int)$h["visits"] : ""?></span>
                                                    <?php if ($h["ip"] === rt_client_ip()): ?><span class="chip">ваш IP</span>
                                                    <?php elseif (ip_is_banned($h["ip"])): ?><span class="badge badge-dec" style="margin:0;">⛔ забанен</span>
                                                    <?php else: ?><a class="btn sm ghost danger" href="<?=htmlspecialchars(ban_link($h["ip"], "Блокировка по IP пользователя " . $login))?>">🎯 Забанить</a><?php endif; ?>
                                                </div>
                                            <?php endforeach; ?>
                                        </div>
                                    <?php endif; ?>
                                    <div class="inline-form">
                                        <?php if ($can_users && $editable && !empty($u["tg_id"])): ?>
                                            <form method="POST" action="?tab=users" onsubmit="return confirm('Отвязать Telegram? Вход будет по паролю, без кода из бота.');"><input type="hidden" name="action" value="unlink_tg"><input type="hidden" name="login" value="<?=htmlspecialchars($login)?>"><button class="btn sm ghost" type="submit">🤖 Отвязать Telegram</button></form>
                                        <?php endif; ?>
                                        <?php if ($can_users && $editable && !empty($u["discord_id"])): ?>
                                            <form method="POST" action="?tab=users" onsubmit="return confirm('Отвязать Discord?');"><input type="hidden" name="action" value="unlink_ds"><input type="hidden" name="login" value="<?=htmlspecialchars($login)?>"><button class="btn sm ghost" type="submit">💬 Отвязать Discord</button></form>
                                        <?php endif; ?>
                                        <?php if ($can_users && $editable && !rt_is_owner($login)): ?>
                                            <form method="POST" action="?tab=users" onsubmit="return confirm('Удалить аккаунт «<?=htmlspecialchars(addslashes($login))?>» навсегда?');"><input type="hidden" name="action" value="del_user"><input type="hidden" name="login" value="<?=htmlspecialchars($login)?>"><button class="btn sm ghost danger" type="submit">🗑 Удалить аккаунт</button></form>
                                        <?php endif; ?>
                                    </div>
                                </div>
                            </details>
                            <?php else: ?><span class="muted" title="<?=$login === $user ? 'Это вы' : 'Старше вас по должности или нет прав'?>">🔒</span><?php endif; ?>
                        </td>
                    </tr>
                <?php endforeach; ?>
                </tbody>
            </table>
        </div>
        <?php if ($upages > 1): ?>
            <div class="row" style="margin-top:12px;">
                <?php if ($upage > 1): ?><a class="btn gray sm" href="<?=$ulink($upage - 1)?>">← Назад</a><?php endif; ?>
                <span class="muted">Страница <?=$upage?> из <?=$upages?></span>
                <?php if ($upage < $upages): ?><a class="btn gray sm" href="<?=$ulink($upage + 1)?>">Вперёд →</a><?php endif; ?>
            </div>
        <?php endif; ?>
        <?php endif; ?>

    <?php elseif ($tab === "team"): ?>
        <?php
        $can_roles = can("roles.manage");
        $assignable = rt_assignable_roles($user, $role);
        $team = $staff_list;
        uksort($team, function($a, $b) use ($team) {
            $la = rt_user_level($a, $team[$a]["role"] ?? ""); $lb = rt_user_level($b, $team[$b]["role"] ?? "");
            return $lb <=> $la ?: strcasecmp($a, $b);
        });
        $by_role = [];
        foreach ($team as $login => $u) $by_role[$u["role"] ?? ""] = ($by_role[$u["role"] ?? ""] ?? 0) + 1;
        $trainees = array_filter($team, fn($u) => ($u["role"] ?? "") === RT_TRAINEE_ROLE);
        ?>
        <div class="chips" style="margin-bottom:6px;">
            <span class="chip">Всего в команде: <b style="color:#fff;"><?=count($team)?></b></span>
            <?php foreach (array_reverse(rt_roles()) as $r => $ri): if (empty($by_role[$r])) continue; ?>
                <span class="role-badge" style="--rc:<?=htmlspecialchars($ri["color"])?>"><?=$ri["icon"]?> <?=htmlspecialchars($r)?> · <?=$by_role[$r]?></span>
            <?php endforeach; ?>
        </div>

        <?php if ($can_roles): ?>
            <div class="card">
                <h3>➕ Добавить в команду или назначить роль</h3>
                <form method="POST" action="?tab=team">
                    <input type="hidden" name="action" value="add_to_team"><input type="hidden" name="back" value="team">
                    <div class="row">
                        <div class="grow"><label>Логин пользователя</label><input type="text" name="login" list="nonStaffLogins" placeholder="Начните вводить логин" required></div>
                        <div class="grow"><label>Роль</label><select name="role" data-role-select><?php foreach ($assignable as $r): if ($r === "Пользователь") continue; ?><option value="<?=htmlspecialchars($r)?>" <?=$r === RT_TRAINEE_ROLE ? "selected" : ""?>><?=rt_role_info($r)["icon"]?> <?=htmlspecialchars($r)?></option><?php endforeach; ?></select></div>
                        <div class="grow" data-dir><label>Направление стажёра</label><select name="direction"><?php foreach (rt_directions() as $dk => $dl): ?><option value="<?=htmlspecialchars($dk)?>"><?=htmlspecialchars($dl)?></option><?php endforeach; ?></select></div>
                    </div>
                    <button class="btn primary" type="submit">Назначить</button>
                    <span class="muted" style="margin-left:8px; font-size:12px;">Вы можете выдавать роли: <?=htmlspecialchars(implode(", ", array_diff($assignable, ["Пользователь"])))?></span>
                </form>
                <?php if (count($users) <= 5000): ?><datalist id="nonStaffLogins"><?php foreach ($users as $l => $u): if (is_array($u) && !isset($team[$l])): ?><option value="<?=htmlspecialchars($l)?>"><?php endif; endforeach; ?></datalist><?php endif; ?>
            </div>
        <?php else: ?>
            <div class="callout"><span class="c-ico">🔒</span><div>Назначать и менять роли могут руководитель и главный администратор. Здесь вы видите состав команды.</div></div>
        <?php endif; ?>

        <?php if ($trainees && $can_roles): ?>
            <h3>🌱 Стажёры (<?=count($trainees)?>)</h3>
            <div class="role-cards" style="margin-top:0;">
                <?php foreach ($trainees as $login => $u): $td = $u["direction"] ?? ""; $promote_to = isset(rt_roles()[$td]) ? $td : null; ?>
                    <div class="role-card" style="--rc:#2fb9a8;">
                        <div class="who row"><div class="avatar sm" style="--rc:#2fb9a8"><?=htmlspecialchars(mb_strtoupper(mb_substr($login, 0, 1)))?></div><b><?=htmlspecialchars($login)?></b></div>
                        <p>Направление: <b style="color:#fff;"><?=htmlspecialchars($td ?: "не указано")?></b><?php if (!empty($u["role_at"])): ?><br><span class="muted">стажёр с <?=htmlspecialchars(substr($u["role_at"], 0, 10))?><?=!empty($u["role_by"]) ? " · назначил " . htmlspecialchars($u["role_by"]) : ""?></span><?php endif; ?></p>
                        <?php if ($promote_to && rt_can_edit_user($user, $role, $login, RT_TRAINEE_ROLE) && rt_can_assign_role($user, $role, $promote_to)): ?>
                            <form method="POST" action="?tab=team" style="margin-top:10px;" onsubmit="return confirm('Повысить <?=htmlspecialchars(addslashes($login))?> до «<?=htmlspecialchars($promote_to)?>»?');">
                                <input type="hidden" name="action" value="change_team_role"><input type="hidden" name="back" value="team">
                                <input type="hidden" name="login" value="<?=htmlspecialchars($login)?>"><input type="hidden" name="role" value="<?=htmlspecialchars($promote_to)?>">
                                <button class="btn ok sm" type="submit">⬆ Повысить до «<?=htmlspecialchars($promote_to)?>»</button>
                            </form>
                        <?php endif; ?>
                    </div>
                <?php endforeach; ?>
            </div>
        <?php endif; ?>

        <div class="search-bar" style="margin-top:18px;">
            <div style="flex:2 1 240px;"><label>Фильтр</label><input type="search" id="teamFilter" placeholder="Логин или роль…" oninput="filterRows('teamTable', this.value)"></div>
        </div>
        <?php if (!$team): ?><div class="empty">Команда пока пуста.</div><?php else: ?>
        <div class="tbl-wrap" style="margin-top:0;">
            <table class="tbl" id="teamTable">
                <thead><tr><th>Сотрудник</th><th>Роль</th><th>Отдел</th><th>Назначил</th><th style="width:1%;">Управление</th></tr></thead>
                <tbody>
                <?php foreach ($team as $login => $u):
                    $ur = $u["role"] ?? "Пользователь"; $ud = $u["direction"] ?? "";
                    $ri = rt_role_info($ur) ?? ["color" => "#718096"];
                    $dept = rt_user_dept($ur, $ud);
                    $editable = $can_roles && rt_can_edit_user($user, $role, $login, $ur); ?>
                    <tr>
                        <td><div class="who"><div class="avatar sm" style="--rc:<?=htmlspecialchars($ri["color"])?>"><?=htmlspecialchars(mb_strtoupper(mb_substr($login, 0, 1)))?></div><div><b><?=htmlspecialchars($login)?><?=$login === $user ? ' <span class="muted" style="font-weight:400;">(вы)</span>' : ''?></b><?php if (!empty($u["last_seen"])): ?><span class="muted" style="font-size:12px;">был(а) <?=htmlspecialchars(date("d.m.Y H:i", strtotime($u["last_seen"])))?></span><?php endif; ?></div></div></td>
                        <td><?=rt_role_badge($ur, $ud, $login)?></td>
                        <td><?=htmlspecialchars($dept ? (rt_departments()[$dept] ?? $dept) : "—")?></td>
                        <td class="muted" style="font-size:12px;"><?=!empty($u["role_by"]) ? htmlspecialchars($u["role_by"]) . "<br>" . htmlspecialchars(substr($u["role_at"] ?? "", 0, 10)) : "—"?></td>
                        <td>
                            <?php if ($editable): ?>
                                <div class="inline-form" style="flex-wrap:nowrap;">
                                    <form method="POST" action="?tab=team" class="inline-form" style="flex-wrap:nowrap;">
                                        <input type="hidden" name="action" value="change_team_role"><input type="hidden" name="back" value="team"><input type="hidden" name="login" value="<?=htmlspecialchars($login)?>">
                                        <select name="role" data-role-select><?php foreach ($assignable as $r): if ($r === "Пользователь") continue; ?><option value="<?=htmlspecialchars($r)?>" <?=$r === $ur ? "selected" : ""?>><?=rt_role_info($r)["icon"]?> <?=htmlspecialchars($r)?></option><?php endforeach; ?></select>
                                        <select name="direction" data-dir style="min-width:120px;"><?php foreach (rt_directions() as $dk => $dl): ?><option value="<?=htmlspecialchars($dk)?>" <?=$dk === $ud ? "selected" : ""?>><?=htmlspecialchars($dl)?></option><?php endforeach; ?></select>
                                        <button class="btn sm blue" type="submit">✓</button>
                                    </form>
                                    <form method="POST" action="?tab=team" onsubmit="return confirm('Исключить <?=htmlspecialchars(addslashes($login))?> из команды? Роль станет «Пользователь».');"><input type="hidden" name="action" value="remove_from_team"><input type="hidden" name="back" value="team"><input type="hidden" name="login" value="<?=htmlspecialchars($login)?>"><button class="btn sm ghost danger" type="submit" title="Исключить из команды">✕</button></form>
                                </div>
                            <?php else: ?><span class="muted" title="<?=$login === $user ? 'Свою роль менять нельзя' : ($can_roles ? 'Старше или равен вам по должности' : 'Нет права менять роли')?>">🔒</span><?php endif; ?>
                        </td>
                    </tr>
                <?php endforeach; ?>
                </tbody>
            </table>
        </div>
        <?php endif; ?>

    <?php elseif ($tab === "perms"): ?>
        <?php
        $matrix = rt_role_perms_matrix();
        $edit_perms = can("perms.manage");
        $role_cols = array_filter(rt_roles(), fn($ri, $r) => $r !== "Пользователь", ARRAY_FILTER_USE_BOTH);
        $role_counts = [];
        foreach ($users as $l => $u) if (is_array($u)) $role_counts[$u["role"] ?? "Пользователь"] = ($role_counts[$u["role"] ?? "Пользователь"] ?? 0) + 1;
        ?>
        <div class="callout"><span class="c-ico">🔐</span><div>
            Чем выше уровень роли, тем больше она может. <b>Стажёр</b> — только чат, файлы и цели. <b>Главные</b> (тестер, кодер, разработчик) — всё, кроме банов, ролей и штрафов, включая почту.
            <b>Банят</b> только главный администратор и руководитель. <b>Роли</b> назначают руководитель и главный администратор — и только тем, кто младше их.
            <?php if ($edit_perms): ?>Отметьте галочки и нажмите «Сохранить» — права обновятся у всех сразу.<?php endif; ?>
            Владельцы сайта (<?=htmlspecialchars(implode(", ", rt_owners()))?>) всегда имеют все права.
        </div></div>

        <div class="role-cards">
            <?php foreach (array_reverse(rt_roles()) as $r => $ri): ?>
                <div class="role-card" style="--rc:<?=htmlspecialchars($ri["color"])?>">
                    <div class="rc-head"><span class="role-badge" style="--rc:<?=htmlspecialchars($ri["color"])?>"><?=$ri["icon"]?> <?=htmlspecialchars($r)?></span><span class="lvl">ур. <?=$ri["level"]?> · <?=($role_counts[$r] ?? 0)?> чел.</span></div>
                    <p><?=htmlspecialchars($ri["desc"])?></p>
                </div>
            <?php endforeach; ?>
        </div>

        <form method="POST" action="?tab=perms">
            <input type="hidden" name="action" value="save_perms">
            <div class="tbl-wrap" style="max-height:75vh;">
                <table class="tbl perm-table">
                    <thead><tr><th>Право</th><?php foreach ($role_cols as $r => $ri): ?><th class="role-col<?=$r === $role ? ' me-col' : ''?>" title="<?=htmlspecialchars($r)?>"><span><?=$ri["icon"]?></span><?=htmlspecialchars($r)?></th><?php endforeach; ?></tr></thead>
                    <tbody>
                    <?php foreach (rt_permissions() as $group => $perms): ?>
                        <tr class="group"><td colspan="<?=count($role_cols) + 1?>"><?=htmlspecialchars($group)?></td></tr>
                        <?php foreach ($perms as $pk => $plabel): ?>
                            <tr>
                                <td><?=htmlspecialchars($plabel)?></td>
                                <?php foreach ($role_cols as $r => $ri): $has = in_array($pk, $matrix[$r] ?? [], true); ?>
                                    <td class="c<?=$r === $role ? ' me-col' : ''?>">
                                        <?php if ($edit_perms && $r !== RT_TOP_ROLE): ?>
                                            <input type="checkbox" name="perm[<?=htmlspecialchars($r)?>][]" value="<?=htmlspecialchars($pk)?>" <?=$has ? "checked" : ""?> aria-label="<?=htmlspecialchars($r . ": " . $plabel)?>">
                                        <?php else: ?>
                                            <span class="<?=$has ? 'perm-yes' : 'perm-no'?>"><?=$has ? '✓' : '—'?></span>
                                        <?php endif; ?>
                                    </td>
                                <?php endforeach; ?>
                            </tr>
                        <?php endforeach; ?>
                    <?php endforeach; ?>
                    </tbody>
                </table>
            </div>
            <?php if ($edit_perms): ?>
                <div class="row" style="margin-top:12px;">
                    <button class="btn primary" type="submit">💾 Сохранить права</button>
                    <button class="btn ghost" type="submit" form="resetPermsForm">Сбросить к стандартным</button>
                </div>
            <?php endif; ?>
        </form>
        <?php if ($edit_perms): ?><form id="resetPermsForm" method="POST" action="?tab=perms" onsubmit="return confirm('Вернуть стандартные права всем ролям?');"><input type="hidden" name="action" value="reset_perms"></form><?php endif; ?>

    <?php elseif ($tab === "gold"): ?>
        <?php if (can("gold.manage")): ?>
        <div class="card gold-card">
            <h3>💳 Платные услуги для держателей золотого билета</h3>
            <p style="color:#aaa; font-size:13px;">Список услуг (можно добавить сколько угодно — хоть 1000 штук), которые видят у себя в профиле <b>все</b> обладатели золотого билета. Каждую можно отредактировать или удалить.</p>
            <form method="POST">
                <input type="hidden" name="action" value="add_gold_service">
                <label>Название услуги</label>
                <input type="text" name="service_name" placeholder="Например: Приоритетная поддержка RTeam" required>
                <label>Ссылка для получения</label>
                <input type="text" name="service_link" placeholder="https://...">
                <button class="btn gray" type="submit" style="margin-top:10px;">➕ Добавить услугу</button>
            </form>
        </div>

        <?php if (!$gold_services): ?>
            <p style="margin-top:14px;">Пока не добавлено ни одной платной услуги.</p>
        <?php else: ?>
            <h3 style="margin-top:24px;">Список услуг (<?=count($gold_services)?>)</h3>
            <?php foreach ($gold_services as $svc): ?>
                <div class="card gold-card">
                    <form method="POST" style="display:flex; flex-wrap:wrap; gap:8px; align-items:flex-end;">
                        <input type="hidden" name="action" value="edit_gold_service">
                        <input type="hidden" name="service_id" value="<?=htmlspecialchars($svc["id"])?>">
                        <div style="flex:1 1 220px;">
                            <label>Название</label>
                            <input type="text" name="service_name" value="<?=htmlspecialchars($svc["name"])?>" required>
                        </div>
                        <div style="flex:1 1 220px;">
                            <label>Ссылка</label>
                            <input type="text" name="service_link" value="<?=htmlspecialchars($svc["link"])?>" placeholder="https://...">
                        </div>
                        <button class="btn gray" type="submit">Сохранить</button>
                    </form>
                    <form method="POST" style="margin-top:6px;">
                        <input type="hidden" name="action" value="delete_gold_service">
                        <input type="hidden" name="service_id" value="<?=htmlspecialchars($svc["id"])?>">
                        <button class="btn no" type="submit" onclick="return confirm('Удалить услугу «<?=htmlspecialchars(addslashes($svc["name"]))?>»?')">🗑 Удалить</button>
                    </form>
                </div>
            <?php endforeach; ?>
        <?php endif; ?>

        <div class="card gold-card">
            <h3>🎫 Выдать / обновить Золотой билет RTeam</h3>
            <p style="color:#aaa; font-size:13px;">Билет дарит владельцу отдельную премиальную страницу профиля со своим названием, ссылкой и личным чатом с закреплённым сотрудником. Всю переписку по золотым билетам видит вся администрация.</p>
            <form method="POST">
                <input type="hidden" name="action" value="grant_golden">
                <label>Кому выдать</label>
                <select name="login" required>
                    <option value="">-- Выберите пользователя --</option>
                    <?php foreach ($users as $login => $u): ?>
                        <option value="<?=htmlspecialchars($login)?>"<?=$login===$user?' data-self="1"':''?>><?=htmlspecialchars($login)?> (<?=htmlspecialchars($u["role"] ?? "Пользователь")?>)<?=!empty($u["golden"])?" — уже есть билет":""?></option>
                    <?php endforeach; ?>
                </select>
                <button type="button" class="btn gray" style="padding:6px 10px; font-size:12px;" onclick="const s=this.previousElementSibling; for (const o of s.options) if (o.value === '<?=htmlspecialchars($user)?>') { s.value = o.value; break; }">Выдать себе</button>
                <label style="margin-top:10px;">Название билета (отображается в профиле)</label>
                <input type="text" name="golden_title" placeholder="Например: Золотой билет RTeam — Победитель" value="Золотой билет RTeam">
                <label>Ссылка в профиле</label>
                <input type="text" name="golden_link" placeholder="https://...">
                <label>Закреплённый сотрудник (кому владелец билета сможет писать)</label>
                <select name="golden_staff">
                    <option value="">-- Без закрепления --</option>
                    <?php foreach ($users as $login => $u): if (($u["role"] ?? "Пользователь") !== "Пользователь"): ?>
                        <option value="<?=htmlspecialchars($login)?>"><?=htmlspecialchars($login)?> (<?=htmlspecialchars($u["role"])?>)</option>
                    <?php endif; endforeach; ?>
                </select>
                <button class="btn gray" type="submit" style="margin-top:10px;">Сохранить билет</button>
            </form>
        </div>

        <?php endif; ?>
        <h3 style="margin-top:24px;">Владельцы золотого билета</h3>
        <?php
        $golden_users = [];
        foreach ($users as $login => $u) if (!empty($u["golden"])) $golden_users[$login] = $u;
        ?>
        <?php if (!$golden_users): ?><p>Пока никто не получил золотой билет.</p><?php else: ?>
            <?php foreach ($golden_users as $login => $u): ?>
                <div class="card gold-card">
                    <h3><?=htmlspecialchars($login)?> <span class="badge badge-gold">🎫 <?=htmlspecialchars($u["golden_title"] ?? "Золотой билет RTeam")?></span></h3>
                    <div class="meta">Ссылка: <?php if (!empty($u["golden_link"])): ?><a href="<?=htmlspecialchars($u["golden_link"])?>" target="_blank" style="color:#ffd76a;"><?=htmlspecialchars($u["golden_link"])?></a><?php else: ?>—<?php endif; ?></div>
                    <div class="meta">Закреплённый сотрудник: <b><?=htmlspecialchars($u["golden_staff"] ?: "не назначен")?></b></div>
                    <div class="meta">Сообщений в чате: <?=count($gold_chats[$login]["messages"] ?? [])?></div>
                    <a href="?tab=gold&thread=<?=urlencode($login)?>" class="btn blue" style="text-decoration:none;">Открыть чат</a>
                    <?php if (can("gold.manage")): ?><form method="POST" style="margin-top:6px; display:inline-block;"><input type="hidden" name="action" value="revoke_golden"><input type="hidden" name="login" value="<?=htmlspecialchars($login)?>"><button class="btn no" type="submit" onclick="return confirm('Забрать золотой билет?')">Отозвать билет</button></form><?php endif; ?>
                </div>
            <?php endforeach; ?>
        <?php endif; ?>

        <?php
        $thread = $_GET["thread"] ?? "";
        if ($thread !== "" && isset($users[$thread]) && !empty($users[$thread]["golden"])):
            $chat = $gold_chats[$thread] ?? ["staff" => "", "messages" => []];
        ?>
            <h3 style="margin-top:24px;">Чат с <?=htmlspecialchars($thread)?> <span class="badge badge-gold">🎫</span></h3>
            <div class="card gold-card">
                <div class="meta" style="margin-bottom:10px;">Закреплённый сотрудник: <b><?=htmlspecialchars($chat["staff"] ?: "не назначен")?></b> — но отвечать может любой сотрудник администрации.</div>
                <div style="max-height:360px; overflow-y:auto; display:flex; flex-direction:column; gap:8px; margin-bottom:12px;">
                    <?php if (!$chat["messages"]): ?><p style="color:#666;">Сообщений пока нет.</p><?php endif; ?>
                    <?php foreach ($chat["messages"] as $m): $isStaff = ($m["from"] === "staff"); ?>
                        <div style="align-self:<?=$isStaff?'flex-end':'flex-start'?>; max-width:75%; background:<?=$isStaff?'#3a2d00':'#101018'?>; border:1px solid <?=$isStaff?'#7a5c00':'#333'?>; border-radius:8px; padding:8px 10px;">
                            <div style="font-size:11px; color:#ffd76a; margin-bottom:4px;"><?=$isStaff?htmlspecialchars($m["author"])." (Rteam)":htmlspecialchars($thread)?> — <?=htmlspecialchars($m["time"])?></div>
                            <div><?=nl2br(htmlspecialchars($m["text"]))?></div>
                        </div>
                    <?php endforeach; ?>
                </div>
                <?php if (can("gold.chat")): ?>
                <form method="POST">
                    <input type="hidden" name="action" value="gold_reply">
                    <input type="hidden" name="login" value="<?=htmlspecialchars($thread)?>">
                    <textarea name="reply_text" placeholder="Написать владельцу золотого билета..." required style="height:60px;"></textarea>
                    <button class="btn gray" type="submit" style="margin-top:8px;">Отправить</button>
                </form>
                <?php endif; ?>
            </div>
        <?php endif; ?>

    <?php elseif ($tab === "recruit"): ?>
        <form method="POST" action="?tab=recruit">
            <input type="hidden" name="action" value="save_recruit">
            <div class="card" style="margin-top:0;">
                <div class="row" style="justify-content:space-between;">
                    <div><h3 style="margin:0;">📣 Набор в команду</h3><div class="meta" style="margin:4px 0 0;">Когда набор закрыт, на сайте вместо формы заявки — надпись «Набор временно закрыт».</div></div>
                    <label class="switch-row" style="font-size:14px;"><input type="checkbox" name="recruit_open" <?=!empty($settings["recruit_open"])?"checked":""?>> Набор открыт</label>
                </div>
            </div>
            <div class="grid-2" style="margin-top:14px;">
                <div class="card" style="margin-top:0;">
                    <h3>👥 Заявка «Команда»</h3>
                    <div class="meta">По одному вопросу на строку. Вопрос со словом <b>«Направление»</b> на сайте показывается списком (Кодер / Разработчик / Тестер) — при одобрении это направление само записывается стажёру.</div>
                    <textarea name="team_questions" style="height:220px;"><?=htmlspecialchars(implode("\n", $questions["team"] ?? []))?></textarea>
                </div>
                <div class="card" style="margin-top:0;">
                    <h3>🛡️ Заявка «Администратор»</h3>
                    <div class="meta">По одному вопросу на строку. Такая заявка при одобрении всегда даёт направление «Администратор».</div>
                    <textarea name="admin_questions" style="height:220px;"><?=htmlspecialchars(implode("\n", $questions["admin"] ?? []))?></textarea>
                </div>
            </div>
            <button class="btn primary" type="submit" style="margin-top:14px;">💾 Сохранить</button>
        </form>

    <?php elseif ($tab === "themes"): ?>
        <?php $curTheme = $THEME_CATALOG[$theme_settings["active"]]; ?>
        <form method="POST" action="?tab=themes">
            <input type="hidden" name="action" value="save_theme">
            <div class="card theme-preview" style="margin-top:0;">
                <div class="row" style="justify-content:space-between; align-items:flex-start;">
                    <div>
                        <h3 style="margin:0;">🎭 Тема сайта</h3>
                        <div class="meta" style="margin:4px 0 0;">На сайте появится баннер и плавающие иконки выбранной темы. Активна одна тема.</div>
                    </div>
                    <label class="switch-row" style="font-size:14px;"><input type="checkbox" name="theme_enabled" <?=!empty($theme_settings["enabled"])?"checked":""?>> Тема включена</label>
                </div>
                <div class="banner-preview" id="themeBanner"><span id="tpIcon"><?=$curTheme["icon"]?></span> <b id="tpName"><?=htmlspecialchars($curTheme["name"])?></b><span id="tpText"><?= trim($theme_settings["text"] ?? "") !== "" ? " — " . htmlspecialchars($theme_settings["text"]) : "" ?></span></div>
                <label style="display:block; margin-top:12px;" id="themeFieldLabel">Текст для баннера</label>
                <input type="text" name="theme_text" id="themeTextInput" value="<?=htmlspecialchars($theme_settings["text"] ?? "")?>" placeholder="" oninput="rteamThemePreview()">
            </div>
            <?php
            $themeGroups = [];
            foreach ($THEME_CATALOG as $tkey => $tinfo) { $themeGroups[$tinfo["group"]][$tkey] = $tinfo; }
            foreach ($themeGroups as $groupName => $items): ?>
                <div class="section-title"><?=htmlspecialchars($groupName)?></div>
                <div class="theme-grid">
                    <?php foreach ($items as $tkey => $tinfo): ?>
                        <label class="theme-tile">
                            <input type="radio" name="theme_active" value="<?=htmlspecialchars($tkey)?>" <?=($theme_settings["active"]===$tkey)?"checked":""?> onchange="rteamUpdateThemeField()">
                            <span class="tt-ico"><?=$tinfo["icon"]?></span>
                            <span class="tt-name"><?=htmlspecialchars($tinfo["name"])?></span>
                        </label>
                    <?php endforeach; ?>
                </div>
            <?php endforeach; ?>
            <button class="btn primary" type="submit" style="margin-top:16px;">💾 Сохранить тему</button>
        </form>

        <div class="card">
            <h3>🦑 Игра в кальмара — управление</h3>
            <p style="color:#999; font-size:13px; margin-top:-6px;">На сайте показываются 3 последовательных сезона (Красный свет/зелёный свет → Дальгона → Стеклянный мост). Прошедшие все три сезона попадают в список претендентов на «Золотой билет RTeam» ниже.</p>

            <form method="POST">
                <input type="hidden" name="action" value="squid_set_paused">
                <label><input type="checkbox" name="squid_paused" <?=!empty($squid_game["paused"])?"checked":""?>> Остановить игру (игроки увидят «Игры приостановлены»)</label>
                <button class="btn gray" type="submit" style="margin-top:10px;">Сохранить</button>
            </form>

            <h4 style="margin-top:18px;">Прогресс игроков</h4>
            <?php if (empty($squid_game["progress"])): ?>
                <p style="color:#888; font-size:13px;">Пока никто не начинал игру.</p>
            <?php else: ?>
                <table style="width:100%; border-collapse:collapse; font-size:13px; margin-top:8px;">
                    <tr style="text-align:left; color:#999; border-bottom:1px solid #333;">
                        <th style="padding:6px 4px;">Игрок</th>
                        <th style="padding:6px 4px;">Сезон</th>
                        <th style="padding:6px 4px;">Статус</th>
                        <th style="padding:6px 4px;">Завершил</th>
                        <th style="padding:6px 4px;"></th>
                    </tr>
                    <?php foreach ($squid_game["progress"] as $sq_login => $sq_p): ?>
                    <tr style="border-bottom:1px solid #222;">
                        <td style="padding:6px 4px;"><b><?=htmlspecialchars($sq_login)?></b></td>
                        <td style="padding:6px 4px;"><?=(int)($sq_p["season"] ?? 0)?> / 3</td>
                        <td style="padding:6px 4px;">
                            <?php if (!empty($sq_p["completed"])): ?>
                                <span style="color:#39ff6a;">✅ прошёл всё</span>
                            <?php else: ?>
                                <span style="color:#ffb84d;">в процессе</span>
                            <?php endif; ?>
                        </td>
                        <td style="padding:6px 4px; color:#999;"><?=htmlspecialchars($sq_p["completed_at"] ?? "—")?></td>
                        <td style="padding:6px 4px;">
                            <form method="POST" style="display:inline;" onsubmit="return confirm('Сбросить прогресс игрока?');">
                                <input type="hidden" name="action" value="squid_reset_user">
                                <input type="hidden" name="login" value="<?=htmlspecialchars($sq_login)?>">
                                <button class="btn no" type="submit" style="padding:4px 10px; font-size:12px;">Сбросить</button>
                            </form>
                            <?php if (!empty($sq_p["completed"]) && empty($users[$sq_login]["golden"])): ?>
                            <form method="POST" style="display:inline;" onsubmit="return confirm('Выдать «Золотой билет RTeam» этому игроку?');">
                                <input type="hidden" name="action" value="grant_golden">
                                <input type="hidden" name="login" value="<?=htmlspecialchars($sq_login)?>">
                                <input type="hidden" name="golden_title" value="Золотой билет RTeam — Игра в кальмара">
                                <input type="hidden" name="golden_link" value="">
                                <input type="hidden" name="golden_staff" value="<?=htmlspecialchars($user)?>">
                                <button class="btn" type="submit" style="padding:4px 10px; font-size:12px; background:linear-gradient(135deg,#ffe066,#d4a017); color:#3a2a00;">🎫 Выдать билет</button>
                            </form>
                            <?php elseif (!empty($users[$sq_login]["golden"])): ?>
                                <span class="badge badge-gold" style="font-size:10px;">🎫 уже есть билет</span>
                            <?php endif; ?>
                        </td>
                    </tr>
                    <?php endforeach; ?>
                </table>
            <?php endif; ?>
        </div>

        <script>
        const RTEAM_THEME_META = <?=json_encode($THEME_CATALOG, JSON_UNESCAPED_UNICODE)?>;
        function rteamUpdateThemeField() {
            const sel = document.querySelector('input[name="theme_active"]:checked');
            const meta = sel && RTEAM_THEME_META[sel.value];
            if (meta) {
                document.getElementById('themeFieldLabel').textContent = meta.field;
                document.getElementById('themeTextInput').placeholder = meta.placeholder || '';
                document.getElementById('tpIcon').textContent = meta.icon;
                document.getElementById('tpName').textContent = meta.name;
            }
            rteamThemePreview();
        }
        function rteamThemePreview() {
            const t = document.getElementById('themeTextInput').value.trim();
            document.getElementById('tpText').textContent = t ? ' — ' + t : '';
        }
        rteamUpdateThemeField();
        </script>

    <?php elseif ($tab === "settings"): ?>
        <form method="POST" action="?tab=settings" class="grid-2" style="align-items:start;">
            <input type="hidden" name="action" value="save_settings">
            <div class="card" style="margin-top:0;">
                <h3>🌐 Сайт</h3>
                <label>Название сайта</label>
                <input type="text" name="site_name" value="<?=htmlspecialchars($settings["site_name"] ?? "")?>">
                <label style="display:block; margin-top:14px;">Цвет акцента — им окрашены кнопки и подсветка на сайте и в этой панели</label>
                <div class="row" style="margin-top:6px;">
                    <input type="color" id="accentPicker" value="<?=htmlspecialchars($accent_css)?>" oninput="document.getElementById('accentText').value=this.value; document.documentElement.style.setProperty('--accent', this.value);">
                    <input type="text" id="accentText" name="accent" value="<?=htmlspecialchars($settings["accent"] ?? "")?>" style="flex:1; margin-top:0;" oninput="if(/^#[0-9a-f]{6}$/i.test(this.value)){document.getElementById('accentPicker').value=this.value; document.documentElement.style.setProperty('--accent', this.value);}">
                </div>
                <div class="swatches">
                    <?php foreach (["#ff2a2a", "#ff6a00", "#f6c445", "#22c55e", "#14b8a6", "#3b82f6", "#8b5cf6", "#ec4899"] as $sw): ?>
                        <button type="button" class="swatch" style="--sw:<?=$sw?>" title="<?=$sw?>" onclick="const t=document.getElementById('accentText'); t.value='<?=$sw?>'; t.dispatchEvent(new Event('input'));"></button>
                    <?php endforeach; ?>
                </div>
            </div>
            <div class="card" style="margin-top:0;">
                <h3>✨ Эффекты</h3>
                <label class="switch-row"><input type="checkbox" name="neon" <?=!empty($settings["neon"])?"checked":""?>> Неон‑эффекты на сайте</label>
                <label class="switch-row" style="margin-top:12px;"><input type="checkbox" name="animations" <?=!empty($settings["animations"])?"checked":""?>> Анимации на сайте</label>
                <button class="btn primary" type="submit" style="width:100%; margin-top:18px;">💾 Сохранить настройки</button>
            </div>
        </form>

    <?php elseif ($tab === "logs"): ?>
        <?php if (!$logs): ?><div class="empty">Логов пока нет.</div><?php else: ?>
            <?php
            $log_types = [];
            foreach ($logs as $lg) { $lt = $lg["type"] ?? "other"; $log_types[$lt] = ($log_types[$lt] ?? 0) + 1; }
            arsort($log_types);
            $log_icons = ["roles" => "👥", "users" => "🗂️", "ban" => "⛔", "blacklist" => "⚫", "geoblock" => "🌍", "application" => "📝", "bot" => "🤖", "golden_ticket" => "🎫", "file_upload" => "📤", "file_del" => "🗑", "fine_ban" => "💸", "squid_game" => "🦑", "director_approve" => "🏫", "director_add" => "🏫", "login" => "🔑", "ny_giveaway" => "🎄", "orders" => "🛒", "schedule" => "📅"];
            ?>
            <div class="search-bar">
                <div style="flex:2 1 240px;"><label>Поиск по логам</label><input type="search" id="logSearch" placeholder="Кто, что сделал…" oninput="filterLogs()"></div>
                <div><label>Тип</label><select id="logType" onchange="filterLogs()"><option value="">Все (<?=count($logs)?>)</option><?php foreach ($log_types as $lt => $lc): ?><option value="<?=htmlspecialchars($lt)?>"><?=($log_icons[$lt] ?? "•") . " " . htmlspecialchars($lt)?> (<?=$lc?>)</option><?php endforeach; ?></select></div>
            </div>
            <div class="tbl-wrap" style="margin-top:0; max-height:72vh;">
                <table class="tbl" id="logTable">
                    <thead><tr><th style="width:150px;">Когда</th><th style="width:140px;">Тип</th><th>Событие</th></tr></thead>
                    <tbody>
                    <?php foreach (array_slice(array_reverse($logs), 0, 1000) as $log): $lt = $log["type"] ?? "other"; ?>
                        <tr data-type="<?=htmlspecialchars($lt)?>"><td class="muted" style="font-size:12px; white-space:nowrap;"><?=htmlspecialchars($log["time"] ?? "")?></td><td><span class="chip"><?=($log_icons[$lt] ?? "•") . " " . htmlspecialchars($lt)?></span></td><td><?=nl2br(htmlspecialchars($log["msg"] ?? ""))?></td></tr>
                    <?php endforeach; ?>
                    </tbody>
                </table>
            </div>
            <?php if (count($logs) > 1000): ?><div class="meta" style="margin-top:8px;">Показаны последние 1000 из <?=count($logs)?>.</div><?php endif; ?>
            <script>
            function filterLogs() {
                const q = document.getElementById('logSearch').value.trim().toLowerCase(), t = document.getElementById('logType').value;
                document.querySelectorAll('#logTable tbody tr').forEach(tr => { tr.style.display = (!t || tr.dataset.type === t) && (!q || tr.textContent.toLowerCase().includes(q)) ? '' : 'none'; });
            }
            </script>
        <?php endif; ?>
    <?php endif; ?>
        </div>
    </main>
</div>

<script>
    // --- ГЛОБАЛЬНЫЕ УВЕДОМЛЕНИЯ ---
    let lastCheckTime = <?=time()?>;
    function showToast(text, type='info') {
        const container = document.getElementById('toast-container');
        const toast = document.createElement('div'); toast.className = 'toast ' + type; toast.innerText = text;
        container.appendChild(toast); setTimeout(() => { toast.style.opacity = '0'; setTimeout(() => toast.remove(), 500); }, 5000);
    }
    // Сообщения о результате последнего действия
    <?php foreach ($_SESSION["flash"] ?? [] as $fl): ?>showToast(<?=json_encode($fl["text"], JSON_UNESCAPED_UNICODE)?>, <?=json_encode($fl["type"])?>);
    <?php endforeach; unset($_SESSION["flash"]); ?>

    // Поле «Направление» показываем только когда выбрана роль «Стажёр»
    document.querySelectorAll('select[data-role-select]').forEach(sel => {
        const dir = sel.form && sel.form.querySelector('[data-dir]');
        if (!dir) return;
        const upd = () => { dir.style.display = sel.value === <?=json_encode(RT_TRAINEE_ROLE, JSON_UNESCAPED_UNICODE)?> ? '' : 'none'; };
        sel.addEventListener('change', upd); upd();
    });

    // --- БЫСТРЫЙ ПЕРЕХОД (Ctrl+K или «/») ---
    const palette = document.getElementById('palette'), palInput = document.getElementById('paletteInput');
    let palSel = 0;
    function palItems() { return [...document.querySelectorAll('#paletteList .palette-item')].filter(a => a.style.display !== 'none'); }
    function palMark() { palItems().forEach((a, i) => a.classList.toggle('sel', i === palSel)); const cur = palItems()[palSel]; if (cur) cur.scrollIntoView({ block: 'nearest' }); }
    function openPalette() { palette.classList.add('open'); palInput.value = ''; palFilter(); palInput.focus(); document.querySelectorAll('details[open].tb-notif').forEach(d => d.removeAttribute('open')); }
    function closePalette() { palette.classList.remove('open'); }
    function palFilter() {
        const q = palInput.value.trim().toLowerCase();
        document.querySelectorAll('#paletteList .palette-item').forEach(a => { a.style.display = !q || a.dataset.q.includes(q) ? '' : 'none'; });
        palSel = 0; palMark();
    }
    palInput.addEventListener('input', palFilter);
    palInput.addEventListener('keydown', e => {
        const items = palItems();
        if (e.key === 'ArrowDown') { e.preventDefault(); palSel = Math.min(items.length - 1, palSel + 1); palMark(); }
        else if (e.key === 'ArrowUp') { e.preventDefault(); palSel = Math.max(0, palSel - 1); palMark(); }
        else if (e.key === 'Enter') { e.preventDefault(); if (items[palSel]) location.href = items[palSel].href; }
    });
    document.addEventListener('keydown', e => {
        const typing = /^(INPUT|TEXTAREA|SELECT)$/.test(document.activeElement.tagName);
        if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'k') { e.preventDefault(); palette.classList.contains('open') ? closePalette() : openPalette(); }
        else if (e.key === '/' && !typing) { e.preventDefault(); openPalette(); }
        else if (e.key === 'Escape') { closePalette(); document.querySelectorAll('details[open].tb-notif').forEach(d => d.removeAttribute('open')); }
    });

    // --- СВЁРНУТОЕ МЕНЮ (запоминается в браузере) ---
    function toggleMiniSidebar() {
        const on = document.documentElement.classList.toggle('sb-mini');
        try { localStorage.setItem('rtSbMini', on ? '1' : '0'); } catch (e) {}
    }

    // --- ЧАСЫ В ВЕРХНЕЙ ПАНЕЛИ ---
    (function clock() {
        const el = document.getElementById('tbClock'); if (!el) return;
        const tick = () => { el.textContent = new Date().toLocaleString('ru-RU', { weekday: 'short', day: 'numeric', month: 'short', hour: '2-digit', minute: '2-digit' }); };
        tick(); setInterval(tick, 30000);
    })();

    // Уведомления закрываются кликом мимо
    document.addEventListener('click', e => { document.querySelectorAll('details[open].tb-notif').forEach(d => { if (!d.contains(e.target)) d.removeAttribute('open'); }); });

    // Быстрый фильтр строк таблицы
    function filterRows(tableId, q) {
        q = q.trim().toLowerCase();
        document.querySelectorAll('#' + tableId + ' tbody tr').forEach(tr => { tr.style.display = !q || tr.textContent.toLowerCase().includes(q) ? '' : 'none'; });
    }

    // Кнопки розыгрышей (итоги / удалить) — без перезагрузки страницы
    document.addEventListener('submit', e => {
        const f = e.target;
        if (!f.classList.contains('ajax-bot-form') || !f.closest('#botGiveawaysFeed') || e.defaultPrevented) return;
        e.preventDefault();
        fetch('admin.php?tab=bot', { method: 'POST', body: new FormData(f) }).then(r => r.text()).then(res => {
            if (res.trim() === 'forbidden') showToast('Недостаточно прав для этого действия.', 'error');
            else { showToast('Готово!', 'success'); loadBotData(); }
        }).catch(() => showToast('Ошибка сети', 'error'));
    });
    setInterval(() => {
        fetch('?ajax_check=1&last_time=' + lastCheckTime).then(r => r.json()).then(data => {
            if (data.status === 'ok') {
                data.msgs.forEach(msg => { showToast(msg, 'info'); });
                data.fines.forEach(fine => { showToast(fine, 'error'); });
                lastCheckTime = data.time;
            }
        });
    }, 5000);

    // --- ПРЯМАЯ ИЗОЛИРОВАННАЯ ФУНКЦИЯ ДЛЯ КНОПОК БОТА (БЕЗ ПЕРЕЗАГРУЗКИ) ---
    function sendBotAction(action, userId, ticketId, text = '') {
        const fd = new FormData();
        fd.append('action', action);
        if (userId) fd.append('user_id', userId);
        if (ticketId) fd.append('ticket_id', ticketId);
        fd.append('is_ajax', '1');
        if (text) fd.append('reply_text', text);

        fetch('admin.php?tab=bot', { method: 'POST', body: fd })
        .then(r => r.text())
        .then(res => {
            if(res.trim() === 'error') showToast('Ошибка! Проверьте токен бота в настройках.', 'error');
            else if(res.trim() === 'forbidden') showToast('Недостаточно прав для этого действия.', 'error');
            else { showToast('Действие выполнено!', 'success'); if (typeof loadBotData === 'function') loadBotData(); }
        }).catch(() => showToast('Ошибка сети', 'error'));
    }

    // --- ОБНОВЛЕНИЕ БЛОКА ТИКЕТОВ И РОЗЫГРЫШЕЙ (AJAX + JSON) ---
    function loadBotData() {
        fetch('admin.php?ajax_bot_data=1')
        .then(r => r.json())
        .then(data => {
            // Обновляем Заявки
            const tickContainer = document.getElementById('botTicketsFeed');
            if (tickContainer) {
                const activeEl = document.activeElement;
                let activeId = null, activeVal = '', cursorStart = 0;
                
                // Сохраняем фокус, если ты пишешь ответ во время авто-обновления
                if (activeEl && activeEl.tagName === 'TEXTAREA' && tickContainer.contains(activeEl)) {
                    activeId = activeEl.id; activeVal = activeEl.value; cursorStart = activeEl.selectionStart;
                }
                
                if (tickContainer.dataset.lasthtml !== data.tickets) {
                    tickContainer.innerHTML = data.tickets;
                    tickContainer.dataset.lasthtml = data.tickets;
                    if (activeId) {
                        const newEl = document.getElementById(activeId);
                        if (newEl) { newEl.focus(); newEl.value = activeVal; newEl.setSelectionRange(cursorStart, cursorStart); }
                    }
                }
            }
            
            // Обновляем Розыгрыши (Live счетчик)
            const gwContainer = document.getElementById('botGiveawaysFeed');
            if (gwContainer && gwContainer.dataset.lasthtml !== data.giveaways) {
                gwContainer.innerHTML = data.giveaways;
                gwContainer.dataset.lasthtml = data.giveaways;
            }
        }).catch(err => console.log('AJAX Sync Error', err));
    }

    // Запускаем синхронизацию
    if (window.location.search.includes('tab=bot')) {
        setInterval(loadBotData, 3000);
    }
</script>

</body>
</html>