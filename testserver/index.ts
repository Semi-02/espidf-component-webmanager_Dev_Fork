import http from "node:http"
import * as fs from "node:fs"
import * as flatbuffers from "flatbuffers"
import { WebSocketServer, WebSocket } from "ws"
import { ResponseSystemData } from "./generated/flatbuffers/webmanager/response-system-data"
import { PartitionInfo } from "./generated/flatbuffers/webmanager/partition-info";
import { Mac6, } from "./generated/flatbuffers/webmanager/mac6";
import { NotifyLiveLogItem } from "./generated/flatbuffers/webmanager/notify-live-log-item";
import { AccessPoint } from "./generated/flatbuffers/webmanager/access-point";
import { BooleanSetting, EnumSetting, Finger, IntegerSetting, JournalItem, NotifyEnrollNewFinger, RequestEnrollNewFinger, RequestGetUserSettings, RequestSetUserSettings, RequestStoreFingerAction, RequestStoreFingerSchedule, RequestTimeseries, RequestWifiConnect, RequestWrapper, Requests, ResponseEnrollNewFinger, ResponseFingerprintSensorInfo, ResponseFingers, ResponseGetUserSettings, ResponseJournal, ResponseNetworkInformation, ResponseSetUserSettings, ResponseWifiConnectFailed, ResponseWifiConnectSuccessful, ResponseWrapper, Responses, Setting, SettingWrapper, StringSetting, TimeGranularity, Uint8x32 } from "./generated/flatbuffers/webmanager";
import { exampleSchedules, processScheduler_RequestScheduler } from "./screens/scheduler"
import { createTimeseries } from "./screens/timeseries"
import { RequestScheduler } from "./generated/flatbuffers/scheduler/request-scheduler";
import { processRequestStoreFingerAction, processRequestStoreFingerSchedule, sendResponseEnrollNewFinger, sendResponseFingerprintSensorInfo, sendResponseFingers } from "./screens/fingerprint"
import { BuildWebsensact, websensact_requestStatus } from "./screens/websensact"
import { RequestStatus } from "./generated/flatbuffers/websensact"
import { ISensactContext } from "./typescript/utils/interfaces"


const PORT = 3000;
const BUNDLED_FILE = "../dist_webui/compressed/app.html.br";

const wss = new WebSocketServer({ noServer: true });


const AP_GOOD="Connect to AP -50dB Auth=2";
const AP_BAD="Connect to AP -100dB Auth=2";

function sendResponseWifiAccesspoints(ws: WebSocket) {
    let b = new flatbuffers.Builder(1024);

    let accesspointsOffset = ResponseNetworkInformation.createAccesspointsVector(b, [
        AccessPoint.createAccessPoint(b, b.createString(AP_BAD), 11, -66, 2),
        AccessPoint.createAccessPoint(b, b.createString(AP_GOOD), 11, -50, 2),
        AccessPoint.createAccessPoint(b, b.createString("AP -76dB Auth=0"), 11, -76, 0),
        AccessPoint.createAccessPoint(b, b.createString("AP -74dB Auth=0"), 11, -74, 0),
        AccessPoint.createAccessPoint(b, b.createString("AP -66dB Auth=0"), 11, -66, 0),
        AccessPoint.createAccessPoint(b, b.createString("AP -59dB Auth=0"), 11, -50, 0)
    ]);
    let r = ResponseNetworkInformation.createResponseNetworkInformation(b, 
        b.createString("MyHostnameKL"), 
        b.createString("MySsidApKL"),  b.createString("Password"), 32,true, b.createString("ssidSta"), 32,43,23,23,accesspointsOffset);
    b.finish(ResponseWrapper.createResponseWrapper(b, Responses.ResponseNetworkInformation, r));
    ws.send(b.asUint8Array());
}

