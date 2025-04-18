#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <memory>
#include <map>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <flatbuffers/flatbuffers.h>
#include "sdkconfig.h"
#include <common.hh>
#include "interfaces.hh"
#include "scheduler_timers.hh"
#include "sunsetsunrise.hh"
#include "webmanager_interfaces.hh"
#define TAG "SCHEDULER"
#include "esp_log.h"
namespace scheduler
{
    class Scheduler : public webmanager::iWebmanagerPlugin, public webmanager::iScheduler
    {
    private:
        webmanager::iWebmanagerCallback *callback;
        nvs_handle_t nvsSchedulerHandle;
        std::map<std::string, aTimer *> name2timer;
        uint32_t julianDay{0};

    public:
        Scheduler(nvs_handle_t nvsSchedulerHandle) : nvsSchedulerHandle(nvsSchedulerHandle) {}

        uint16_t GetCurrentValueOfSchedule(const char *schedulerName) override
        {
            if (!this->name2timer.contains(std::string(schedulerName)))
                return 0;
            time_t currentTime;

            time(&currentTime); // Get the current time
            return GetCurrentValueOfSchedule(schedulerName, currentTime);
        }

        uint16_t GetCurrentValueOfSchedule(const char *schedulerName, time_t currentTime)
        {

            scheduler::aTimer *o = this->name2timer.at(std::string(schedulerName));
            if (currentTime > 1800 && currentTime < 1716498366L)
            { // Failsafe: If System runs for at least half an hour, but has no valid timestamp (constant is epoch time when writing this code)
                return true;
            }
            struct tm *localTime;
            localTime = localtime(&currentTime); // Convert the current time to the local time
            int day_of_week = localTime->tm_wday;
            int h = localTime->tm_hour;
            int m = localTime->tm_min;
            int s = localTime->tm_sec;
            uint16_t val = o->GetCurrentValue(currentTime, day_of_week, h, m, s);
            ESP_LOGI(TAG, "On %d:%d:%d und weekday %d timer is %d", h, m, s, day_of_week, val);
            return val;
        }
        void Begin()
        {
            name2timer.clear();
            name2timer[ALWAYS.GetName()] = &ALWAYS;
            name2timer[NEVER.GetName()] = &NEVER,
            name2timer[DAILY_6_22.GetName()] = &DAILY_6_22;
            name2timer[WORKING_DAYS_7_18.GetName()] = &WORKING_DAYS_7_18;
            name2timer[TestEvenMinutesOnOddMinutesOff.GetName()] = &TestEvenMinutesOnOddMinutesOff;
            nvs_iterator_t it = nullptr;
            esp_err_t res = nvs_entry_find_in_handle(this->nvsSchedulerHandle, NVS_TYPE_BLOB, &it);
            while (res == ESP_OK)
            {
                nvs_entry_info_t info;
                nvs_entry_info(it, &info); // Can omit error check if parameters are guaranteed to be non-NULL
                size_t length{0};
                nvs_get_blob(this->nvsSchedulerHandle, info.key, nullptr, &length);
                uint8_t data[length];
                nvs_get_blob(this->nvsSchedulerHandle, info.key, data, &length);
                const scheduler::Schedule *fbSched = flatbuffers::GetRoot<scheduler::Schedule>(data);
                aTimer *t = Builder::BuildFromFlatbuffer(fbSched);
                this->name2timer[t->GetName()] = t;
                res = nvs_entry_next(&it);
            }
            nvs_release_iterator(it);
        }

        esp_err_t Loop(time_t unixSecs)
        {

            uint32_t newJulianDay = sunsetsunrise::JulianDate(unixSecs);
            if (newJulianDay == this->julianDay)
                return ESP_OK;
            this->julianDay = newJulianDay;
            double latDeg = 52.0965;
            double lonDeg = 7.6171;
            time_t sunriseUnixSecs{0};
            time_t sunsetUnixSecs{0};
            sunsetsunrise::NextSunriseAndSunset<double>(this->julianDay, latDeg, lonDeg, sunsetsunrise::eDawn::CIVIL, sunriseUnixSecs, sunsetUnixSecs);
            ESP_LOGI(TAG, "A new julian day %lu has begun. sunrise %lld, sunset %lld", julianDay, sunriseUnixSecs, sunsetUnixSecs);
            for (auto const &[key, val] : name2timer)
            {
                val->NewDayHasBegun(julianDay, sunriseUnixSecs, sunsetUnixSecs);
            }
            return ESP_OK;
        }

