<?php
if (session_status() === PHP_SESSION_NONE) {
    session_start();
}

// База данных пользователей (Логин => Пароль)
$allowed_users = [
    "Roma_07b" => "98322268",
    "Petryha"  => "Petryha_18.08.2003_+375291759772"
];

// Автономные функции работы с JSON
if (!function_exists('load_json')) {
    function load_json($file, $default = []) {
        if (!file_exists($file)) {
            file_put_contents($file, json_encode($default, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT));
            return $default;
        }
        return json_decode(file_get_contents($file), true) ?: $default;
    }
}
if (!function_exists('save_json')) {
    function save_json($file, $data) {
        file_put_contents($file, json_encode($data, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT));
    }
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

// Функция сжатия и конвертации изображений в WebP
function process_and_compress_image($file_info) {
    $dir = "uploads/icons/";
    if (!is_dir($dir)) mkdir($dir, 0755, true);
    
    $extension = strtolower(pathinfo($file_info['name'], PATHINFO_EXTENSION));
    $target_path = $dir . time() . "_" . uniqid() . ".webp";
    
    switch ($extension) {
        case 'jpeg':
        case 'jpg': $img = @imagecreatefromjpeg($file_info['tmp_name']); break;
        case 'png': $img = @imagecreatefrompng($file_info['tmp_name']); break;
        case 'webp': $img = @imagecreatefromwebp($file_info['tmp_name']); break;
        default: return "";
    }
    
    if (!$img) return "";

    $width = imagesx($img);
    $height = imagesy($img);
    $max_size = 200;
    
    if ($width > $max_size || $height > $max_size) {
        if ($width > $height) {
            $new_width = $max_size;
            $new_height = floor($height * ($max_size / $width));
        } else {
            $new_height = $max_size;
            $new_width = floor($width * ($max_size / $height));
        }
        $tmp_img = imagecreatetruecolor($new_width, $new_height);
        imagealphablending($tmp_img, false);
        imagesavealpha($tmp_img, true);
        imagecopyresampled($tmp_img, $img, 0, 0, 0, 0, $new_width, $new_height, $width, $height);
        imagedestroy($img);
        $img = $tmp_img;
    }

    imagewebp($img, $target_path, 80);
    imagedestroy($img);
    return $target_path;
}

// Авторизация
if (isset($_POST['admin_login'])) {
    $username = trim($_POST['username']);
    $password = $_POST['password'];
    
    if (isset($allowed_users[$username]) && $allowed_users[$username] === $password) {
        $_SESSION['rteam_logged_in'] = true;
        $_SESSION['rteam_user'] = $username;
        header("Location: " . $_SERVER['PHP_SELF'] . "?mode=admin");
        exit;
    }
    header("Location: " . $_SERVER['PHP_SELF']);
    exit;
}

// Выход
if (isset($_GET['logout'])) {
    unset($_SESSION['rteam_logged_in']);
    unset($_SESSION['rteam_user']);
    header("Location: " . $_SERVER['PHP_SELF']);
    exit;
}

$is_admin = (!empty($_SESSION['rteam_logged_in']) && isset($_GET['mode']) && $_GET['mode'] === 'admin');
$user = $_SESSION['rteam_user'] ?? "Admin";

$projects = load_json("projects.json", []);

// Настройка редактируемого проекта (если выбран)
$edit_project = null;
if ($is_admin && isset($_GET['edit_id'])) {
    foreach ($projects as $p) {
        if ((string)$p['id'] === (string)$_GET['edit_id']) {
            $edit_project = $p;
            break;
        }
    }
}

// ОБРАБОТКА ДЕЙСТВИЙ АДМИНИСТРАТОРА
if ($is_admin && $_SERVER["REQUEST_METHOD"] === "POST" && isset($_POST["action"])) {
    
    // Добавление нового проекта
    if ($_POST["action"] === "add_project") {
        $title = trim($_POST["title"]);
        $desc = trim($_POST["description"]);
        $type = $_POST["type"];
        $path = "";
        $icon_path = "";

        if (isset($_FILES["icon"]) && $_FILES["icon"]['error'] === UPLOAD_ERR_OK) {
            $icon_path = process_and_compress_image($_FILES["icon"]);
        }

        if ($type === "link") {
            $path = filter_var($_POST["link"], FILTER_SANITIZE_URL);
        } elseif (in_array($type, ["file", "zip_view"]) && isset($_FILES["file"])) {
            $dir = "uploads/files/";
            if (!is_dir($dir)) mkdir($dir, 0755, true);
            $path = $dir . time() . "_" . basename($_FILES["file"]["name"]);
            move_uploaded_file($_FILES["file"]["tmp_name"], $path);
        } elseif ($type === "site" && isset($_FILES["file"])) {
            $zip = new ZipArchive;
            if ($zip->open($_FILES["file"]["tmp_name"]) === TRUE) {
                $site_dir = "uploads/sites/" . time() . "_" . uniqid() . "/";
                if (!is_dir($site_dir)) mkdir($site_dir, 0755, true);
                
                $zip->extractTo($site_dir);
                $zip->close();
                
                $entry_point = $site_dir; 
                $iterator = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($site_dir));
                foreach ($iterator as $file) {
                    $filename = strtolower($file->getFilename());
                    if ($filename === 'index.html' || $filename === 'index.php') {
                        $entry_point = str_replace("\\", "/", $file->getPathname());
                        break;
                    }
                }
                $path = $entry_point;
            }
        }

        if (!empty($title) && !empty($path)) {
            $projects[] = [
                "id" => time(),
                "title" => $title,
                "description" => $desc,
                "type" => $type,
                "path" => $path,
                "icon" => $icon_path,
                "hidden" => false
            ];
            save_json("projects.json", $projects);
        }
        header("Location: " . $_SERVER['PHP_SELF'] . "?mode=admin"); exit;
    }

    // Редактирование существующего проекта
    if ($_POST["action"] === "edit_project") {
        $id = $_POST["id"];
        foreach ($projects as &$p) {
            if ((string)$p["id"] === (string)$id) {
                $p["title"] = trim($_POST["title"]);
                $p["description"] = trim($_POST["description"]);
                
                // Изменение иконки, если загружена новая
                if (isset($_FILES["icon"]) && $_FILES["icon"]['error'] === UPLOAD_ERR_OK) {
                    if (!empty($p["icon"]) && file_exists($p["icon"])) {
                        unlink($p["icon"]);
                    }
                    $p["icon"] = process_and_compress_image($_FILES["icon"]);
                }

                // Изменение контента в зависимости от типа управления
                $type_changed = ($p["type"] !== $_POST["type"]);
                $file_uploaded = (isset($_FILES["file"]) && $_FILES["file"]['error'] === UPLOAD_ERR_OK);

                if ($type_changed || $file_uploaded) {
                    // Старый файл удаляем при смене структуры
                    if (in_array($p["type"], ["file", "zip_view"]) && file_exists($p["path"])) {
                        unlink($p["path"]);
                    }
                    if ($p["type"] === "site") {
                        preg_match('/uploads\/sites\/[^\/]+\//', $p["path"] . '/', $matches);
                        if (!empty($matches[0]) && is_dir($matches[0])) {
                            delete_directory($matches[0]);
                        }
                    }

                    $p["type"] = $_POST["type"];

                    if ($p["type"] === "link") {
                        $p["path"] = filter_var($_POST["link"], FILTER_SANITIZE_URL);
                    } elseif (in_array($p["type"], ["file", "zip_view"]) && $file_uploaded) {
                        $dir = "uploads/files/";
                        if (!is_dir($dir)) mkdir($dir, 0755, true);
                        $p["path"] = $dir . time() . "_" . basename($_FILES["file"]["name"]);
                        move_uploaded_file($_FILES["file"]["tmp_name"], $p["path"]);
                    } elseif ($p["type"] === "site" && $file_uploaded) {
                        $zip = new ZipArchive;
                        if ($zip->open($_FILES["file"]["tmp_name"]) === TRUE) {
                            $site_dir = "uploads/sites/" . time() . "_" . uniqid() . "/";
                            if (!is_dir($site_dir)) mkdir($site_dir, 0755, true);
                            
                            $zip->extractTo($site_dir);
                            $zip->close();
                            
                            $entry_point = $site_dir; 
                            $iterator = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($site_dir));
                            foreach ($iterator as $file) {
                                $filename = strtolower($file->getFilename());
                                if ($filename === 'index.html' || $filename === 'index.php') {
                                    $entry_point = str_replace("\\", "/", $file->getPathname());
                                    break;
                                }
                            }
                            $p["path"] = $entry_point;
                        }
                    }
                } elseif ($p["type"] === "link" && isset($_POST["link"])) {
                    $p["path"] = filter_var($_POST["link"], FILTER_SANITIZE_URL);
                }
                break;
            }
        }
        unset($p);
        save_json("projects.json", $projects);
        header("Location: " . $_SERVER['PHP_SELF'] . "?mode=admin"); exit;
    }
    
    // Скрытие / Показ проекта
    if ($_POST["action"] === "toggle_project") {
        $id = $_POST["id"];
        foreach ($projects as &$p) {
            if ((string)$p["id"] === (string)$id) {
                $p["hidden"] = !empty($p["hidden"]) ? false : true;
                break;
            }
        }
        unset($p);
        save_json("projects.json", $projects);
        header("Location: " . $_SERVER['PHP_SELF'] . "?mode=admin"); exit;
    }
    
    // Удаление проекта
    if ($_POST["action"] === "del_project") {
        $id = $_POST["id"];
        $new = [];
        foreach ($projects as $p) {
            if ((string)$p["id"] === (string)$id) {
                if (in_array($p["type"], ["file", "zip_view"]) && file_exists($p["path"])) {
                    unlink($p["path"]);
                }
                if ($p["type"] === "site") {
                    preg_match('/uploads\/sites\/[^\/]+\//', $p["path"] . '/', $matches);
                    if (!empty($matches[0]) && is_dir($matches[0])) {
                        delete_directory($matches[0]);
                    }
                }
                if (!empty($p["icon"]) && file_exists($p["icon"])) unlink($p["icon"]);
                continue;
            }
            $new[] = $p;
        }
        $projects = $new;
        save_json("projects.json", $projects);
        header("Location: " . $_SERVER['PHP_SELF'] . "?mode=admin"); exit;
    }
}

