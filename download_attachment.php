<?php
session_start();
define('RTEAM_ADMIN', true);

if (!isset($_SESSION['role']) || ($_SESSION['role'] !== 'Главный разработчик' && $_SESSION['role'] !== 'Администратор')) {
    http_response_code(403);
    exit('Доступ запрещён');
}

$msg  = intval($_GET['msg'] ?? 0);
$part = intval($_GET['part'] ?? 0);
$name = $_GET['name'] ?? 'file';

if ($msg <= 0 || $part <= 0) {
    exit('Неверные параметры');
}

$mailbox = "{imap.mail.ru:993/imap/ssl}INBOX";
$login   = "rtcccv@bk.ru";
$pass    = "HJZpT4HHP9cxq2TmRgNt";

$inbox = @imap_open($mailbox, $login, $pass);
if (!$inbox) {
    exit('Не удалось открыть почтовый ящик');
}

$structure = imap_fetchstructure($inbox, $msg);
if (!isset($structure->parts[$part-1])) {
    imap_close($inbox);
    exit('Вложение не найдено');
}

$partStruct = $structure->parts[$part-1];
$body = imap_fetchbody($inbox, $msg, $part);
$encoding = strtolower($partStruct->encoding ?? 0);

if ($encoding == 3) { // base64
    $body = base64_decode($body);
} elseif ($encoding == 4) { // quoted-printable
    $body = quoted_printable_decode($body);
}

imap_close($inbox);

header('Content-Description: File Transfer');
header('Content-Type: application/octet-stream');
header('Content-Disposition: attachment; filename="'.basename($name).'"');
header('Content-Length: ' . strlen($body));

echo $body;
