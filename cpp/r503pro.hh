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

namespace fingerprint
{
    constexpr TickType_t POWER_UP_DELAY_TICKS{pdMS_TO_TICKS(50)};
    constexpr size_t NOTEPAD_SIZE_BYTES{16 * 32};
    constexpr size_t FEATURE_BUFFER_MAX{6};
    constexpr uint32_t DEFAULT_ADDRESS{UINT32_MAX};
    constexpr uint32_t DEFAULT_BAUD_RATE{57600};
    constexpr TickType_t DEFAULT_TIMEOUT_TICKS{pdMS_TO_TICKS(1000)}; //!< UART reading timeout in milliseconds
    constexpr size_t MAX_FINGERNAME_LEN=NVS_KEY_NAME_MAX_SIZE-1;
    constexpr const char* enrollStep2description[]{
        "",
        "Collect image for the first time",
        "Generate Feature for the first time",
        "Collect image for the second time",
        "Generate Feature for the second time",
        "Collect image for the third time",
        "Generate Feature for the third time",
        "Collect image for the fourth time",
        "Generate Feature for the fourth time",
        "Collect image for the fifth time",
        "Generate Feature for the fifth time",
        "Collect image for the sixth time",
        "Generate Feature for the sixth time",
        "Repeat fingerprint check",
        "Merge feature",
        "Storage template",
    };

    constexpr const char* identifyStep2description[]{
        "",
        "Collect Image",
        "Generate Feature",
        "Search",
    };

    enum class INSTRUCTION : uint8_t
    {
        GenImg = 0x01,         // Collect finger image
        Img2Tz = 0x02,         // To generate character file from image
        Match = 0x03,          // Carry out precise matching of two templates;
        Search = 0x04,         // Search the finger library
        RegModel = 0x05,       // To combine character files and generate template
        Store = 0x06,          // To store template
        LoadChar = 0x07,       // to read/load template
        UpChar = 0x08,         // to upload template
        DownChr = 0x09,        // to download template
        UpImage = 0x0A,        // To upload image
        DownImage = 0x0B,      // To download image
        DeleteChar = 0x0C,     // to delete templates
        Empty = 0x0d,          // to empty the library
        SetSysPara = 0x0e,     // To set system Parameter
        ReadSysPara = 0x0f,    // To read system Parameter
        SetPwd = 0x12,         // To set password
        VfyPwd = 0x13,         // To verify password
        GetRandomCode = 0x14,  // to get random code
        SetAddr = 0x15,        // To set device address
        ReadInfPage = 0x16,    // Read information page
        WriteNotepad = 0x18,   // Write Notepad on sensor
        ReadNotepad = 0x19,    // Read Notepad from sensor
        TemplateNum = 0x1D,    // To read finger template numbers
        ReadIndexTable = 0x1F, // Read-fingerprint template index table
        Cancel = 0x30,         // Cancel instruction
        HandShake = 0x40,      // HandShake
        GetAlgVer = 0x39,      // Get the algorithm library version
        GetFwVer = 0x3A,       // Get the firmware version
        AuraControl = 0x35,    // AuraLedConfig
        AutoEnroll = 0x31,     // Automatic registration template
        AutoIdentify = 0x32,   // Automatic fingerprint verification
    };


    enum class RET
    {
        OK = 0x00,                           //!< Command execution is complete
        PACKET_RECIEVE_ERR = 0x01,           //!< Error when receiving data package
        NO_FINGER_ON_SENSOR = 0x02,          //!< No finger on the sensor
        ENROLL_FAIL = 0x03,                  //!< Failed to enroll the finger
        GENERATE_CHARACTER_FILE_FAIL = 0x06, //!< Failed to generate character file due to overly disorderly fingerprint image
        FEATURE_FAIL = 0x07,                 //!< Failed to generate character file due to the lack of character point or small fingerprint image
        NO_MATCH = 0x08,                     //!< Finger doesn't match
        FINGER_NOT_FOUND = 0x09,             //!< Failed to find matching finger
        FAILTO_COMBINE_FINGER_FILES = 0x0A,  //!< Failed to combine the character files
        BAD_LOCATION = 0x0B,                 //!< Addressed PageID is beyond the finger library
        DB_RANGE_FAIL = 0x0C,                //!< Error when reading template from library or invalid template
        UPLOAD_TEMPLATE_FAIL = 0x0D,         //!< Error when uploading template
        PACKETRESPONSEFAIL = 0x0E,           //!< Module failed to receive the following data packages
        UPLOADFAIL = 0x0F,                   //!< Error when uploading image
        DELETEFAIL = 0x10,                   //!< Failed to delete the template
        DBCLEARFAIL = 0x11,                  //!< Failed to clear finger library
        WRONG_PASSWORD = 0x13,               //!< wrong password!
        INVALIDIMAGE = 0x15,                 //!< Failed to generate image because of lac of valid primary image
        FLASHERR = 0x18,                     //!< Error when writing flash
        NO_DEFINITION_ERROR = 0x19,
        INVALIDREG = 0x1A, //!< Invalid register number
        INCORRECT_CONFIGURATION = 0x1b,
        WRONG_NOTEPAD_PAGE_NUMBER = 0x1c,
        COMMUNICATION_PORT_FAILURE = 0x1d,
        FINGERPRINT_LIBRARY_IS_FULL = 0x1f,