function sendResponseWifiConnectionSuccessOrFailed(ws: WebSocket, req: RequestWifiConnect){
    let b = new flatbuffers.Builder(1024);
    if(req.ssid()==AP_GOOD){
        let r = ResponseWifiConnectSuccessful.createResponseWifiConnectSuccessful(b, b.createString(AP_GOOD), 0xFF101001,0x10101002,0xFF101003, -62);
        b.finish(ResponseWrapper.createResponseWrapper(b, Responses.ResponseWifiConnectSuccessful, r));
    }else{
        let r = ResponseWifiConnectFailed.createResponseWifiConnectFailed(b, b.createString(AP_GOOD));
        b.finish(ResponseWrapper.createResponseWrapper(b, Responses.ResponseWifiConnectFailed, r));
    }
    ws.send(b.asUint8Array());
}


let toggler: boolean = false;
let counter: number = 42;

function sendResponseGetUserSettings(ws: WebSocket, req: RequestGetUserSettings) {
    var groupName= req.groupKey();
    let b = new flatbuffers.Builder(1024);
    let settingsOffset: number = 0;
    if (groupName == "Group1") {
        settingsOffset = ResponseGetUserSettings.createSettingsVector(b, [
            SettingWrapper.createSettingWrapper(b, b.createString("G1_1_S"), Setting.StringSetting, StringSetting.createStringSetting(b, b.createString("Test String Item1 Value " + counter))),
            SettingWrapper.createSettingWrapper(b, b.createString("G1_2_S"), Setting.StringSetting, StringSetting.createStringSetting(b, b.createString("Test String Item2 Value " + counter))),
        ]);
    }
    else if (groupName == "Group2") {
        settingsOffset = ResponseGetUserSettings.createSettingsVector(b, [
            SettingWrapper.createSettingWrapper(b, b.createString("G2_1_S"), Setting.StringSetting, StringSetting.createStringSetting(b, b.createString("Test String sub1item2 Value"))),
            SettingWrapper.createSettingWrapper(b, b.createString("G2_2_I"), Setting.IntegerSetting, IntegerSetting.createIntegerSetting(b, counter)),
            SettingWrapper.createSettingWrapper(b, b.createString("G2_3_B"), Setting.BooleanSetting, BooleanSetting.createBooleanSetting(b, toggler)),
            SettingWrapper.createSettingWrapper(b, b.createString("G2_4_E"), Setting.EnumSetting, EnumSetting.createEnumSetting(b, counter % 4)),

        ]);
        counter++;
        toggler = !toggler;
    }

    let r = ResponseGetUserSettings.createResponseGetUserSettings(b, b.createString(groupName), settingsOffset);
    b.finish(ResponseWrapper.createResponseWrapper(b, Responses.ResponseGetUserSettings, r));
    ws.send(b.asUint8Array());
}

function sendResponseSetUserSettings(ws: WebSocket, req: RequestSetUserSettings) {
    let groupName = req.groupKey()!;
    let names: string[] = [];
    for (let i = 0; i < req.settingsLength(); i++) {
        let name = req.settings(i)!.settingKey()!;
        names.push(name);
    }
    console.log(`Received setting for group ${groupName} with settings ${names.join(", ")}`);
    let b = new flatbuffers.Builder(1024);
    let stringsOffset: number[] = [];
    names.forEach((v) => stringsOffset.push(b.createString(v)));
    let settingsOffset = ResponseSetUserSettings.createSettingKeysVector(b, stringsOffset);
    let r = ResponseSetUserSettings.createResponseSetUserSettings(b, b.createString(groupName), settingsOffset);
    b.finish(ResponseWrapper.createResponseWrapper(b, Responses.ResponseSetUserSettings, r));
    ws.send(b.asUint8Array());
}

function sendResponseJournal(ws: WebSocket) {
    let b = new flatbuffers.Builder(1024);
    let itemsOffset = ResponseJournal.createJournalItemsVector(b, [
        JournalItem.createJournalItem(b, 22n, 1,b.createString("I2C_COMM"), 0, counter),
        JournalItem.createJournalItem(b, 222n, 2,b.createString("SPI_COMM"), 0, 1),
        JournalItem.createJournalItem(b, 2222n, 3,b.createString("I2S_COMM"), counter, 1),
        JournalItem.createJournalItem(b, 22222n, 4, b.createString("ETH_COMM"), 0, 1),
    ]);

    counter++;

    let r = ResponseJournal.createResponseJournal(b, itemsOffset);
    b.finish(ResponseWrapper.createResponseWrapper(b, Responses.ResponseJournal, r));
    ws.send(b.asUint8Array());
}

