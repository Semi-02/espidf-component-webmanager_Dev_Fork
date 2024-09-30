#pragma once
#include <cstdio>
#include <ctime>
#include <map>
#include <vector>
#include <string>
#include <flatbuffers/flatbuffers.h>
#include "esp_random.h"
#include "sunsetsunrise.hh"
#define TAG "SCHEDULER"
#include "esp_log.h"
namespace SCHEDULER
{
    class aTimer
    {
    protected:
        std::string name;
    public:
        std::string GetName(){
            return name;
        }
        aTimer(std::string name):name(name){}
        virtual ~aTimer(){

        }

        virtual uint16_t GetCurrentValue(int d, int h, int m, int s) const = 0;

        static aTimer* BuildFromBlob(uint8_t* data){
            return nullptr;
        }



        virtual void NewDayHasBegun(uint32_t julianDay, float todaysSunriseHour, float todaysSunsetHour){
            return;
        }
       
        virtual void FillListOfResponseSchedulerListItems(flatbuffers::FlatBufferBuilder &b, std::vector<flatbuffers::Offset<scheduler::ResponseSchedulerListItem>> &items)=0;

        virtual flatbuffers::Offset<scheduler::Schedule> CreateFlatbufferScheduleOffset(flatbuffers::FlatBufferBuilder &b)=0;

        void RenameAndFillNvsBlob(std::string newName, uint8_t* data, size_t& len_in_out){
            this->name=newName;
            FillNvsBlob(data, len_in_out);
        }

        void FillNvsBlob(uint8_t* data, size_t& len_in_out){
            flatbuffers::FlatBufferBuilder b(len_in_out);
            b.Finish(this->CreateFlatbufferScheduleOffset(b));
            len_in_out = b.GetSize();
            std::memcpy(data, b.GetBufferPointer(), len_in_out);
        }
    };

    class cALWAYS: public aTimer
    {
        public:
        cALWAYS(std::string name):aTimer(name){}
        void FillListOfResponseSchedulerListItems(flatbuffers::FlatBufferBuilder &b, std::vector<flatbuffers::Offset<scheduler::ResponseSchedulerListItem>> &items) override{
            items.push_back(scheduler::CreateResponseSchedulerListItemDirect(b, this->name.c_str(), scheduler::eSchedule::eSchedule_Predefined));
        }

        flatbuffers::Offset<scheduler::Schedule> CreateFlatbufferScheduleOffset(flatbuffers::FlatBufferBuilder &b) override{
            return scheduler::CreateScheduleDirect(b,
                name.c_str(),
                scheduler::uSchedule::uSchedule_Predefined,
                scheduler::CreatePredefined(b).Union()
                );
        }
        protected:
        uint16_t GetCurrentValue(int d, int h, int m, int s) const override
        {
            return UINT16_MAX;
        }

        
    } ALWAYS(NAME_OF_THE_ALWAYS_SCHEDULER);

    class cNEVER: public aTimer
    {
        public:
        cNEVER(std::string name):aTimer(name){}
        void FillListOfResponseSchedulerListItems(flatbuffers::FlatBufferBuilder &b, std::vector<flatbuffers::Offset<scheduler::ResponseSchedulerListItem>> &items) override{
            items.push_back(scheduler::CreateResponseSchedulerListItemDirect(b, this->name.c_str(), scheduler::eSchedule::eSchedule_Predefined));
        }
        flatbuffers::Offset<scheduler::Schedule> CreateFlatbufferScheduleOffset(flatbuffers::FlatBufferBuilder &b) override{
            return scheduler::CreateScheduleDirect(b,
                name.c_str(),
                scheduler::uSchedule::uSchedule_Predefined,
                scheduler::CreatePredefined(b).Union()
                );
        }
        protected:
        uint16_t GetCurrentValue(int d, int h, int m, int s) const override
        {
            return 0;
        }


        
    } NEVER("NEVER");

    class cDAILY_6_22: public aTimer
    {
        public:
        cDAILY_6_22(std::string name):aTimer(name){}
        void FillListOfResponseSchedulerListItems(flatbuffers::FlatBufferBuilder &b, std::vector<flatbuffers::Offset<scheduler::ResponseSchedulerListItem>> &items) override{
            items.push_back(scheduler::CreateResponseSchedulerListItemDirect(b, this->name.c_str(), scheduler::eSchedule::eSchedule_Predefined));
        }

