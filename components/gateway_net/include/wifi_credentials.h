#ifndef WIFI_CREDENTIALS_H
#define WIFI_CREDENTIALS_H

/*
 * Tracked wrapper with safe defaults.
 * Real credentials should be placed into:
 *   main/net/wifi_credentials_local.h
 * (this file is gitignored).
 */

#if __has_include("wifi_credentials_local.h")
#include "wifi_credentials_local.h"
#endif

#ifndef MY_WIFI_SSID
#define MY_WIFI_SSID "YOUR_SSID_HERE"
#endif

#ifndef MY_WIFI_PASSWORD
#define MY_WIFI_PASSWORD "YOUR_PASSWORD_HERE"
#endif

#endif
