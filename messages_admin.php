<?php
session_start();

function load($f,$d){return file_exists($f)?json_decode(file_get_contents($f),true):$d;}
function save($f,$d){file_put_contents($f,json_encode($d,JSON_PRETTY_PRINT|JSON_UNESCAPED_UNICODE));}

$messages = load("messages.json",[]);
$q = trim($_GET["q"] ?? "");
if($q!==""){
    $s = mb_strtolower($q);
    $messages = array_filter($messages,function($m) use($s){
        $hay = mb_strtolower($m["name"]." ".$m["email"]." ".$m["text"]);
        return mb_strpos($hay,$s)!==false;
    });
}

$newCount = count(array_filter($messages,function($m){return $m["status"]==="new";}));
usort($messages,function($a,$b){return $b["id"]<=>$a["id"];});
?>
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<title>(<?=$newCount?>) Сообщения — Rteam</title>
<style>
body{margin:0;font-family:"Segoe UI",Arial,sans-serif;background:#050509;color:#eee;}
main{max-width:900px;margin:30px auto;padding:0 16px;}
.card{background:#101018;border:1px solid #2a0000;border-radius:12px;padding:14px;margin-top:12px;}
.btn{padding:6px 12px;border:none;border-radius:8px;cursor:pointer;font-size:13px;margin-right:6px;}
.ok{background:#2f855a;color:#fff;}
.no{background:#c53030;color:#fff;}
.gray{background:#2d3748;color:#fff;}
.badge{padding:2px 8px;border-radius:999px;font-size:11px;margin-left:6px;}
.badge-new{background:#2b6cb0;}
.badge-read{background:#b7791f;}
textarea{width:100%;height:80px;background:#050509;color:#fff;border:1px solid #333;border-radius:8px;padding:8px;margin-top:8px;display:none;}
</style>
<script>
function toggleReply(id){
    let box=document.getElementById("reply_"+id);
    box.style.display=box.style.display==="block"?"none":"block";
}
</script>
</head>
<body>
<main>
<h1>Сообщения Rteam</h1>
<form method="GET"><input type="text" name="q" placeholder="Поиск..." value="<?=htmlspecialchars($q)?>"><button class="btn gray">Искать</button></form>

<?php foreach($messages as $m): ?>
<div class="card">
    <b><?=htmlspecialchars($m["name"])?></b> — <?=htmlspecialchars($m["email"])?>
    <br><?=htmlspecialchars($m["time"])?>
    <?php if($m["status"]==="new"): ?><span class="badge badge-new">Новое</span><?php else: ?><span class="badge badge-read">Прочитано</span><?php endif; ?>
    <p><?=nl2br(htmlspecialchars($m["text"]))?></p>

    <button class="btn ok" onclick="toggleReply(<?=$m['id']?>)">Ответить</button>
    <form action="reply.php" method="POST" style="display:inline;">
        <input type="hidden" name="delete_only" value="1">
        <input type="hidden" name="id" value="<?=$m['id']?>">
        <button class="btn no">Удалить</button>
    </form>

    <form action="reply.php" method="POST">
        <textarea id="reply_<?=$m['id']?>" name="reply" placeholder="Введите ответ..."></textarea>
        <input type="hidden" name="id" value="<?=$m['id']?>">
        <button class="btn ok" style="margin-top:8px;display:none;" id="sendbtn_<?=$m['id']?>">Отправить и удалить</button>
    </form>

    <script>
    document.getElementById("reply_<?=$m['id']?>").addEventListener("input",function(){
        document.getElementById("sendbtn_<?=$m['id']?>").style.display="inline-block";
    });
    </script>
</div>
<?php endforeach; ?>
</main>
</body>
</html>
