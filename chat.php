<?php
session_start();
if (!isset($_SESSION["user"])) {
    header("Location: login.php");
    exit;
}
?>
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <title>Рабочая область Rai</title>
    <link rel="stylesheet" href="style.css">
</head>
<body>

<header class="header">
    <div class="logo">Rai</div>
    <nav>
        <a href="index.php">Главная</a>
        <a href="login.php">Выход</a>
    </nav>
</header>

<div class="chat-container">
    <div id="messages" class="messages"></div>

    <div class="input-row">
        <input id="userInput" type="text" placeholder="Введите запрос…">
        <button id="sendBtn" class="btn">➤</button>
    </div>
</div>

<script src="chat.js"></script>
</body>
</html>