        ADDRESS_CODE_INCORRECT = 0x20,
        PASSWORT_MUST_BE_VERIFIED = 0x21,     // password must be verified;
        FINGERPRINT_TEMPLATE_IS_EMPTY = 0x22, // fingerprint template is empty;
        FINGERPRINT_LIB_IS_EMPTY = 0x24,
        TIMEOUT = 0x26,
        FINGERPRINT_ALREADY_EXISTS = 0x27,
        SENSOR_HARDWARE_ERROR = 0x29,

        UNSUPPORTED_COMMAND = 0xfc,
        HARDWARE_ERROR = 0xfd,
        COMMAND_EXECUTION_FAILURE = 0xfe,

        xPARSER_CANNOT_FIND_STARTCODE = 0x100,
        xPARSER_WRONG_MODULE_ADDRESS = 0x101,
        xPARSER_ACKNOWLEDGE_PACKET_EXPECTED=0x102,
        xPARSER_UNEXPECTED_LENGTH=0x103,
        xPARSER_CHECKSUM_ERROR=0x104,
        xPARSER_TIMEOUT=0x105,
        xNVS_READWRITE_ERROR=0x106,
        xNVS_NAME_ALREADY_EXISTS=0x107,
        xNVS_NAME_UNKNOWN=0x108,
        xNVS_NAME_TOO_LONG=0x109,
        xCANNOT_GET_MUTEX=0x10A,
        xNVS_NOT_AVAILABLE=0x10B,
        xNAME_IS_NULL=0x10C,
        //if changes are made here, update the enum fingerprint_controller.ts on web client
    };
    


    enum class PARAM_INDEX : uint8_t
    {
        BAUD_RATE_CONTROL = 4, // N= [1/2/4/6/12]. Corresponding baud rate is 9600*N bps。, Default 6
        SECURITY_LEVEL = 5,
        DATA_PACKAGE_LENGTH = 6, // Its value is 0, 1, 2, 3, corresponding to 32 bytes, 64 bytes, 128 bytes, 256 bytes respectively.
    };

    enum class PARAM_BAUD : uint8_t
    {
        _9600 = 0x1,   //!< UART baud 9600
        _19200 = 0x2,  //!< UART baud 19200
        _28800 = 0x3,  //!< UART baud 28800
        _38400 = 0x4,  //!< UART baud 38400
        _48000 = 0x5,  //!< UART baud 48000
        _57600 = 0x6,  //!< UART baud 57600
        _67200 = 0x7,  //!< UART baud 67200
        _76800 = 0x8,  //!< UART baud 76800
        _86400 = 0x9,  //!< UART baud 86400
        _96000 = 0xA,  //!< UART baud 96000
        _105600 = 0xB, //!< UART baud 105600
        _115200 = 0xC, //!< UART baud 115200
    };

    enum class PARAM_SECURITY : uint8_t
    {
        //!< Security level register address
        // The safety level is 1 The highest rate of false recognition , The rejection
        // rate is the lowest . The safety level is 5 The lowest tate of false
        // recognition, The rejection rate is the highest .
        _1 = 0X1, //!< Security level 1
        _2 = 0X2, //!< Security level 2
        _3 = 0X3, //!< Security level 3
        _4 = 0X4, //!< Security level 4
        _5 = 0X5, //!< Security level 5
    };

    enum class PARAM_PACKETSIZE : uint8_t
    {

        _32 = 0X0,  //!< Packet size is 32 Byte
        _64 = 0X1,  //!< Packet size is 64 Byte
        _128 = 0X2, //!< Packet size is 128 Byte
        _256 = 0X3, //!< Packet size is 256 Byte
    };

