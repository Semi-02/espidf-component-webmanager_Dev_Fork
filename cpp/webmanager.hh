#pragma once
#include <sdkconfig.h>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_timer.h>
#include <esp_chip_info.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include "esp_netif.h"

#include "esp_tls.h"
#include <hal/efuse_hal.h>
#include <nvs_flash.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>
#include <lwip/ip4_addr.h>
#include <driver/gpio.h>
#include <nvs.h>
#include <spi_flash_mmap.h>
#include <esp_sntp.h>
#include <time.h>
#include <mdns.h>
#include <common-esp32.hh>
#include <esp_log.h>
#include <sys/time.h>
#if (CONFIG_HTTPD_MAX_REQ_HDR_LEN < 1024)
#error "CONFIG_HTTPD_MAX_REQ_HDR_LEN<1024 (Max HTTP Request Header Length)"
#endif

#ifndef CONFIG_HTTPD_WS_SUPPORT
#error "Enable Websocket support for HTTPD in menuconfig"
#endif
#include "esp_vfs.h"

#define TAG "WMAN"
#include "webmanager_constants.hh"
#include "webmanager_interfaces.hh"
#include "webmanager_async_response.hh"
#include "flatbuffers_cpp/ns01wifimanager_generated.h"

namespace webmanager
{
    extern const char webmanager_html_br_start[] asm("_binary_index_compressed_br_start");
    extern const size_t webmanager_html_br_length asm("index_compressed_br_length");

    class M : public webmanager::iWebmanagerCallback
    {
    private:
        static M *singleton;
        uint8_t *http_buffer;
        const char* hostname{nullptr};

        esp_netif_t *wifi_netif_sta{nullptr};
        esp_netif_t *wifi_netif_ap{nullptr};

        wifi_config_t wifi_config_sta = {}; // 132byte
        wifi_config_t wifi_config_ap = {};  // 132byte
        bool fallbackToStoredStaConfig{false};//false means: Fallback is AccessPoint
        //wird auf true gesetzt, wenn eine Sta-Verbindung erfolgreich ist und die config im NVS gespeichert wurde
        //wird auf false gesetzt, wenn der accessPoint gestartet wird

        SemaphoreHandle_t webmanager_semaphore{nullptr}; // stellt sicher, dass die Timer-Aufrufe nicht überlappen können
        TimerHandle_t timSupervisor{nullptr};

        httpd_handle_t http_server{nullptr};
        int websocket_file_descriptor{-1};

        // Das ist der Status, der alles beschreiben muss
        WorkingState workingState{WorkingState::AP_STARTED};
        time_t tTimeout_us{INT64_MAX};
        time_t tShutdownAp_us{INT64_MAX};
        time_t tReconnect_us{INT64_MAX};

        bool staConnectionState{false};
        
        uint32_t remainingAttempsToConnectAsSTA{1};

        const char* ws2c(WorkingState w){
            return WorkingStateStrings[static_cast<size_t>(w)];
        }
        
        void setStatus(WorkingState workingState, time_t tTimeout_us=FAR_FUTURE, time_t tShutdownAp_us=INT64_MAX, time_t tReconnect_us=INT64_MAX)
        {
            if(this->workingState!=workingState){
                this->workingState = workingState;
                ESP_LOGI(TAG, "Switch to workingState %s", ws2c(this->workingState));
            }
            if(this->tReconnect_us!=tReconnect_us){
                this->tReconnect_us=tReconnect_us;
                if(this->tReconnect_us==FAR_FUTURE){
                    ESP_LOGI(TAG, "Deactivating tReconnect while beeing in state %s", ws2c(this->workingState));
                }else{
                    ESP_LOGI(TAG, "Setting tReconnect to %llums while beeing in state %s", tReconnect_us/1000, ws2c(this->workingState));
                }
            }
            if(this->tTimeout_us!=tTimeout_us){
                this->tTimeout_us=tTimeout_us;
                if(this->tTimeout_us==FAR_FUTURE){
                    ESP_LOGI(TAG, "Deactivating tTimeout_us while beeing in state %s", ws2c(this->workingState));
                }else{
                    ESP_LOGI(TAG, "Setting tTimeout_us to %llums while beeing in state %s", tTimeout_us/1000, ws2c(this->workingState));
                }
            }
            if(this->tShutdownAp_us!=tShutdownAp_us){
                this->tShutdownAp_us=tShutdownAp_us;
                if(this->tShutdownAp_us==FAR_FUTURE){
                    ESP_LOGI(TAG, "Deactivating tShutdownAp_us while beeing in state %s", ws2c(this->workingState));
                }else{
                    ESP_LOGI(TAG, "Setting tShutdownAp_us to %llums while beeing in state %s", tShutdownAp_us/1000, ws2c(this->workingState));
                }
            }
        }

        std::vector<iWebmanagerPlugin *> *plugins{nullptr};

        M() { http_buffer = new uint8_t[HTTP_BUFFER_SIZE]; }

