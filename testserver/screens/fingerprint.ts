import * as flatbuffers from "flatbuffers"
import { WebSocket } from "ws"
import { Finger, NotifyEnrollNewFinger, RequestEnrollNewFinger, RequestStoreFingerAction, RequestStoreFingerSchedule, ResponseEnrollNewFinger, ResponseFingerprintSensorInfo, ResponseFingers, ResponseStoreFingerAction, ResponseStoreFingerSchedule, ResponseWrapper, Responses, Uint8x32 } from "../generated/flatbuffers/webmanager";
import { exampleSchedules} from "./scheduler"

export var exampleFingers=[
    {name:"Klaus rechts mitte", index:1, schedule:"OneWeekIn15MinutesA", action:1},
    {name:"Steffi links mitte", index:2, schedule:"PredefinedA", action:3},
    {name:"Klara rechts zeige", index:3, schedule:"SunRandomA", action:2},
]

export function processRequestStoreFingerAction(ws:WebSocket, req: RequestStoreFingerAction){
    let b = new flatbuffers.Builder(1024);
    exampleFingers.find(v=>v.index==req.fingerIndex()).action=req.actionIndex()
    b.finish(ResponseWrapper.createResponseWrapper(b, Responses.ResponseStoreFingerAction,
        ResponseStoreFingerAction.createResponseStoreFingerAction(b)
        ));
    ws.send(b.asUint8Array());
}

export function processRequestStoreFingerSchedule(ws:WebSocket, req: RequestStoreFingerSchedule){
    let b = new flatbuffers.Builder(1024);
    exampleFingers.find(v=>v.index==req.fingerIndex()).schedule=req.scheduleName()
    b.finish(ResponseWrapper.createResponseWrapper(b, Responses.ResponseStoreFingerSchedule,
        ResponseStoreFingerSchedule.createResponseStoreFingerSchedule(b)
        ));
    ws.send(b.asUint8Array());
}

export function sendResponseFingerprintSensorInfo(ws: WebSocket) {
    let b = new flatbuffers.Builder(1024);
    var usedIndices =new Array(32).fill(0);
    usedIndices[0]=0b10101010;
    usedIndices[1]=0b01010101;
    
    var alg=b.createString("AlgVer1.1")
    var fw=b.createString("FwVer1.2")
    ResponseFingerprintSensorInfo.startResponseFingerprintSensorInfo(b);
    ResponseFingerprintSensorInfo.addAlgVer(b, alg);
    ResponseFingerprintSensorInfo.addFwVer(b, fw);
    ResponseFingerprintSensorInfo.addBaudRateTimes9600(b, 6);
    ResponseFingerprintSensorInfo.addDataPacketSizeCode(b, 2);
    ResponseFingerprintSensorInfo.addDeviceAddress(b, 0xffff);
    ResponseFingerprintSensorInfo.addLibrarySizeMax(b, 1500);
    ResponseFingerprintSensorInfo.addLibrarySizeUsed(b, 2);
    ResponseFingerprintSensorInfo.addLibraryUsedIndices(b, Uint8x32.createUint8x32(b, usedIndices));
    ResponseFingerprintSensorInfo.addSecurityLevel(b, 3)
    ResponseFingerprintSensorInfo.addStatus(b, 0);
    b.finish(ResponseWrapper.createResponseWrapper(b, Responses.ResponseFingerprintSensorInfo, ResponseFingerprintSensorInfo.endResponseFingerprintSensorInfo(b)));
    ws.send(b.asUint8Array());
}


export function sendResponseFingers(ws: WebSocket) {
    let b = new flatbuffers.Builder(1024);
    var fingers:number[]=[];
    exampleFingers.forEach(f=>{
        fingers.push(Finger.createFinger(b, b.createString(f.name), f.index, b.createString(f.schedule), f.action))
    })
  
    var scheduleNames: number[]=[];
    exampleSchedules.forEach((v,k) => {
        scheduleNames.push(b.createString(k))
    });

    b.finish(ResponseWrapper.createResponseWrapper(b, Responses.ResponseFingers,
        ResponseFingers.createResponseFingers(b, ResponseFingers.createScheduleNamesVector(b, scheduleNames),  ResponseFingers.createFingersVector(b, fingers))
        ));
    ws.send(b.asUint8Array());
}
var newFingerIndex=42;

function sendNotifyEnrollNewFinger(ws:WebSocket, step:number, index:number, name:string, delay_ms:number){
    
    setTimeout(() => {
        console.log(`sendNotifyEnrollNewFinger step=${step}, index=${index}, name=${name}`)
        let b = new flatbuffers.Builder(1024);
        b.finish(ResponseWrapper.createResponseWrapper(b, Responses.NotifyEnrollNewFinger, NotifyEnrollNewFinger.createNotifyEnrollNewFinger(b, b.createString(name), index, step, 0)));
        ws.send(b.asUint8Array());
        if(step==15){
            exampleFingers.push({name:name, index:newFingerIndex, schedule:exampleSchedules[0].name, action:0})
            newFingerIndex++;
        }
    }, delay_ms);

}

export function sendResponseEnrollNewFinger(ws:WebSocket, req:RequestEnrollNewFinger){
    var fpName= req.name();
    console.log(`sendResponseEnrollNewFinger name=${fpName}`)
    let b = new flatbuffers.Builder(1024);
    b.finish(ResponseWrapper.createResponseWrapper(b, Responses.ResponseEnrollNewFinger, ResponseEnrollNewFinger.createResponseEnrollNewFinger(b, 0)));
    ws.send(b.asUint8Array());
    for(var step=0; step<16;step++){
        sendNotifyEnrollNewFinger(ws, step, newFingerIndex, fpName, step*500+2000);
    }
}