        flatbuffers::Offset<scheduler::Schedule> CreateFlatbufferScheduleOffset(flatbuffers::FlatBufferBuilder &b) override{
            return scheduler::CreateScheduleDirect(b,
                name.c_str(),
                scheduler::uSchedule::uSchedule_Predefined,
                scheduler::CreatePredefined(b).Union()
                );
        }
        protected:

        uint16_t GetCurrentValue(int d, int h, int m, int s) const override
        {
            return (h >= 6 && h < 22)?UINT16_MAX:0;
        }

    } DAILY_6_22("DAILY_6_22");

    class cWORKING_DAYS_7_18: public aTimer
    {
        public:
        cWORKING_DAYS_7_18(std::string name):aTimer(name){}
        void FillListOfResponseSchedulerListItems(flatbuffers::FlatBufferBuilder &b, std::vector<flatbuffers::Offset<scheduler::ResponseSchedulerListItem>> &items) override{
            items.push_back(scheduler::CreateResponseSchedulerListItemDirect(b, this->name.c_str(), scheduler::eSchedule::eSchedule_Predefined));
        }
        flatbuffers::Offset<scheduler::Schedule> CreateFlatbufferScheduleOffset(flatbuffers::FlatBufferBuilder &b) override{
            return scheduler::CreateScheduleDirect(b,
                name.c_str(),
                scheduler::uSchedule::uSchedule_Predefined,
                scheduler::CreatePredefined(b).Union()
                );
        }
        protected:
        uint16_t GetCurrentValue(int d, int h, int m, int s) const override
        {
            if (d > 5)
                return 0;
            return (h >= 7 && h < 18)?UINT16_MAX:0;
        }

    } WORKING_DAYS_7_18("WORKING_DAYS_7_18");


    class cTestEvenMinutesOnOddMinutesOff: public aTimer
    {
        public:
        cTestEvenMinutesOnOddMinutesOff(std::string name):aTimer(name){}
        
        void FillListOfResponseSchedulerListItems(flatbuffers::FlatBufferBuilder &b, std::vector<flatbuffers::Offset<scheduler::ResponseSchedulerListItem>> &items) override{
            items.push_back(scheduler::CreateResponseSchedulerListItemDirect(b, this->name.c_str(), scheduler::eSchedule::eSchedule_Predefined));
        }
        flatbuffers::Offset<scheduler::Schedule> CreateFlatbufferScheduleOffset(flatbuffers::FlatBufferBuilder &b) override{
            return scheduler::CreateScheduleDirect(b,
                name.c_str(),
                scheduler::uSchedule::uSchedule_Predefined,
                scheduler::CreatePredefined(b).Union()
                );
        }
        protected:
        uint16_t GetCurrentValue(int d, int h, int m, int s) const override
        {
            return (m%2==0)?UINT16_MAX:0;
        }

    } TestEvenMinutesOnOddMinutesOff("TestEvenOdd");

    class OneWeekIn15MinutesTimer :public aTimer{
        private:
        std::array<uint8_t, 84> data;
        public:
        OneWeekIn15MinutesTimer(std::string name, std::array<uint8_t, 84> data):aTimer(name), data(data){}

        uint16_t GetCurrentValue(int d, int h, int m, int s) const override
        {
            uint8_t twoHours=data[d*12+(h>>1)];
            uint8_t fifteenMinutesSlot = 4*(h&1)+(m/15);
            return (twoHours&(1<<fifteenMinutesSlot))?UINT16_MAX:0;
        }

        static aTimer* BuildFromFlatbuffer(std::string name, const scheduler::OneWeekIn15Minutes *owi15m){
            
            auto data_buf = owi15m->data()->v();
            std::array<uint8_t, 84> data;
            for (int i = 0; i < 84; i++)
            {
                data[i] = data_buf->Get(i);
            }
            return new SCHEDULER::OneWeekIn15MinutesTimer(name, data);
        }


        void FillListOfResponseSchedulerListItems(flatbuffers::FlatBufferBuilder &b, std::vector<flatbuffers::Offset<scheduler::ResponseSchedulerListItem>> &items) override{
            items.push_back(scheduler::CreateResponseSchedulerListItemDirect(b, this->name.c_str(), scheduler::eSchedule::eSchedule_OneWeekIn15Minutes));
        }

