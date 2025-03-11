#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <array>
#include <memory>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include <common.hh>
#include "fingerprint_interfaces.hh"
#include "grow_fingerprint_serial_protocol.hh"

#define TAG "r503pro"
namespace r503pro
{
    constexpr uint32_t DEFAULT_PASSWORD{0x00000000};
    constexpr uint16_t SYSTEM_IDENFIFIER_CODE{600};//According to "R503Pro fingerprint module user manual-V1.1" this should be "0", but is "600"
    class R503Pro:public grow_fingerprint::PackageCreatorAndParser
    {
    private:
        gpio_num_t gpio_irq;
        grow_fingerprint::SystemParameter params;
        bool previousIrqLineValue{true};

        bool isInEnrollment{false};
        fingerprint::iFingerprintHandler *handler;

        void task()
        {
            vTaskDelay(grow_fingerprint::POWER_UP_DELAY_TICKS);
            ESP_LOGI(TAG, "Fingerprint Task started");
            while (true)
            {
                if (isInEnrollment)
                {
                    task_enroll();
                }
                else
                {
                    vTaskDelay(pdMS_TO_TICKS(100));
                    task_detect();
                }
            }
        }

        void task_enroll()
        {
            const size_t wireLength{0x6 + 9};
            uint8_t buffer[wireLength];
            grow_fingerprint::RET ret = receiveAndCheckPackage(buffer, wireLength, pdMS_TO_TICKS(20000));
            if (ret != grow_fingerprint::RET::OK)
            {
                ESP_LOGE(TAG, "Parser error in task_enroll %d", (int)ret);
                if (handler)
                    handler->HandleEnrollmentUpdate((uint8_t)ret, 0, 0, fingerName);
                this->isInEnrollment = false;
                return;
            }
            ret = (grow_fingerprint::RET)buffer[9];
            uint8_t step = buffer[10];
            uint16_t fingerIndex = ParseU16_BigEndian(buffer, 11);
            if (step == 0x0F)
            {
                isInEnrollment = false;
            }
            if (handler)
                handler->HandleEnrollmentUpdate((uint8_t)ret, step, fingerIndex, fingerName);
            return;
        }

        void task_detect()
        {
            bool newIrqValue = gpio_get_level(gpio_irq);
            if (previousIrqLineValue == true && newIrqValue == false)
            {
                // negative edge detected
                ESP_LOGD(TAG, "Negative edge detected, trying to read fingerprint");
                uint16_t fingerIndex;
                uint16_t score;
                grow_fingerprint::RET ret = AutoIdentify(fingerIndex, score);

                if (ret == grow_fingerprint::RET::OK)
                {
                    ESP_LOGD(TAG, "Fingerprint detected successfully: fingerIndex=%d, score=%d", fingerIndex, score);
                    if (this->handler)
                        handler->HandleFingerprintDetected(0, fingerIndex, score);
                }
                else
                {
                    ESP_LOGW(TAG, "AutoIdentify returns %d", (int)ret);
                    if (this->handler)
                        handler->HandleFingerprintDetected((uint8_t)ret, 0, 0);
                }
            }
            previousIrqLineValue = newIrqValue;
        }

        grow_fingerprint::RET ReadAllSysPara(grow_fingerprint::SystemParameter &outParams)
        {
            grow_fingerprint::RET ret;
            ret= ReadSysPara(outParams, SYSTEM_IDENFIFIER_CODE);
            if(ret!=grow_fingerprint::RET::OK) return ret;
            ret= GetAlgVer(&outParams.algVer);
            if(ret!=grow_fingerprint::RET::OK) return ret;
            ret= GetFwVer(&outParams.fwVer);
            if(ret!=grow_fingerprint::RET::OK) return ret;
            ret= GetTemplateNumber(outParams.librarySizeUsed);
            if(ret!=grow_fingerprint::RET::OK)return ret;
            ret = GetTemplateIndexTable(0, outParams.libraryIndicesUsed);
            return ret;
        }

    protected:
        char fingerName[grow_fingerprint::MAX_FINGERNAME_LEN + 1];

    public:
        grow_fingerprint::RET AutoEnroll(uint16_t &fingerIndexOr0xFFFF_inout, bool overwriteExisting, bool duplicateFingerAllowed, bool returnStatusDuringProcess, bool fingerHasToLeaveBetweenScans)
        {
            grow_fingerprint::PackageCreatorAndParser::AutoEnroll(fingerIndexOr0xFFFF_inout, overwriteExisting, duplicateFingerAllowed, returnStatusDuringProcess, fingerHasToLeaveBetweenScans);
            this->isInEnrollment=true;
            return grow_fingerprint::RET::OK;
        }

        grow_fingerprint::SystemParameter* GetAllParams(){
            return &this->params;
        }

