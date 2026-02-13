const ICONS = {
    on: '<svg class="icon" viewBox="0 0 24 24"><path d="M9 21c0 .55.45 1 1 1h4c.55 0 1-.45 1-1v-1H9v1zm3-19C8.14 2 5 5.14 5 9c0 2.38 1.19 4.47 3 5.74V17c0 .55.45 1 1 1h6c.55 0 1-.45 1-1v-2.26c1.81-1.27 3-3.36 3-5.74 0-3.86-3.14-7-7-7z"/></svg>',
    off: '<svg class="icon" viewBox="0 0 24 24"><path d="M9 21c0 .55.45 1 1 1h4c.55 0 1-.45 1-1v-1H9v1zm3-19C8.14 2 5 5.14 5 9c0 2.38 1.19 4.47 3 5.74V17c0 .55.45 1 1 1h6c.55 0 1-.45 1-1v-2.26c1.81-1.27 3-3.36 3-5.74 0-3.86-3.14-7-7-7zm2.85 11.1l-.85.6V16h-4v-2.3l-.85-.6C7.8 12.16 7 10.63 7 9c0-2.76 2.24-5 5-5s5 2.24 5 5c0 1.63-.8 3.16-2.15 4.1z"/></svg>',
    edit: '<svg class="icon" viewBox="0 0 24 24"><path d="M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04c.39-.39.39-1.02 0-1.41l-2.34-2.34c-.39-.39-1.02-.39-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z"/></svg>',
    delete: '<svg class="icon" viewBox="0 0 24 24"><path d="M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z"/></svg>',
    lock: '<svg class="icon" viewBox="0 0 24 24"><path d="M18 8h-1V6c0-2.76-2.24-5-5-5S7 3.24 7 6v2H6c-1.1 0-2 .9-2 2v10c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V10c0-1.1-.9-2-2-2zm-6 9c-1.1 0-2-.9-2-2s.9-2 2-2 2 .9 2 2-.9 2-2 2zm3.1-9H8.9V6c0-1.71 1.39-3.1 3.1-3.1 1.71 0 3.1 1.39 3.1 3.1v2z"/></svg>',
    unlock: '<svg class="icon" viewBox="0 0 24 24"><path d="M12 17c1.1 0 2-.9 2-2s-.9-2-2-2-2 .9-2 2 .9 2 2 2zm6-9h-1V6c0-2.76-2.24-5-5-5S7 3.24 7 6h1.9c.55 0 1 .45 1 1s-.45 1-1 1H7c-1.1 0-2 .9-2 2v10c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V10c0-1.1-.9-2-2-2zm0 10H6V10h12v8z"/></svg>',
    online: '<svg class="icon" viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 15l-5-5 1.41-1.41L10 14.17l7.59-7.59L19 8l-9 9z"/></svg>',
    offline: '<svg class="icon" viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-1 15v-2h2v2h-2zm0-10v6h2V7h-2z"/></svg>'
};
const API_BASE = '/api/v1';

function apiUrl(path) {
    return API_BASE + path;
}

document.addEventListener('DOMContentLoaded', () => {
    // Завантажуємо статус при завантаженні сторінки
    fetchStatus();
    fetchHealth();
    
    // Ініціалізуємо WebSocket
    initWebSocket();

    // Обробник кнопки "Permit Join"
    const permitBtn = document.getElementById('permitJoinBtn');
    if (permitBtn) {
        permitBtn.addEventListener('click', permitJoin);
    }
});

async function requestJson(url, options = {}) {
    const response = await fetch(url, options);
    const text = await response.text();
    let data = {};
    if (text) {
        try {
            data = JSON.parse(text);
        } catch (_) {
            data = { message: text };
        }
    }
    if (!response.ok) {
        throw new Error((data.error && data.error.message) || data.message || `HTTP ${response.status}`);
    }
    if (data && data.status && data.status !== 'ok') {
        throw new Error((data.error && data.error.message) || data.message || 'API error');
    }
    return data;
}

/**
 * Ініціалізація WebSocket з'єднання
 */
function initWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;
    const ws = new WebSocket(wsUrl);

    ws.onopen = () => {
        console.log('WS Connected');
        updateConnectionStatus(true);
    };

    ws.onmessage = (event) => {
        const data = JSON.parse(event.data);

        // New WS protocol: typed events
        if (data && data.type === 'devices_delta' && data.data && Array.isArray(data.data.devices)) {
            renderDevices(data.data.devices);
            return;
        }
        if (data && data.type === 'health_state' && data.data) {
            applyHealthData(data.data);
            return;
        }

        // Legacy fallback format
        if (data.pan_id !== undefined) updateElementText('panId', '0x' + data.pan_id.toString(16).toUpperCase());
        if (data.channel !== undefined) updateElementText('channel', data.channel);
        if (data.short_addr !== undefined) updateElementText('gwAddr', '0x' + data.short_addr.toString(16).toUpperCase());
        if (data.devices) renderDevices(data.devices);
    };

    ws.onclose = () => {
        console.log('WS Disconnected, retrying in 3s...');
        updateConnectionStatus(false);
        setTimeout(initWebSocket, 3000);
    };
}

function updateConnectionStatus(connected) {
    const el = document.getElementById('ws-status');
    if (!el) return;
    el.innerHTML = connected ? ICONS.online + ' Online' : ICONS.offline + ' Offline';
    el.style.color = connected ? '#28a745' : 'red';
}

/**
 * Отримання статусу шлюзу та списку пристроїв
 */
function fetchStatus() {
    requestJson(apiUrl('/status'))
        .then(resp => {
            const data = resp.data || {};
            // Оновлення інформації про шлюз
            updateElementText('panId', '0x' + data.pan_id.toString(16).toUpperCase());
            updateElementText('channel', data.channel);
            updateElementText('gwAddr', '0x' + data.short_addr.toString(16).toUpperCase());

            // Оновлення списку пристроїв
            renderDevices(data.devices);
        })
        .catch(err => console.error('Error fetching status:', err));
}

function fetchHealth() {
    requestJson(apiUrl('/health'))
        .then(resp => {
            const data = resp.data || {};
            applyHealthData(data);
        })
        .catch(err => console.error('Error fetching health:', err));
}

function applyHealthData(data) {
    if (!data || typeof data !== 'object') return;
    if (data.zigbee) {
        if (data.zigbee.pan_id !== undefined) updateElementText('panId', '0x' + Number(data.zigbee.pan_id).toString(16).toUpperCase());
        if (data.zigbee.channel !== undefined) updateElementText('channel', data.zigbee.channel);
        if (data.zigbee.short_addr !== undefined) updateElementText('gwAddr', '0x' + Number(data.zigbee.short_addr).toString(16).toUpperCase());
        updateElementText('zbStarted', data.zigbee.started ? 'Started' : 'Stopped');
    }

    if (data.wifi) {
        const state = data.wifi.fallback_ap_active
            ? `Fallback AP (${data.wifi.active_ssid || '-'})`
            : (data.wifi.sta_connected ? `STA Connected (${data.wifi.active_ssid || '-'})` : 'STA Disconnected');
        updateElementText('wifiState', state);
    }

    if (data.nvs) {
        updateElementText('nvsSchema', data.nvs.schema_version !== undefined ? String(data.nvs.schema_version) : '--');
    }
    if (data.heap && data.heap.free !== undefined) {
        updateElementText('heapFree', `${Math.round(Number(data.heap.free) / 1024)} KB`);
    }
}

/**
 * Допоміжна функція для безпечного оновлення тексту
 */
function updateElementText(id, text) {
    const el = document.getElementById(id);
    if (el) el.innerText = text;
}

/**
 * Відображення списку пристроїв у HTML
 */
function renderDevices(devices) {
    const list = document.getElementById('deviceList');
    if (!list) return;

    list.innerHTML = ''; // Очищення списку
    const currentCount = devices ? devices.length : 0;

    if (!devices || devices.length === 0) {
        list.innerHTML = '<li>No devices connected</li>';
        lastRenderedDeviceCount = 0;
        return;
    }

    devices.forEach(dev => {
        const li = document.createElement('li');
        li.className = 'device-item';
        
        // Форматування адреси
        const addrHex = '0x' + dev.short_addr.toString(16).toUpperCase().padStart(4, '0');
        const rawName = (dev.name || 'Пристрій').trim();
        const displayName = rawName.replace(/\s*0x[0-9a-fA-F]{1,4}\s*$/i, '').trim() || 'Пристрій';

        li.innerHTML = `
            <div class="dev-info">
                <strong>${displayName}</strong>
                <small>Addr: ${addrHex}</small>
            </div>
            <div class="dev-actions">
                <button class="btn-on" onclick="controlDevice(${dev.short_addr}, 1, 1)">${ICONS.on} ON</button>
                <button class="btn-off" onclick="controlDevice(${dev.short_addr}, 1, 0)">${ICONS.off} OFF</button>
                <button class="btn-edit" onclick="openEditModal(${dev.short_addr}, '${displayName.replace(/'/g, "\\'")}')">${ICONS.edit}</button>
                <button class="btn-del" onclick="deleteDevice(${dev.short_addr})">${ICONS.delete}</button>
            </div>
        `;
        list.appendChild(li);
    });

    if (permitJoinActive && currentCount > permitJoinStartDeviceCount) {
        showToast('Пристрій додано, режим join завершено');
        resetPermitJoinButton();
    }
    lastRenderedDeviceCount = currentCount;
}

