<?php
if (!defined('RTEAM_ADMIN')) exit;

/* Маты для авто-мута */
$BAD_WORDS = [
    'хуй','хуя','пизд','еба','ёба','ебл','бля','бляд','сука','суки','мразь','пидор','пидр'
];

function has_bad_words($text) {
    global $BAD_WORDS;
    $low = mb_strtolower($text);
    foreach ($BAD_WORDS as $w) {
        if (mb_strpos($low, $w) !== false) return true;
    }
    return false;
}

/* JSON-хранилища */
function mail_load_json($file, $default) {
    if (!file_exists($file)) {
        file_put_contents($file, json_encode($default, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE));
    }
    $data = json_decode(file_get_contents($file), true);
    return $data ?: $default;
}
function mail_save_json($file, $data) {
    file_put_contents($file, json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE));
}

/* Муты и прочитанные */
$mutes      = mail_load_json(__DIR__ . "/mutes.json", []);
$read_mails = mail_load_json(__DIR__ . "/read_mails.json", []);

function set_mute($email, $minutes) {
    global $mutes;
    $until = time() + $minutes * 60;
    $mutes[$email] = $until;
    mail_save_json(__DIR__ . "/mutes.json", $mutes);
}
function is_muted($email) {
    global $mutes;
    if (!isset($mutes[$email])) return false;
    return time() < $mutes[$email];
}

/* Авто-мут на 3 дня за мат */
function auto_mute_if_needed($email, $text) {
    if (!$email) return;
    if (!has_bad_words($text)) return;
    if (is_muted($email)) return;
    set_mute($email, 3 * 24 * 60);
}

/* Декодирование IMAP-текста */
function decode_imap_text($text, $encoding) {
    $encoding = strtolower($encoding ?? '');
    if ($encoding === 'base64') {
        return base64_decode($text);
    }
    if ($encoding === 'quoted-printable') {
        return quoted_printable_decode($text);
    }
    return $text;
}

/* Кликабельные ссылки: показываем слово "ссылка" */
function make_links_clickable($text) {
    $pattern = '~(https?://[^\s<]+)~iu';
    return preg_replace($pattern, '<a href="$1" target="_blank">ссылка</a>', $text);
}

/* Чтение писем с IMAP */
function fetch_mail() {
    $mailbox = "{imap.mail.ru:993/imap/ssl}INBOX";
    $login   = "rtcccv@bk.ru";
    $pass    = "HJZpT4HHP9cxq2TmRgNt";

    $inbox = @imap_open($mailbox, $login, $pass);
    if (!$inbox) return [];

    $emails = imap_search($inbox, 'ALL');
    $list = [];

    if ($emails) {
        rsort($emails);

        foreach ($emails as $num) {
            $header = imap_headerinfo($inbox, $num);
            $from   = $header->from[0] ?? null;
            if (!$from) continue;

            $email = ($from->mailbox ?? '') . '@' . ($from->host ?? '');
            $name  = trim($from->personal ?? '') ?: $email;

            $structure = imap_fetchstructure($inbox, $num);
            $bodyHtml  = '';
            $bodyText  = '';
            $attachments = [];

            if (!isset($structure->parts)) {
                $raw = imap_body($inbox, $num);
                $decoded = decode_imap_text($raw, $structure->encoding ?? 0);
                if (strtoupper($structure->subtype ?? '') === 'HTML') {
                    $bodyHtml = $decoded;
                } else {
                    $bodyText = nl2br(htmlspecialchars($decoded));
                }
            } else {
                foreach ($structure->parts as $i => $part) {
                    $partNum = $i + 1;
                    $isAttachment = false;
                    $filename = '';

                    if (!empty($part->dparameters)) {
                        foreach ($part->dparameters as $dp) {
                            if (strtolower($dp->attribute) === 'filename') {
                                $isAttachment = true;
                                $filename = $dp->value;
                            }
                        }
                    }
                    if (!$filename && !empty($part->parameters)) {
                        foreach ($part->parameters as $p) {
                            if (strtolower($p->attribute) === 'name') {
                                $isAttachment = true;
                                $filename = $p->value;
                            }
                        }
                    }

                    if ($isAttachment) {
                        $attachments[] = [
                            'part' => $partNum,
                            'name' => $filename
                        ];
                        continue;
                    }

                    $partBody = imap_fetchbody($inbox, $num, $partNum);
                    $decoded  = decode_imap_text($partBody, $part->encoding ?? 0);

                    $subtype = strtoupper($part->subtype ?? '');
                    if ($subtype === 'HTML') {
                        if ($bodyHtml === '') $bodyHtml = $decoded;
                    } elseif ($subtype === 'PLAIN') {
                        if ($bodyText === '') $bodyText = nl2br(htmlspecialchars($decoded));
                    }
                }
            }

            $body = $bodyHtml ?: $bodyText ?: '(Пустое письмо)';
            $body = make_links_clickable($body);

            $list[] = [
                'id'          => $num,
                'name'        => $name,
                'email'       => $email,
                'text'        => $body,
                'time'        => date('Y-m-d H:i:s', $header->udate ?? time()),
                'attachments' => $attachments
            ];
        }
    }

    imap_close($inbox);
    return $list;
}
