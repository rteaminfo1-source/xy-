<?php
if (!defined('RTEAM_ADMIN')) exit;

require_once __DIR__ . '/mail_core.php';

/* Обработка действий с письмами */
function handle_mail_actions(&$logs, $currentUser) {
    global $read_mails;

    if ($_SERVER['REQUEST_METHOD'] !== 'POST') return;
    $action = $_POST['action'] ?? '';

    /* Ответ на письмо */
    if ($action === 'reply_mail') {
        $to    = trim($_POST['email'] ?? '');
        $text  = trim($_POST['reply'] ?? '');
        $admin = $currentUser ?: 'Администратор';

        if ($to === '' || $text === '') {
            header("Location: admin.php?tab=messages");
            exit;
        }

        if (is_muted($to)) {
            header("Location: admin.php?tab=messages");
            exit;
        }

        $full = $text . "\n\nС уважением, Rteam\nАдминистратор: " . $admin;

        $headers  = "From: Rteam Support <rtcccv@bk.ru>\r\n";
        $headers .= "Content-Type: text/plain; charset=utf-8\r\n";

        mail($to, "Ответ от Rteam", $full, $headers);

        $logs[] = [
            "time" => date("Y-m-d H:i:s"),
            "type" => "mail_reply",
            "msg"  => "Ответ пользователю $to админом $admin"
        ];
        mail_save_json(__DIR__ . "/logs.json", $logs);

        header("Location: admin.php?tab=messages");
        exit;
    }

    /* Пометка прочитанным */
    if ($action === 'mark_read') {
        $id = $_POST['id'] ?? '';
        if ($id !== '') {
            $read_mails[$id] = true;
            mail_save_json(__DIR__ . "/read_mails.json", $read_mails);
        }
        header("Location: admin.php?tab=messages");
        exit;
    }

    /* Ручной мут */
    if ($action === 'mute_user') {
        $email   = trim($_POST['email'] ?? '');
        $minutes = intval($_POST['minutes'] ?? 0);
        if ($email !== '' && $minutes > 0) {
            set_mute($email, $minutes);
            $logs[] = [
                "time" => date("Y-m-d H:i:s"),
                "type" => "mail_mute",
                "msg"  => "Мут для $email на $minutes минут"
            ];
            mail_save_json(__DIR__ . "/logs.json", $logs);
        }
        header("Location: admin.php?tab=messages");
        exit;
    }
}