/**
 * Відправка команди Permit Join (відкриття мережі)
 */
let permitTimer = null;
let permitJoinActive = false;
let permitJoinStartDeviceCount = 0;
let lastRenderedDeviceCount = -1;

function resetPermitJoinButton() {
    const btn = document.getElementById('permitJoinBtn');
    if (!btn) return;
    if (permitTimer) {
        clearInterval(permitTimer);
        permitTimer = null;
    }
    permitJoinActive = false;
    permitJoinStartDeviceCount = 0;
    btn.innerHTML = '<svg class="icon" viewBox="0 0 24 24"><path d="M19 13h-6v6h-2v-6H5v-2h6V5h2v6h6v2z"/></svg> Permit Join (60s)';
    btn.disabled = false;
    btn.classList.remove('active');
}

function permitJoin() {
    const btn = document.getElementById('permitJoinBtn');
    if (btn.disabled) return;

    requestJson(apiUrl('/permit_join'), { method: 'POST' })
        .then(resp => {
            showToast((resp.data && resp.data.message) || resp.message || 'Network opened');
            
            // Запуск таймера на кнопці
            let timeLeft = 60;
            permitJoinActive = true;
            permitJoinStartDeviceCount = lastRenderedDeviceCount < 0 ? 0 : lastRenderedDeviceCount;
            btn.disabled = true;
            btn.classList.add('active');
            
            if (permitTimer) clearInterval(permitTimer);
            permitTimer = setInterval(() => {
                btn.innerText = `Joining... (${timeLeft}s)`;
                timeLeft--;
                if (timeLeft < 0) {
                    resetPermitJoinButton();
                }
            }, 1000);
        })
        .catch(err => {
            console.error('Error:', err);
            showToast(err.message || 'Permit join failed');
        });
}

/**
 * Відправка команди керування (On/Off)
 * @param {number} addr - Short Address пристрою
 * @param {number} ep - Endpoint (зазвичай 1)
 * @param {number} cmd - 1 (On) або 0 (Off)
 */
