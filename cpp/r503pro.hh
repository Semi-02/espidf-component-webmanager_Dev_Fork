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
#include <common.hh>

namespace r503pro
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


    class R503Pro
    {
    private:
        gpio_num_t gpio_irq;
        SystemParameter params;
        bool previousIrqLineValue{true};

        bool isInEnrollment{false};
        iFingerprintHandler* handler;

        void task()
        {
            vTaskDelay(POWER_UP_DELAY_TICKS);
            ESP_LOGI(TAG, "Fingerprint Task started");
            while(true){
                if(isInEnrollment){
                    task_enroll();
                }
                else{
                    vTaskDelay(pdMS_TO_TICKS(100));
                    task_detect();
                }
            }
        }

        void task_enroll(){
            const size_t wireLength{0x6+9};
            uint8_t buffer[wireLength];
            RET ret=receiveAndCheckPackage(buffer, wireLength, pdMS_TO_TICKS(20000));
            if(ret!=RET::OK){
                ESP_LOGE(TAG, "Parser error in task_enroll %d", (int)ret);
                if(handler)handler->HandleEnrollmentUpdate((uint8_t)ret, 0, 0, fingerName);
                this->isInEnrollment=false;
                return;
            }
            ret = (RET)buffer[9];
            uint8_t step = buffer[10];
            uint16_t fingerIndex =ParseU16_BigEndian(buffer, 11);
            if(step==0x0F){
                isInEnrollment=false;
            }
            if(handler)handler->HandleEnrollmentUpdate((uint8_t)ret, step, fingerIndex, fingerName);
            return;
        }

        void task_detect(){    
            bool newIrqValue=gpio_get_level(gpio_irq);
            if(previousIrqLineValue==true && newIrqValue==false){
                //negative edge detected
                ESP_LOGD(TAG, "Negative edge detected, trying to read fingerprint");
                uint16_t fingerIndex;
                uint16_t score;
                RET ret= AutoIdentify(fingerIndex, score);
                
                if(ret==RET::OK){
                    ESP_LOGD(TAG, "Fingerprint detected successfully: fingerIndex=%d, score=%d", fingerIndex, score);
                    if(this->handler) handler->HandleFingerprintDetected(0, fingerIndex, score);
                }else{
                    ESP_LOGW(TAG, "AutoIdentify returns %d", (int)ret);
                    if(this->handler) handler->HandleFingerprintDetected((uint8_t)ret, 0, 0);
                }
            }
            previousIrqLineValue=newIrqValue;
        }

        RET ReadAllSysPara(SystemParameter& outParams){
            return ReadAllSysPara(outParams, 0x0000);
        }
       
        
protected:
        char fingerName[MAX_FINGERNAME_LEN+1];
public:       
     

       
        RET AutoEnroll(uint16_t& fingerIndexOr0xFFFF_inout, bool overwriteExisting, bool duplicateFingerAllowed, bool returnStatusDuringProcess, bool fingerHasToLeaveBetweenScans){
            uint8_t data[7];
            data[0]=(uint8_t)INSTRUCTION::AutoEnroll;
            WriteU16_BigEndian(fingerIndexOr0xFFFF_inout, data, 1);
            data[3]=overwriteExisting?1:0;
            data[4]=duplicateFingerAllowed?1:0;
            data[5]=returnStatusDuringProcess?1:0;
            data[6]=fingerHasToLeaveBetweenScans?1:0;
            createAndSendDataPackage(PacketIdentifier::COMMANDPACKET, data, sizeof(data), true);
            this->isInEnrollment=true;
            ESP_LOGI(TAG, "AutoEnroll started"); 
            return RET::OK;
        }

        RET AutoIdentify(uint16_t& fingerIndex_out, uint16_t& score_out,  PARAM_SECURITY securityLevel=PARAM_SECURITY::_3, bool returnStatusDuringProcess=true, uint8_t maxScanAttempts=1,  uint32_t targetAddress=DEFAULT_ADDRESS){
            uint8_t data[8];
            data[0]=(uint8_t)INSTRUCTION::AutoIdentify;
            data[1]=(uint8_t)securityLevel;
            WriteU16_BigEndian(0x0, data, 2);//search over all fingers
            WriteU16_BigEndian(0x05DC, data, 4);//search over all fingers
            data[6]=returnStatusDuringProcess?1:0;
            data[7]=maxScanAttempts;
            createAndSendDataPackage(PacketIdentifier::COMMANDPACKET, data, sizeof(data));
            const size_t wireLength{0x8+9};
            uint8_t buffer[wireLength];
            while(true){
                RET ret=receiveAndCheckPackage(buffer, wireLength);
                if(ret!=RET::OK){
                    ESP_LOGE(TAG, "Parser error in AutoIdentify: %d", (int)ret);
                    return ret;
                }
                if(buffer[9]!=0){
                    return (RET)buffer[9];
                }
                uint8_t step = buffer[10];
                fingerIndex_out =ParseU16_BigEndian(buffer, 11);
                score_out = ParseU16_BigEndian(buffer, 13);
                ESP_LOGD(TAG, "'%s', Finger is stored in index %d", identifyStep2description[step], fingerIndex_out);
                if(step==3) break;
            }
            return RET::OK;
        }
        
        R503Pro(uart_port_t uart_num, gpio_num_t gpio_irq, iFingerprintHandler* handler, uint32_t targetAddress=DEFAULT_ADDRESS) : public grow_fingerprint::PackageCreatorAndParser(uart_num), gpio_irq(gpio_irq), handler(handler), targetAddress(targetAddress) {
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

            gpio_pullup_en(gpio_irq);
            gpio_set_direction(gpio_irq, GPIO_MODE_INPUT);

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