function sendResponseTimeseries(ws: WebSocket, req:RequestTimeseries) {
    ws.send(createTimeseries(req.granularity()));
}

function sendResponseSystemData(ws: WebSocket) {
    let b = new flatbuffers.Builder(1024);
    var partitionsOffset = new Array<number>();
    partitionsOffset.push(PartitionInfo.createPartitionInfo(b, b.createString("Label0"), 0, 0x10, 3072, 1, true, b.createString("AppName"), b.createString("AppVersion"), b.createString("AppDate"), b.createString("AppTime")));
    partitionsOffset.push(PartitionInfo.createPartitionInfo(b, b.createString("Label1"), 1, 0x01, 16384, 1, true, b.createString("AppName"), b.createString("AppVersion"), b.createString("AppDate"), b.createString("AppTime")));
    ResponseSystemData.startResponseSystemData(b);
    ResponseSystemData.addChipCores(b, 2);
    ResponseSystemData.addChipFeatures(b, 255);
    ResponseSystemData.addPartitions(b, ResponseSystemData.createPartitionsVector(b, partitionsOffset));
    ResponseSystemData.addChipModel(b, 2);
    ResponseSystemData.addChipRevision(b, 3);
    ResponseSystemData.addChipTemperature(b, 23.4);
    ResponseSystemData.addFreeHeap(b, 1203);
    ResponseSystemData.addMacAddressBt(b, Mac6.createMac6(b, [1, 2, 3, 4, 5, 6]));
    ResponseSystemData.addMacAddressEth(b, Mac6.createMac6(b, [1, 2, 3, 4, 5, 6]));
    ResponseSystemData.addMacAddressIeee802154(b, Mac6.createMac6(b, [1, 2, 3, 4, 5, 6]));
    ResponseSystemData.addMacAddressWifiSoftap(b, Mac6.createMac6(b, [1, 2, 3, 4, 5, 6]));
    ResponseSystemData.addMacAddressWifiSta(b, Mac6.createMac6(b, [1, 2, 3, 4, 5, 6]));
    ResponseSystemData.addSecondsEpoch(b, BigInt(Math.floor(new Date().getTime() / 1000)));
    ResponseSystemData.addSecondsUptime(b, BigInt(10));
    let rsd = ResponseSystemData.endResponseSystemData(b);
    b.finish(ResponseWrapper.createResponseWrapper(b, Responses.ResponseSystemData, rsd));
    ws.send(b.asUint8Array());
}

