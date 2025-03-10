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
#include <common-esp32.hh>
#include "webmanager_interfaces.hh"
#include "fingerprint_interfaces.hh"

#define TAG "FINGER"
#include "esp_log.h"
namespace fingerprint
{

    class R503ProManager : public r503pro::R503Pro, public fingerprint::iFingerprintHandler
    {
    private:
    fingerprint::iFingerprintActionHandler *handler;
        webmanager::iScheduler *scheduler;
        nvs_handle_t nvsFingerName2FingerIndex;
        nvs_handle_t nvsFingerIndex2SchedulerName;
        nvs_handle_t nvsFingerIndex2ActionIndex;
        SemaphoreHandle_t mutex;

    public:
        R503ProManager(uart_port_t uart_num, gpio_num_t gpio_irq, fingerprint::iFingerprintActionHandler *handler, webmanager::iScheduler *scheduler, nvs_handle_t nvsFingerName2FingerIndex, nvs_handle_t nvsFingerIndex2SchedulerName, nvs_handle_t nvsFingerIndex2ActionIndex, uint32_t targetAddress = grow_fingerprint::DEFAULT_ADDRESS) : R503Pro(uart_num, gpio_irq, this), handler(handler), scheduler(scheduler), nvsFingerName2FingerIndex(nvsFingerName2FingerIndex), nvsFingerIndex2SchedulerName(nvsFingerIndex2SchedulerName), nvsFingerIndex2ActionIndex(nvsFingerIndex2ActionIndex) {}

        void HandleFingerprintDetected(uint16_t errorCode, uint16_t fingerIndex, uint16_t score)
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
        
        void HandleEnrollmentUpdate(uint16_t errorCode, uint8_t step, uint16_t fingerIndex, const char *fingerName) override
        {
            ESP_LOGI(TAG, "'%s'", grow_fingerprint::enrollStep2description[step]);
            esp_err_t ret;
            if (step != 0x0F){
                if(handler) handler->HandleEnrollmentUpdate(errorCode, step, fingerIndex, fingerName);
                return;
            }
            
            char fingerIndexAsString[6];
            snprintf(fingerIndexAsString, 6, "%d", fingerIndex);

            GOTO_ERROR_ON_ERROR(nvs_set_u16(this->nvsFingerName2FingerIndex, fingerName, fingerIndex), "nvs");
            GOTO_ERROR_ON_ERROR(nvs_set_u16(this->nvsFingerIndex2ActionIndex, fingerIndexAsString, 0), "nvs");
            GOTO_ERROR_ON_ERROR(nvs_set_str(this->nvsFingerIndex2SchedulerName, fingerIndexAsString, "ALWAYS"), "nvs");
            
            GOTO_ERROR_ON_ERROR(nvs_commit(this->nvsFingerName2FingerIndex), "nvs");
            GOTO_ERROR_ON_ERROR(nvs_commit(this->nvsFingerIndex2ActionIndex), "nvs");
            GOTO_ERROR_ON_ERROR(nvs_commit(this->nvsFingerIndex2SchedulerName), "nvs");
            ESP_LOGI(TAG, "'%s', Finger is stored in index %d", grow_fingerprint::enrollStep2description[step], fingerIndex);
            if(handler) handler->HandleEnrollmentUpdate(errorCode, step, fingerIndex, fingerName);   
            return;   
        error:
            ESP_LOGE(TAG, "Finger with index %d could not be stored in nvs with key %s. Error code %d", fingerIndex, fingerName, ret);
            if(handler) handler->HandleEnrollmentUpdate(errorCode, step, fingerIndex, fingerName);
            return;
        }

