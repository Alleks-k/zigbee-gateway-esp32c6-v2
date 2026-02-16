# ESP32-C6 Zigbee Gateway

Цей проєкт перетворює мікроконтролер **ESP32-C6** на Zigbee-шлюз (Coordinator) з веб-інтерфейсом для керування пристроями.

## Основні можливості

- **Zigbee Coordinator**: робота в ролі координатора Zigbee-мережі.
- **Веб-інтерфейс**: керування мережею та пристроями через браузер.
- **mDNS**: доступ за адресою `http://zigbee-gw.local` (або hostname з налаштувань).
- **Автозбереження**: список підключених пристроїв зберігається в NVS.
- **Керування пристроями**: On/Off команди для Zigbee-пристроїв.
- **RCP Auto-update**: підтримка оновлення прошивки радіо-копроцесора (за налаштуванням).
- **SPIFFS**: зберігання веб-файлів у флеш-пам'яті пристрою.
- **AP fallback**: при невдалому STA-підключенні піднімається fallback AP для веб-налаштування.

## Апаратне забезпечення

- **Мікроконтролер**: ESP32-C6 (рекомендовано DevKit).
- **Живлення**: USB або зовнішнє 5V.

## Програмне забезпечення

- **Framework**: [ESP-IDF](https://github.com/espressif/esp-idf) `v5.5.x`.
- **Компоненти**: `esp-zboss-lib`, `esp-zigbee-lib`, `esp_rcp_update`, `mdns`.

## Налаштування та збірка

### 1. Клонування репозиторію

```bash
git clone https://github.com/Alleks-k/zigbee-gateway-esp32c6-v2.git
cd zigbee-gateway-esp32c6-v2
```

### 2. Конфігурація Wi-Fi та Zigbee

1. Скопіюйте файл-приклад у локальний override:

```bash
cp components/gateway_net/include/wifi_credentials.h.example components/gateway_net/include/wifi_credentials_local.h
```

2. Відкрийте `components/gateway_net/include/wifi_credentials_local.h` та впишіть SSID/пароль.
3. Опціонально, для локальних політик STA/AP:

```bash
cp components/gateway_net/include/wifi_settings.h.example components/gateway_net/include/wifi_settings_local.h
```

Також у розділі **ESP Zigbee gateway rcp update** можна налаштувати піни для RCP (для зовнішнього модуля) або залишити стандартні для вбудованого радіо.

### 3. Збірка та прошивка

```bash
./idfw build
./idfw flash monitor
```

## CI parity локально

Щоб локально проганяти ті самі основні перевірки, що і в CI (`self-tests`), встановіть інструменти:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cppcheck
```

Далі запустіть:

```bash
./tools/check_arch_rules.sh
./tools/check_component_edges.sh
./tools/run_host_tests.sh
./tools/run_static_analysis.sh
./tools/run_target_self_tests.sh
```

`run_target_self_tests.sh` потребує доступного `idf.py` (через `IDF_PY`, `IDF_PATH` або після source ESP-IDF environment).
Self-test build overlay конфігурації: `sdkconfig.selftest`.

## Використання

1. Після прошивки пристрій намагається підключитися до STA Wi-Fi.
2. При успіху відкрийте: **[http://zigbee-gw.local](http://zigbee-gw.local)**.
3. Якщо STA не підключився, піднімається fallback AP і веб-налаштування доступні через AP IP.

## API

- Поточна версія API: `/api/v1/*`.
- Legacy alias `/api/*` залишено для сумісності.
- `POST /api/v1/factory_reset` повертає `details` по групах reset: `wifi`, `devices`, `zigbee_storage`, `zigbee_fct`.

## Структура проєкту

- `components/gateway_app/` — bootstrap/orchestration і Zigbee runtime wiring.
- `components/gateway_core_facade/` — фасади для web/api: `gateway_device_zigbee_*`, `gateway_wifi_system_*`, `gateway_jobs_*`.
- `components/gateway_core/` — core business rules/use-cases (`config_service`, `device_service`).
- `components/gateway_core_state/` — runtime state store (network/wifi/lqi cache).
- `components/gateway_core_storage/` — persistence layer (NVS KV/repositories/schema/partitions).
- `components/gateway_core_zigbee|gateway_core_wifi|gateway_core_system|gateway_core_jobs|gateway_core_events/` — доменні та platform adapters.
- `components/gateway_net/` — Wi-Fi STA/AP fallback та мережеві налаштування.
- `components/gateway_web/` — web facade (routes + composition API/WS/static).
- `components/gateway_web_api|gateway_web_ws|gateway_web_static/` — API handlers/use-cases, WebSocket, static serving.
- `main/main.c` — thin app entrypoint that delegates to bootstrap component.
- `main/web/www/` — фронтенд (index.html, style.css, script.js).
- `partitions.csv` — таблиця розділів.

## Модель статусів у core

- У core-компонентах (`gateway_core`, `gateway_core_state`) використовується `gateway_status_t`.
- На межах з ESP-IDF (`gateway_app`, `gateway_net`, `gateway_core_facade`, `gateway_core_zigbee`) робиться явний мапінг `gateway_status_t <-> esp_err_t` через `gateway_status_esp.h`.
- `gateway_state` підтримує вибір lock backend через `gateway_state_set_lock_backend(...)`:
  - `GATEWAY_STATE_LOCK_BACKEND_FREERTOS` (default на ESP-IDF),
  - `GATEWAY_STATE_LOCK_BACKEND_NOOP` (лише коли увімкнено `CONFIG_GATEWAY_STATE_ALLOW_NOOP_LOCK_BACKEND`).
- Неявного fallback на `NOOP` немає: `NOOP` дозволено лише через явний opt-in (`gateway_state_set_lock_backend(GATEWAY_STATE_LOCK_BACKEND_NOOP)`) до першого `gateway_state_init/create`.
- У production тримайте `CONFIG_GATEWAY_STATE_ALLOW_NOOP_LOCK_BACKEND=n` (default); опція призначена для host/test-like профілів.

## Події пристроїв (boundary)

- `device_service` у core не публікує `esp_event` напряму.
- Core віддає події через notifier callbacks (`device_service_set_notifier`).
- `gateway_app` є boundary-adapter: приймає notifier callbacks і публікує `GATEWAY_EVENT_*` в ESP event loop.

## Архітектура

Детальний опис шарів і правил залежностей: [`ARCHITECTURE.md`](./ARCHITECTURE.md).

## Важливі зауваження

- Wi-Fi/Zigbee coexistence працює в 2.4 GHz на спільному радіоканалі.
- Поточне обмеження `MAX_DEVICES` за замовчуванням: 10 (див. `components/gateway_shared_config/include/gateway_config_types.h` і Kconfig).

---
**Author**: [Alleks-k](https://github.com/Alleks-k)
**License**: GNU Affero General Public License v3.0
