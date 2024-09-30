#pragma once
#include <stdio.h>
#include <inttypes.h>
#include <algorithm>
#include <array>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <common-esp32.hh>
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "../generated/flatbuffers_cpp/app_generated.h"
#include "interfaces.hh"
#include <esp_log.h>
#define TAG "USET"
// Prinzip codeerzeugung
/*
browser und esp 32 kommunizieren nur über groupIndex und settingIndex
die namen der namespaces und der keys sind in constexpr char arrays hinterlegt

bei getUserSettings übermittelt der browser nur die wohldefinierte groupIndex
der esp32 holt sich das entsprechend
*/
namespace settings{

}

namespace webmanager
{

    struct SettingCfg
    {
        const char *settingKey;
        webmanager::Setting type;
    };

    class GroupCfg
    {
    public:
        const char *groupKey;
        size_t setting_len;
        SettingCfg settings[];
    };

    struct GroupAndStringSetting
    {
        const char *groupKey;
        const char *settingkey;
    };

    struct GroupAndIntegerSetting
    {
        const char *groupKey;
        const char *settingkey;
    };

    struct GroupAndBooleanSetting
    {
        const char *groupKey;
        const char *settingkey;
    };

    struct GroupAndEnumSetting
    {
        const char *groupKey;
        const char *settingkey;
    };


    

#include "../generated/usersettings/usersettings_config.hh.inc"



    class UserSettings
    {
    private:
        const char *partitionName{nullptr};
        MessageSender *messageSender{nullptr};

        const GroupCfg *GetGroup(const char *groupKey)
        {

            for (const GroupCfg *group : groups)
            {
                if (strcmp(groupKey, group->groupKey) == 0)
                {
                    return group;
                }
            }
            return nullptr;
        }

    public:
        UserSettings(const char *partitionName, MessageSender *messageSender) : partitionName(partitionName), messageSender(messageSender) {}

