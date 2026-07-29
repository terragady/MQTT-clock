# MQTT LED Matrix Clock (ZegarTV)

An ESP8266 (Wemos D1 mini) driving a MAX72xx 4×1 LED matrix. It shows the time,
adjusts brightness on a day/night schedule, and integrates with Home Assistant
over MQTT with auto-discovery. Notifications, animations, OTA updates and a web
updater are supported.

## Hardware

- **Board:** Wemos D1 mini (ESP8266)
- **Display:** 4× MAX72xx 8×8 modules (32×8 px), rotation configurable
- **Wiring:** `CLK → D5 (SCK)`, `CS → D6`, `DIN → D7 (MOSI)`

## Build & flash (PlatformIO)

```bash
pio run                                   # build
pio run -t upload --upload-port <port>    # flash over USB
pio run -t upload --upload-port <ip>      # flash over OTA (ArduinoOTA)
```

### Secrets

Credentials are kept out of source control. Copy the template and fill it in:

```bash
cp src/secrets.example.h src/secrets.h
```

`src/secrets.h` is git-ignored. It defines `SECRET_MQTT_USER`,
`SECRET_MQTT_PASSWORD`, and `SECRET_TIMEZONE_DB_API_KEY`.

Other settings (MQTT broker IP, timezone, pins, display size, default schedule
and brightness) live in `src/Settings.h`.

## First boot / Wi-Fi

On first boot (or when it can't join a known network) the clock opens a
WiFiManager config-portal access point named **"Zegar TV"**. Connect to it and
enter your Wi-Fi credentials. If the very first connect after a flash fails,
power-cycle the device in its normal location.

## Reliability

- OTA and the web updater start **before** MQTT/time, so firmware recovery is
  always reachable even if a network service is slow or down.
- No internet, failed time sync, or an unreachable MQTT broker never blocks the
  device. Until the first successful time sync the display shows `--:--`.
- If the clock drops offline, its MQTT Last Will marks it unavailable in HA.

## Home Assistant

The clock publishes MQTT discovery configs, so a single **"MQTT Clock"** device
appears automatically with these entities:

| Entity | Type | Purpose |
| --- | --- | --- |
| Clock Status | sensor | online/offline status |
| Day/Night Mode | sensor | current mode |
| Day Brightness | number (0–15) | brightness during the day |
| Night Brightness | number (0–15) | brightness at night |
| Day Start Time | time (HH:MM) | when day mode begins |
| Night Start Time | time (HH:MM) | when night mode begins |
| Send Notification | text | send a message (see below) |
| Animation | select | `heart` / `wave` / `pulse` |

> The **Send Notification** entity carries the notification schema (below) in
> its **Attributes** in Home Assistant, so the usage is discoverable in-app.

### Driving the schedule from the sun

Because the schedule uses `time` entities, a Sun-based automation can set them:

```yaml
- alias: "Clock day mode at sunrise"
  trigger: { platform: sun, event: sunrise }
  action:
    - service: time.set_value
      target: { entity_id: time.mqtt_clock_day_start_time }
      data: { time: "{{ now().strftime('%H:%M:%S') }}" }

- alias: "Clock night mode at sunset"
  trigger: { platform: sun, event: sunset }
  action:
    - service: time.set_value
      target: { entity_id: time.mqtt_clock_night_start_time }
      data: { time: "{{ now().strftime('%H:%M:%S') }}" }
```

## MQTT topics

Base prefix: `clock/zegarTV`

| Topic | Direction | Payload |
| --- | --- | --- |
| `clock/zegarTV/notification` | in | plain text or JSON (see below) |
| `clock/zegarTV/notification/help` | out (retained) | usage docs (HA attributes) |
| `clock/zegarTV/animation` | in | `heart` / `wave` / `pulse` |
| `clock/zegarTV/brightness/day` | in | `0`–`15` |
| `clock/zegarTV/brightness/night` | in | `0`–`15` |
| `clock/zegarTV/schedule/day_start` | in | `HH:MM:SS` |
| `clock/zegarTV/schedule/night_start` | in | `HH:MM:SS` |
| `clock/zegarTV/status` | out (retained) | JSON status |
| `clock/zegarTV/discovery` | in | any payload re-sends discovery |

## Notifications

Publish to `clock/zegarTV/notification`.

- **Plain text** — scrolls once, e.g. `Hello`.
- **JSON object** — any of the following fields:

| Field | Type | Default | Notes |
| --- | --- | --- | --- |
| `message` | string | — | required |
| `scrolling` | bool | `true` | `false` = static, centered |
| `speed` | int 5–100 | `35` | ms per step; lower = faster |
| `repeat` | int 1–10 | `1` | scroll repeats |
| `brightness` | int 0–15 or `-1` | `-1` | `-1` = keep current |
| `flash` | bool | `false` | static messages only |
| `flash_count` | int 1–10 | `3` | flashes when `flash` is true |

Examples:

```json
{"message": "Dinner!", "speed": 15, "repeat": 2}
{"message": "ALERT", "scrolling": false, "flash": true, "flash_count": 5, "brightness": 15}
```

Text is UTF-8 and mapped to the display's CP437 font. The degree sign `°` is
handled automatically (e.g. `21°C` renders correctly).

## Animations

Publish one of `heart`, `wave`, `pulse` to `clock/zegarTV/animation`.

## OTA & web updater

- **ArduinoOTA** on port 8266 (PlatformIO OTA / espota).
- **Web updater** on port 80 for uploading a firmware `.bin` from a browser.