        flatbuffers::Offset<scheduler::Schedule> CreateFlatbufferScheduleOffset(flatbuffers::FlatBufferBuilder &b) override{
            ESP_LOGI(TAG, "Create Offset<scheduler::Schedule> for OneWeekIn15MinutesTimer %s", name.c_str());
            scheduler::OneWeekIn15MinutesData owi15md(data);
            return scheduler::CreateScheduleDirect(b,
                name.c_str(),
                scheduler::uSchedule::uSchedule_OneWeekIn15Minutes,
                scheduler::CreateOneWeekIn15Minutes(b, &owi15md).Union()
                );
        }
    };

    class SunRandom:public aTimer{
        private:
            const float offsetHours;
            const float randomHours;
            uint8_t todaysStartHour{0};
            uint8_t todaysStartMin{0};
            uint8_t todaysEndHour{0};
            uint8_t todaysEndMin{0};

        public:
        SunRandom(std::string name, float offsetHours, float randomHours):aTimer(name), offsetHours(offsetHours), randomHours(randomHours){
            
        }
        flatbuffers::Offset<scheduler::Schedule> CreateFlatbufferScheduleOffset(flatbuffers::FlatBufferBuilder &b) override{
            ESP_LOGI(TAG, "Create Offset<scheduler::Schedule> for SunRandom %s", name.c_str());
            return scheduler::CreateScheduleDirect(b,
                name.c_str(),
                scheduler::uSchedule::uSchedule_SunRandom,
                scheduler::CreateSunRandom(b, offsetHours*60, randomHours*60).Union()
                );
        }

        static aTimer* BuildFromFlatbuffer(std::string name, const scheduler::SunRandom *sr){
            return new SCHEDULER::SunRandom(name, sr->offset_minutes() / 60.0, sr->random_minutes() / 60.0);
        }

        void FillListOfResponseSchedulerListItems(flatbuffers::FlatBufferBuilder &b, std::vector<flatbuffers::Offset<scheduler::ResponseSchedulerListItem>> &items) override{
            items.push_back(scheduler::CreateResponseSchedulerListItemDirect(b, this->name.c_str(), scheduler::eSchedule::eSchedule_SunRandom));
        }

        uint16_t GetCurrentValue(int d, int h, int m, int s) const override
        {
            return (h>=todaysStartHour && m>=todaysStartMin && h<=todaysEndHour && m<=todaysEndMin)?UINT16_MAX:0;
        }



        void NewDayHasBegun(uint32_t julianDay, float todaysSunriseHour, float todaysSunsetHour) override{
            

            float randomSunrise = (((float)esp_random()/(float)UINT32_MAX)*2*randomHours)-randomHours;
            float randomSunset = (((float)esp_random()/(float)UINT32_MAX)*2*randomHours)-randomHours;
            float sunriseHoursCorr = todaysSunriseHour+offsetHours+randomSunrise;
            float sunsetHoursCorr = todaysSunsetHour-offsetHours+randomSunset;
            todaysStartHour = (uint8_t)sunriseHoursCorr;
            todaysStartMin = (uint8_t)(60.0f * std::fmod(sunriseHoursCorr, 1.0f));

            todaysEndHour = (uint8_t)sunsetHoursCorr;
            todaysEndMin = (uint8_t)(60.0f * std::fmod(sunsetHoursCorr, 1.0f));

            return;
        }
    };

    class Builder{
        public:
        static aTimer* BuildFromFlatbuffer(const scheduler::Schedule *schedule){
            switch(schedule->schedule_type()){
                case scheduler::uSchedule::uSchedule_OneWeekIn15Minutes: return OneWeekIn15MinutesTimer::BuildFromFlatbuffer(schedule->name()->str(), schedule->schedule_as_OneWeekIn15Minutes());
                case scheduler::uSchedule::uSchedule_SunRandom: return SunRandom::BuildFromFlatbuffer(schedule->name()->str(), schedule->schedule_as_SunRandom());
                default: return nullptr;
            }
            return nullptr;
        }
    };
    
   
    
}
#undef TAG