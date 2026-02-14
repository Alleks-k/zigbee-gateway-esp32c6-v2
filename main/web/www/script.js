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
const WS_PROTOCOL_VERSION = 1;
let lastWsSeq = 0;
const JOB_POLL_INTERVAL_MS = 600;
const JOB_TIMEOUT_MS = 30000;
const LQI_STALE_MS = 60000;
const LQI_HISTORY_WINDOW_MS = 15 * 60 * 1000;
const TEMP_WARN_C = 75;
const TEMP_CRIT_C = 85;
const WS_RETRY_BASE_MS = 1000;
const WS_RETRY_MAX_MS = 10000;
let jobIndicatorHideTimer = null;
let backendMode = 'unknown';
let lastLqiUpdateCounter = 0;
let lastLqiMetaReceivedAtMs = 0;
let currentLqiMeta = null;
let currentLqiRows = [];
const lqiHistoryByAddr = new Map();
let wsRetryAttempt = 0;
let wsReconnectTimer = null;

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
    const refreshLqiBtn = document.getElementById('refreshLqiBtn');
    if (refreshLqiBtn) {
        refreshLqiBtn.addEventListener('click', refreshLqiActive);
    }

    // WS is primary; periodic status refresh is a safety fallback.
    setInterval(fetchStatus, 10000);
    setInterval(() => {
        updateLqiMeta(currentLqiMeta);
        updateLqiAgeCells();
    }, 5000);
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

function setButtonBusy(buttonId, busy) {
    const btn = document.getElementById(buttonId);
    if (!btn) return;
    btn.disabled = !!busy;
}

function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

function setBackendModeBadge(mode) {
    backendMode = mode;
    const el = document.getElementById('backend-mode-badge');
    if (!el) return;
    el.classList.remove('mode-jobs', 'mode-unknown');
    if (mode === 'jobs') {
        el.classList.add('mode-jobs');
        el.textContent = 'Backend mode: jobs';
        return;
    }
    el.classList.add('mode-unknown');
    el.textContent = 'Backend mode: detecting...';
}

function updateJobIndicator(label, state, detail = '') {
    const root = document.getElementById('job-indicator');
    const opEl = document.getElementById('job-indicator-op');
    const stateEl = document.getElementById('job-indicator-state');
    const detailEl = document.getElementById('job-indicator-detail');
    if (!root || !opEl || !stateEl || !detailEl) return;

    if (jobIndicatorHideTimer) {
        clearTimeout(jobIndicatorHideTimer);
        jobIndicatorHideTimer = null;
    }

    root.style.display = 'block';
    root.classList.remove('state-queued', 'state-running', 'state-succeeded', 'state-failed');
    root.classList.add(`state-${state}`);
    opEl.textContent = label;
    stateEl.textContent = state;
    stateEl.className = `job-state ${state}`;
    detailEl.textContent = detail || '';

    if (state === 'succeeded' || state === 'failed') {
        jobIndicatorHideTimer = setTimeout(() => {
            root.style.display = 'none';
        }, 3500);
    }
}

async function submitJob(jobType, extraPayload = {}, options = {}) {
    const timeoutMs = Number.isFinite(options.timeoutMs) ? options.timeoutMs : JOB_TIMEOUT_MS;
    const label = options.label || `job:${jobType}`;
    const submitResp = await requestJson(apiUrl('/jobs'), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ type: jobType, ...extraPayload })
    });

    const jobData = submitResp.data || {};
    const jobId = Number(jobData.job_id);
    if (!Number.isFinite(jobId) || jobId <= 0) {
        throw new Error('Invalid job id');
    }
    updateJobIndicator(label, 'queued', `job #${jobId}`);
    setBackendModeBadge('jobs');

    const start = Date.now();
    let lastState = 'queued';
    while ((Date.now() - start) < timeoutMs) {
        const pollResp = await requestJson(apiUrl(`/jobs/${jobId}`));
        const info = pollResp.data || {};
        const state = String(info.state || 'running');
        if (state !== lastState) {
            updateJobIndicator(label, state, `job #${jobId}`);
            lastState = state;
        }
        if (info.done === true || info.state === 'succeeded' || info.state === 'failed') {
            if (info.state === 'failed') {
                const reason = (info.result && info.result.error) || info.error || 'Job failed';
                updateJobIndicator(label, 'failed', reason);
                throw new Error(reason);
            }
            updateJobIndicator(label, 'succeeded', `job #${jobId}`);
            return info;
        }
        await sleep(JOB_POLL_INTERVAL_MS);
    }

    updateJobIndicator(label, 'failed', 'timeout');
    throw new Error('Job timeout');
}

