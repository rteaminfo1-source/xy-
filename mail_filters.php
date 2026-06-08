<?php
if (!defined('RTEAM_ADMIN')) exit;

require_once __DIR__ . '/mail_core.php';

function filter_and_search_mail($mail_list, $mail_search, $mail_filter) {
    global $read_mails;

    // авто-мут за мат
    foreach ($mail_list as $mm) {
        auto_mute_if_needed($mm['email'], strip_tags($mm['text']));
    }

    // поиск
    if ($mail_search !== '') {
        $s = mb_strtolower($mail_search);
        $mail_list = array_filter($mail_list, function($m) use ($s) {
            $hay = mb_strtolower($m['email'].' '.$m['name'].' '.strip_tags($m['text']));
            return mb_strpos($hay, $s) !== false;
        });
    }

    // фильтр
    $mail_list = array_values($mail_list);
    $mail_list = array_filter($mail_list, function($m) use ($mail_filter, $read_mails) {
        $id = (string)$m['id'];
        $isRead = !empty($read_mails[$id]);
        $muted  = is_muted($m['email']);

        if ($mail_filter === 'new')   return !$isRead;
        if ($mail_filter === 'read')  return $isRead;
        if ($mail_filter === 'muted') return $muted;
        return true;
    });

    return array_values($mail_list);
}