        esp_err_t handleRequest(webmanager::iWebmanagerCallback *callback, flatbuffers::FlatBufferBuilder &b, const scheduler::RequestSchedulerOpen *req)
        {
            auto c_name = req->name()->c_str();
            auto s_name = req->name()->str();
            if (!this->name2timer.contains(s_name))
            {
                ESP_LOGW(TAG, "Did not found scheduler '%s' in local database", c_name);
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "Send uResponseScheduler_ResponseSchedulerOpen for '%s' back to browser", c_name);
            scheduler::aTimer *o = this->name2timer.at(s_name);


            b.Finish(
                scheduler::CreateResponseWrapper(
                    b,
                    scheduler::Responses::Responses_ResponseSchedulerOpen,
                    scheduler::CreateResponseSchedulerOpen(b, o->CreateFlatbufferScheduleOffset(b)).Union()
                )
            );
            return callback->WrapAndSendAsync(scheduler::Namespace::Namespace_Value, b);
        }

        void FillFlatbufferWithAvailableNames(flatbuffers::FlatBufferBuilder &b, std::vector<flatbuffers::Offset<flatbuffers::String>> &vect) override
        {
            for (auto const &[key, val] : name2timer)
            {
                vect.push_back(b.CreateString(key));
            }
        }

        esp_err_t handleRequestList(webmanager::iWebmanagerCallback *callback, flatbuffers::FlatBufferBuilder &b)
        {
            std::vector<flatbuffers::Offset<scheduler::ResponseSchedulerListItem>> items;
            for (auto const &[key, val] : name2timer)
            {
                val->FillListOfResponseSchedulerListItems(b, items);
            }
            b.Finish(
                scheduler::CreateResponseWrapper(
                    b,
                    scheduler::Responses::Responses_ResponseSchedulerList,
                    scheduler::CreateResponseSchedulerListDirect(b, &items).Union()
                )
            );
            return callback->WrapAndSendAsync(scheduler::Namespace::Namespace_Value, b);
        }

        esp_err_t handleRequest(webmanager::iWebmanagerCallback *callback, flatbuffers::FlatBufferBuilder &b, const scheduler::RequestSchedulerDelete *req)
        {
            if (!this->name2timer.contains(req->name()->str()))
                return ESP_FAIL;
            this->name2timer.erase(req->name()->str());
            nvs_erase_key(this->nvsSchedulerHandle, req->name()->c_str());
            nvs_commit(this->nvsSchedulerHandle);
            ESP_LOGI(TAG, "Successfully deleted fingerprint %s", req->name()->c_str());
            return handleRequestList(callback, b);
        }

        esp_err_t handleRequest(webmanager::iWebmanagerCallback *callback, flatbuffers::FlatBufferBuilder &b, const scheduler::RequestSchedulerRename *req)
        {
            if (!this->name2timer.contains(req->old_name()->str()))
                return ESP_FAIL;
            if (this->name2timer.contains(req->new_name()->str()))
                return ESP_FAIL;
            scheduler::aTimer *o = this->name2timer.at(req->old_name()->str());
            size_t max_size{128};
            uint8_t blob[max_size];
            o->RenameAndFillNvsBlob(req->new_name()->str(), blob, max_size);
            this->name2timer.erase(req->old_name()->str());
            this->name2timer[req->new_name()->str()] = o;
            nvs_erase_key(this->nvsSchedulerHandle, req->old_name()->c_str());
            nvs_set_blob(this->nvsSchedulerHandle, req->new_name()->c_str(), blob, max_size);
            nvs_commit(this->nvsSchedulerHandle);
            ESP_LOGI(TAG, "Successfully renamed fingerprint %s to %s", req->old_name()->c_str(), req->new_name()->c_str());
            return handleRequestList(callback, b);
        }

