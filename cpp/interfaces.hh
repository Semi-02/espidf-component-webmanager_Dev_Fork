#pragma once

#include <esp_err.h>
#include "../generated/flatbuffers_cpp/app_generated.h"

#define NAME_OF_THE_ALWAYS_SCHEDULER "ALWAYS"

enum class eMessageReceiverResult{
    OK=0,
    NOT_FOR_ME=1,
    FOR_ME_BUT_FAILED,
};

class MessageSender{
    public:
    virtual esp_err_t WrapAndFinishAndSendAsync(::flatbuffers::FlatBufferBuilder &_fbb, webmanager::Responses response_type = webmanager::Responses_NONE, ::flatbuffers::Offset<void> message = 0)=0;
};

class iMessageReceiver{
    public:
    virtual eMessageReceiverResult provideWebsocketMessage(MessageSender* callback, httpd_req_t *req, httpd_ws_frame_t *ws_pkt, const webmanager::RequestWrapper *mw)=0;
};

class iScheduler{
    public:
    virtual uint16_t GetCurrentValueOfSchedule(const char* schedulerName)=0;
    virtual void FillFlatbufferWithAvailableNames(flatbuffers::FlatBufferBuilder &b, std::vector<flatbuffers::Offset<flatbuffers::String>> &vect)=0;
};