#pragma once
//TODO: _Eigentlich_ könnte der WifiManager auch nur ein Plugin für den Webmanager sein.
//Problematisch ist aber, den wegen der extremen Verflechtung sauber rauszuoperieren
//Möglicherweise ist das sinnlos, Möglicherweise genügen einige wenige "OnXYZ"-Eventhandler
//Im webmanager würde vermutlich wenig bleiben - primär der HTTP-Server mit den Handlern und dem Websocket-Gedöns
#include "webmanager_interfaces.hh"
#include "flatbuffers/flatbuffers.h"
#include "../generated/flatbuffers_cpp/wifimanager_generated.h"
using namespace webmanager;
class WifimanagerPlugin : public webmanager::iWebmanagerPlugin
{
    private:
    DeviceManager* devicemanager;
    
    public:
    WifimanagerPlugin(DeviceManager* devicemanager):devicemanager(devicemanager){

    }
    
    void OnBegin(webmanager::iWebmanagerCallback *callback) override {
        
    }
    void OnWifiConnect(webmanager::iWebmanagerCallback *callback) override { (void)(callback); }
    void OnWifiDisconnect(webmanager::iWebmanagerCallback *callback) override { (void)(callback); }
    void OnTimeUpdate(webmanager::iWebmanagerCallback *callback) override { (void)(callback); }
    webmanager::eMessageReceiverResult ProvideWebsocketMessage(webmanager::iWebmanagerCallback *callback, httpd_req_t *req, httpd_ws_frame_t *ws_pkt, uint32_t ns, uint8_t *buf) override
    {
        if(ns!=wifimanager::Namespace::Namespace_Value) return eMessageReceiverResult::NOT_FOR_ME;
        auto rw = wifimanager::GetRoot<functionblock::RequestWrapper>(buf);
        auto reqType=rw->request_type();
        
        switch (reqType){
        case functionblock::Requests::Requests_RequestDebugData:{
            return webmanager::eMessageReceiverResult::OK;
        }
        case functionblock::Requests::Requests_RequestFbdRun:
        {
            return webmanager::eMessageReceiverResult::OK;
        }
        default:
            return webmanager::eMessageReceiverResult::FOR_ME_BUT_FAILED;

        }   
    }
};