        grow_fingerprint::RET AutoIdentify(uint16_t &fingerIndex_out, uint16_t &score_out, grow_fingerprint::PARAM_SECURITY securityLevel = grow_fingerprint::PARAM_SECURITY::_3, bool returnStatusDuringProcess = true, uint8_t maxScanAttempts = 1)
        {
            uint8_t data[8];
            data[0] = (uint8_t)grow_fingerprint::INSTRUCTION::AutoIdentify;
            data[1] = (uint8_t)securityLevel;
            WriteU16_BigEndian(0x0, data, 2);    // search over all fingers
            WriteU16_BigEndian(0x05DC, data, 4); // search over all fingers
            data[6] = returnStatusDuringProcess ? 1 : 0;
            data[7] = maxScanAttempts;
            createAndSendDataPackage(grow_fingerprint::PacketIdentifier::COMMANDPACKET, data, sizeof(data));
            const size_t wireLength{0x8 + 9};
            uint8_t buffer[wireLength];
            while (true)
            {
                grow_fingerprint::RET ret = receiveAndCheckPackage(buffer, wireLength);
                if (ret != grow_fingerprint::RET::OK)
                {
                    ESP_LOGE(TAG, "Parser error in AutoIdentify: %d", (int)ret);
                    return ret;
                }
                if (buffer[9] != 0)
                {
                    return (grow_fingerprint::RET)buffer[9];
                }
                uint8_t step = buffer[10];
                fingerIndex_out = ParseU16_BigEndian(buffer, 11);
                score_out = ParseU16_BigEndian(buffer, 13);
                ESP_LOGD(TAG, "'%s', Finger is stored in index %d", grow_fingerprint::identifyStep2description[step], fingerIndex_out);
                if (step == 3)
                    break;
            }
            return grow_fingerprint::RET::OK;
        }

        grow_fingerprint::RET CancelInstruction(){
            return grow_fingerprint::PackageCreatorAndParser::CancelInstruction();
        }
        
        R503Pro(uart_port_t uart_num, gpio_num_t gpio_irq, fingerprint::iFingerprintHandler *handler, uint32_t targetAddress = grow_fingerprint::DEFAULT_ADDRESS) : grow_fingerprint::PackageCreatorAndParser(uart_num, targetAddress), gpio_irq(gpio_irq), handler(handler)
        {
            fingerName[0] = 0;
            fingerName[grow_fingerprint::MAX_FINGERNAME_LEN + 1] = 0;
        }

        grow_fingerprint::RET Begin(gpio_num_t tx_host, gpio_num_t rx_host)
        {

            ESP_LOGI(TAG, "Install UART driver for Fingerprint TX_HOST=%d, RX_HOST=%d", tx_host, rx_host);
            uart_config_t c = {};
            c.baud_rate = 57600;
            c.data_bits = UART_DATA_8_BITS;
            c.parity = UART_PARITY_DISABLE;
            c.stop_bits = UART_STOP_BITS_1;
            c.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
            c.source_clk = UART_SCLK_DEFAULT;

            ESP_ERROR_CHECK(uart_driver_install(uart_num, 256, 0, 1, nullptr, 0));
            ESP_ERROR_CHECK(uart_param_config(uart_num, &c));
            ESP_ERROR_CHECK(uart_set_pin(uart_num, (int)tx_host, (int)rx_host, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
            ESP_ERROR_CHECK(uart_flush(uart_num));

            gpio_pullup_en(gpio_irq);
            gpio_set_direction(gpio_irq, GPIO_MODE_INPUT);
            
            uint32_t password = DEFAULT_PASSWORD;
            grow_fingerprint::RET ret=VerifyPassword(password);
            if (ret==grow_fingerprint::RET::WRONG_PASSWORD)
            {
                ESP_LOGE(TAG, "Default Password %lu is not accepted. Trying to crack it...", password);
                for (password = 0; password < UINT32_MAX; password++)
                {
                    ret=VerifyPassword(password);
                    if (ret==grow_fingerprint::RET::OK)
                    {
                        ESP_LOGE(TAG, "Password %lu IS ACCEPTED. ", password);
                        break;
                    }
                    else if (password % 100 == 0)
                    {
                        ESP_LOGI(TAG, "Probing %lu", password);
                    }
                }
            }
            else if(ret==grow_fingerprint::RET::OK)
            {
                ESP_LOGI(TAG, "Default Password %lu is accepted", password);
            }
            else{
                ESP_LOGE(TAG, "Communication error with fingerprint reader. Error code %d", (int)ret);
                return grow_fingerprint::RET::HARDWARE_ERROR;
            }

            ret = ReadAllSysPara(this->params);
            if (ret != grow_fingerprint::RET::OK)
            {
                ESP_LOGE(TAG, "Communication error with fingerprint reader. Error code %d", (int)ret);
                return grow_fingerprint::RET::HARDWARE_ERROR;
            }
            

            //ret = SetSysPara(fingerprint::PARAM_INDEX::BAUD_RATE_CONTROL, (uint8_t)PARAM_BAUD::_115200);
            //uart_set_baudrate(uart_num, 115200);

            ESP_LOGI(TAG, "Successfully connected with fingerprint {'addr':%lu, 'securityLevel':%u, 'libSize':%u, 'libUsed':%u, 'fwVer':'%s', 'algVer'='%s', 'status':%u, 'baud9600':%u}", params.deviceAddress, params.securityLevel, params.librarySizeMax, params.librarySizeUsed, params.fwVer, params.algVer, params.status, params.baudRateTimes9600);

            xTaskCreate([](void *p)
                        { ((R503Pro *)p)->task(); }, "fingerprint", 3072, this, 10, nullptr);
            return grow_fingerprint::RET::OK;
        }
    };

}
#undef TAG