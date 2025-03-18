#pragma once
#include <inttypes.h>
#include <cstring>
#include <ctime>
#include <esp_wifi.h>

namespace webmanager{
    constexpr size_t MAX_AP_NUM = 8;    
    constexpr uint32_t ATTEMPTS_TO_RECONNECT_ON_STARTUP_BEFORE_OPENING_AN_ACCESS_POINT{3};
    constexpr uint32_t ATTEMPTS_TO_RECONNECT_DURING_OPERATION_BEFORE_OPENING_AN_ACCESS_POINT{UINT32_MAX};
    constexpr BaseType_t TIMER_INTERVAL_TICKS{pdMS_TO_TICKS(1000)};
    constexpr time_t WIFI_MANAGER_RETRY_TIMER = 8000;
    constexpr time_t WIFI_MANAGER_SHUTDOWN_AP_TIMER = 60000;
    constexpr wifi_auth_mode_t AP_AUTHMODE{wifi_auth_mode_t::WIFI_AUTH_WPA2_PSK};
    constexpr char NVS_PARTITION[]{"nvs"};
    constexpr char WIFI_NVS_NAMESPACE[]{"wifimananger"};
    constexpr char nvs_key_wifi_ssid[]{"ssid"};
    constexpr char nvs_key_wifi_password[]{"password"};
}