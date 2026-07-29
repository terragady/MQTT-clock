#pragma once

// Template for local secrets. Copy this file to "secrets.h" (same folder)
// and fill in your real values. secrets.h is git-ignored and must NOT be
// committed. secrets.example.h (this file) IS committed as documentation.

// MQTT broker login (Mosquitto add-on -> Configuration -> Logins)
#define SECRET_MQTT_USER ""
#define SECRET_MQTT_PASSWORD ""

// TimezoneDB API key (https://timezonedb.com/account)
#define SECRET_TIMEZONE_DB_API_KEY ""