    constexpr uint16_t STARTCODE{0xEF01}; //!< Fixed falue of EF01H; High byte transferred first

    enum class PacketIdentifier : uint8_t
    {
        COMMANDPACKET = 0x1, //!< Command packet
        DATAPACKET = 0x2,    //!< Data packet, must follow command packet or acknowledge packet
        ACKPACKET = 0x7,     //!< Acknowledge packet
        ENDDATAPACKET = 0x8, //!< End of data packet
    };

    struct SystemParameter{
        uint16_t status;
        uint16_t librarySizeMax;
        uint16_t librarySizeUsed;
        std::array<uint8_t, 32U> libraryIndicesUsed;
        uint8_t securityLevel;
        uint32_t deviceAddress;
        uint8_t dataPacketSizeCode;
        uint8_t baudRateTimes9600;
        char* algVer;
        char* fwVer;
    };

    class iFingerprintHandler{
        public:
        virtual void HandleFingerprintDetected(uint8_t errorCode, uint16_t finger, uint16_t score)=0;
        virtual void HandleEnrollmentUpdate(uint8_t errorCode, uint8_t step, uint16_t finger, const char* name)=0;
    };

    class iFingerprintActionHandler:public iFingerprintHandler{
        public:
        //call this, when the action should be executed (no error, timetable ok)
        virtual void HandleFingerprintAction(uint16_t fingerIndex, int action)=0;
    };


    class R503Pro
    {
    private:
        uart_port_t uart_num;
        gpio_num_t gpio_irq;
        SystemParameter params;
        bool previousIrqLineValue{true};

        bool isInEnrollment{false};
        iFingerprintHandler* handler;
        uint32_t targetAddress{0xFFFFFFFF};
        

        static void static_task(void *args)
        {
            R503Pro *myself = static_cast<R503Pro *>(args);
            myself->task();
        }

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

        void createAndSendDataPackage(PacketIdentifier pid, uint8_t* contents, size_t contentsLength, bool printHex=false){
            contentsLength=std::min((size_t)64, contentsLength);
            uint16_t packageLength=contentsLength+2;//data and checksum
            uint16_t wireLength=2+4+1+2+contentsLength+2;
            uint8_t buffer[wireLength];
            WriteU16_BigEndian(STARTCODE, buffer, 0);
            WriteU32_BigEndian(this->targetAddress, buffer, 2);
            WriteU8((uint8_t)pid, buffer, 6);
            WriteU16_BigEndian(packageLength, buffer, 7);
            std::memcpy(buffer+9, contents, contentsLength);
            uint16_t sum{0};
            for(size_t i=6; i<wireLength-2;i++){ sum+=buffer[i]; }
            WriteU16_BigEndian(sum, buffer, wireLength-2);
            if(printHex) ESP_LOG_BUFFER_HEX(TAG, buffer, wireLength);
            uart_flush_input(this->uart_num);
            uart_write_bytes(this->uart_num, buffer, wireLength);
        }

        RET receiveAndCheckPackage(uint8_t* buf, size_t bufLen, TickType_t ticks_to_wait=DEFAULT_TIMEOUT_TICKS){
            int receivedBytes=uart_read_bytes(this->uart_num, buf, bufLen, ticks_to_wait);
            if(receivedBytes!=bufLen) return RET::xPARSER_TIMEOUT;
            if(ParseU16_BigEndian(buf, 0)!=STARTCODE) return RET::xPARSER_CANNOT_FIND_STARTCODE;
            if(ParseU32_BigEndian(buf, 2)!=targetAddress){
                ESP_LOGE(TAG, "Wrong Module Address %lu",ParseU32_BigEndian(buf, 2));
                return RET::xPARSER_WRONG_MODULE_ADDRESS;
            } 
            if(buf[6]!=(uint8_t)PacketIdentifier::ACKPACKET) return RET::xPARSER_ACKNOWLEDGE_PACKET_EXPECTED;
            if(ParseU16_BigEndian(buf, 7)!=bufLen-9) return RET::xPARSER_UNEXPECTED_LENGTH;
            uint16_t sum{0};
            for(size_t i=6; i<bufLen-2;i++){ sum+=buf[i];}
            if(ParseU16_BigEndian(buf, bufLen-2)!=sum) return RET::xPARSER_CHECKSUM_ERROR;
            //if(buf[9]!=0) return (RET)buf[9];
            return RET::OK;
        }

protected:
        char fingerName[MAX_FINGERNAME_LEN+1];
public:       
        RET GetRandomCode(uint32_t& randomCode_out){
            uint8_t data[]{(uint8_t)INSTRUCTION::GetRandomCode};
            createAndSendDataPackage(PacketIdentifier::COMMANDPACKET, data, sizeof(data), true);
            size_t wireLength{16};
            uint8_t buffer[wireLength];
            RET ret=receiveAndCheckPackage(buffer, wireLength);
            ESP_LOG_BUFFER_HEX(TAG, buffer, wireLength);
            if(ret!=RET::OK){
                ESP_LOGE(TAG, "In GetRandomCode Parser error %d", (int)ret);
                return ret;
            }
            randomCode_out=ParseU32_BigEndian(buffer, 10);
            return (RET)buffer[9];
        }

