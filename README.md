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

## Використання

1. Після прошивки пристрій намагається підключитися до STA Wi-Fi.
2. При успіху відкрийте: **[http://zigbee-gw.local](http://zigbee-gw.local)**.
3. Якщо STA не підключився, піднімається fallback AP і веб-налаштування доступні через AP IP.

## API

- Поточна версія API: `/api/v1/*`.
- Legacy alias `/api/*` залишено для сумісності.
- `POST /api/v1/factory_reset` повертає `details` по групах reset: `wifi`, `devices`, `zigbee_storage`, `zigbee_fct`.

## Структура проєкту

- `components/gateway_core/` — service layer, settings manager, event loop, zigbee domain/state.
- `components/gateway_net/` — Wi-Fi STA/AP fallback та мережеві налаштування.
- `components/gateway_web/` — HTTP routes, API handlers, WebSocket manager, static serving.
- `components/gateway_app/` — runtime/bootstrap orchestration (startup flow).
- `main/main.c` — thin app entrypoint that delegates to bootstrap component.
- `main/web/www/` — фронтенд (index.html, style.css, script.js).
- `partitions.csv` — таблиця розділів.

## Архітектура

Детальний опис шарів і правил залежностей: [`ARCHITECTURE.md`](./ARCHITECTURE.md).

## Важливі зауваження

- Wi-Fi/Zigbee coexistence працює в 2.4 GHz на спільному радіоканалі.
- Поточне обмеження `MAX_DEVICES` за замовчуванням: 10 (див. `components/gateway_core/include/device_manager.h`).

---
**Author**: [Alleks-k](https://github.com/Alleks-k)
**License**: GNU Affero General Public License v3.0