function process(buffer: Buffer, ws: WebSocket) {
    let data = new Uint8Array(buffer);
    let b_input = new flatbuffers.ByteBuffer(data);
    let b = new flatbuffers.Builder(1024);
    let mw = RequestWrapper.getRootAsRequestWrapper(b_input);
    console.log(`Received buffer length ${buffer.byteLength} and Type is ${mw.requestType()}`);
    switch (mw.requestType()) {
        case Requests.RequestNetworkInformation:
            setTimeout(() => { sendResponseWifiAccesspoints(ws); }, 500);
            break;
        case Requests.RequestSystemData:
            setTimeout(() => { sendResponseSystemData(ws); }, 500);
            break;
        case Requests.RequestGetUserSettings:
            setTimeout(() => { sendResponseGetUserSettings(ws, <RequestGetUserSettings>mw.request(new RequestGetUserSettings())); }, 500);
            break;
        case Requests.RequestSetUserSettings:
            setTimeout(() => { sendResponseSetUserSettings(ws, <RequestSetUserSettings>mw.request(new RequestSetUserSettings())); }, 100);
            break;
        case Requests.RequestJournal:
            setTimeout(() => { sendResponseJournal(ws);}, 100);
            break;
        case Requests.RequestTimeseries:
            setTimeout(()=>{sendResponseTimeseries(ws, <RequestTimeseries>mw.request(new RequestTimeseries())), 200});
            break;
        case Requests.RequestWifiConnect:{
            setTimeout(()=>{sendResponseWifiConnectionSuccessOrFailed(ws, <RequestWifiConnect>mw.request(new RequestWifiConnect()));}, 3000);
            break;
        }
        case Requests.RequestFingerprintSensorInfo:
            sendResponseFingerprintSensorInfo(ws)
            break;
        case Requests.RequestFingers:
            sendResponseFingers(ws)
            break;
        case Requests.RequestStoreFingerAction:
            processRequestStoreFingerAction(ws, <RequestStoreFingerAction>mw.request(new RequestStoreFingerAction)); 
            break;
        case Requests.RequestStoreFingerSchedule:
            processRequestStoreFingerSchedule(ws, <RequestStoreFingerSchedule>mw.request(new RequestStoreFingerSchedule)); 
            break
        case Requests.RequestEnrollNewFinger:
            setTimeout(()=>sendResponseEnrollNewFinger(ws, <RequestEnrollNewFinger>mw.request(new RequestEnrollNewFinger())), 100);
            break;
        case Requests.scheduler_RequestScheduler:
            setTimeout(()=>processScheduler_RequestScheduler(ws, <RequestScheduler>mw.request(new RequestScheduler())), 100);
            break;
        case Requests.websensact_RequestStatus:
            setTimeout(()=>websensact_requestStatus(ws, <RequestStatus>mw.request(new RequestStatus())), 100);
        default:
            break;
    }
}

//let hostCert =fs.readFileSync("./../certificates/testserver.pem.crt").toString();
//let hostPrivateKey = fs.readFileSync("./../certificates/testserver.pem.prvtkey").toString();

class Foo implements ISensactContext{

}

BuildWebsensact();

let server = http.createServer((req, res) => {
//let server = https.createServer({key: hostPrivateKey, cert: hostCert}, (req, res) => {
    console.log(`Request received for '${req.url}'`);
    //var local_path = new URL(req.url).pathname;

    if (req.url == "/sensact") {
        const req_body_chunks: any[] = [];
        req.on("data", (chunk: any) => req_body_chunks.push(chunk));
        req.on("end", () => {
            //process(res, req.url!, Buffer.concat(req_body_chunks));
        });

    } else if (req.url == "/") {
        fs.readFile(BUNDLED_FILE, (err, data) => {
            res.writeHead(200, { 'Content-Type': 'text/html', "Content-Encoding": "br" });
            res.end(data);
        });
    } else {
        res.writeHead(404);
        res.end("Not found");
        return;
    }
});

server.on('upgrade', (req, sock, head) => {
    if (req.url == '/webmanager_ws') {
        console.info("Handle upgrade to websocket");
        wss.handleUpgrade(req, sock, head, ws => wss.emit('connection', ws, req));
    } else {
        sock.destroy();
    }
});

var messageChanger = 0;

wss.on('connection', (ws) => {
    console.info("Handle connection");
    ws.on('error', console.error);

    ws.on('message', (data: Buffer, isBinary: boolean) => {
        
        process(data, ws);
    });
});

server.on("error",(e)=>{
    console.error(e);
})

server.listen(PORT, () => {
    console.log(`Server is running on port ${PORT}`);
    const interval = setInterval(() => {
        let b = new flatbuffers.Builder(1024);
        let mw: number;
        switch (messageChanger) {
            case 0:
                let text = b.createString(`Dies ist eine Meldung ${new Date().getTime()}`);
                NotifyLiveLogItem.startNotifyLiveLogItem(b);
                NotifyLiveLogItem.addText(b, text);
                let li = NotifyLiveLogItem.endNotifyLiveLogItem(b);
                mw = ResponseWrapper.createResponseWrapper(b, Responses.NotifyLiveLogItem, li);
                b.finish(mw);
                //wss.clients.forEach(ws => ws.send(b.asUint8Array()));
                break;
            default:
                break;
        }
        messageChanger++;
        messageChanger %= 3;

    }, 1000);

});