        RET SetSysPara(PARAM_INDEX param, uint8_t value){
            uint8_t data[]{(uint8_t)INSTRUCTION::SetSysPara,(uint8_t)param,value};
            createAndSendDataPackage(PacketIdentifier::COMMANDPACKET, data, sizeof(data));
            size_t wireLength{12};
            uint8_t buffer[wireLength];
            RET ret=receiveAndCheckPackage(buffer, wireLength);
            if(ret!=RET::OK){
                ESP_LOGE(TAG, "In SetSysPara Parser error %d", (int)ret);
                return ret;
            }
            return (RET)buffer[9];
        }

        RET ReadSysPara(SystemParameter& outParams){
            uint8_t data[]{(uint8_t)INSTRUCTION::ReadSysPara};
            createAndSendDataPackage(PacketIdentifier::COMMANDPACKET, data, sizeof(data));
            const size_t wireLength{0x13+9};
            uint8_t buffer[wireLength];
            RET ret=receiveAndCheckPackage(buffer, wireLength);
            if(ret!=RET::OK){
                ESP_LOGE(TAG, "In ReadSysPara Parser error %d", (int)ret);
                return ret;
            }
            if(buffer[9]!=0){
                return (RET)buffer[9];
            }
            outParams.status=ParseU16_BigEndian(buffer, 10);
            outParams.librarySizeMax=ParseU16_BigEndian(buffer, 14);
            outParams.securityLevel=ParseU16_BigEndian(buffer, 16);
            outParams.deviceAddress=ParseU32_BigEndian(buffer, 18);
            outParams.dataPacketSizeCode=ParseU16_BigEndian(buffer, 22);
            outParams.baudRateTimes9600=ParseU16_BigEndian(buffer, 24);
            return RET::OK;
        }

        RET Get32ByteString(INSTRUCTION instr, char** stringToBeDeletedByCaller){
            char* string = new char[33];
            string[32]='\0';
            createAndSendDataPackage(PacketIdentifier::COMMANDPACKET, (uint8_t*)&instr, 1);
            const size_t wireLength{0x23+9};
            uint8_t buffer[wireLength];
            RET ret=receiveAndCheckPackage(buffer, wireLength);
            if(ret!=RET::OK){
                ESP_LOGE(TAG, "In Get32ByteString Parser error %d", (int)ret);
                return ret;
            }
            if(buffer[9]!=0){
                return (RET)buffer[9];
            }
            strncpy(string, (char*)(buffer+10), 32);
            *stringToBeDeletedByCaller=string;
            return RET::OK;
        }

        RET GetAlgVer(char** string){
            return Get32ByteString(INSTRUCTION::GetAlgVer, string);
        }

        RET GetFwVer(char** string){
            return Get32ByteString(INSTRUCTION::GetFwVer, string);
        }
        
        /**
         * How many fingerprints are there stored in the R503Pro
        */
        RET GetTemplateNumber(uint16_t& num){
            uint8_t data[]{(uint8_t)INSTRUCTION::TemplateNum};
            createAndSendDataPackage(PacketIdentifier::COMMANDPACKET, data, sizeof(data));
            const size_t wireLength{14};
            uint8_t buffer[wireLength];
            RET ret=receiveAndCheckPackage(buffer, wireLength);
            if(ret!=RET::OK){
                ESP_LOGE(TAG, "In GetTemplateNumber Parser error %d", (int)ret);
                return ret;
            }
            if(buffer[9]!=0){
                return (RET)buffer[9];
            }
            num=ParseU16_BigEndian(buffer, 10);
            ESP_LOGI(TAG, "Fingerprint reader has %d (0x%04X) valid templates", num, num);
            return RET::OK;
        }

