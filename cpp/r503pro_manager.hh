#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <memory>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "sdkconfig.h"
#include "r503pro.hh"
#include <common.hh>
#include "webmanager_interfaces.hh"

#define TAG "FINGER"
#include "esp_log.h"
namespace fingerprint
{

    class R503ProManager : public R503Pro, public iFingerprintHandler
    {
    private:
        iFingerprintActionHandler *handler;
        webmanager::iScheduler *scheduler;
        nvs_handle_t nvsFingerName2FingerIndex;
        nvs_handle_t nvsFingerIndex2SchedulerName;
        nvs_handle_t nvsFingerIndex2ActionIndex;
        SemaphoreHandle_t mutex;

    public:
        R503ProManager(uart_port_t uart_num, gpio_num_t gpio_irq, iFingerprintActionHandler *handler, webmanager::iScheduler *scheduler, nvs_handle_t nvsFingerName2FingerIndex, nvs_handle_t nvsFingerIndex2SchedulerName, nvs_handle_t nvsFingerIndex2ActionIndex, uint32_t targetAddress = DEFAULT_ADDRESS) : R503Pro(uart_num, gpio_irq, this), handler(handler), scheduler(scheduler), nvsFingerName2FingerIndex(nvsFingerName2FingerIndex), nvsFingerIndex2SchedulerName(nvsFingerIndex2SchedulerName), nvsFingerIndex2ActionIndex(nvsFingerIndex2ActionIndex) {}