// ОБРАБОТКА ИНСПЕКТОРА ZIP (ДЛЯ ПОЛЬЗОВАТЕЛЕЙ)
$zip_files_list = [];
$viewing_zip_title = "";
if (isset($_GET['view_zip']) && file_exists($_GET['view_zip'])) {
    $zip_path = $_GET['view_zip'];
    foreach($projects as $p) {
        if($p['path'] === $zip_path) { $viewing_zip_title = $p['title']; break; }
    }
    $zip = new ZipArchive;
    if ($zip->open($zip_path) === TRUE) {
        for ($i = 0; $i < $zip->numFiles; $i++) {
            $zip_files_list[] = $zip->getNameIndex($i);
        }
        $zip->close();
    }
}
?>
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<title><?= $is_admin ? "Панель управления — Rteam" : "Rteam проекты" ?></title>
<style>
body { margin: 0; font-family: "Segoe UI", Arial, sans-serif; background: #050509; color: #eee; }
.wrap { max-width: 1100px; margin: 30px auto; padding: 0 16px 40px; }
h1 { color: #ff2a2a; text-shadow: 0 0 12px #ff000066; margin-bottom: 10px; }
a { color:#ff7777; text-decoration:none; }
.card { background: #101018; border: 1px solid #2a0000; padding: 18px; border-radius: 10px; margin-top: 15px; }
.card h3 { margin: 0 0 6px; color: #ff2a2a; }
.meta { font-size: 13px; color: #aaa; margin-bottom: 6px; }
textarea, input, select { width: 100%; padding: 8px; border-radius: 8px; border: 1px solid #333; background: #050509; color: #fff; resize: none; font-size: 13px; margin-top: 6px; box-sizing: border-box; }
textarea { height: 80px; }
.btn { display: inline-block; margin-top: 8px; padding: 7px 14px; border-radius: 8px; border: none; cursor: pointer; font-size: 13px; text-decoration: none; font-weight: bold; text-align: center; }
.ok { background:#1f9d55; color:#fff; }
.no { background:#c53030; color:#fff; }
.gray { background:#2d3748; color:#fff; }
.blue { background:#2b6cb0; color:#fff; }
.orange { background:#e67e22; color:#fff; }

/* Витрина */
.grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 20px; margin-top: 30px; }
.public-card { background: #101018; border: 1px solid #2a0000; padding: 20px; border-radius: 10px; display: flex; flex-direction: column; justify-content: space-between; transition: 0.3s; }
.public-card:hover { border-color: #ff2a2a; box-shadow: 0 0 10px #ff000033; }
.card-header-block { display: flex; gap: 12px; align-items: center; margin-bottom: 10px; }
.project-icon { width: 48px; height: 48px; border-radius: 8px; background: #050509; border: 1px solid #2a0000; object-fit: cover; }
.public-card h3 { margin: 0; color: #ff2a2a; font-size: 20px; }
.public-card p { color: #ccc; font-size: 14px; line-height: 1.6; margin: 0 0 20px 0; flex-grow: 1; display: -webkit-box; -webkit-line-clamp: 3; -webkit-box-orient: vertical; overflow: hidden; }
.public-btn { display: block; text-align: center; padding: 10px; border-radius: 8px; font-weight: bold; text-decoration: none; font-size: 14px; margin-top: 5px; }

/* Модалки */
.modal { display: none; position: fixed; z-index: 999; left: 0; top: 0; width: 100%; height: 100%; background-color: rgba(0,0,0,0.85); align-items: center; justify-content: center; }
.modal-content { background: #101018; border: 2px solid #ff2a2a; padding: 30px; border-radius: 12px; width: 100%; max-width: 450px; box-shadow: 0 0 20px #ff000044; position: relative; color: #fff; }
.close-modal { position: absolute; right: 14px; top: 10px; color: #aaa; font-size: 24px; cursor: pointer; }
.close-modal:hover { color: #ff2a2a; }

/* Детализация внутри модалки */
#modalDesc { line-height: 1.6; font-size: 14px; color: #ddd; white-space: pre-wrap; margin-top: 15px; background: #050509; padding: 12px; border-radius: 8px; border: 1px solid #222; max-height: 200px; overflow-y: auto; }

/* Списки файлов */
.zip-list { background: #050509; border: 1px solid #222; border-radius: 8px; padding: 10px; max-height: 300px; overflow-y: auto; text-align: left; margin-top: 10px;}
.zip-item { font-family: monospace; font-size: 12px; padding: 4px 0; color: #ff7777; border-bottom: 1px solid #111; }
</style>

<script>
function updateFormType(prefix = '') {
    var type = document.getElementById(prefix + 'project_type').value;
    var admLink = document.getElementById(prefix + 'adm_link');
    var admFile = document.getElementById(prefix + 'adm_file');
    var fileLabel = document.getElementById(prefix + 'file_label');
    var fileInput = document.getElementById(prefix + 'file_input');

    if (type === 'link') {
        admLink.style.display = 'block';
        admFile.style.display = 'none';
        fileInput.removeAttribute('required');
    } else {
        admLink.style.display = 'none';
        admFile.style.display = 'block';
        
        if (prefix === '') {
            fileInput.setAttribute('required', 'true');
        }

        if (type === 'site') {
            fileLabel.innerText = 'Выберите ZIP-архив с сайтом (авто-распаковка на сервере)';
            fileInput.setAttribute('accept', '.zip');
        } else if (type === 'zip_view') {
            fileLabel.innerText = 'Выберите ZIP-архив для просмотра содержимого онлайн';
            fileInput.setAttribute('accept', '.zip');
        } else {
            fileLabel.innerText = 'Выберите любой файл (Если это .EXE, пользователь сможет его скачать)';
            fileInput.removeAttribute('accept');
        }
    }
}

function openInfoModal(title, description, iconHtml, actionButtonHtml) {
    document.getElementById('modalTitle').innerText = title;
    document.getElementById('modalDesc').innerHTML = description;
    document.getElementById('modalIconContainer').innerHTML = iconHtml;
    document.getElementById('modalActionContainer').innerHTML = actionButtonHtml;
    document.getElementById('infoModal').style.display = 'flex';
}

window.onload = function() { 
    if(document.getElementById('project_type')) updateFormType(''); 
    if(document.getElementById('edit_project_type')) updateFormType('edit_'); 
}
</script>
</head>
<body>
<div class="wrap">

    <!-- ОКНО ПРОСМОТРА ZIP -->
    <?php if (!empty($zip_files_list)): ?>
        <div class="card" style="border-color: #e67e22; margin-bottom: 30px;">
            <a href="<?= $_SERVER['PHP_SELF'] ?>" class="btn gray" style="float: right; margin-top:0;">Закрыть просмотр</a>
            <h3 style="color: #e67e22;">Содержимое архива: <?= htmlspecialchars($viewing_zip_title) ?></h3>
            <p style="font-size: 13px; color: #aaa; margin: 5px 0;">Ниже представлен список файлов, находящихся внутри архива:</p>
            <div class="zip-list">
                <?php foreach($zip_files_list as $file_in_zip): ?>
                    <div class="zip-item">📄 <?= htmlspecialchars($file_in_zip) ?></div>
                <?php endforeach; ?>
            </div>
            <a href="<?= htmlspecialchars($zip_path) ?>" download class="btn orange" style="margin-top:12px;">Скачать весь архив целиком</a>
        </div>
    <?php endif; ?>

    <?php if ($is_admin): ?>
        <!-- ========================================== -->
        <!-- РЕЖИМ АДМИНИСТРАТОРА: УПРАВЛЕНИЕ ПРОЕКТАМИ -->
        <!-- ========================================== -->
        <div style="display:flex; justify-content:space-between; align-items:center; border-bottom:1px solid #2a0000; padding-bottom:15px; margin-bottom:20px;">
            <div>
                <h1 style="margin:0;">Панель управления</h1>
                <p style="margin:5px 0 0 0; font-size:14px; color:#aaa;">Вы вошли как: <b style="color:#ff2a2a;"><?=htmlspecialchars($user)?></b></p>
            </div>
            <div style="display:flex; gap:10px;">
                <a href="<?= $_SERVER['PHP_SELF'] ?>" class="btn gray" style="padding:10px 16px;">Перейти на витрину</a>
                <a href="?logout=1" class="btn no" style="padding:10px 16px;">Выйти</a>
            </div>
        </div>

        <?php if ($edit_project): ?>
            <!-- ФОРМА РЕДАКТИРОВАНИЯ ПРОЕКТА -->
            <h2 style="color: #e67e22;">Редактировать проект: <?= htmlspecialchars($edit_project['title']) ?></h2>
            <form action="" method="POST" enctype="multipart/form-data" class="card" style="max-width: 600px; margin-bottom:40px; border-color: #e67e22;">
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
                    <a href="?mode=admin" class="btn gray" style="padding:10px 20px;">Отмена</a>
                </div>
            </form>
        <?php else: ?>
            <!-- ФОРМА ДОБАВЛЕНИЯ ПРОЕКТА -->
            <h2>Добавить новый проект</h2>
            <form action="" method="POST" enctype="multipart/form-data" class="card" style="max-width: 600px; margin-bottom:40px;">
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
                
                <button class="btn ok" type="submit" style="margin-top:15px; padding:10px 20px;">Опубликовать проект</button>
            </form>
        <?php endif; ?>

        <h2>Управление загрузками</h2>
        <?php if (empty($projects)): ?>
            <p style="color:#666;">Список пуст.</p>
        <?php else: ?>
            <?php foreach (array_reverse($projects, true) as $key => $p): ?>
                <div class="card" style="<?= !empty($p['hidden']) ? 'opacity:0.4; border-color:#444;' : '' ?>">
                    <div style="float: right; display: flex; gap: 5px;">
                        <a href="?mode=admin&edit_id=<?=$p['id']?>" class="btn blue">Редактировать</a>
                        <form action="" method="POST" style="display:inline;">
                            <input type="hidden" name="action" value="toggle_project"><input type="hidden" name="id" value="<?=$p['id']?>">
                            <button class="btn gray" type="submit"><?= !empty($p['hidden']) ? 'Показать' : 'Скрыть' ?></button>
                        </form>
                        <form action="" method="POST" style="display:inline;" onsubmit="return confirm('Удалить проект и все его файлы безвозвратно?');">
                            <input type="hidden" name="action" value="del_project"><input type="hidden" name="id" value="<?=$p['id']?>">
                            <button class="btn no" type="submit">Удалить</button>
                        </form>
                    </div>
                    <div style="display:flex; gap:12px; align-items:center;">
                        <?php if(!empty($p['icon']) && file_exists($p['icon'])): ?>
                            <img src="<?=$p['icon']?>" style="width:44px; height:44px; object-fit:cover; border-radius:8px; border:1px solid #2a0000;">
                        <?php endif; ?>
                        <h3 style="margin:0; font-size:18px;"><?=htmlspecialchars($p['title'])?></h3>
                    </div>
                    <p style="color:#ccc; font-size:14px; margin:10px 0; max-width:70%;"><?=nl2br(htmlspecialchars($p['description']))?></p>
                    <div class="meta">
                        Тип: <b>
                        <?php 
                            if($p['type']==='link') echo 'Ссылка (Перейти)';
                            elseif($p['type']==='site') echo 'Распакованный сайт (Открыть)';
                            elseif($p['type']==='zip_view') echo 'ZIP-архив (Просмотр содержимого)';
                            else echo 'Файл / Программа';
                        ?>
                        </b> | Путь: <code style="color:#ff7777; word-break: break-all;"><?=htmlspecialchars($p['path'])?></code>
                    </div>
                </div>
            <?php endforeach; ?>
        <?php endif; ?>

    <?php else: ?>
        <!-- ========================================== -->
        <!-- РЕЖИМ ПОЛЬЗОВАТЕЛЯ: ПУБЛИЧНАЯ ВИТРИНА      -->
        <!-- ========================================== -->
        <div style="display:flex; justify-content:space-between; align-items:center; border-bottom:1px solid #2a0000; padding-bottom:15px;">
            <h1 style="margin:0;">Rteam проекты</h1>
            <div style="display:flex; gap:10px;">
                <a href="https://rteam.info" target="_blank" class="btn blue" style="padding:8px 14px;">Наш сайт rteam.info</a>
                <?php if (!empty($_SESSION['rteam_logged_in'])): ?>
                    <a href="?mode=admin" class="btn ok" style="padding:8px 14px;">Панель управления</a>
                <?php else: ?>
                    <button class="btn gray" onclick="document.getElementById('loginModal').style.display='flex'" style="padding:8px 14px;">Войти</button>
                <?php endif; ?>
            </div>
        </div>
        
        <div class="grid">
            <?php 
            $visible_count = 0;
            foreach ($projects as $p): 
                if (!empty($p["hidden"])) continue; 
                $visible_count++;
                
                $file_ext = strtolower(pathinfo($p["path"], PATHINFO_EXTENSION));
                
                // Генерируем HTML иконки и целевой кнопки для передачи в JavaScript модального окна
                if (!empty($p["icon"]) && file_exists($p["icon"])) {
                    $icon_html = '<img src="' . htmlspecialchars($p["icon"]) . '" class="project-icon" alt="icon">';
                } else {
                    $letter = ($file_ext === 'exe') ? 'EXE' : (($p['type'] === 'zip_view') ? 'ZIP' : 'R');
                    $icon_html = '<div class="project-icon" style="display:flex; align-items:center; justify-content:center; color:#ff2a2a; font-weight:bold; font-size:18px;">'.$letter.'</div>';
                }

                if ($p["type"] === "link") {
                    $action_btn_html = '<a href="' . htmlspecialchars($p["path"]) . '" target="_blank" class="public-btn ok">Перейти</a>';
                } elseif ($p["type"] === "site") {
                    $action_btn_html = '<a href="' . htmlspecialchars($p["path"]) . '" target="_blank" class="public-btn ok">Открыть сайт</a>';
                } elseif ($p["type"] === "zip_view") {
                    $action_btn_html = '<a href="?view_zip=' . urlencode($p["path"]) . '" class="public-btn orange">Открыть архив (Онлайн)</a><a href="' . htmlspecialchars($p["path"]) . '" download class="public-btn gray" style="font-size:12px;">Скачать ZIP</a>';
                } else {
                    if ($file_ext === 'exe') {
                        $action_btn_html = '<a href="' . htmlspecialchars($p["path"]) . '" download class="public-btn no">Скачать и запустить (.exe)</a>';
                    } else {
                        $action_btn_html = '<a href="' . htmlspecialchars($p["path"]) . '" download class="public-btn blue">Скачать файл</a>';
                    }
                }
            ?>
                <div class="public-card">
                    <div>
                        <div class="card-header-block">
                            <?= $icon_html ?>
                            <h3><?= htmlspecialchars($p["title"]) ?></h3>
                        </div>
                        <p><?= nl2br(htmlspecialchars($p["description"])) ?></p>
                    </div>

                    <div style="margin-top: 15px;">
                        <!-- КНОПКА ПОДРОБНЕЕ -->
                        <button class="public-btn blue" style="width: 100%; box-sizing: border-box; cursor: pointer;" 
                                onclick="openInfoModal(
                                    '<?= htmlspecialchars(addslashes($p['title'])) ?>', 
                                    '<?= htmlspecialchars(addslashes(nl2br($p['description']))) ?>', 
                                    '<?= htmlspecialchars(addslashes($icon_html)) ?>', 
                                    '<?= htmlspecialchars(addslashes($action_btn_html)) ?>'
                                )">Подробнее</button>
                    </div>
                </div>
            <?php endforeach; ?>

            <?php if ($visible_count === 0): ?>
                <p style="text-align:center; grid-column: 1/-1; color:#555; font-size:16px; margin-top:40px;">Активных проектов в настоящее время не опубликовано.</p>
            <?php endif; ?>
        </div>

        <!-- МОДАЛЬНОЕ ОКНО «ПОДРОБНЕЕ» -->
        <div id="infoModal" class="modal" onclick="if(event.target===this) this.style.display='none'">
            <div class="modal-content" style="max-width: 550px;">
                <span class="close-modal" onclick="document.getElementById('infoModal').style.display='none'">&times;</span>
                
                <div style="display: flex; gap: 15px; align-items: center; border-bottom: 1px solid #2a0000; padding-bottom: 15px;">
                    <div id="modalIconContainer"></div>
                    <h2 id="modalTitle" style="margin: 0; color: #ff2a2a;"></h2>
                </div>
                
                <div id="modalDesc"></div>
                
                <div id="modalActionContainer" style="margin-top: 20px;"></div>
            </div>
        </div>

        <!-- МОДАЛКА АВТОРИЗАЦИИ -->
        <div id="loginModal" class="modal" onclick="if(event.target===this) this.style.display='none'">
            <div class="modal-content" style="max-width: 340px;">
                <span class="close-modal" onclick="document.getElementById('loginModal').style.display='none'">&times;</span>
                <h3 style="color:#ff2a2a; margin-top:0; text-shadow: 0 0 8px rgba(255,0,0,0.3); font-size:20px;">Авторизация Rteam</h3>
                <form action="" method="POST">
                    <label style="font-size:12px; color:#aaa;">Логин</label>
                    <input type="text" name="username" placeholder="Введите логин" required autocomplete="username">
                    
                    <label style="display:block; margin-top:10px; font-size:12px; color:#aaa;">Пароль</label>
                    <input type="password" name="password" placeholder="Введите пароль" required autocomplete="current-password">
                    
                    <button class="btn ok" name="admin_login" type="submit" style="width:100%; margin-top:18px; padding:10px; font-size:14px;">Выполнить вход</button>
                </form>
            </div>
        </div>
    <?php endif; ?>

</div>
</body>
</html>