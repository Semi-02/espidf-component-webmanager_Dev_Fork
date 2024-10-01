import { WebSocket } from "ws"
import * as flatbuffers from "flatbuffers"
import BuildApps from "../generated/sensact/sensactapps_copied_during_build";
import { SensactApplication } from "../typescript/utils/sensactapps_base";
import { WrapAndFinishAndSend } from "../utils";
import { Responses } from "../generated/flatbuffers/webmanager";
import { RequestStatus, ResponseStatus, ResponseStatusItem } from "../generated/flatbuffers/websensact";

let sensactApps:Map<number, SensactApplication>;

export function BuildWebsensact(){
    sensactApps=new Map<number, SensactApplication>(BuildApps({}).map(v=>[v.applicationId, v])); 
}


export function websensact_requestStatus(ws: WebSocket, m: RequestStatus) {
    console.info(`websensact_requestStatus for ${m.idsLength()} ApplicationIDs`);

    let b = new flatbuffers.Builder(1024);
    ResponseStatus.startStatesVector(b, m.idsLength());
    for(var i=0;i< m.idsLength();i++){
        var id = m.ids(i);
        var state = 0xFFFFFFFF;
        if(sensactApps.has(id)){
            state = sensactApps.get(id)!.GetState();
            console.warn(`Application ${id} has state ${state}.`);
        }else{
            console.warn(`Application ${id} not found. Assuming default state 0xFFFFFFFF.`);
        }
        ResponseStatusItem.createResponseStatusItem(b,id,state);
    }
    WrapAndFinishAndSend(ws, b, Responses.websensact_ResponseStatus, ResponseStatus.createResponseStatus(b, b.endVector()));     
}