        esp_err_t handleRequestSetUserSettings(httpd_req_t *httpreq, httpd_ws_frame_t *ws_pkt, const webmanager::RequestSetUserSettings *req)
        {
            const char *groupKey = req->groupKey()->c_str();
            const GroupCfg *group = GetGroup(groupKey);
            RETURN_FAIL_ON_FALSE(group != nullptr, "There is no group with key '%s'", groupKey);
            ESP_LOGI(TAG, "In handleRequestSetUserSettings for GroupKey %s ItemCount %u", groupKey, group->setting_len);
            
            flatbuffers::FlatBufferBuilder b(1024);
            std::vector<flatbuffers::Offset<flatbuffers::String>> keys_vector;

            nvs_handle_t nvs_handle{0};
            RETURN_ON_ERROR(nvs_open_from_partition(partitionName, group->groupKey, NVS_READWRITE, &nvs_handle));
            ESP_LOGI(TAG, "Successfully opened partition, group: %s with %u items. Updating %lu items", group->groupKey, group->setting_len, req->settings()->size());

            auto settings = req->settings();
            for (size_t i = 0; i < settings->size(); i++)
            {
                auto settingWrapper = settings->Get(i);
                const char* settingKey = settingWrapper->settingKey()->c_str();
                webmanager::Setting setting_type = settingWrapper->setting_type();
                switch (setting_type)
                {
                case Setting_IntegerSetting:
                {
                    int32_t value = static_cast<const IntegerSetting *>(settingWrapper->setting())->value();
                    ESP_ERROR_CHECK(nvs_set_i32(nvs_handle, settingKey, value));
                    
                    break;
                }
                case Setting_EnumSetting:
                {
                    int32_t value = static_cast<const EnumSetting *>(settingWrapper->setting())->value();
                    ESP_ERROR_CHECK(nvs_set_i32(nvs_handle, settingKey, value));
                    break;
                }
                case Setting_BooleanSetting:
                {
                    uint8_t value = static_cast<const EnumSetting *>(settingWrapper->setting())->value();
                    ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, settingKey, value));
                    break;
                }
                case Setting_StringSetting:
                {
                    const char *value = static_cast<const StringSetting *>(settingWrapper->setting())->value()->c_str();
                    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, settingKey, value));
                    break;
                }
                default:
                    break;
                }
                keys_vector.push_back(b.CreateString(settingKey));
            }
            // Close
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
            
            auto resp = CreateResponseSetUserSettingsDirect(b, groupKey, &keys_vector);
            messageSender->WrapAndFinishAndSendAsync(b, webmanager::Responses_ResponseSetUserSettings, resp.Union());
            return ESP_OK;
        }

        esp_err_t GetIntegerSetting(const GroupAndIntegerSetting& s, int32_t* value){
            nvs_handle_t nvs_handle{0};
            RETURN_ON_ERROR(nvs_open(s.groupKey, NVS_READONLY, &nvs_handle));
            esp_err_t ret = nvs_get_i32(nvs_handle, s.settingkey, value);
            nvs_close(nvs_handle);
            return ret;
        }

        esp_err_t GetStringSetting(const GroupAndStringSetting& s, char* value, size_t maxLen){
            nvs_handle_t nvs_handle{0};
            RETURN_ON_ERROR(nvs_open(s.groupKey, NVS_READONLY, &nvs_handle));
            esp_err_t ret = nvs_get_str(nvs_handle, s.settingkey, value, &maxLen);
            nvs_close(nvs_handle);
            return ret;
        }

        esp_err_t GetBoolSetting(const GroupAndBooleanSetting& s, bool* value){
            nvs_handle_t nvs_handle{0};
            RETURN_ON_ERROR(nvs_open(s.groupKey, NVS_READONLY, &nvs_handle));
            uint8_t tmp{0};
            esp_err_t ret = nvs_get_u8(nvs_handle, s.settingkey, &tmp);
            nvs_close(nvs_handle);
            *value=tmp!=0;
            return ret;
        }

        esp_err_t GetEnumSetting(const GroupAndEnumSetting& s, int32_t* value){
            nvs_handle_t nvs_handle{0};
            RETURN_ON_ERROR(nvs_open(s.groupKey, NVS_READONLY, &nvs_handle));
            esp_err_t ret = nvs_get_i32(nvs_handle, s.settingkey, value);
            nvs_close(nvs_handle);
            return ret;
        }
        
        esp_err_t handleRequestGetUserSettings(httpd_req_t *httpreq, httpd_ws_frame_t *ws_pkt, const webmanager::RequestGetUserSettings *req)
        {
            const char *groupKey = req->groupKey()->c_str();
            const GroupCfg *group = GetGroup(groupKey);
            RETURN_FAIL_ON_FALSE(group != nullptr, "There is no group with key '%s'", groupKey);
            ESP_LOGI(TAG, "In handleRequestGetUserSettings for GroupKey %s ItemCount %u", groupKey, group->setting_len);

            nvs_handle_t nvs_handle{0};
            RETURN_ON_ERROR(nvs_open_from_partition(partitionName, group->groupKey, NVS_READONLY, &nvs_handle));
            ESP_LOGI(TAG, "Successfully opened partition, reading %u items", group->setting_len);
            flatbuffers::FlatBufferBuilder b(1024);
            std::vector<flatbuffers::Offset<SettingWrapper>> sw_vector;
            for (size_t i = 0; i < group->setting_len; i++)
            {
                const SettingCfg *settingCfg = &group->settings[i];
                const char* settingKey = settingCfg->settingKey;
                switch (settingCfg->type)
                {
                case Setting_IntegerSetting:
                {
                    int32_t value{0};
                    ESP_ERROR_CHECK(nvs_get_i32(nvs_handle, settingKey, &value));
                    auto sw = CreateSettingWrapperDirect(b, settingKey, webmanager::Setting_IntegerSetting, CreateIntegerSetting(b, value).Union());
                    sw_vector.push_back(sw);
                    break;
                }
                case Setting_EnumSetting:
                {
                    int32_t value{0};
                    ESP_ERROR_CHECK(nvs_get_i32(nvs_handle, settingKey, &value));
                    auto sw = CreateSettingWrapperDirect(b, settingKey, webmanager::Setting_EnumSetting, CreateEnumSetting(b, value).Union());
                    sw_vector.push_back(sw);
                    break;
                }
                case Setting_BooleanSetting:
                {
                    uint8_t value{0};
                    ESP_ERROR_CHECK(nvs_get_u8(nvs_handle, settingKey, &value));
                    auto sw = CreateSettingWrapperDirect(b, settingKey, webmanager::Setting_BooleanSetting, CreateBooleanSetting(b, value).Union());
                    sw_vector.push_back(sw);
                    break;
                }
                case Setting_StringSetting:
                {
                    /*
                    To get the size necessary to store the value, call nvs_get_str with zero out_value and non-zero pointer to length.
                    Variable pointed to by length argument will be set to the required length.
                    For nvs_get_str, this length includes the zero terminator.
                    When calling nvs_get_str with non-zero out_value, length has to be non-zero and has to point to the length available in out_value.
                    */

                    char *value{nullptr};
                    size_t length{0};
                    if (nvs_get_str(nvs_handle, settingKey, value, &length) != ESP_OK)
                    {
                        ESP_LOGW(TAG, "Can`t sead StringSetting %s", settingKey);
                        break;
                    }
                    // now, length contains the necessary size
                    if (length == 0)
                    {
                        ESP_LOGW(TAG, "Read StringSetting %s successfully, but with zero length value", settingKey);
                        value = new char[1]{'\0'};
                    }
                    else
                    {
                        value = new char[length];
                        ESP_ERROR_CHECK(nvs_get_str(nvs_handle, settingKey, value, &length));
                        ESP_LOGI(TAG, "Read StringSetting %s successfully with value %s", settingKey, value);
                    }
                    auto sw = CreateSettingWrapperDirect(b, settingKey, webmanager::Setting_StringSetting, CreateStringSettingDirect(b, value).Union());
                    delete[] value;
                    sw_vector.push_back(sw);
                    break;
                }
                default:
                    break;
                }
            }
            // Close
            nvs_close(nvs_handle);
            auto resp = CreateResponseGetUserSettingsDirect(b, groupKey, &sw_vector);
            messageSender->WrapAndFinishAndSendAsync(b, webmanager::Responses_ResponseGetUserSettings, resp.Union());
            return ESP_OK;
        }
    };
}
#undef TAG