function controlDevice(addr, ep, cmd) {
    const payload = {
        addr: addr,
        ep: ep,
        cmd: cmd
    };

    requestJson(apiUrl('/control'), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(resp => {
        console.log('Control response:', resp);
    })
    .catch(err => console.error('Control error:', err));
}

/**
 * Видалення пристрою
 * @param {number} addr - Short Address пристрою
 */
function deleteDevice(addr) {
    showConfirm('Видалити цей пристрій?', () => {
        const payload = { short_addr: addr };

        requestJson(apiUrl('/delete'), {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        })
        .then(resp => {
            if (resp.status === 'ok') {
                fetchStatus(); // Одразу оновлюємо список
            } else {
                showToast('Failed to delete device');
            }
        })
        .catch(err => console.error('Delete error:', err));
    });
}


function scanWifi() {
    const resultsDiv = document.getElementById('wifi-scan-results');
    const ssidInput = document.getElementById('wifi-ssid');
    
    resultsDiv.style.display = 'block';
    resultsDiv.innerHTML = '<div class="scan-item">Сканування...</div>';

    requestJson(apiUrl('/wifi/scan'))
        .then(resp => {
            const networks = Array.isArray(resp.data) ? resp.data : [];
            resultsDiv.innerHTML = '';
            if (networks.length === 0) {
                resultsDiv.innerHTML = '<div class="scan-item">Мереж не знайдено</div>';
                return;
            }

            // Сортуємо за рівнем сигналу (RSSI від більшого до меншого)
            networks.sort((a, b) => b.rssi - a.rssi);

            networks.forEach(net => {
                const div = document.createElement('div');
                div.className = 'scan-item';
                
                // 0 = OPEN, інші значення = захищена мережа
                const lockIcon = net.auth === 0 ? ICONS.unlock : ICONS.lock;
                
                div.innerHTML = `
                    <span>${lockIcon} <strong>${net.ssid}</strong></span>
                    <span class="scan-rssi">${net.rssi} dBm</span>
                `;
                
                // При кліку підставляємо SSID у поле вводу
                div.onclick = function() {
                    ssidInput.value = net.ssid;
                    resultsDiv.style.display = 'none';
                    document.getElementById('wifi-pass').focus();
                };
                
                resultsDiv.appendChild(div);
            });
        })
        .catch(err => {
            console.error(err);
            resultsDiv.innerHTML = '<div class="scan-item" style="color:red;">Помилка сканування</div>';
        });
}

function saveWifiSettings() {
    const ssid = document.getElementById('wifi-ssid').value;
    const password = document.getElementById('wifi-pass').value;

    if (!ssid) {
        showToast("Будь ласка, введіть SSID мережі.");
        return;
    }

    // Валідація на стороні клієнта (синхронізовано з backend)
    if (password.length < 8 || password.length > 64) {
        showToast("Пароль повинен бути від 8 до 64 символів.");
        return;
    }

    requestJson(apiUrl('/settings/wifi'), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ssid: ssid, password: password })
    })
    .then(resp => {
        if (resp.status === 'ok') {
            showToast("Налаштування збережено. Пристрій перезавантажується...");
        } else {
            showToast("Помилка: " + (resp.message || "Невідома помилка"));
        }
    })
    .catch(err => {
        console.error(err);
        showToast("Помилка з'єднання з сервером");
    });
}

function rebootGateway() {
    showConfirm("Перезавантажити шлюз?", () => {
        requestJson(apiUrl('/reboot'), { method: 'POST' })
            .then(resp => showToast((resp.data && resp.data.message) || "Перезавантаження..."))
            .catch(err => showToast("Помилка запиту"));
    });
}

function factoryResetGateway() {
    showConfirm("Скинути пристрій до заводських налаштувань?", () => {
        requestJson(apiUrl('/factory_reset'), { method: 'POST' })
            .then(resp => showToast((resp.data && resp.data.message) || "Factory reset..."))
            .catch(err => showToast(err.message || "Помилка factory reset"));
    });
}

function showToast(message) {
    const x = document.getElementById("toast");
    if (!x) return;
    x.innerText = message;
    x.className = "show";
    setTimeout(function(){ x.className = x.className.replace("show", ""); }, 3000);
}

function showConfirm(message, onYes) {
    const modal = document.getElementById('confirmModal');
    const msg = document.getElementById('confirmMessage');
    const yesBtn = document.getElementById('confirmYes');
    const noBtn = document.getElementById('confirmNo');

    if (!modal) {
        if (confirm(message)) onYes();
        return;
    }

    msg.innerText = message;
    modal.style.display = "block";

    yesBtn.onclick = function() {
        modal.style.display = "none";
        onYes();
    };

    noBtn.onclick = function() {
        modal.style.display = "none";
    };
}

/* --- Функції редагування імені --- */

function openEditModal(addr, currentName) {
    const modal = document.getElementById('editModal');
    const nameInput = document.getElementById('editNameInput');
    const addrInput = document.getElementById('editAddrInput');

    if (modal && nameInput && addrInput) {
        nameInput.value = currentName;
        addrInput.value = addr;
        modal.style.display = "block";
        nameInput.focus();
    }
}

function closeEditModal() {
    const modal = document.getElementById('editModal');
    if (modal) modal.style.display = "none";
}

function saveDeviceName() {
    const nameInput = document.getElementById('editNameInput');
    const addrInput = document.getElementById('editAddrInput');
    
    const newName = nameInput.value.trim();
    const addr = parseInt(addrInput.value);

    if (!newName) {
        showToast("Name cannot be empty");
        return;
    }

    requestJson(apiUrl('/rename'), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ short_addr: addr, name: newName })
    })
    .then(resp => {
        if (resp.status === 'ok') {
            closeEditModal();
            fetchStatus(); // Оновлюємо список
            showToast("Renamed successfully");
        } else {
            showToast("Error renaming device");
        }
    })
    .catch(err => console.error(err));
}
