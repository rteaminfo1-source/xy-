<?php
session_start();
?>
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<title>Rteam — Контакты</title>
<style>
body{margin:0;font-family:"Segoe UI",Arial,sans-serif;background:#050509;color:#eee;}
main{max-width:600px;margin:40px auto 40px;padding:0 16px;}
h1{color:#ff2a2a;text-shadow:0 0 12px #ff000066;}
.box{background:#101018;border:1px solid #2a0000;border-radius:12px;padding:18px;box-shadow:0 0 18px #ff000022;}
input,textarea{width:100%;padding:9px;margin-top:8px;border-radius:8px;border:1px solid #333;background:#050509;color:#fff;font-size:14px;resize:none;}
textarea{height:120px;}
.btn{margin-top:12px;width:100%;padding:9px 14px;border-radius:8px;border:none;cursor:pointer;background:#ff2a2a;color:#fff;box-shadow:0 0 14px #ff000066;}
.btn:hover{background:#ff4444;}
p{color:#aaa;}
</style>
</head>
<body>
<main>
    <h1>Написать в Rteam</h1>
    <p>Ответ придёт на указанный email.</p>
    <div class="box">
        <form action="messages_send.php" method="POST">
            <input type="text" name="name" placeholder="Ваш ник / имя" required>
            <input type="email" name="email" placeholder="Ваш email" required>
            <textarea name="text" placeholder="Ваше сообщение..." required></textarea>
            <button class="btn" type="submit">Отправить</button>
        </form>
    </div>
</main>
</body>
</html>
