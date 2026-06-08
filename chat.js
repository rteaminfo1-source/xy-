const backendUrl = "rai.php";

const messages = document.getElementById("messages");
const input = document.getElementById("userInput");
const sendBtn = document.getElementById("sendBtn");

function addBlock(text, type) {
    const div = document.createElement("div");
    div.className = "msg msg-" + type;
    div.textContent = text;
    messages.appendChild(div);
    messages.scrollTop = messages.scrollHeight;
}

async function sendRequest() {
    const text = input.value.trim();
    if (!text) return;

    addBlock(text, "user");
    input.value = "";
    sendBtn.disabled = true;

    try {
        const response = await fetch(backendUrl, {
            method: "POST",
            headers: {"Content-Type": "application/json"},
            body: JSON.stringify({message: text})
        });

        const data = await response.json();
        addBlock(data.answer || "Результат не получен.", "rai");

    } catch {
        addBlock("Не удалось получить результат.", "rai");
    }

    sendBtn.disabled = false;
}

sendBtn.onclick = sendRequest;
input.onkeydown = e => e.key === "Enter" && sendRequest();

addBlock("Рабочая область готова.", "rai");
