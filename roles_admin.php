<?php
session_start();

function load($f,$d){return file_exists($f)?json_decode(file_get_contents($f),true):$d;}
function save($f,$d){file_put_contents($f,json_encode($d,JSON_PRETTY_PRINT|JSON_UNESCAPED_UNICODE));}

$roles=load("roles.json",[]);
$quests=load("quests.json",[]);

if($_SERVER["REQUEST_METHOD"]==="POST"){
    if($_POST["action"]==="add_role"){
        $roles[]=["id"=>time(),"name"=>trim($_POST["name"]),"description"=>trim($_POST["desc"])];
        save("roles.json",$roles);
    }
    if($_POST["action"]==="add_quest"){
        $quests[]=["id"=>time(),"title"=>trim($_POST["title"]),"reward_role"=>(int)$_POST["reward"]];
        save("quests.json",$quests);
    }
    header("Location: roles_admin.php");
    exit;
}
?>
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<title>Роли и квесты — Rteam</title>
<style>
body{margin:0;font-family:"Segoe UI",Arial,sans-serif;background:#050509;color:#eee;}
main{max-width:900px;margin:30px auto;padding:0 16px;}
.card{background:#101018;border:1px solid #2a0000;border-radius:12px;padding:14px;margin-top:12px;}
input,textarea,select{width:100%;padding:8px;margin-top:6px;border-radius:8px;border:1px solid #333;background:#050509