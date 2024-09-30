#pragma once
#include <inttypes.h>
#include <cstring>
#include <ctime>
#include <esp_wifi.h>
#include "../generated/flatbuffers_cpp/app_generated.h"

namespace webmanager{

    enum class WifiStationState
    {
        NO_CONNECTION,
        SHOULD_CONNECT,   // Daten sind verfügbar, die passen könnten. Es soll beim nächsten Retry-Tick ein Verbindungsversuch gestartet werden. Gerade im Moment wurde aber noch kein Verbindungsversiuch gestartet. -->Scan möglich
        ABOUT_TO_CONNECT, // es wurde gerade ein Verbindungsaufbau gestartet, es ist aber noch nicht klar, ob der erfolgreich war -->Scan nicht möglich
        CONNECTED,
    };

    struct MessageLogEntry
    {
        uint32_t messageCode;
        uint32_t lastMessageData;
        uint32_t messageCount;
        time_t lastMessageTimestamp;

        MessageLogEntry(uint32_t messageCode, uint32_t lastMessageData, uint32_t messageCount, time_t lastMessageTimestamp) : messageCode(messageCode),
                                                                                                                              lastMessageData(lastMessageData),
                                                                                                                              messageCount(messageCount),
                                                                                                                              lastMessageTimestamp(lastMessageTimestamp)
        {
        }
        MessageLogEntry() : messageCode(0),
                            lastMessageData(0),
                            messageCount(0),
                            lastMessageTimestamp(0)
        {
        }

        bool operator<(const MessageLogEntry &str) const
        {
            return (lastMessageTimestamp < str.lastMessageTimestamp);
        }
    };

    class AsyncResponse{
        public:
        uint8_t* buffer;
        size_t buffer_len;
       
        AsyncResponse(flatbuffers::FlatBufferBuilder* b){
            uint8_t* bp=b->GetBufferPointer();
            buffer_len = b->GetSize();
            buffer = new uint8_t[buffer_len];
            std::memcpy(buffer, bp, buffer_len);
        }

        ~AsyncResponse(){
            delete[] buffer;
        }
    };
    
    class M;
}