        grow_fingerprint::RET Begin(gpio_num_t tx_host, gpio_num_t rx_host)
        {
            mutex = xSemaphoreCreateMutex();
            auto ret = R503Pro::Begin(tx_host, rx_host);
            if(ret!=grow_fingerprint::RET::OK){
                return ret;
            }
            //Die ganzen Parameter wurden intern im R503Pro::Begin bereits eingelesen und stehen jetzt zur verfügung
            auto params=this->GetAllParams();
            uint16_t fingerIndex = 0;
            for (uint8_t bi = 0; bi < 32; bi++) {
                auto the_byte = params->libraryIndicesUsed[bi];
                for (uint8_t biti = 0; biti < 8; biti++) {
                    if (the_byte & (1 << biti)) {
                        //"fingerIndex" is used. Check, whether there exists an extry in nvs
                        char fingerIndexAsString[6];
                        snprintf(fingerIndexAsString, 6, "%d", fingerIndex);
                        uint16_t dummy;
                        if(nvs_get_u16(this->nvsFingerIndex2ActionIndex, fingerIndexAsString, &dummy)==ESP_ERR_NVS_NOT_FOUND){
                            //Index existiert im Reader, aber nicht im nvs -->Eintrag anlegen mit Dummy-Namen
                            char fingerName[14];
                            snprintf(fingerName, 14, "Finger %d", fingerIndex);
                            ESP_ERROR_CHECK(nvs_set_u16(this->nvsFingerName2FingerIndex, fingerName, fingerIndex));
                            ESP_ERROR_CHECK(nvs_set_u16(this->nvsFingerIndex2ActionIndex, fingerIndexAsString, 0));
                            ESP_ERROR_CHECK(nvs_set_str(this->nvsFingerIndex2SchedulerName, fingerIndexAsString, "ALWAYS"));
                            
                            ESP_ERROR_CHECK(nvs_commit(this->nvsFingerName2FingerIndex));
                            ESP_ERROR_CHECK(nvs_commit(this->nvsFingerIndex2ActionIndex));
                            ESP_ERROR_CHECK(nvs_commit(this->nvsFingerIndex2SchedulerName));
                            ESP_LOGI(TAG, "Finger index %d got name '%s'", fingerIndex, fingerName);
           
                        }
                    }
                    fingerIndex++;
                }
            }
            return grow_fingerprint::RET::OK;

        }

        grow_fingerprint::RET TryEnrollAndStore(const char *fingerName)
        {
            ESP_LOGI(TAG, "TryEnrollAndStore name=%s", fingerName);
            uint16_t fingerIndex{0};
            if (!fingerName)
            {
                return grow_fingerprint::RET::xNAME_IS_NULL;
            }
            if (std::strlen(fingerName) > grow_fingerprint::MAX_FINGERNAME_LEN)
            {
                ESP_LOGE(TAG, "name is too long");
                return grow_fingerprint::RET::xNVS_NAME_TOO_LONG;
            }

            if (nvs_get_u16(this->nvsFingerName2FingerIndex, fingerName, &fingerIndex) != ESP_ERR_NVS_NOT_FOUND)
            {
                ESP_LOGE(TAG, "Finger with name '%s' already exists", fingerName);
                return grow_fingerprint::RET::xNVS_NAME_ALREADY_EXISTS;
            }

            std::strncpy(this->fingerName, fingerName, grow_fingerprint::MAX_FINGERNAME_LEN);

            if (!xSemaphoreTake(mutex, pdMS_TO_TICKS(3000)))
            {
                ESP_LOGE(TAG, "Cannot take mutex in enrollment after 3000ms");
                return grow_fingerprint::RET::xCANNOT_GET_MUTEX;
            }
            fingerIndex = 0x05DC;
            grow_fingerprint::RET ret = AutoEnroll(fingerIndex, false, true, true, true);
            if (ret != grow_fingerprint::RET::OK)
            {
                ESP_LOGE(TAG, "Error %d while calling AutoEnroll.", (int)ret);
                // Important: do not return here, because mutex will not be given back
            }

            xSemaphoreGive(mutex);
            return ret;
        }

        grow_fingerprint::RET TryRename(const char *oldName, const char *newName)
        {
            if (!nvsFingerName2FingerIndex)
                return grow_fingerprint::RET::xNVS_NOT_AVAILABLE;
            uint16_t fingerIndex{0};
            if (nvs_get_u16(this->nvsFingerName2FingerIndex, newName, &fingerIndex) != ESP_ERR_NVS_NOT_FOUND)
            {
                return grow_fingerprint::RET::xNVS_NAME_ALREADY_EXISTS;
            }
            if (nvs_get_u16(this->nvsFingerName2FingerIndex, oldName, &fingerIndex) != ESP_OK)
            {
                return grow_fingerprint::RET::xNVS_NAME_UNKNOWN;
            }

            char fingerIndexAsString[6];
            snprintf(fingerIndexAsString, 6, "%d", fingerIndex);

            nvs_erase_key(this->nvsFingerName2FingerIndex, oldName);
            RETURN_ERRORCODE_ON_ERROR(nvs_set_u16(this->nvsFingerName2FingerIndex, newName, fingerIndex), grow_fingerprint::RET::xNVS_NOT_AVAILABLE);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerName2FingerIndex), grow_fingerprint::RET::xNVS_NOT_AVAILABLE);

