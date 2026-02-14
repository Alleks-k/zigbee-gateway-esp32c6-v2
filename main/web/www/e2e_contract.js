(function () {
    const resultsEl = document.getElementById('results');

    function addResult(ok, text) {
        const li = document.createElement('li');
        li.className = ok ? 'pass' : 'fail';
        li.textContent = `${ok ? 'PASS' : 'FAIL'}: ${text}`;
        resultsEl.appendChild(li);
        if (!ok) {
            throw new Error(text);
        }
    }

    function sleep(ms) {
        return new Promise((resolve) => setTimeout(resolve, ms));
    }

    function waitFor(predicate, timeoutMs, label) {
        return new Promise((resolve, reject) => {
            const start = Date.now();
            const tick = () => {
                if (predicate()) {
                    resolve();
                    return;
                }
                if (Date.now() - start > timeoutMs) {
                    reject(new Error(`timeout: ${label}`));
                    return;
                }
                setTimeout(tick, 20);
            };
            tick();
        });
    }

    class MockWebSocket {
        static instances = [];
        constructor(url) {
            this.url = url;
            this.readyState = 0;
            MockWebSocket.instances.push(this);
            setTimeout(() => {
                this.readyState = 1;
                if (typeof this.onopen === 'function') {
                    this.onopen();
                }
            }, 10);
        }
        send() {}
        close() {
            this.readyState = 3;
            if (typeof this.onclose === 'function') {
                this.onclose({ code: 1000, reason: 'closed', wasClean: true });
            }
        }
        emitJson(obj) {
            if (typeof this.onmessage === 'function') {
                this.onmessage({ data: JSON.stringify(obj) });
            }
        }
    }

    const jobs = new Map();
    const submittedJobTypes = [];
    let nextJobId = 1;
    const statusPayload = {
        status: 'ok',
        data: {
            pan_id: 0x1234,
            channel: 15,
            short_addr: 0,
            devices: [{ short_addr: 0x1001, name: 'Lamp A' }],
        },
    };
    const healthPayload = {
        status: 'ok',
        data: {
            zigbee: { started: true, pan_id: 0x1234, channel: 15, short_addr: 0 },
            wifi: { sta_connected: true, fallback_ap_active: false, active_ssid: 'TestNet', rssi: -52, link_quality: 'good', ip: '192.168.1.11' },
            nvs: { schema_version: 1 },
            system: { uptime_ms: 10000, heap_free: 200000, heap_min: 180000, heap_largest_block: 128000, temperature_c: 45.0 },
            errors: [],
        },
    };
    const lqiPayload = { status: 'ok', data: { neighbors: [], source: 'unknown', updated_ms: 1 } };

    window.WebSocket = MockWebSocket;
    window.fetch = async (url, options = {}) => {
        const method = (options.method || 'GET').toUpperCase();
        const path = String(url);

        function jsonResponse(payload, status) {
            return new Response(JSON.stringify(payload), {
                status: status || 200,
                headers: { 'Content-Type': 'application/json' },
            });
        }

        if (path.endsWith('/api/v1/status') && method === 'GET') {
            return jsonResponse(statusPayload);
        }
        if (path.endsWith('/api/v1/health') && method === 'GET') {
            return jsonResponse(healthPayload);
        }
        if (path.endsWith('/api/v1/lqi') && method === 'GET') {
            return jsonResponse(lqiPayload);
        }
        if (path.endsWith('/api/v1/jobs') && method === 'POST') {
            const body = options.body ? JSON.parse(options.body) : {};
            submittedJobTypes.push(String(body.type || ''));
            const id = nextJobId++;
            const result = body.type === 'scan'
                ? { networks: [{ ssid: 'TestNet-A', rssi: -40, auth: 3 }, { ssid: 'TestNet-B', rssi: -70, auth: 0 }] }
                : { message: 'ok' };
            jobs.set(id, { state: 'succeeded', done: true, result });
            return jsonResponse({ status: 'ok', data: { job_id: id } });
        }
        const jobMatch = path.match(/\/api\/v1\/jobs\/(\d+)$/);
        if (jobMatch && method === 'GET') {
            const id = Number(jobMatch[1]);
            const job = jobs.get(id) || { state: 'failed', done: true, result: { error: 'not found' } };
            return jsonResponse({ status: 'ok', data: job });
        }
        return jsonResponse({ status: 'error', error: { message: `unhandled mock route: ${method} ${path}` } }, 404);
    };

    async function runContracts() {
        resultsEl.innerHTML = '';
        addResult(true, 'Harness started');

        await waitFor(() => MockWebSocket.instances.length > 0, 1000, 'websocket init');
        await waitFor(() => {
            const ws = document.getElementById('ws-status');
            return ws && ws.textContent.includes('Online');
        }, 2000, 'ws online');
        addResult(true, 'WS connected and online badge updated');

        await waitFor(() => document.getElementById('panId').textContent.includes('0x1234'), 2000, 'status pan_id');
        addResult(true, 'status contract rendered to DOM');

        await waitFor(() => document.getElementById('wifiState').textContent.includes('STA Connected'), 2000, 'health wifi');
        addResult(true, 'health contract rendered to DOM');

        const ws = MockWebSocket.instances[0];
        ws.emitJson({
            version: 1,
            seq: 7,
            ts: 1700001000,
            type: 'lqi_update',
            data: {
                neighbors: [{
                    short_addr: 4097,
                    name: 'Lamp A',
                    lqi: 150,
                    rssi: null,
                    quality: 'warn',
                    direct: true,
                    source: 'mgmt_lqi',
                    updated_ms: 1000,
                }],
                source: 'mgmt_lqi',
                updated_ms: 1000,
            },
        });
        await waitFor(() => document.querySelectorAll('#lqiTableBody tr').length === 1, 2000, 'lqi table render');
        addResult(true, 'ws lqi_update contract rendered');
        await waitFor(() => {
            const badge = document.getElementById('lqiRssiInfoBadge');
            return badge && badge.style.display !== 'none' && badge.textContent.includes('no RSSI in Mgmt LQI');
        }, 1500, 'mgmt_lqi badge');
        addResult(true, 'mgmt_lqi RSSI badge rendered');

        window.renderLqiTable(
            [{
                short_addr: 0x2002,
                name: 'Weak End Device',
                lqi: 60,
                rssi: -90,
                quality: 'bad',
                direct: false,
                source: 'neighbor_table',
                updated_ms: 1000,
            }],
            { source: 'neighbor_table', updated_ms: 1000 }
        );
        await waitFor(() => document.querySelector('.lqi-weak-badge') !== null, 1500, 'weak link badge');
        addResult(true, 'weak-link badge rendered for low LQI');

        const staleBadge = document.getElementById('lqiStaleBadge');
        window.updateLqiMeta({ source: 'mgmt_lqi', updated_ms: 2000 });
        await waitFor(() => staleBadge && staleBadge.textContent === 'fresh', 1000, 'fresh badge');
        const originalDateNow = Date.now;
        Date.now = () => originalDateNow() + 65000;
        window.updateLqiMeta({ source: 'mgmt_lqi', updated_ms: 2000 });
        await waitFor(() => staleBadge && staleBadge.textContent === 'stale', 1000, 'stale badge');
        Date.now = originalDateNow;
        window.updateLqiMeta({ source: 'mgmt_lqi', updated_ms: 3000 });
        await waitFor(() => staleBadge && staleBadge.textContent === 'fresh', 1000, 'fresh after update');
        addResult(true, 'LQI age stale/fresh branch covered');

        window.maybeAutoRefreshLqi();
        await waitFor(() => submittedJobTypes.includes('lqi_refresh'), 3000, 'auto lqi refresh job');
        addResult(true, 'LQI auto-refresh branch covered');

        window.scanWifi();
        await waitFor(() => document.getElementById('wifi-scan-results').textContent.includes('TestNet-A'), 3000, 'wifi scan render');
        addResult(true, 'wifi scan job contract rendered');

        await sleep(50);
        addResult(true, 'All browser e2e contracts passed');
    }

    document.getElementById('run').addEventListener('click', () => {
        runContracts().catch((err) => addResult(false, err.message || String(err)));
    });

    window.addEventListener('load', () => {
        runContracts().catch((err) => addResult(false, err.message || String(err)));
    });
})();