        esp_err_t handleRequest(webmanager::iWebmanagerCallback *callback, flatbuffers::FlatBufferBuilder &b, const scheduler::RequestSchedulerSave *req)
        {
            auto name = req->payload()->name();
            if (this->name2timer.contains(name->str()))
            {
                ESP_LOGI(TAG, "%s is as existing fingerprint -->erase old in map and in flash", name->c_str());
                scheduler::aTimer *o = this->name2timer.at(name->str());
                this->name2timer.erase(name->str());
                nvs_erase_key(this->nvsSchedulerHandle, name->c_str());
                delete (o);
            }
            aTimer *t = Builder::BuildFromFlatbuffer(req->payload());
            this->name2timer[name->str()] = t;
            size_t max_size{128};
            uint8_t blob[max_size];
            t->FillNvsBlob(blob, max_size);
            nvs_set_blob(this->nvsSchedulerHandle, name->c_str(), blob, max_size);
            nvs_commit(this->nvsSchedulerHandle);
            ESP_LOGI(TAG, "Successfully saved fingerprint %s ", name->c_str());
            return handleRequestList(callback, b);
        }

        void OnBegin(webmanager::iWebmanagerCallback*) override{}
        void OnWifiConnect(webmanager::iWebmanagerCallback*) override{}
        void OnWifiDisconnect(webmanager::iWebmanagerCallback*)override{}
        void OnTimeUpdate(webmanager::iWebmanagerCallback*)override{}
        webmanager::eMessageReceiverResult ProvideWebsocketMessage(webmanager::iWebmanagerCallback *callback, httpd_req_t *req, httpd_ws_frame_t *ws_pkt, uint32_t ns, uint8_t *buf) override
        {
            if (ns != scheduler::Namespace::Namespace_Value)
                return webmanager::eMessageReceiverResult::NOT_FOR_ME;
            auto rw = flatbuffers::GetRoot<scheduler::RequestWrapper>(buf);
            flatbuffers::FlatBufferBuilder b(1024);
            switch (rw->request_type())
            {

            case scheduler::Requests::Requests_RequestSchedulerList:
            {
                return handleRequestList(callback, b) == ESP_OK ? webmanager::eMessageReceiverResult::OK : webmanager::eMessageReceiverResult::FOR_ME_BUT_FAILED;
            }
            case scheduler::Requests::Requests_RequestSchedulerDelete:
            {
                return handleRequest(callback, b, rw->request_as_RequestSchedulerDelete()) == ESP_OK ? webmanager::eMessageReceiverResult::OK : webmanager::eMessageReceiverResult::FOR_ME_BUT_FAILED;
            }
            case scheduler::Requests::Requests_RequestSchedulerRename:
            {
                return handleRequest(callback, b, rw->request_as_RequestSchedulerRename()) == ESP_OK ? webmanager::eMessageReceiverResult::OK : webmanager::eMessageReceiverResult::FOR_ME_BUT_FAILED;
            }
            case scheduler::Requests::Requests_RequestSchedulerSave:
            {
                return handleRequest(callback, b, rw->request_as_RequestSchedulerSave()) == ESP_OK ? webmanager::eMessageReceiverResult::OK : webmanager::eMessageReceiverResult::FOR_ME_BUT_FAILED;
            }
            case scheduler::Requests::Requests_RequestSchedulerOpen:
            {
                return handleRequest(callback, b, rw->request_as_RequestSchedulerOpen()) == ESP_OK ? webmanager::eMessageReceiverResult::OK : webmanager::eMessageReceiverResult::FOR_ME_BUT_FAILED;
            }
            default:
            {
                break;
            }
            }
            return webmanager::eMessageReceiverResult::NOT_FOR_ME;
        }
    };

}
#undef TAG