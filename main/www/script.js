document.addEventListener('DOMContentLoaded', () => {
    // Завантажуємо статус при завантаженні сторінки
    fetchStatus();
    
    // Ініціалізуємо WebSocket
    initWebSocket();

    // Обробник кнопки "Permit Join"
    const permitBtn = document.getElementById('permitJoinBtn');
    if (permitBtn) {
        permitBtn.addEventListener('click', permitJoin);
    }

    // Обробник форми Wi-Fi (якщо вона є на сторінці)
    const wifiForm = document.getElementById('wifiForm');
    if (wifiForm) {
        wifiForm.addEventListener('submit', saveWifiSettings);
    }
});

/**
 * Ініціалізація WebSocket з'єднання
 */
function initWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;
    const ws = new WebSocket(wsUrl);

    ws.onopen = () => {
        console.log('WS Connected');
    };

    ws.onmessage = (event) => {
        const data = JSON.parse(event.data);
        // Оновлення UI даними з WebSocket
        if (data.pan_id) updateElementText('panId', '0x' + data.pan_id.toString(16).toUpperCase());
        if (data.channel) updateElementText('channel', data.channel);
        if (data.short_addr) updateElementText('gwAddr', '0x' + data.short_addr.toString(16).toUpperCase());
        if (data.devices) renderDevices(data.devices);
    };

    ws.onclose = () => {
        console.log('WS Disconnected, retrying in 3s...');
        setTimeout(initWebSocket, 3000);
    };
}

/**
 * Отримання статусу шлюзу та списку пристроїв
 */
function fetchStatus() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            // Оновлення інформації про шлюз
            updateElementText('panId', '0x' + data.pan_id.toString(16).toUpperCase());
            updateElementText('channel', data.channel);
            updateElementText('gwAddr', '0x' + data.short_addr.toString(16).toUpperCase());

            // Оновлення списку пристроїв
            renderDevices(data.devices);
        })
        .catch(err => console.error('Error fetching status:', err));
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

    if (!devices || devices.length === 0) {
        list.innerHTML = '<li>No devices connected</li>';
        return;
    }

    devices.forEach(dev => {
        const li = document.createElement('li');
        li.className = 'device-item';
        
        // Форматування адреси
        const addrHex = '0x' + dev.short_addr.toString(16).toUpperCase().padStart(4, '0');

        li.innerHTML = `
            <div class="dev-info">
                <strong>${dev.name}</strong>
                <small>Addr: ${addrHex}</small>
            </div>
            <div class="dev-actions">
                <button class="btn-on" onclick="controlDevice(${dev.short_addr}, 1, 1)">ON</button>
                <button class="btn-off" onclick="controlDevice(${dev.short_addr}, 1, 0)">OFF</button>
                <button class="btn-del" onclick="deleteDevice(${dev.short_addr})">🗑</button>
            </div>
        `;
        list.appendChild(li);
    });
}

/**
 * Відправка команди Permit Join (відкриття мережі)
 */
function permitJoin() {
    fetch('/api/permit_join', { method: 'POST' })
        .then(res => res.json())
        .then(data => alert(data.message || 'Network opened'))
        .catch(err => console.error('Error:', err));
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

    fetch('/api/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(res => res.json())
    .then(data => {
        console.log('Control response:', data);
        if (data.status !== 'ok') {
            alert('Error: ' + (data.message || 'Unknown error'));
        }
    })
    .catch(err => console.error('Control error:', err));
}

/**
 * Видалення пристрою
 * @param {number} addr - Short Address пристрою
 */
function deleteDevice(addr) {
    if (!confirm('Are you sure you want to remove this device?')) return;

    const payload = { short_addr: addr };

    fetch('/api/delete', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(res => res.json())
    .then(data => {
        if (data.status === 'ok') {
            fetchStatus(); // Одразу оновлюємо список
        } else {
            alert('Failed to delete device');
        }
    })
    .catch(err => console.error('Delete error:', err));
}

/**
 * Збереження налаштувань Wi-Fi
 */
function saveWifiSettings(e) {
    e.preventDefault();
    
    const ssid = document.getElementById('ssid').value;
    const pass = document.getElementById('password').value;

    if (!ssid || !pass) {
        alert('Please enter SSID and Password');
        return;
    }

    const payload = {
        ssid: ssid,
        password: pass
    };

    fetch('/api/settings/wifi', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(res => res.json())
    .then(data => {
        alert(data.message || 'Settings saved');
    })
    .catch(err => console.error('Wifi save error:', err));
}