/**
 * Ініціалізація WebSocket з'єднання
 */
function scheduleWsReconnect() {
    if (wsReconnectTimer) return;
    const delayMs = Math.min(WS_RETRY_BASE_MS * (2 ** wsRetryAttempt), WS_RETRY_MAX_MS);
    wsRetryAttempt = Math.min(wsRetryAttempt + 1, 10);
    console.warn(`WS reconnect scheduled in ${delayMs} ms (attempt ${wsRetryAttempt})`);
    wsReconnectTimer = setTimeout(() => {
        wsReconnectTimer = null;
        initWebSocket();
    }, delayMs);
}

function initWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;
    const ws = new WebSocket(wsUrl);

    ws.onopen = () => {
        console.log('WS Connected');
        lastWsSeq = 0;
        wsRetryAttempt = 0;
        if (wsReconnectTimer) {
            clearTimeout(wsReconnectTimer);
            wsReconnectTimer = null;
        }
        updateConnectionStatus(true);
        // WS-first mode. HTTP pull only as one-time safety fallback.
        setTimeout(fetchLqiMap, 1200);
    };

    ws.onmessage = (event) => {
        let data = null;
        try {
            data = JSON.parse(event.data);
        } catch (_) {
            return;
        }

        if (data && data.status === 'ok' && data.data && typeof data.data === 'object') {
            data = data.data;
        }
        if (data && Number.isFinite(data.seq)) {
            if (data.seq <= lastWsSeq) {
                return;
            }
            lastWsSeq = data.seq;
        }
        if (data && data.version !== undefined && data.version !== WS_PROTOCOL_VERSION) {
            console.warn('WS protocol version mismatch:', data.version);
        }

        // New WS protocol: typed events
        if (data && data.type === 'devices_delta' && data.data && Array.isArray(data.data.devices)) {
            renderDevices(data.data.devices);
            return;
        }
        if (data && data.type === 'health_state' && data.data) {
            applyHealthData(data.data);
            return;
        }
        // Accept both canonical and legacy names for backward compatibility.
        if (data && (data.type === 'lqi_update' || data.type === 'lqi_state') && data.data) {
            const neighbors = Array.isArray(data.data.neighbors) ? data.data.neighbors : [];
            renderLqiTable(neighbors, data.data);
            return;
        }

        // Legacy fallback format
        if (data.pan_id !== undefined) updateElementText('panId', '0x' + data.pan_id.toString(16).toUpperCase());
        if (data.channel !== undefined) updateElementText('channel', data.channel);
        if (data.short_addr !== undefined) updateElementText('gwAddr', '0x' + data.short_addr.toString(16).toUpperCase());
        if (data.devices) renderDevices(data.devices);
    };

    ws.onerror = (event) => {
        console.error('WS error:', {
            url: wsUrl,
            readyState: ws.readyState,
            eventType: event && event.type ? event.type : 'error'
        });
    };

    ws.onclose = (event) => {
        console.warn('WS Disconnected', {
            url: wsUrl,
            code: event.code,
            reason: event.reason || '(empty)',
            wasClean: event.wasClean,
            readyState: ws.readyState
        });
        updateConnectionStatus(false);
        scheduleWsReconnect();
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

function fetchLqiMap() {
    requestJson(apiUrl('/lqi'))
        .then(resp => {
            const data = (resp && resp.data) ? resp.data : {};
            const neighbors = Array.isArray(data.neighbors) ? data.neighbors : [];
            renderLqiTable(neighbors, data);
        })
        .catch(err => {
            console.error('Error fetching LQI:', err);
            renderLqiTable([], null);
        });
}

function refreshLqiActive() {
    setButtonBusy('refreshLqiBtn', true);
    submitJob('lqi_refresh', {}, { label: 'LQI refresh', timeoutMs: 12000 })
        .then(() => fetchLqiMap())
        .catch(err => {
            console.error('LQI refresh failed:', err);
            showToast(err.message || 'LQI refresh failed');
        })
        .finally(() => setButtonBusy('refreshLqiBtn', false));
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
        if (data.wifi.rssi !== undefined && data.wifi.rssi !== null) {
            updateElementText('wifiRssi', `${data.wifi.rssi} dBm`);
        } else {
            updateElementText('wifiRssi', 'N/A');
        }
        if (data.wifi.link_quality !== undefined && data.wifi.link_quality !== null) {
            updateElementText('wifiLinkQuality', String(data.wifi.link_quality).toUpperCase());
        } else {
            updateElementText('wifiLinkQuality', 'UNKNOWN');
        }
        if (data.wifi.ip !== undefined && data.wifi.ip !== null && data.wifi.ip !== '') {
            updateElementText('wifiIp', String(data.wifi.ip));
        } else {
            updateElementText('wifiIp', '--');
        }
    }

    if (data.nvs) {
        updateElementText('nvsSchema', data.nvs.schema_version !== undefined ? String(data.nvs.schema_version) : '--');
    }
    if (data.system) {
        if (data.system.uptime_ms !== undefined) {
            updateElementText('uptime', formatUptime(Number(data.system.uptime_ms)));
        }
        if (data.system.heap_free !== undefined) {
            updateElementText('heapFree', `${Math.round(Number(data.system.heap_free) / 1024)} KB`);
        }
        if (data.system.heap_min !== undefined) {
            updateElementText('heapMinFree', `${Math.round(Number(data.system.heap_min) / 1024)} KB`);
        }
        if (data.system.heap_largest_block !== undefined) {
            updateElementText('heapLargest', `${Math.round(Number(data.system.heap_largest_block) / 1024)} KB`);
        }
        if (data.system.main_stack_hwm_bytes !== undefined) {
            const v = Number(data.system.main_stack_hwm_bytes);
            updateElementText('mainStackHwm', v >= 0 ? `${Math.round(v / 1024)} KB` : 'N/A');
        }
        if (data.system.httpd_stack_hwm_bytes !== undefined) {
            const v = Number(data.system.httpd_stack_hwm_bytes);
            updateElementText('httpdStackHwm', v >= 0 ? `${Math.round(v / 1024)} KB` : 'N/A');
        }
        if (data.system.temperature_c !== undefined && data.system.temperature_c !== null) {
            const t = Number(data.system.temperature_c);
            updateElementText('chipTemp', `${t.toFixed(1)} C`);
            applyTempAlarmState(t);
        } else {
            updateElementText('chipTemp', 'N/A');
            applyTempAlarmState(null);
        }
    }
    if (data.telemetry && data.telemetry.updated_ms !== undefined) {
        updateElementText('telemetryUpdated', formatUptime(Number(data.telemetry.updated_ms)));
    }
    if (Array.isArray(data.errors) && data.errors.length > 0) {
        const e = data.errors[0];
        const src = e.source ? String(e.source) : 'sys';
        const msg = e.message ? String(e.message) : 'error';
        const code = (e.code !== undefined && e.code !== null) ? `(${e.code})` : '';
        updateElementText('lastError', `${src}${code}: ${msg}`);
    } else {
        updateElementText('lastError', 'none');
    }
}