        void HandleFingerprintDetected(uint8_t errorCode, uint16_t fingerIndex, uint16_t score)
        {
            
            if (handler)
                handler->HandleFingerprintDetected(errorCode, fingerIndex, score);
            if (errorCode != 0)
                return;
            

            if (handler)
            {
                char fingerIndexAsString[6];
                snprintf(fingerIndexAsString, 6, "%d", fingerIndex);
                uint16_t actionIndex{0};
                
                nvs_get_u16(this->nvsFingerIndex2ActionIndex, fingerIndexAsString, &actionIndex);
                size_t schedulerNameLen{16};
                nvs_get_str(this->nvsFingerIndex2SchedulerName, fingerIndexAsString, nullptr, &schedulerNameLen);
                char schedulerName[schedulerNameLen];
                nvs_get_str(this->nvsFingerIndex2SchedulerName, fingerIndexAsString, schedulerName, &schedulerNameLen);
                ESP_LOGI(TAG, "Fingerprint detected successfully: fingerIndex=%d, schedulerName=%s actionIndex=%d", fingerIndex, schedulerName, actionIndex);
                if (scheduler->GetCurrentValueOfSchedule(schedulerName)>0){
                    handler->HandleFingerprintAction(fingerIndex, actionIndex);
                }
            }
        }
        void HandleEnrollmentUpdate(uint8_t errorCode, uint8_t step, uint16_t fingerIndex, const char *fingerName) override
        {
            ESP_LOGI(TAG, "'%s'", enrollStep2description[step]);
            if (step == 0x0F)
            {
                esp_err_t err{ESP_OK};
                char fingerIndexAsString[6];
                snprintf(fingerIndexAsString, 6, "%d", fingerIndex);

                ESP_ERROR_CHECK(nvs_set_u16(this->nvsFingerName2FingerIndex, fingerName, fingerIndex));
                ESP_ERROR_CHECK(nvs_commit(this->nvsFingerName2FingerIndex));
                ESP_ERROR_CHECK(nvs_set_u16(this->nvsFingerIndex2ActionIndex, fingerIndexAsString, 0));
                ESP_ERROR_CHECK(nvs_commit(this->nvsFingerIndex2ActionIndex));
                ESP_ERROR_CHECK(nvs_set_str(this->nvsFingerIndex2SchedulerName, fingerIndexAsString, "ALWAYS"));
                ESP_ERROR_CHECK(nvs_commit(this->nvsFingerIndex2SchedulerName));

                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "Finger with index %d could not be stored in nvs with key %s. Error code %d", fingerIndex, fingerName, err);
                }
                else
                {
                    ESP_LOGI(TAG, "'%s', Finger is stored in index %d", enrollStep2description[step], fingerIndex);
                }
            }
            if (handler)
                handler->HandleEnrollmentUpdate(errorCode, step, fingerIndex, fingerName);
        }

        RET Begin(gpio_num_t tx_host, gpio_num_t rx_host)
        {
            mutex = xSemaphoreCreateMutex();
            auto ret = R503Pro::begin(tx_host, rx_host);
            if(ret!=RET::OK){
                return ret;
            }
            //auto params=this->GetAllParams();
            //TODO: Abgleich interner Speicher des R503 mit dem, was im Flash steht
            //TODO: Fingerprint-Infos sollten in einem einzigen Objekt gespeichert werden.
            return RET::OK;

        }

        RET TryEnrollAndStore(const char *fingerName)
        {
            ESP_LOGI(TAG, "TryEnrollAndStore name=%s", fingerName);
            uint16_t fingerIndex{0};
            if (!fingerName)
            {
                return RET::xNAME_IS_NULL;
            }
            if (std::strlen(fingerName) > MAX_FINGERNAME_LEN)
            {
                ESP_LOGE(TAG, "name is too long");
                return RET::xNVS_NAME_TOO_LONG;
            }

            if (nvs_get_u16(this->nvsFingerName2FingerIndex, fingerName, &fingerIndex) != ESP_ERR_NVS_NOT_FOUND)
            {
                ESP_LOGE(TAG, "Finger with name '%s' already exists", fingerName);
                return RET::xNVS_NAME_ALREADY_EXISTS;
            }

            std::strncpy(this->fingerName, fingerName, MAX_FINGERNAME_LEN);

            if (!xSemaphoreTake(mutex, pdMS_TO_TICKS(3000)))
            {
                ESP_LOGE(TAG, "Cannot take mutex in enrollment after 3000ms");
                return RET::xCANNOT_GET_MUTEX;
            }
            fingerIndex = 0x05DC;
            RET ret = AutoEnroll(fingerIndex, false, true, true, true);
            if (ret != RET::OK)
            {
                ESP_LOGE(TAG, "Error %d while calling AutoEnroll.", (int)ret);
                // Important: do not return here, because mutex will not be given back
            }

            xSemaphoreGive(mutex);
            return ret;
        }

        RET TryRename(const char *oldName, const char *newName)
        {
            if (!nvsFingerName2FingerIndex)
                return RET::xNVS_NOT_AVAILABLE;
            uint16_t fingerIndex{0};
            if (nvs_get_u16(this->nvsFingerName2FingerIndex, newName, &fingerIndex) != ESP_ERR_NVS_NOT_FOUND)
            {
                return RET::xNVS_NAME_ALREADY_EXISTS;
            }
            if (nvs_get_u16(this->nvsFingerName2FingerIndex, oldName, &fingerIndex) != ESP_OK)
            {
                return RET::xNVS_NAME_UNKNOWN;
            }

            char fingerIndexAsString[6];
            snprintf(fingerIndexAsString, 6, "%d", fingerIndex);

            nvs_erase_key(this->nvsFingerName2FingerIndex, oldName);
            RETURN_ERRORCODE_ON_ERROR(nvs_set_u16(this->nvsFingerName2FingerIndex, newName, fingerIndex), RET::xNVS_NOT_AVAILABLE);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerName2FingerIndex), RET::xNVS_NOT_AVAILABLE);

            // other nvs namespaces need no renaming as they work with indices rather than names
            return RET::OK;
        }

        RET TryDelete(const char *name)
        {
            if (!nvsFingerName2FingerIndex)
                return RET::xNVS_NOT_AVAILABLE;
            uint16_t fingerIndex{0};
            RETURN_ERRORCODE_ON_ERROR(nvs_get_u16(this->nvsFingerName2FingerIndex, name, &fingerIndex), RET::xNVS_NAME_UNKNOWN);
            RET ret = DeleteChar(fingerIndex);
            if (ret != RET::OK)
            {
                ESP_LOGE(TAG, "Error %d while calling TryDelete.", (int)ret);
                return ret;
            }

            RETURN_ERRORCODE_ON_ERROR(nvs_erase_key(this->nvsFingerName2FingerIndex, name), RET::xNVS_NOT_AVAILABLE);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerName2FingerIndex), RET::xNVS_NOT_AVAILABLE);

            char fingerIndexAsString[6];
            snprintf(fingerIndexAsString, 6, "%d", fingerIndex);

            nvs_erase_key(this->nvsFingerIndex2ActionIndex, fingerIndexAsString);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerIndex2ActionIndex), RET::xNVS_NOT_AVAILABLE);

            nvs_erase_key(this->nvsFingerIndex2SchedulerName, fingerIndexAsString);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerIndex2SchedulerName), RET::xNVS_NOT_AVAILABLE);
            return RET::OK;
        }

        RET TryDeleteAll()
        {
            if (!nvsFingerName2FingerIndex)
                return RET::xNVS_NOT_AVAILABLE;
            RET ret = EmptyLibrary();
            if (ret != RET::OK)
            {
                ESP_LOGE(TAG, "Error %d (hradware) while calling TryDeleteAll.", (int)ret);
                return ret;
            }
            RETURN_ERRORCODE_ON_ERROR(nvs_erase_all(this->nvsFingerName2FingerIndex), RET::xNVS_NOT_AVAILABLE);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerName2FingerIndex), RET::xNVS_NOT_AVAILABLE);

            RETURN_ERRORCODE_ON_ERROR(nvs_erase_all(this->nvsFingerIndex2ActionIndex), RET::xNVS_NOT_AVAILABLE);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerIndex2ActionIndex), RET::xNVS_NOT_AVAILABLE);

            RETURN_ERRORCODE_ON_ERROR(nvs_erase_all(this->nvsFingerIndex2SchedulerName), RET::xNVS_NOT_AVAILABLE);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerIndex2SchedulerName), RET::xNVS_NOT_AVAILABLE);
            ESP_LOGI(TAG, "Successfully deleted all Fingerprints on the sensor hardware and in flash");
            return RET::OK;
        }

        RET TryStoreFingerAction(uint16_t fingerIndex, uint16_t actionIndex)
        {
            char fingerIndexAsString[6];
            snprintf(fingerIndexAsString, 6, "%d", fingerIndex);
            RETURN_ERRORCODE_ON_ERROR(nvs_set_u16(this->nvsFingerIndex2ActionIndex, fingerIndexAsString, actionIndex), RET::xNVS_NOT_AVAILABLE);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerIndex2ActionIndex), RET::xNVS_NOT_AVAILABLE);
            ESP_LOGI(TAG, "Successfully stored finger action. index=%s action=%d", fingerIndexAsString, actionIndex);
            return RET::OK;
        }

        RET TryStoreFingerScheduler(uint16_t fingerIndex, const char* schedulerName)
        {
            char fingerIndexAsString[6];
            snprintf(fingerIndexAsString, 6, "%d", fingerIndex);
            RETURN_ERRORCODE_ON_ERROR(nvs_set_str(this->nvsFingerIndex2SchedulerName, fingerIndexAsString, schedulerName), RET::xNVS_NOT_AVAILABLE);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerIndex2SchedulerName), RET::xNVS_NOT_AVAILABLE);
            //char schedulerNameRead[NVS_KEY_NAME_MAX_SIZE];
            //size_t s;
            //nvs_get_str(this->nvsFingerIndex2SchedulerName, fingerIndexAsString, schedulerNameRead, &s);
            ESP_LOGI(TAG, "Successfully stored finger scheduler. fingerIndex=%d fingerIndexAsString=%s schedulerName=%s", fingerIndex, fingerIndexAsString, schedulerName);
            return RET::OK;
        }
    };

}
#undef TAG