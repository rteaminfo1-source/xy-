<?php
if (!defined('RTEAM_ADMIN')) exit;

require_once __DIR__ . '/mail_core.php';
require_once __DIR__ . '/mail_filters.php';

$mail_search = trim($_GET['mail_search'] ?? '');
$mail_filter = $_GET['mail_filter'] ?? 'all';

$mail_list = fetch_mail();
$mail_list = filter_and_search_mail($mail_list, $mail_search, $mail_filter);
?>

<div class="card">
    <h3>Поиск и фильтр писем</h3>
    <form method="GET" class="search-bar">
        <input type="hidden" name="tab" value="messages">
        <div>
            <label>Поиск</label><br>
            <input type="text" name="mail_search" value="<?=htmlspecialchars($mail_search)?>" placeholder="Email, ник или текст">
        </div>
        <div>
            <label>Фильтр</label><br>
            <select name="mail_filter">
                <option value="all"   <?=$mail_filter==='all'?'selected':''?>>Все</option>
                <option value="new"   <?=$mail_filter==='new'?'selected':''?>>Только новые</option>
                <option value="read"  <?=$mail_filter==='read'?'selected':''?>>Только прочитанные</option>
                <option value="muted" <?=$mail_filter==='muted'?'selected':''?>>Только в муте</option>
            </select>
        </div>
        <div style="align-self:flex-end;">
            <button class="btn gray" type="submit">Применить</button>
        </div>
    </form>
</div>

<?php if (!$mail_list): ?>
    <p>Писем по заданным условиям нет.</p>
<?php else: ?>
    <?php foreach ($mail_list as $m): ?>
        <?php
        $id    = (string)$m["id"];
        $email = $m["email"];
        $isRead = !empty($read_mails[$id]);
        $muted  = is_muted($email);
        ?>
        <div class="card">
            <h3><?=htmlspecialchars($m["name"])?></h3>
            <div class="meta">
                Email: <?=htmlspecialchars($email)?> |
                <?=htmlspecialchars($m["time"])?> |
                Статус: <?= $isRead ? "Прочитано" : "Новое" ?> |
                <?= $muted ? "В муте (авто/ручной)" : "Без мута" ?>
            </div>

            <div style="margin-top:6px; background:#0a0a12; padding:10px; border-radius:8px; max-height:300px; overflow:auto;">
                <?= $m["text"] ?>
            </div>

            <?php if (!empty($m['attachments'])): ?>
                <div class="meta" style="margin-top:6px;">Вложения:</div>
                <?php foreach ($m['attachments'] as $att): ?>
                    <a class="btn gray" style="margin-right:6px;"
                       href="download_attachment.php?msg=<?=$m['id']?>&part=<?=$att['part']?>&name=<?=urlencode($att['name'])?>">
                        <?=htmlspecialchars($att['name'])?>
                    </a>
                <?php endforeach; ?>
            <?php endif; ?>

            <form method="POST" style="margin-top:10px;">
                <input type="hidden" name="action" value="reply_mail">
                <input type="hidden" name="email" value="<?=htmlspecialchars($email)?>">
                <textarea name="reply" id="reply_<?=$id?>" placeholder="Ваш ответ пользователю..." required></textarea>
                <button class="btn ok" type="submit" <?= $muted ? "disabled" : "" ?>>Ответить</button>
                <button class="btn gray" type="button"
                        onclick="quoteReply('reply_<?=$id?>', <?=json_encode(strip_tags($m['text']))?>)">
                    Ответить с цитированием
                </button>
            </form>

            <form method="POST" style="margin-top:6px;display:inline-block;">
                <input type="hidden" name="action" value="mark_read">
                <input type="hidden" name="id" value="<?=htmlspecialchars($id)?>">
                <button class="btn gray" type="submit">Отметить как прочитано</button>
            </form>

            <form method="POST" style="margin-top:6px;display:inline-block;">
                <input type="hidden" name="action" value="mute_user">
                <input type="hidden" name="email" value="<?=htmlspecialchars($email)?>">
                <input type="number" name="minutes" placeholder="Минуты мута" min="1" style="width:140px;">
                <button class="btn no" type="submit">Выдать мут</button>
            </form>
        </div>
    <?php endforeach; ?>
<?php endif; ?>
