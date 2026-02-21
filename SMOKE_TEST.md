# SMOKE TEST CHECKLIST

Цей чекліст покриває базову перевірку після рефакторингу на компонентну архітектуру:
- `gateway_core` + `gateway_core_*`
- `gateway_web` + `gateway_web_*`
- `gateway_net`

## 1. Build / Flash

- [ ] `idf.py fullclean build` завершується без помилок.
- [ ] Прошивка і `www`-розділ успішно залиті.
- [ ] Девайс завантажився без panic/reboot loop.

## 2. Boot Logs

- [ ] Є старт `WEB_SERVER` і реєстрація URI handlers.
- [ ] Є старт mDNS (`zigbee-gw2.local`).
- [ ] Zigbee stack ініціалізований без критичних помилок.
- [ ] Немає `Guru Meditation Error`.

## 3. HTTP API (v1)

- [ ] `GET /api/v1/status` повертає `{"status":"ok","data":...}`.
- [ ] `GET /api/v1/health` повертає валідний snapshot.
- [ ] `GET /api/v1/lqi` повертає `neighbors[]` + `source` + `updated_ms`.
- [ ] `POST /api/v1/jobs {"type":"scan"}` створює job (`job_id`).
- [ ] `GET /api/v1/jobs/{id}` повертає коректний `state` + `result`.
- [ ] `POST /api/v1/jobs {"type":"lqi_refresh"}` завершується `succeeded`.

## 4. WebSocket

- [ ] WS підключення до `/ws` успішне.
- [ ] Є події `devices_delta`.
- [ ] Є події `health_state`.
- [ ] Є події `lqi_update`.
- [ ] Envelope містить `version`, `seq`, `ts`, `type`, `data`.
- [ ] Немає постійних `send_frame_async failed` при стабільному клієнті.

## 5. UI Smoke

- [ ] Веб-сторінка відкривається без JS error у консолі.
- [ ] Кнопка Wi-Fi scan працює.
- [ ] Apply Wi-Fi settings відпрацьовує (через jobs/API шлях).
- [ ] Список Zigbee девайсів відображається.
- [ ] LQI таблиця/граф рендеряться без поламаного layout.
- [ ] `Age` у LQI не зависає на `0s`.

## 6. AP Fallback

- [ ] При невдалому STA девайс піднімає fallback AP.
- [ ] Підключення до AP можливе.
- [ ] Сторінка доступна по `http://192.168.4.1/`.
- [ ] Wi-Fi scan у fallback режимі працює.

## 7. Component Wiring (post-split)

### gateway_core facade
- [ ] API, які очікують `gateway_core`, працюють без зміни include-контракту.

### gateway_core_* subcomponents
- [ ] `gateway_core_storage`: NVS Wi-Fi/devices/schema читається/пишеться.
- [ ] `gateway_core_zigbee`: permit join/control/lqi refresh працюють.
- [ ] `gateway_core_wifi`: scan/save credentials працюють.
- [ ] `gateway_core_system`: reboot/factory reset/telemetry працюють.
- [ ] `gateway_core_jobs`: submit/get job працюють для scan/lqi_refresh/reboot.

### gateway_web facade
- [ ] `web_server` + `http_routes` стартують і маршрути доступні.

### gateway_web_* subcomponents
- [ ] `gateway_web_api`: handlers/usecases/contracts/error format працюють.
- [ ] `gateway_web_ws`: broadcast працює без регресу.
- [ ] `gateway_web_static`: static assets + mDNS працюють.

## 8. Regression Guards

- [ ] `GET /api/v1/health` не ламається на великому `errors[]`.
- [ ] `http_success_send_data_json()` не обрізає нормальні payload.
- [ ] `jobs/{id}` має ліміти `result_json` по типах job.
- [ ] Немає регресу legacy alias маршрутів (`/api/*`) якщо вони ще використовуються.

## 9. Minimal Curl Set

```bash
curl -sS http://zigbee-gw2.local/api/v1/status
curl -sS http://zigbee-gw2.local/api/v1/health
curl -sS http://zigbee-gw2.local/api/v1/lqi
curl -sS -X POST http://zigbee-gw2.local/api/v1/jobs -H "Content-Type: application/json" -d '{"type":"scan"}'
curl -sS -X POST http://zigbee-gw2.local/api/v1/jobs -H "Content-Type: application/json" -d '{"type":"lqi_refresh"}'
curl -sS http://zigbee-gw2.local/api/v1/jobs/<job_id>
```

## 10. Sign-off

- [ ] Smoke пройдено на desktop браузері.
- [ ] Smoke пройдено на mobile клієнті.
- [ ] Логи збережені для релізного артефакту.
- [ ] Лог target self-tests перевірено: `./tools/check_target_selftest_log.sh selftest_run_YYYY-MM-DD_HHMM.log`.