        void connectAsSTA(time_t now_us, uint32_t remainingAttempsToConnectAsSTA)
        {
            this->remainingAttempsToConnectAsSTA = remainingAttempsToConnectAsSTA;
            ESP_LOGI(TAG, "Trying to connect as station. {'ssid':'%s', 'password':'%s', 'fallback':'%s'}", wifi_config_sta.sta.ssid, wifi_config_sta.sta.password, fallbackToStoredStaConfig?"STORED_STA":"AP");
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta));
            ESP_ERROR_CHECK(esp_wifi_connect());
            this->setStatus(WorkingState::KEEP_CONNECTION, now_us+COMMON_TIMEOUT_US, FAR_FUTURE, FAR_FUTURE);
        }

        void configureAndOpenAccessPointAndSetStatus()
        {
            fallbackToStoredStaConfig=false;//because now, the AP must be the fallback
            ESP_LOGI(TAG, "Opening Access Point. {'ssid':'%s', 'password':'%s'}", wifi_config_ap.ap.ssid, wifi_config_ap.ap.password);
            wifi_mode_t mode;
            esp_wifi_get_mode(&mode);
            if(mode!=WIFI_MODE_APSTA){
                //has to be done "lazy", because otherwise already connected stations loose their connections
                ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
                ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap));
            }
            this->setStatus(WorkingState::AP_STARTED, FAR_FUTURE);
        }

        void sendWifiConnectionNotSuccessfulMessage()
        {
            flatbuffers::FlatBufferBuilder b(256);
            b.Finish(
                wifimanager::CreateResponseWrapper(
                    b,
                    wifimanager::Responses::Responses_ResponseWifiConnect,
                    wifimanager::CreateResponseWifiConnectDirect(b, false, "", 0, 0, 0).Union()));
            WrapAndSendAsync(wifimanager::Namespace::Namespace_Value, b);
        }

        void sendWifiConnectionSuccessfulMessage(const esp_netif_ip_info_t *ip){
            create_or_update_sta_config();
            wifi_ap_record_t ap = {};
            esp_wifi_sta_get_ap_info(&ap);
            flatbuffers::FlatBufferBuilder b(256);
            b.Finish(
                wifimanager::CreateResponseWrapper(
                    b,
                    wifimanager::Responses::Responses_ResponseWifiConnect,
                    wifimanager::CreateResponseWifiConnectDirect(b, true, (char *)wifi_config_sta.sta.ssid, ip->ip.addr, ip->netmask.addr, ip->gw.addr).Union()));
            WrapAndSendAsync(wifimanager::Namespace::Namespace_Value, b);
        }

        esp_err_t delete_sta_config()
        {
            esp_err_t ret;
            nvs_handle handle{0};
            // Removed unused variable 'ret'
            GOTO_ERROR_ON_ERROR(nvs_open_from_partition(NVS_PARTITION, WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle), "Unable to open nvs partition");
            GOTO_ERROR_ON_ERROR(nvs_erase_key(handle, nvs_key_wifi_ssid), "Unable to delete wifi ssid");
            GOTO_ERROR_ON_ERROR(nvs_erase_key(handle, nvs_key_wifi_password), "Unable to delete wifi password");
            ret = nvs_commit(handle);
            ESP_LOGI(TAG, "Successfully erased Wifi Sta configuration in flash");
        error:
            nvs_close(handle);
            return ret;
        }

        esp_err_t create_or_update_sta_config()
        {
            nvs_handle handle;
            esp_err_t ret = ESP_OK;
            char tmp_ssid[33];     /**< SSID of target AP. */
            char tmp_password[64]; /**< Password of target AP. */
            bool changeSsid{false};
            bool changePassword{false};
            size_t sz{0};

            ESP_LOGD(TAG, "About to save config to flash!!");
            GOTO_ERROR_ON_ERROR(nvs_open_from_partition(NVS_PARTITION, WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle), "Unable to open nvs partition");
            sz = sizeof(tmp_ssid);
            ret = nvs_get_str(handle, nvs_key_wifi_ssid, tmp_ssid, &sz);
            if ((ret == ESP_OK && strcmp((char *)tmp_ssid, (char *)wifi_config_sta.sta.ssid) != 0) || ret == ESP_ERR_NVS_NOT_FOUND)
            {
                /* different ssid or ssid does not exist in flash: save new ssid */
                GOTO_ERROR_ON_ERROR(nvs_set_str(handle, nvs_key_wifi_ssid, (const char *)wifi_config_sta.sta.ssid), "Unable to nvs_set_str(handle, \"ssid\", ssid_sta)");
                ESP_LOGD(TAG, "wifi_manager_wrote wifi_sta_config: ssid: %s", wifi_config_sta.sta.ssid);
                changeSsid = true;
            }

            sz = sizeof(tmp_password);
            ret = nvs_get_str(handle, nvs_key_wifi_password, tmp_password, &sz);
            if ((ret == ESP_OK && strcmp((char *)tmp_password, (char *)wifi_config_sta.sta.password) != 0) || ret == ESP_ERR_NVS_NOT_FOUND)
            {
                /* different password or password does not exist in flash: save new password */
                GOTO_ERROR_ON_ERROR(nvs_set_str(handle, nvs_key_wifi_password, (const char *)wifi_config_sta.sta.password), "Unable to nvs_set_str(handle, \"password\", password_sta)");
                ESP_LOGI(TAG, "wifi_manager_wrote wifi_sta_config: password: %s", wifi_config_sta.sta.password);
                changePassword = true;
            }
            if (changeSsid || changePassword)
            {
                ret = nvs_commit(handle);
                ESP_LOGI(TAG, "Updated Ssid '%s' and/or password '%s' have been written to flash", wifi_config_sta.sta.ssid, wifi_config_sta.sta.password);
            }
            else
            {
                ESP_LOGI(TAG, "Ssid '%s' and/or password '%s' have not been changed.", wifi_config_sta.sta.ssid, wifi_config_sta.sta.password);
            }
        error:
            nvs_close(handle);
            return ret;
        }

        esp_err_t read_sta_config()
        {
            nvs_handle handle;
            esp_err_t ret = ESP_OK;
            size_t sz;
            GOTO_ERROR_ON_ERROR(nvs_open_from_partition(NVS_PARTITION, WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle), "Unable to open nvs partition '%s' and namespace '%s' ", NVS_PARTITION, WIFI_NVS_NAMESPACE);
            sz = sizeof(wifi_config_sta.sta.ssid);
            GOTO_ERROR_ON_ERROR(nvs_get_str(handle, nvs_key_wifi_ssid, (char *)wifi_config_sta.sta.ssid, &sz), "Unable to read Wifi SSID from NVS");
            sz = sizeof(wifi_config_sta.sta.password);
            GOTO_ERROR_ON_ERROR(nvs_get_str(handle, nvs_key_wifi_password, (char *)wifi_config_sta.sta.password, &sz), "Unable to read Wifi password from NVS");
            ESP_LOGI(TAG, "Successfully read Wifi credentials {'ssid':'%s', 'password':'%s'}", wifi_config_sta.sta.ssid, wifi_config_sta.sta.password);
            ret = (wifi_config_sta.sta.ssid[0] == '\0') ? ESP_FAIL : ESP_OK;
        error:
            nvs_close(handle);
            return ret;
        }

        void supervisorTask(){
            while(true){
                this->Supervise();
                vTaskDelay(pdMS_TO_TICKS(4000));
            }
        }

        void wifi_event_handler(esp_event_base_t event_base, int32_t event_id, void *event_data)
        {
            xSemaphoreTake(webmanager_semaphore, portMAX_DELAY);
            time_t now_us = esp_timer_get_time();
            switch (event_id)
            {
            case WIFI_EVENT_SCAN_DONE:
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                staConnectionState = false;
                if (remainingAttempsToConnectAsSTA == 0){
                    if(fallbackToStoredStaConfig && read_sta_config()){
                        ESP_LOGW(TAG, "Establishing connection to new SSID failed finally. Go back to stored SSID {'ssid':'%s', 'password':'%s'}", wifi_config_ap.ap.ssid, wifi_config_ap.ap.password);
                        fallbackToStoredStaConfig=false;
                        connectAsSTA(now_us, 1);
                        this->setStatus(WorkingState::KEEP_CONNECTION, now_us + COMMON_TIMEOUT_US);
                        this->sendWifiConnectionNotSuccessfulMessage();
                    }
                    else{
                        ESP_LOGW(TAG, "Establishing connection to SSID failed finally. Go back to Access Point Mode {'ssid':'%s', 'password':'%s'}", wifi_config_ap.ap.ssid, wifi_config_ap.ap.password);
                        this->sendWifiConnectionNotSuccessfulMessage();
                        configureAndOpenAccessPointAndSetStatus();
                    }
                }
                else{
                    ESP_LOGW(TAG, "Establishing connection with SSID '%s' failed. Still %lu attempts to try.", wifi_config_sta.sta.ssid, remainingAttempsToConnectAsSTA);
                    this->setStatus(WorkingState::KEEP_CONNECTION, now_us + COMMON_TIMEOUT_US, FAR_FUTURE, now_us+RECONNECT_TIMEOUT_US);
                }
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Established connection to SSID successfully. Now, waiting for a IP address... {'ssid':'%s', 'password':'%s'}", wifi_config_sta.sta.ssid, wifi_config_sta.sta.password);
                staConnectionState = true;
                create_or_update_sta_config();
                fallbackToStoredStaConfig=true;
                this->remainingAttempsToConnectAsSTA=RECONNECTS_ON_OPERATION;
                //Das Timeout muss hier auf einen sinnvollen wert gesetzt werden, weil wir ja noch keine IP-Adresse haben
                //erst wenn diese im IP-Handler gesetzt wird, kann das timeout auf FAR-FUTURE gesetzt werden
                this->setStatus(WorkingState::KEEP_CONNECTION, now_us + COMMON_TIMEOUT_US, FAR_FUTURE, FAR_FUTURE);
                //Nein, erst wenn die IP-Adresse gesetzt wurde... this->sendWifiConnectionSuccessfulMessage()
                break;
            case WIFI_EVENT_AP_START:
            {
                ESP_LOGI(TAG, "Successfully started Access Point with ssid %s and password '%s'. Webmanager is here: https://%s", wifi_config_ap.ap.ssid, wifi_config_ap.ap.password, hostname);
                break;
            }
            case WIFI_EVENT_AP_STOP:
            {
                ESP_LOGI(TAG, "Successfully closed Access Point.");
                break;
            }
            case WIFI_EVENT_AP_STACONNECTED:
            {
                wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
                ESP_LOGI(TAG, "Station " MACSTR " joined this AccessPoint, AID=%d", MAC2STR(event->mac), event->aid);
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED:
            {
                wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
                ESP_LOGI(TAG, "Station " MACSTR " leaved this AccessPoint, AID=%d", MAC2STR(event->mac), event->aid);
                break;
            }
            }
            xSemaphoreGive(webmanager_semaphore);
        }

        void ip_event_handler(esp_event_base_t event_base, int32_t event_id, void *event_data)
        {
            xSemaphoreTake(webmanager_semaphore, portMAX_DELAY);
            time_t now_us = esp_timer_get_time();
            switch (event_id)
            {
            case IP_EVENT_AP_STAIPASSIGNED:{
                const ip_event_ap_staipassigned_t *ip = (ip_event_ap_staipassigned_t *)event_data;
                ESP_LOGI(TAG, "Connected Wifi Station got IP from DHCP {'ip':'" IPSTR "'}", IP2STR(&ip->ip));
                break;
            }
            case IP_EVENT_STA_GOT_IP:
            {
                const ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                const esp_netif_ip_info_t *ip = &(event->ip_info);
                ESP_LOGI(TAG, "Wifi Sta got IP from DHCP {'ip':'" IPSTR "', 'netmask':'" IPSTR "','gw':'" IPSTR "', 'hostname':'%s'}", IP2STR(&ip->ip), IP2STR(&ip->netmask), IP2STR(&ip->gw), hostname);
                this->setStatus(WorkingState::KEEP_CONNECTION, FAR_FUTURE, now_us+SHUTDOWN_AP_TIMEOUT_US, FAR_FUTURE);
                this->sendWifiConnectionSuccessfulMessage(ip);
                esp_sntp_init();
                break;
            }
            case IP_EVENT_ETH_GOT_IP:
            {
                const ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                const esp_netif_ip_info_t *ip = &(event->ip_info);
                ESP_LOGI(TAG, "ETHERNET got IP from DHCP: {'ip':'" IPSTR "', 'netmask':'" IPSTR "','gw':'" IPSTR "', 'hostname':'%s'}", IP2STR(&ip->ip), IP2STR(&ip->netmask), IP2STR(&ip->gw), hostname);
                esp_sntp_init(); // seems to be safe if called twice (ETH and WIFI STA!)
                break;
            }
            case IP_EVENT_ETH_LOST_IP:
            {
                ESP_LOGI(TAG, "IP_EVENT_ETH_LOST_IP");
                break;
            }
            case IP_EVENT_STA_LOST_IP:
            {
                ESP_LOGD(TAG, "IP_EVENT_STA_LOST_IP");
                break;
            }
            }
            xSemaphoreGive(webmanager_semaphore);
        }

        void sntp_handler()
        {
            time_t now;
            char strftime_buf[64];
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);
            strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
            ESP_LOGI(TAG, "Notification of a time synchronization. The current date/time in Berlin is: %s", strftime_buf);
            for (const auto &p : *this->plugins)
            {
                p->OnTimeUpdate(this);
            }
            // LogJournal(messagecodes::C::SNTP, esp_timer_get_time() / 1000);
        }

        static void ws_async_send(void *arg)
        {
            M *myself = M::GetSingleton();
            AsyncResponse *a = static_cast<AsyncResponse *>(arg);
            assert(a);
            assert(a->buffer);
            assert(a->buffer_len);
            assert(myself);
            if (myself->http_server && myself->websocket_file_descriptor != -1)
            {
                httpd_ws_frame_t ws_pkt = {false, false, HTTPD_WS_TYPE_BINARY, a->buffer, a->buffer_len};
                httpd_ws_send_frame_async(myself->http_server, myself->websocket_file_descriptor, &ws_pkt);
                ESP_LOGD(TAG, "httpd_ws_send_frame_async: data_len:%u\n", ws_pkt.len);
                // should be syncronous. So the buffer can be deleted, when the function returns
            }
            delete a;
        }

        esp_err_t handle_webmanager_ws(httpd_req_t *req)
        {
            if (req->method == HTTP_GET)
            {
                ESP_LOGI(TAG, "Handshake done, the new websocket connection was opened");
                return ESP_OK;
            }

            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");

            httpd_ws_frame_t ws_pkt = {false, false, HTTPD_WS_TYPE_BINARY, nullptr, 0};

            // always store the last websocket file descriptor
            this->websocket_file_descriptor = httpd_req_to_sockfd(req);

            /* Set max_len = 0 to get the frame len */
            ESP_ERROR_CHECK(httpd_ws_recv_frame(req, &ws_pkt, 0));
            if (ws_pkt.len == 0 || ws_pkt.type != HTTPD_WS_TYPE_BINARY)
            {
                ESP_LOGE(TAG, "Received an empty or an non binary websocket frame");
                return ESP_OK;
            }
            uint8_t *buf = new uint8_t[ws_pkt.len];
            assert(buf);
            ws_pkt.payload = buf;
            esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);

            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
                delete[] buf;
                return ret;
            }
            uint32_t ns = *((uint32_t *)buf);
            uint8_t *fb_buf = buf + 4;
            if (ns == wifimanager::Namespace::Namespace_Value)
            {
                auto rw = flatbuffers::GetRoot<wifimanager::RequestWrapper>(fb_buf);
                wifimanager::Requests reqType = rw->request_type();
                ESP_LOGI(TAG, "Received wifimanager request: len=%d, requestType=%d ", (int)ws_pkt.len, reqType);
                switch (reqType)
                {
                case wifimanager::Requests::Requests_RequestNetworkInformation:
                    sendResponseNetworkInformation(req, &ws_pkt, rw->request_as_RequestNetworkInformation());
                    break;

                case wifimanager::Requests::Requests_RequestWifiConnect:
                    handleRequestWifiConnect(req, &ws_pkt, rw->request_as_RequestWifiConnect());
                    break;
                case wifimanager::Requests::Requests_RequestWifiDisconnect:
                    handleRequestWifiDisconnect(req, &ws_pkt, rw->request_as_RequestWifiDisconnect());
                    break;
                default:
                    break;
                }
            }
            else
            {
                eMessageReceiverResult success{eMessageReceiverResult::NOT_FOR_ME};
                if (plugins)
                {
                    for (auto p : *plugins)
                    {
                        success = p->ProvideWebsocketMessage(this, req, &ws_pkt, ns, fb_buf);
                        if (success != eMessageReceiverResult::NOT_FOR_ME)
                        {
                            break;
                        }
                    }
                }
                if (success == eMessageReceiverResult::NOT_FOR_ME)
                {
                    ESP_LOGW(TAG, "Not yet implemented request for namespace %lu, neither internal nor in a plugin", ns);
                }
                else if (success == eMessageReceiverResult::FOR_ME_BUT_FAILED)
                {
                    ESP_LOGW(TAG, "Request for namespace %lu has been implemented by plugin, but processing failed", ns);
                }
            }
            delete[] buf;
            return ESP_OK;
        }

        esp_err_t handleRequestWifiConnect(httpd_req_t *req, httpd_ws_frame_t *ws_pkt, const wifimanager::RequestWifiConnect *wifiConnect)
        {

            esp_err_t ret{ESP_OK};
            time_t now_us{0};
            const char *ssid = wifiConnect->ssid()->c_str();
            const char *password = wifiConnect->password()->c_str();
            size_t len{0};
            len = strlen(ssid);
            ESP_GOTO_ON_FALSE(len <= MAX_SSID_LEN - 1, ESP_FAIL, negativeresponse, TAG, "SSID too long");
            len = strlen(password);
            ESP_GOTO_ON_FALSE(len <= MAX_PASSPHRASE_LEN - 1, ESP_FAIL, negativeresponse, TAG, "PASSPHRASE too long");
            ESP_GOTO_ON_FALSE(len > 0, ESP_FAIL, negativeresponse, TAG, "no PASSPHRASE given");
            strncpy((char *)wifi_config_sta.sta.ssid, ssid, MAX_SSID_LEN - 1);
            strncpy((char *)wifi_config_sta.sta.password, password, MAX_PASSPHRASE_LEN - 1 );
            wifi_config_sta.sta.ssid[MAX_SSID_LEN - 1] = '\0'; 
            wifi_config_sta.sta.password[MAX_PASSPHRASE_LEN - 1] = '\0';
            ESP_LOGI(TAG, "Got a new ssid '%s' and password '%s' from browser.", wifi_config_sta.sta.ssid, wifi_config_sta.sta.password);
            if (!xSemaphoreTake(webmanager_semaphore, portMAX_DELAY))
                return ESP_FAIL;
            now_us = esp_timer_get_time();
            connectAsSTA(now_us, 1);
            xSemaphoreGive(webmanager_semaphore);
            return ret;
        negativeresponse:
            flatbuffers::FlatBufferBuilder b(256);
            b.Finish(
                wifimanager::CreateResponseWrapper(
                    b,
                    wifimanager::Responses::Responses_ResponseWifiConnect,
                    wifimanager::CreateResponseWifiConnectDirect(b, false, (char *)wifi_config_sta.sta.ssid, 0, 0, 0, 0).Union()));
            return WrapAndSendAsync(wifimanager::Namespace::Namespace_Value, b);
        }

        esp_err_t handleRequestWifiDisconnect(httpd_req_t *req, httpd_ws_frame_t *ws_pkt, const wifimanager::RequestWifiDisconnect *wifiDisconnect)
        {
            
            flatbuffers::FlatBufferBuilder b(256);
            b.Finish(
                wifimanager::CreateResponseWrapper(
                    b,
                    wifimanager::Responses::Responses_ResponseWifiDisconnect,
                    wifimanager::CreateResponseWifiDisconnect(b).Union()));
            WrapAndSendAsync(wifimanager::Namespace::Namespace_Value, b);
            vTaskDelay(pdMS_TO_TICKS(2000)); // warte 200ms, um die Beantwortung des Requests noch zu ermöglichen

            if (!xSemaphoreTake(webmanager_semaphore, portMAX_DELAY))
                return ESP_FAIL;
            ESP_ERROR_CHECK(esp_wifi_disconnect());
            delete_sta_config();
            ESP_LOGI(TAG, "Disconnected as STA from ssid '%s'.", wifi_config_sta.sta.ssid);
            configureAndOpenAccessPointAndSetStatus();
            xSemaphoreGive(webmanager_semaphore);
            return ESP_OK;
        }

        esp_err_t sendResponseNetworkInformation(httpd_req_t *req, httpd_ws_frame_t *ws_pkt, const wifimanager::RequestNetworkInformation *netInfo)
        {
            //bool forceUpdate = netInfo->force_new_search();
            ESP_LOGI(TAG, "Prepare to send CreateResponseNetworkInformationDirect");
            esp_err_t ret{ESP_OK};
            
            wifi_ap_record_t *ap{nullptr};
            wifi_ap_record_t my_ap={};
            esp_netif_ip_info_t ap_ip_info = {};
            esp_netif_ip_info_t sta_ip_info = {};
            wifi_ap_record_t accessp_records[MAX_AP_NUM];
            uint16_t accessp_records_len = MAX_AP_NUM;

            flatbuffers::FlatBufferBuilder b(1024);
            std::vector<flatbuffers::Offset<wifimanager::AccessPoint>> ap_vector;

            //if (!xSemaphoreTake(webmanager_semaphore, portMAX_DELAY)) return ESP_ERR_INVALID_STATE;

            GOTO_ERROR_ON_ERROR(esp_wifi_scan_start(nullptr, true), "Wifi Scan did NOT complete successfully.");
            GOTO_ERROR_ON_ERROR(esp_wifi_scan_get_ap_records(&accessp_records_len, accessp_records), "Could not get access point list");
            ESP_LOGI(TAG, "Wifi Scan successfully completed. Found %d access points.", accessp_records_len);
            
            
            for (size_t i = 0; i < accessp_records_len; i++)
            {
                ap = accessp_records + i;
                ap_vector.push_back(wifimanager::CreateAccessPoint(b, b.CreateString((char *)ap->ssid), ap->primary, ap->rssi, ap->authmode));
                ESP_LOGI(TAG, "  AP %25s; %4d", (char *)ap->ssid, ap->rssi);
            }

            ESP_ERROR_CHECK(esp_netif_get_ip_info(wifi_netif_ap, &ap_ip_info));
            ESP_ERROR_CHECK(esp_netif_get_ip_info(wifi_netif_sta, &sta_ip_info));
            esp_wifi_sta_get_ap_info(&my_ap);
        error:
            //xSemaphoreGive(webmanager_semaphore);
            b.Finish(
                wifimanager::CreateResponseWrapper(
                    b,
                    wifimanager::Responses::Responses_ResponseNetworkInformation,
                    wifimanager::CreateResponseNetworkInformationDirect(
                        b,
                        hostname,
                        (char *)wifi_config_ap.ap.ssid,
                        (char *)wifi_config_ap.ap.password,
                        ap_ip_info.ip.addr,
                        this->staConnectionState,
                        (char *)wifi_config_sta.sta.ssid,
                        sta_ip_info.ip.addr,
                        sta_ip_info.netmask.addr,
                        sta_ip_info.gw.addr,
                        my_ap.rssi,
                        &ap_vector)
                        .Union()));

            ret= WrapAndSendAsync(wifimanager::Namespace::Namespace_Value, b);
            return ret;
        }

        esp_err_t handle_ota_post(httpd_req_t *req)
        {
            ESP_LOGI(TAG, "in handle_ota_post");
            char buf[1024];
            esp_ota_handle_t ota_handle;
            size_t remaining = req->content_len;

            const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
            ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));

            while (remaining > 0)
            {
                int recv_len = httpd_req_recv(req, buf, std::min(remaining, (size_t)sizeof(buf)));
                if (recv_len <= 0)
                {
                    // Serious Error: Abort OTA
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
                    return ESP_FAIL;
                }
                if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
                {
                    // Timeout Error: Just retry
                    continue;
                }
                if (esp_ota_write(ota_handle, (const void *)buf, recv_len) != ESP_OK)
                {
                    // Successful Upload: Flash firmware chunk
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
                    return ESP_FAIL;
                }

                remaining -= recv_len;
            }

            // Validate and switch to new OTA image and reboot
            if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK)
            {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
                return ESP_FAIL;
            }

            httpd_resp_sendstr(req, "Firmware update complete, rebooting now!\n");

            vTaskDelay(500 / portTICK_PERIOD_MS);
            esp_restart();

            return ESP_OK;
        }

        esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath)
        {
            struct dirent *entry;
            DIR *dir = opendir(dirpath);
            if (!dir)
            {
                ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath);
                httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
                return ESP_FAIL;
            }
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr_chunk(req, "{'files':[");
            while ((entry = readdir(dir)) != nullptr)
            {
                if (entry->d_type == DT_DIR)
                    continue;
                httpd_resp_sendstr_chunk(req, "'");
                httpd_resp_sendstr_chunk(req, entry->d_name);
                httpd_resp_sendstr_chunk(req, "',");
            }
            closedir(dir);
            dir = opendir(dirpath);

            httpd_resp_sendstr_chunk(req, "], 'dirs':[");
            while ((entry = readdir(dir)) != nullptr)
            {
                if (entry->d_type != DT_DIR)
                    continue;
                httpd_resp_sendstr_chunk(req, "'");
                httpd_resp_sendstr_chunk(req, entry->d_name);
                httpd_resp_sendstr_chunk(req, "',");
            }
            closedir(dir);
            httpd_resp_sendstr_chunk(req, "]}");
            httpd_resp_sendstr_chunk(req, NULL);
            return ESP_OK;
        }

        esp_err_t handle_files_get(httpd_req_t *req)
        {
            FILE *fd = nullptr;
            struct stat file_stat;

            const char *path = req->uri + FILES_BASE_PATH_LEN;
            ESP_LOGI(TAG, "Got GET files for filename %s ", path);

            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");

            /* If name has trailing '/', respond with directory contents */
            if (path[strlen(path) - 1] == '/')
            {
                return http_resp_dir_html(req, path);
            }

            if (stat(path, &file_stat) == -1)
            {
                httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
                return ESP_FAIL;
            }

            fd = fopen(path, "r");
            if (!fd)
            {
                ESP_LOGE(TAG, "Failed to read existing file : %s", path);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
                return ESP_FAIL;
            }

            ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", path, file_stat.st_size);

            size_t chunksize;
            do
            {
                /* Read file in chunks into the scratch buffer */
                chunksize = fread(http_buffer, 1, HTTP_BUFFER_SIZE, fd);

                if (chunksize > 0)
                {
                    if (httpd_resp_send_chunk(req, (const char *)http_buffer, chunksize) != ESP_OK)
                    {
                        fclose(fd);
                        ESP_LOGE(TAG, "File sending failed!");
                        httpd_resp_sendstr_chunk(req, nullptr);
                        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                        return ESP_FAIL;
                    }
                }
            } while (chunksize != 0);

            fclose(fd);
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_OK;
        }

        esp_err_t handle_files_post(httpd_req_t *req)
        {
            FILE *fd = NULL;
            struct stat file_stat;

            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");

            const char *path = req->uri + FILES_BASE_PATH_LEN;
            ESP_LOGI(TAG, "Got POST files for filename %s ", path);

            if (path[strlen(path) - 1] == '/')
            {
                ESP_LOGE(TAG, "We need a filename, not a directory name : %s", path);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "We need a filename, not a directory name!");
                return ESP_FAIL;
            }

            if (false && stat(path, &file_stat) == 0)
            {
                // Files should be overwritten, hence "false &&"
                ESP_LOGE(TAG, "File already exists : %s", path);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
                return ESP_FAIL;
            }

            if (req->content_len > MAX_FILE_SIZE)
            {
                ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File too large");
                return ESP_FAIL;
            }

            fd = fopen(path, "w");
            if (!fd)
            {
                ESP_LOGE(TAG, "Failed to create file : %s", path);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
                return ESP_FAIL;
            }

            ESP_LOGI(TAG, "Receiving file : %s...", path);
            size_t received;
            size_t remaining = req->content_len;

            while (remaining > 0)
            {

                ESP_LOGI(TAG, "Remaining size : %d", remaining);
                /* Receive the file part by part into a buffer */
                if ((received = httpd_req_recv(req, (char *)http_buffer, std::min(remaining, HTTP_BUFFER_SIZE))) <= 0)
                {
                    if (received == HTTPD_SOCK_ERR_TIMEOUT)
                        continue;
                    /* In case of unrecoverable error,
                     * close and delete the unfinished file*/
                    fclose(fd);
                    unlink(path);
                    ESP_LOGE(TAG, "File reception failed!");
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
                    return ESP_FAIL;
                }

                /* Write buffer content to file on storage */
                if (received && (received != fwrite(http_buffer, 1, received, fd)))
                {
                    /* Couldn't write everything to file!
                     * Storage may be full? */
                    fclose(fd);
                    unlink(path);

                    ESP_LOGE(TAG, "File write failed!");
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
                    return ESP_FAIL;
                }
                remaining -= received;
            }

            fclose(fd);
            if (stat(path, &file_stat) != 0)
            {
                ESP_LOGE(TAG, "File stat was not possible. write failed!");
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File stat was not possible. write failed!");
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "File reception for %s complete. File has %ldbytes", path, file_stat.st_size);
            httpd_resp_sendstr(req, "File uploaded successfully");
            return ESP_OK;
        }

        esp_err_t handle_files_delete(httpd_req_t *req)
        {
            struct stat file_stat;
            const char *path = req->uri + FILES_BASE_PATH_LEN;
            ESP_LOGI(TAG, "Got DELETE files for filename %s ", path);

            /* Filename cannot have a trailing '/' */
            if (path[strlen(path) - 1] == '/')
            {
                ESP_LOGE(TAG, "We need a filename, not a directory name : %s", path);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "We need a filename, not a directory name!");
                return ESP_FAIL;
            }

            if (stat(path, &file_stat) == -1)
            {
                ESP_LOGE(TAG, "File does not exist : %s", path);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
                return ESP_FAIL;
            }

            ESP_LOGI(TAG, "Deleting file : %s", path);
            unlink(path);
            httpd_resp_sendstr(req, "File deleted successfully");
            return ESP_OK;
        }

    public:
        static M *GetSingleton()
        {
            if (!singleton)
            {
                singleton = new M();
            }
            return singleton;
        }

        bool GetStaState()
        {
            return this->staConnectionState;
        }

        const char *GetHostname()
        {
            esp_netif_get_hostname(this->wifi_netif_sta, &hostname);
            return hostname;
        }

        esp_ip4_addr_t GetIpAddress()
        {  
            esp_netif_ip_info_t  ip={};
            esp_netif_get_ip_info(this->wifi_netif_sta, &ip);
            return ip.ip;
        }

        const char *GetSsid()
        {
            return (const char *)this->wifi_config_sta.sta.ssid;
        }

        bool HasRealtime()
        {
            struct timeval tv_now;
            gettimeofday(&tv_now, nullptr);
            time_t seconds_epoch = tv_now.tv_sec;
            return seconds_epoch > 1684412222; // epoch time when this code has been written
        }

        esp_err_t WrapAndSendAsync(uint32_t ns, ::flatbuffers::FlatBufferBuilder &b) override
        {
            if (!http_server)
                return ESP_FAIL;
            auto *a = new AsyncResponse(ns, &b);
            if (httpd_queue_work(http_server, M::ws_async_send, a) != ESP_OK)
            {
                delete (a);
            }
            return ESP_OK;
        }

        void RegisterHTTPDHandlers(httpd_handle_t httpd_handle)
        {
            httpd_uri_t files_get = {
                FILES_GLOB,
                HTTP_GET,
                [](httpd_req_t *req)
                { return static_cast<M *>(req->user_ctx)->handle_files_get(req); },
                this, false, false, nullptr};
            ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &files_get));

            httpd_uri_t files_post = {
                FILES_GLOB,
                HTTP_POST,
                [](httpd_req_t *req)
                { return static_cast<M *>(req->user_ctx)->handle_files_post(req); },
                this, false, false, nullptr};
            ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &files_post));

            httpd_uri_t files_delete = {
                FILES_GLOB,
                HTTP_DELETE,
                [](httpd_req_t *req)
                { return static_cast<M *>(req->user_ctx)->handle_files_delete(req); },
                this, false, false, nullptr};
            ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &files_delete));

            httpd_uri_t ota_post = {
                "/ota",
                HTTP_POST,
                [](httpd_req_t *req)
                { return static_cast<M *>(req->user_ctx)->handle_ota_post(req); },
                this, false, false, nullptr};
            ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &ota_post));
            httpd_uri_t webmanager_ws = {
                "/webmanager_ws",
                HTTP_GET,
                [](httpd_req_t *req)
                { return static_cast<webmanager::M *>(req->user_ctx)->handle_webmanager_ws(req); }, this, true, false, nullptr};
            ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &webmanager_ws));
            httpd_uri_t webmanager_get = {
                "/*", HTTP_GET,
                [](httpd_req_t *req)
                {
                    httpd_resp_set_type(req, "text/html");
                    httpd_resp_set_hdr(req, "Content-Encoding", "br");
                    httpd_resp_send(req, webmanager_html_br_start, webmanager_html_br_length);
                    return ESP_OK;
                },
                this, false, false, nullptr};
            ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &webmanager_get));
            this->http_server = httpd_handle;
        }



        esp_err_t Begin(const char *accessPointSsid, const char *accessPointPassword, const char *hostname, bool resetStoredWifiConnection, std::vector<iWebmanagerPlugin *> *plugins, bool init_netif_and_create_event_loop = true, bool startOwnSupervisorTask=true, esp_log_level_t wifiLogLevel=ESP_LOG_WARN)
        {
            ESP_LOGI(TAG, "Stating Webmanager");
            
            this->hostname=hostname;
            
            if (strlen(accessPointPassword) < 8 && AP_AUTHMODE != WIFI_AUTH_OPEN){
                ESP_LOGE(TAG, "Password too short for authentication. Minimal length is 8. Exiting Webmanager");
                return ESP_FAIL;
            }

            if (webmanager_semaphore != nullptr){
                ESP_LOGE(TAG, "webmanager already started. Exiting 'Begin'-method");
                return ESP_FAIL;
            }
            
            webmanager_semaphore = xSemaphoreCreateBinary();
            xSemaphoreGive(webmanager_semaphore);

            if (init_netif_and_create_event_loop)
            {
                ESP_ERROR_CHECK(esp_netif_init());
                ESP_ERROR_CHECK(esp_event_loop_create_default());
            }

            this->plugins = plugins;

            // Create and check netifs
            wifi_netif_sta = esp_netif_create_default_wifi_sta();
            wifi_netif_ap = esp_netif_create_default_wifi_ap();
            assert(wifi_netif_sta);
            assert(wifi_netif_ap);

            // attach event handler for wifi & ip
            ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, [](void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
                                                                { static_cast<webmanager::M *>(arg)->wifi_event_handler(event_base, event_id, event_data); }, this, nullptr));
            ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, [](void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
                                                                { static_cast<webmanager::M *>(arg)->ip_event_handler(event_base, event_id, event_data); }, this, nullptr));

            // init WIFI base
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            ESP_ERROR_CHECK(esp_wifi_init(&cfg));
            ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

            // Prepare WIFI_CONFIG for sta mode
            wifi_config_sta.sta.scan_method = WIFI_FAST_SCAN;
            wifi_config_sta.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
            wifi_config_sta.sta.threshold.rssi = -127;
            wifi_config_sta.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            wifi_config_sta.sta.pmf_cfg.capable = true;
            wifi_config_sta.sta.pmf_cfg.required = false;

            wifi_config_ap.ap.channel = 0;
            wifi_config_ap.ap.max_connection = 1;
            wifi_config_ap.ap.authmode = AP_AUTHMODE;
            std::strcpy((char *)(wifi_config_ap.ap.ssid), accessPointSsid);
            std::strcpy((char *)(wifi_config_ap.ap.password), accessPointPassword);
            

            ESP_ERROR_CHECK(esp_netif_set_hostname(wifi_netif_sta, hostname));
            ESP_ERROR_CHECK(esp_netif_set_hostname(wifi_netif_ap, hostname));

            ESP_ERROR_CHECK(mdns_init());
            ESP_ERROR_CHECK(mdns_hostname_set(hostname));
            const char *MDNS_INSTANCE = "ESP32_MDNS_INSTANCE";
            ESP_ERROR_CHECK(mdns_instance_name_set(MDNS_INSTANCE));

            // set wifi logging 
            esp_log_level_set("wifi", wifiLogLevel);

            // Turn Power Saving off
            ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

            // SNTP (simple network time protocol) client and start it, when we got an IP address (see event handler)
            esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
            esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
            esp_sntp_setservername(0, "pool.ntp.org");
            esp_sntp_set_time_sync_notification_cb([](struct timeval *tv)
                                                   { webmanager::M::GetSingleton()->sntp_handler(); });
            setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); // Germany
            tzset();
            time_t now_us = esp_timer_get_time();
            if (resetStoredWifiConnection)
            {
                ESP_LOGI(TAG, "Forced to delete saved wifi configuration. Starting access point and do an initial scan.");
                delete_sta_config();
                configureAndOpenAccessPointAndSetStatus();
            }
            else if (read_sta_config() != ESP_OK)
            {
                ESP_LOGI(TAG, "Unable to read WIFI SSID or PASSWORD from flash. Starting access point and do an initial scan.");
                configureAndOpenAccessPointAndSetStatus();
            }
            else
            {
                // auf keinen Fall einen AccessPoint aufmachen
                ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
                ESP_ERROR_CHECK(esp_wifi_start());
                connectAsSTA(now_us, RECONNECTS_ON_STARTUP);
                this->setStatus(WorkingState::KEEP_CONNECTION, now_us + COMMON_TIMEOUT_US);
            }
            ESP_ERROR_CHECK(esp_wifi_start());
            for (const auto &i : *this->plugins)
            {
                i->OnBegin(this);
            }

            ESP_LOGI(TAG, "Webmanager has been succcessfully initialized");

            // Configure and start timer
            if(startOwnSupervisorTask){
                xTaskCreate([](void* arg){((webmanager::M *)(arg))->supervisorTask();}, "wifi_supervisor", 4*4096, this, 12, nullptr);
            }
            return ESP_OK;
        }

        esp_err_t CallMeAfterInitializationToMarkCurrentPartitionAsValid()
        {
            /* Mark current app as valid */
            ESP_LOGI(TAG, "Webmanager marks current Partition as valid");
            const esp_partition_t *partition = esp_ota_get_running_partition();
            esp_ota_img_states_t ota_state;
            if (esp_ota_get_state_partition(partition, &ota_state) == ESP_OK)
            {
                if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
                {
                    esp_ota_mark_app_valid_cancel_rollback();
                }
            }
            return ESP_OK;
        }

        void Supervise(){
            xSemaphoreTake(webmanager_semaphore, portMAX_DELAY);
            time_t now_us = esp_timer_get_time();
            ESP_LOGD("WMSV", "timSupervisor_cb {'workingState':'%s', 'tReconnect':%lld, 'tShutdownAp':%lld, 'tTimeout':%lld}",
                ws2c(workingState),
               tReconnect_us/1000,
               tShutdownAp_us/1000,
               tTimeout_us/1000
               );
            if(now_us>tReconnect_us){
                connectAsSTA(now_us, remainingAttempsToConnectAsSTA-1);
            }
            if(now_us>tShutdownAp_us){
                wifi_mode_t mode;
                esp_wifi_get_mode(&mode);
                if(mode!=WIFI_MODE_STA){
                    esp_wifi_set_mode(WIFI_MODE_STA);
                    ESP_LOGI("WMSV", "Switching off AccessPoint");
                }else{
                    ESP_LOGI("WMSV", "Switching off AccessPoint...but it was already off.");
                }
                setStatus(WorkingState::KEEP_CONNECTION, FAR_FUTURE, FAR_FUTURE, FAR_FUTURE);
            }
            if(now_us>tTimeout_us){
                ESP_LOGW("WMSV", "Unexpected full Timeout in Webmanager while beeing in state %s. Go back to AccessPoint-Mode", ws2c(workingState));
                configureAndOpenAccessPointAndSetStatus();
            }
            xSemaphoreGive(webmanager_semaphore);
        }
    };
}
#undef TAG
