#pragma once
#include <cstdint>
namespace fingerprint{
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


}