        RET GetTemplateIndexTable(uint8_t page_0_1_2_3, std::array<uint8_t, 32> &buf){
            uint8_t data[]{(uint8_t)INSTRUCTION::ReadIndexTable, (uint8_t)(page_0_1_2_3%4)};
            createAndSendDataPackage(PacketIdentifier::COMMANDPACKET, data, sizeof(data));
            const size_t wireLength{2+4+1+2+1+32+2};
            uint8_t buffer[wireLength];
            RET ret=receiveAndCheckPackage(buffer, wireLength);
            if(ret!=RET::OK){
                ESP_LOGE(TAG, "In GetTemplateIndexTable Parser error %d", (int)ret);
                return ret;
            }
            if(buffer[9]!=0){
                return (RET)buffer[9];
            }
            for(int i=0;i<32;i++){
                buf[i]=buffer[10+i];
            }
            return RET::OK;
        }
        RET ReadAllSysPara(SystemParameter& outParams){
            RET ret;
            ret= ReadSysPara(outParams);
            if(ret!=RET::OK) return ret;
            ret= GetAlgVer(&outParams.algVer);
            if(ret!=RET::OK) return ret;
            ret= GetFwVer(&outParams.fwVer);
            if(ret!=RET::OK) return ret;
            ret= GetTemplateNumber(outParams.librarySizeUsed);
            if(ret!=RET::OK)return ret;
            ret = GetTemplateIndexTable(0, outParams.libraryIndicesUsed);
            return ret;
        }

        RET DeleteChar(uint16_t index){
            ESP_LOGI(TAG, "DeleteChar for index %d", index);
            uint8_t data[5];
            data[0]=(uint8_t)INSTRUCTION::DeleteChar;
            WriteU16_BigEndian(index, data, 1);
            WriteU16_BigEndian(1, data, 3);
            createAndSendDataPackage(PacketIdentifier::COMMANDPACKET, data, sizeof(data));

            const size_t wireLength{0x3+9};
            uint8_t buffer[wireLength];
            RET ret=receiveAndCheckPackage(buffer, wireLength);
            if(ret!=RET::OK){
                ESP_LOGE(TAG, "In DeleteChar: Parser error %d", (int)ret);
                return ret;
            }
            return (RET)buffer[9];
        }

        RET EmptyLibrary(){
            uint8_t data[]{(uint8_t)INSTRUCTION::Empty};
            createAndSendDataPackage(PacketIdentifier::COMMANDPACKET, data, sizeof(data));
            const size_t wireLength{0x3+9};
            uint8_t buffer[wireLength];
            RET ret=receiveAndCheckPackage(buffer, wireLength);
            if(ret!=RET::OK){
                ESP_LOGE(TAG, "In EmptyLibrary: Parser error %d", (int)ret);
                return ret;
            }
            return (RET)buffer[9];
        }

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
        
        RET CancelInstruction(){
            uint8_t data[]{(uint8_t)INSTRUCTION::Cancel};
            createAndSendDataPackage(PacketIdentifier::COMMANDPACKET, data, sizeof(data));
            const size_t wireLength{0x3+9};
            uint8_t buffer[wireLength];
            RET ret=receiveAndCheckPackage(buffer, wireLength);
            if(ret!=RET::OK){
                ESP_LOGE(TAG, "In EmptyLibrary: Parser error %d", (int)ret);
                return ret;
            }
            return (RET)buffer[9];
        }

        R503Pro(uart_port_t uart_num, gpio_num_t gpio_irq, iFingerprintHandler* handler, uint32_t targetAddress=DEFAULT_ADDRESS) : uart_num(uart_num), gpio_irq(gpio_irq), handler(handler), targetAddress(targetAddress) {
            fingerName[0]=0;
            fingerName[MAX_FINGERNAME_LEN+1]=0;
        }
        
        RET begin(gpio_num_t tx_host, gpio_num_t rx_host)
        {

            /* Install UART friver */
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
            ESP_LOGI(TAG, "Successfully connected with fingerprint addr=%lu; securityLevel=%u; libSize=%u; libUsed=%u fwVer=%s; algVer=%s; status=%u", params.deviceAddress,params.securityLevel, params.librarySizeMax, params.librarySizeUsed, params.fwVer, params.algVer, params.status);
            
            xTaskCreate(static_task, "fingerprint", 3072, this, 10, nullptr);
            return RET::OK;
        }

        fingerprint::SystemParameter* GetAllParams(){
            return &this->params;
        }
    };

}
#undef TAG