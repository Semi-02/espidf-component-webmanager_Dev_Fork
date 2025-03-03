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
#define TAG "FINGER_HW"
#include "grow_fingerprint_serial_protocol.hh"
#include <common.hh>
using grow_fingerprint;
namespace r303s
{




    class iFingerprintHandler{
        public:
        virtual void HandleFingerprintDetected(uint16_t errorCode, uint16_t finger, uint16_t score)=0;
        virtual void HandleEnrollmentUpdate(uint16_t errorCode, uint8_t step, uint16_t finger, const char* name)=0;
    };

    class iFingerprintActionHandler:public iFingerprintHandler{
        public:
        //call this, when the action should be executed (no error, timetable ok)
        virtual void HandleFingerprintAction(uint16_t fingerIndex, int action)=0;
    };


    class R303S: public grow_fingerprint::PackageCreatorAndParser
    {
    private:
        
        SystemParameter params;
        iFingerprintHandler* handler;
        uint32_t targetAddress{0xFFFFFFFF};

        RET ReadAllSysPara(SystemParameter& outParams){
            return ReadAllSysPara(outParams, 0x0009);
        }

public:       
        
        R303S(uart_port_t uart_num, gpio_num_t gpio_irq, iFingerprintHandler* handler, uint32_t targetAddress=DEFAULT_ADDRESS) : grow_fingerprint::PackageCreatorAndParser(uart_num), gpio_irq(gpio_irq), handler(handler), targetAddress(targetAddress) {
            fingerName[0]=0;
            fingerName[MAX_FINGERNAME_LEN+1]=0;
        }
        
        RET Begin(gpio_num_t tx_host, gpio_num_t rx_host)
        {

            /* Install UART driver */
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


            RET ret =  ReadAllSysPara(this->params);
            if(ret!=RET::OK){
                ESP_LOGE(TAG, "Communication error with fingerprint reader. Error code %d", (int)ret);
                return RET::HARDWARE_ERROR;
            }
            uint32_t password=DEFAULT_PASSWORD;
            bool passwordOk{false};
            VerifyPassword(password, passwordOk);
            if(!passwordOk){
                ESP_LOGE(TAG, "Default Password %lu is not accepted. Trying to crack it...", password);
                for(password=0; password<UINT32_MAX;password++){
                    VerifyPassword(password, passwordOk);
                    if(passwordOk){
                        ESP_LOGE(TAG, "Password %lu IS ACCEPTED. ", password);
                        break;
                    }else if(password%100==0){
                        ESP_LOGI(TAG, "Probing %lu", password);
                    }
                }
            }else{
                ESP_LOGI(TAG, "Default Password %lu is accepted", password);
            }

            ret=SetSysPara(fingerprint::PARAM_INDEX::BAUD_RATE_CONTROL, (uint8_t)PARAM_BAUD::_115200);
            uart_set_baudrate(uart_num, 115200);

            ESP_LOGI(TAG, "Successfully connected with fingerprint addr=%lu; securityLevel=%u; libSize=%u; libUsed=%u fwVer=%s; algVer=%s; status=%u", params.deviceAddress,params.securityLevel, params.librarySizeMax, params.librarySizeUsed, params.fwVer, params.algVer, params.status);
            
            xTaskCreate([](void *p){((R503Pro*)p)->task(); }, "fingerprint", 3072, this, 10, nullptr);
            return RET::OK;
        }

       
    };

}
#undef TAG