function formatUptime(ms) {
    if (!Number.isFinite(ms) || ms < 0) return '--';
    const totalSec = Math.floor(ms / 1000);
    const days = Math.floor(totalSec / 86400);
    const hours = Math.floor((totalSec % 86400) / 3600);
    const mins = Math.floor((totalSec % 3600) / 60);
    const secs = totalSec % 60;
    if (days > 0) return `${days}d ${hours}h ${mins}m`;
    if (hours > 0) return `${hours}h ${mins}m ${secs}s`;
    return `${mins}m ${secs}s`;
}

function applyTempAlarmState(tempC) {
    const el = document.getElementById('chipTemp');
    if (!el) return;
    el.classList.remove('temp-ok', 'temp-warn', 'temp-critical');
    if (!Number.isFinite(tempC)) return;
    if (tempC >= TEMP_CRIT_C) {
        el.classList.add('temp-critical');
        return;
    }
    if (tempC >= TEMP_WARN_C) {
        el.classList.add('temp-warn');
        return;
    }
    el.classList.add('temp-ok');
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

function lqiSourceLabel(source) {
    if (source === 'mgmt_lqi') return 'Mgmt LQI';
    if (source === 'neighbor_table') return 'Neighbor Table';
    return 'Unknown';
}

function normalizeLqiQuality(raw) {
    const q = String(raw || 'unknown').toLowerCase();
    if (q === 'fair') return 'warn';
    if (q === 'poor') return 'bad';
    if (q === 'good' || q === 'warn' || q === 'bad' || q === 'unknown') return q;
    return 'unknown';
}

function getLqiAgeSeconds() {
    if (!(lastLqiMetaReceivedAtMs > 0)) {
        return null;
    }
    const sec = Math.floor((Date.now() - lastLqiMetaReceivedAtMs) / 1000);
    return sec >= 0 ? sec : 0;
}

function formatAge(ageSec) {
    if (ageSec === null || ageSec === undefined) {
        return '--';
    }
    if (ageSec < 60) {
        return `${ageSec}s`;
    }
    const min = Math.floor(ageSec / 60);
    const sec = ageSec % 60;
    return `${min}m ${sec}s`;
}

function updateLqiAgeCells() {
    const ageSec = getLqiAgeSeconds();
    const ageText = formatAge(ageSec);
    document.querySelectorAll('.lqi-age-cell').forEach((el) => {
        el.textContent = ageText;
    });
}

function qualityToScore(quality) {
    const q = normalizeLqiQuality(quality);
    if (q === 'good') return 3;
    if (q === 'warn') return 2;
    if (q === 'bad') return 1;
    return 0;
}

function updateLqiHistory(rows) {
    const now = Date.now();
    const seen = new Set();
    rows.forEach((row) => {
        const addr = Number(row.short_addr || 0);
        if (!addr) return;
        seen.add(addr);
        const score = qualityToScore(row.quality);
        const history = lqiHistoryByAddr.get(addr) || [];
        history.push({ ts: now, score });
        const minTs = now - LQI_HISTORY_WINDOW_MS;
        while (history.length > 0 && history[0].ts < minTs) {
            history.shift();
        }
        lqiHistoryByAddr.set(addr, history);
    });

    lqiHistoryByAddr.forEach((history, addr) => {
        if (!seen.has(addr)) {
            const minTs = now - LQI_HISTORY_WINDOW_MS;
            while (history.length > 0 && history[0].ts < minTs) {
                history.shift();
            }
            if (history.length === 0) {
                lqiHistoryByAddr.delete(addr);
            }
        }
    });
}

function trendForAddr(addr) {
    const history = lqiHistoryByAddr.get(addr);
    if (!history || history.length < 2) {
        return { cls: 'na', text: '--' };
    }
    const first = history[0].score;
    const last = history[history.length - 1].score;
    const delta = last - first;
    if (delta >= 1) {
        return { cls: 'up', text: '↑ improving' };
    }
    if (delta <= -1) {
        return { cls: 'down', text: '↓ degrading' };
    }
    return { cls: 'flat', text: '→ stable' };
}

function shortAddrHex(addr) {
    return '0x' + Number(addr || 0).toString(16).toUpperCase().padStart(4, '0');
}

function buildRouterHints(rows) {
    const hints = [];
    const badRows = [];
    const degradingRows = [];

    rows.forEach((row) => {
        const quality = normalizeLqiQuality(row.quality);
        const trend = trendForAddr(Number(row.short_addr || 0));
        if (quality === 'bad') {
            badRows.push(row);
        }
        if ((quality === 'warn' || quality === 'bad') && trend.cls === 'down') {
            degradingRows.push(row);
        }
    });

    badRows.slice(0, 2).forEach((row) => {
        hints.push({
            cls: 'hint-bad',
            text: `Weak link for ${(row.name || shortAddrHex(row.short_addr))} (${shortAddrHex(row.short_addr)}). Consider placing a router midway between GW and this device.`,
        });
    });

    degradingRows.slice(0, 2).forEach((row) => {
        hints.push({
            cls: 'hint-warn',
            text: `Link degrading for ${(row.name || shortAddrHex(row.short_addr))}. Consider moving/adding a router closer to this zone.`,
        });
    });

    if (badRows.length >= 3) {
        hints.push({
            cls: 'hint-warn',
            text: 'Multiple bad links detected. Consider at least one powered Zigbee router in each far room.',
        });
    }

    return hints.slice(0, 4);
}

function renderRouterHints(rows) {
    const root = document.getElementById('lqiHints');
    if (!root) return;

    const hints = buildRouterHints(rows || []);
    if (!hints.length) {
        root.innerHTML = '<li class="hint-empty">Topology looks stable. No router hints right now.</li>';
        return;
    }

    root.innerHTML = hints.map((h) => `<li class="${h.cls}">${h.text}</li>`).join('');
}

function qualityStrokeColor(quality) {
    if (quality === 'good') return '#16a34a';
    if (quality === 'warn') return '#d97706';
    if (quality === 'bad') return '#dc2626';
    return '#94a3b8';
}

function qualityStrokeWidth(quality, lqiValue) {
    if (Number.isFinite(Number(lqiValue))) {
        const lqi = Number(lqiValue);
        if (lqi >= 200) return 4;
        if (lqi >= 160) return 3;
        if (lqi >= 120) return 2.5;
        return 2;
    }
    if (quality === 'good') return 3;
    if (quality === 'warn') return 2.5;
    if (quality === 'bad') return 2;
    return 1.5;
}

function renderLqiGraph(rows) {
    const root = document.getElementById('lqiGraph');
    if (!root) return;

    if (!Array.isArray(rows) || rows.length === 0) {
        root.innerHTML = '<div class="lqi-graph-empty">No LQI graph data</div>';
        return;
    }

    const width = Math.max(root.clientWidth || 640, 360);
    const height = 280;
    const cx = width / 2;
    const cy = height / 2;
    const radius = Math.min(width, height) * 0.34;

    const gwAddr = document.getElementById('gwAddr');
    const gwLabel = gwAddr && gwAddr.textContent ? gwAddr.textContent.trim() : 'Gateway';

    const nodes = rows.map((row, idx) => {
        const angle = (Math.PI * 2 * idx) / Math.max(rows.length, 1) - (Math.PI / 2);
        return {
            x: cx + radius * Math.cos(angle),
            y: cy + radius * Math.sin(angle),
            name: (row.name || `0x${Number(row.short_addr || 0).toString(16)}`),
            quality: normalizeLqiQuality(row.quality),
            lqi: row.lqi
        };
    });

    const edgesSvg = nodes.map((n) => {
        const color = qualityStrokeColor(n.quality);
        const widthPx = qualityStrokeWidth(n.quality, n.lqi);
        const dash = n.quality === 'unknown' ? ' stroke-dasharray="5 4"' : '';
        return `<line x1="${cx}" y1="${cy}" x2="${n.x}" y2="${n.y}" stroke="${color}" stroke-width="${widthPx}"${dash}></line>`;
    }).join('');

    const nodeSvg = nodes.map((n) => `
        <circle cx="${n.x}" cy="${n.y}" r="8" fill="#ffffff" stroke="${qualityStrokeColor(n.quality)}" stroke-width="2"></circle>
        <text x="${n.x + 11}" y="${n.y + 4}" font-size="11" fill="#334155">${n.name}</text>
    `).join('');

    root.innerHTML = `
        <svg viewBox="0 0 ${width} ${height}" width="100%" height="${height}" role="img" aria-label="LQI graph">
            <rect x="0" y="0" width="${width}" height="${height}" fill="transparent"></rect>
            ${edgesSvg}
            <circle cx="${cx}" cy="${cy}" r="12" fill="#0d6efd" stroke="#1e3a8a" stroke-width="2"></circle>
            <text x="${cx + 16}" y="${cy + 4}" font-size="12" font-weight="700" fill="#1e293b">GW ${gwLabel}</text>
            ${nodeSvg}
        </svg>
    `;
}

function updateLqiMeta(meta) {
    const sourceEl = document.getElementById('lqiSource');
    const updatedEl = document.getElementById('lqiUpdated');
    const staleEl = document.getElementById('lqiStaleBadge');
    if (!sourceEl || !updatedEl || !staleEl) return;

    if (meta && typeof meta === 'object') {
        currentLqiMeta = meta;
    }

    const source = currentLqiMeta && typeof currentLqiMeta.source === 'string' ? currentLqiMeta.source : 'unknown';
    const updatedMs = currentLqiMeta && Number.isFinite(Number(currentLqiMeta.updated_ms))
        ? Number(currentLqiMeta.updated_ms)
        : 0;
    if (updatedMs > 0) {
        if (updatedMs !== lastLqiUpdateCounter) {
            lastLqiUpdateCounter = updatedMs;
            lastLqiMetaReceivedAtMs = Date.now();
        } else if (lastLqiMetaReceivedAtMs === 0) {
            lastLqiMetaReceivedAtMs = Date.now();
        }
    }

    sourceEl.textContent = lqiSourceLabel(source);
    if (updatedMs > 0) {
        updatedEl.textContent = `${Math.round(updatedMs / 1000)}s uptime`;
    } else {
        updatedEl.textContent = '--';
    }

    const ageMs = lastLqiMetaReceivedAtMs > 0 ? (Date.now() - lastLqiMetaReceivedAtMs) : Number.POSITIVE_INFINITY;
    const stale = !Number.isFinite(ageMs) || ageMs > LQI_STALE_MS;
    staleEl.classList.remove('ok', 'stale');
    staleEl.classList.add(stale ? 'stale' : 'ok');
    staleEl.textContent = stale ? 'stale' : 'fresh';
}

function renderLqiTable(rows, meta) {
    const tbody = document.getElementById('lqiTableBody');
    if (!tbody) return;
    currentLqiRows = Array.isArray(rows) ? rows : [];
    updateLqiMeta(meta || null);

    tbody.innerHTML = '';
    if (!currentLqiRows || currentLqiRows.length === 0) {
        tbody.innerHTML = '<tr><td colspan="9" class="lqi-empty">No LQI data available</td></tr>';
        renderLqiGraph([]);
        renderRouterHints([]);
        return;
    }

    updateLqiHistory(currentLqiRows);
    const ageText = formatAge(getLqiAgeSeconds());
    currentLqiRows.forEach(row => {
        const tr = document.createElement('tr');
        const addrHex = '0x' + Number(row.short_addr || 0).toString(16).toUpperCase().padStart(4, '0');
        const quality = normalizeLqiQuality(row.quality);
        const lqiText = row.lqi === null || row.lqi === undefined ? '--' : String(row.lqi);
        const rssiText = row.rssi === null || row.rssi === undefined ? '--' : `${row.rssi} dBm`;
        const directText = row.direct ? 'direct' : 'indirect';
        const sourceText = lqiSourceLabel(row.source || (meta && meta.source) || 'unknown');
        const trend = trendForAddr(Number(row.short_addr || 0));

        tr.innerHTML = `
            <td>${(row.name || 'Пристрій')}</td>
            <td>${addrHex}</td>
            <td>${lqiText}</td>
            <td class="lqi-age-cell">${ageText}</td>
            <td>${rssiText}</td>
            <td><span class="lqi-quality ${quality}">${quality}</span></td>
            <td><span class="lqi-trend ${trend.cls}">${trend.text}</span></td>
            <td><span class="lqi-link">${directText}</span></td>
            <td>${sourceText}</td>
        `;
        tbody.appendChild(tr);
    });

    renderLqiGraph(currentLqiRows);
    renderRouterHints(currentLqiRows);
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
    setButtonBusy('scanWifiBtn', true);
    const resultsDiv = document.getElementById('wifi-scan-results');
    const ssidInput = document.getElementById('wifi-ssid');
    
    resultsDiv.style.display = 'block';
    resultsDiv.innerHTML = '<div class="scan-item">Сканування...</div>';

    const renderNetworks = (networks) => {
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
    };

    submitJob('scan', {}, { label: 'Wi-Fi scan' })
        .then(info => {
            const result = info.result || {};
            const networks = Array.isArray(result.networks) ? result.networks : [];
            renderNetworks(networks);
        })
        .catch(err => {
            console.error(err);
            resultsDiv.innerHTML = `<div class="scan-item" style="color:red;">Помилка сканування: ${err.message || 'unknown'}</div>`;
        })
        .finally(() => setButtonBusy('scanWifiBtn', false));
}

function clearWifiSettingsBlock() {
    const ssidInput = document.getElementById('wifi-ssid');
    const passInput = document.getElementById('wifi-pass');
    const resultsDiv = document.getElementById('wifi-scan-results');

    if (ssidInput) ssidInput.value = '';
    if (passInput) passInput.value = '';
    if (resultsDiv) {
        resultsDiv.innerHTML = '';
        resultsDiv.style.display = 'none';
    }
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
            clearWifiSettingsBlock();
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
        setButtonBusy('rebootBtn', true);
        submitJob('reboot', {}, { label: 'Gateway reboot' })
            .then(info => {
                const message = (info.result && info.result.message) || 'Перезавантаження заплановано...';
                showToast(message);
            })
            .catch(err => showToast(err.message || "Помилка запиту"))
            .finally(() => setButtonBusy('rebootBtn', false));
    });
}

function factoryResetGateway() {
    showConfirm("Скинути пристрій до заводських налаштувань?", () => {
        setButtonBusy('factoryResetBtn', true);
        submitJob('factory_reset', {}, { label: 'Factory reset' })
            .then(info => {
                const message = (info.result && info.result.message) || "Factory reset completed";
                showToast(message);
            })
            .catch(err => showToast(err.message || "Помилка factory reset"))
            .finally(() => setButtonBusy('factoryResetBtn', false));
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
