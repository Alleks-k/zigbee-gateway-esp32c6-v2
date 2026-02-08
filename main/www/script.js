let countdownTimer = null;

function showToast(message) {
    const toast = document.getElementById("toast");
    if (!toast) return;
    toast.innerText = message;
    toast.className = "show";
    setTimeout(function() { 
        toast.className = toast.className.replace("show", ""); 
    }, 3000);
}

function update() {
    fetch("/api/status")
        .then(function(response) {
            if (!response.ok) throw new Error("Network error");
            return response.json();
        })
        .then(function(data) {
            const statusDiv = document.getElementById("status");
            if (statusDiv) {
                const panId = data.pan_id !== undefined ? "0x" + data.pan_id.toString(16).toUpperCase() : "---";
                const channel = data.channel !== undefined ? data.channel : "---";
                const shortAddr = data.short_addr !== undefined ? "0x" + data.short_addr.toString(16).toUpperCase() : "---";

                statusDiv.innerHTML = "<p>PAN ID: <strong>" + panId + "</strong></p>" +
                                     "<p>Канал: <strong>" + channel + "</strong></p>" +
                                     "<p>Адреса: <strong>" + shortAddr + "</strong></p>";
            }

            const devicesList = document.getElementById("devices");
            if (devicesList) {
                devicesList.innerHTML = "";
                if (!data.devices || data.devices.length === 0) {
                    devicesList.innerHTML = '<li class="empty">Пристроїв не знайдено</li>';
                } else {
                    data.devices.forEach(function(device) {
                        const li = document.createElement("li");
                        li.className = "device-item";
                        const hexAddr = "0x" + device.short_addr.toString(16).toUpperCase();
                        
                        // Додано кнопку видалення до оригінальної розмітки
                        li.innerHTML = '<div class="device-info"><strong>' + device.name + '</strong>' +
                                       '<small>' + hexAddr + '</small></div>' +
                                       '<div class="device-controls">' +
                                       '<button class="btn-on" onclick="control(' + device.short_addr + ', 1, 1)">Ввімк</button>' +
                                       '<button class="btn-off" onclick="control(' + device.short_addr + ', 1, 0)">Вимк</button>' +
                                       '<button class="btn-delete" style="background:#ff4d4d;color:white;margin-left:5px;border:none;padding:5px 10px;border-radius:4px;cursor:pointer;" onclick="deleteDevice(\'' + hexAddr + '\')">Видалити</button>' +
                                       '</div>';
                        devicesList.appendChild(li);
                    });
                }
            }
        })
        .catch(function(error) {
            console.error("Update error:", error);
        });
}

function permitJoin() {
    const btn = document.querySelector(".btn-permit");
    if (countdownTimer) return;

    fetch("/api/permit_join", { method: "POST" })
        .then(function(response) {
            if (response.ok) {
                showToast("Режим пошуку активовано");
                startCountdown(60, btn);
            } else {
                showToast("Помилка активації");
            }
        })
        .catch(function() {
            showToast("Шлюз недоступний");
        });
}

function startCountdown(seconds, btn) {
    let timeLeft = seconds;
    btn.classList.add("active");
    btn.disabled = true;

    countdownTimer = setInterval(function() {
        btn.innerText = "Пошук (" + timeLeft + "с)...";
        timeLeft--;

        if (timeLeft < 0) {
            clearInterval(countdownTimer);
            countdownTimer = null;
            btn.innerText = "Додати новий пристрій";
            btn.classList.remove("active");
            btn.disabled = false;
            showToast("Час вичерпано");
        }
    }, 1000);
}

function control(addr, endpoint, cmd) {
    fetch("/api/control", {
        method: "POST",
        headers: { "Content-Type": "text/plain" },
        body: addr + "," + endpoint + "," + cmd
    })
    .catch(function() {
        showToast("Помилка команди");
    });
}

// НОВА ФУНКЦІЯ: Видалення пристрою
function deleteDevice(addr) {
    if (!confirm("Видалити пристрій " + addr + "?")) return;
    
    fetch("/api/delete", {
        method: "POST",
        body: addr
    })
    .then(function(response) {
        if (response.ok) {
            showToast("Пристрій видалено");
            update();
        } else {
            showToast("Помилка видалення");
        }
    })
    .catch(function() {
        showToast("Шлюз недоступний");
    });
}

update();
setInterval(update, 5000);