            // other nvs namespaces need no renaming as they work with indices rather than names
            return grow_fingerprint::RET::OK;
        }

        grow_fingerprint::RET TryDelete(const char *name)
        {
            if (!nvsFingerName2FingerIndex)
                return grow_fingerprint::RET::xNVS_NOT_AVAILABLE;
            uint16_t fingerIndex{0};
            RETURN_ERRORCODE_ON_ERROR(nvs_get_u16(this->nvsFingerName2FingerIndex, name, &fingerIndex), grow_fingerprint::RET::xNVS_NAME_UNKNOWN);
            grow_fingerprint::RET ret = DeleteChar(fingerIndex, 1);
            if (ret != grow_fingerprint::RET::OK)
            {
                ESP_LOGE(TAG, "Error %d while calling TryDelete.", (int)ret);
                return ret;
            }

            RETURN_ERRORCODE_ON_ERROR(nvs_erase_key(this->nvsFingerName2FingerIndex, name), grow_fingerprint::RET::xNVS_NOT_AVAILABLE);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerName2FingerIndex), grow_fingerprint::RET::xNVS_NOT_AVAILABLE);

            char fingerIndexAsString[6];
            snprintf(fingerIndexAsString, 6, "%d", fingerIndex);

            nvs_erase_key(this->nvsFingerIndex2ActionIndex, fingerIndexAsString);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerIndex2ActionIndex), grow_fingerprint::RET::xNVS_NOT_AVAILABLE);

            nvs_erase_key(this->nvsFingerIndex2SchedulerName, fingerIndexAsString);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerIndex2SchedulerName), grow_fingerprint::RET::xNVS_NOT_AVAILABLE);
            return grow_fingerprint::RET::OK;
        }

        grow_fingerprint::RET TryDeleteAll()
        {
            if (!nvsFingerName2FingerIndex)
                return grow_fingerprint::RET::xNVS_NOT_AVAILABLE;
            grow_fingerprint::RET ret = EmptyLibrary();
            if (ret != grow_fingerprint::RET::OK)
            {
                ESP_LOGE(TAG, "Error %d (hradware) while calling TryDeleteAll.", (int)ret);
                return ret;
            }
            RETURN_ERRORCODE_ON_ERROR(nvs_erase_all(this->nvsFingerName2FingerIndex), grow_fingerprint::RET::xNVS_NOT_AVAILABLE);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerName2FingerIndex), grow_fingerprint::RET::xNVS_NOT_AVAILABLE);

            RETURN_ERRORCODE_ON_ERROR(nvs_erase_all(this->nvsFingerIndex2ActionIndex), grow_fingerprint::RET::xNVS_NOT_AVAILABLE);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerIndex2ActionIndex), grow_fingerprint::RET::xNVS_NOT_AVAILABLE);

            RETURN_ERRORCODE_ON_ERROR(nvs_erase_all(this->nvsFingerIndex2SchedulerName), grow_fingerprint::RET::xNVS_NOT_AVAILABLE);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerIndex2SchedulerName), grow_fingerprint::RET::xNVS_NOT_AVAILABLE);
            ESP_LOGI(TAG, "Successfully deleted all Fingerprints on the sensor hardware and in flash");
            return grow_fingerprint::RET::OK;
        }

        grow_fingerprint::RET TryStoreFingerAction(uint16_t fingerIndex, uint16_t actionIndex)
        {
            char fingerIndexAsString[6];
            snprintf(fingerIndexAsString, 6, "%d", fingerIndex);
            RETURN_ERRORCODE_ON_ERROR(nvs_set_u16(this->nvsFingerIndex2ActionIndex, fingerIndexAsString, actionIndex), grow_fingerprint::RET::xNVS_NOT_AVAILABLE);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerIndex2ActionIndex), grow_fingerprint::RET::xNVS_NOT_AVAILABLE);
            ESP_LOGI(TAG, "Successfully stored finger action. index=%s action=%d", fingerIndexAsString, actionIndex);
            return grow_fingerprint::RET::OK;
        }

        grow_fingerprint::RET TryStoreFingerScheduler(uint16_t fingerIndex, const char* schedulerName)
        {
            char fingerIndexAsString[6];
            snprintf(fingerIndexAsString, 6, "%d", fingerIndex);
            RETURN_ERRORCODE_ON_ERROR(nvs_set_str(this->nvsFingerIndex2SchedulerName, fingerIndexAsString, schedulerName), grow_fingerprint::RET::xNVS_NOT_AVAILABLE);
            RETURN_ERRORCODE_ON_ERROR(nvs_commit(this->nvsFingerIndex2SchedulerName), grow_fingerprint::RET::xNVS_NOT_AVAILABLE);
            //char schedulerNameRead[NVS_KEY_NAME_MAX_SIZE];
            //size_t s;
            //nvs_get_str(this->nvsFingerIndex2SchedulerName, fingerIndexAsString, schedulerNameRead, &s);
            ESP_LOGI(TAG, "Successfully stored finger scheduler. fingerIndex=%d fingerIndexAsString=%s schedulerName=%s", fingerIndex, fingerIndexAsString, schedulerName);
            return grow_fingerprint::RET::OK;
        }
    };

}
#undef TAG