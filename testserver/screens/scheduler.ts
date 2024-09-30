import { WebSocket } from "ws"
import * as flatbuffers from "flatbuffers"
import { RequestScheduler } from "../generated/flatbuffers/scheduler/request-scheduler";
import { RequestSchedulerOpen } from "../generated/flatbuffers/scheduler/request-scheduler-open";
import { uRequestScheduler } from "../generated/flatbuffers/scheduler/u-request-scheduler";
import { eSchedule } from "../generated/flatbuffers/scheduler/e-schedule";
import { ResponseSchedulerOpen } from "../generated/flatbuffers/scheduler/response-scheduler-open";
import { OneWeekIn15Minutes } from "../generated/flatbuffers/scheduler/one-week-in15-minutes";
import { OneWeekIn15MinutesData, RequestSchedulerSave, ResponseScheduler, ResponseSchedulerList, ResponseSchedulerListItem, ResponseSchedulerSave, Schedule, SunRandom, uResponseScheduler, uSchedule } from "../generated/flatbuffers/scheduler";
import { ResponseWrapper, Responses } from "../generated/flatbuffers/webmanager";


export var exampleSchedules:Map<string, any>=new Map<string, any>()

exampleSchedules.set("OneWeekIn15MinutesA", {type:eSchedule.OneWeekIn15Minutes, data:new Array<number>(84).fill(0x55)})
exampleSchedules.set("OneWeekIn15MinutesB",{type:eSchedule.OneWeekIn15Minutes, data:new Array<number>(84).fill(0xAA)})
exampleSchedules.set("PredefinedA",{type:eSchedule.Predefined})
exampleSchedules.set("PredefinedB",{type:eSchedule.Predefined})
exampleSchedules.set("SunRandomA",{type:eSchedule.SunRandom, offsetMinutes:15, randomMinutes:15})
exampleSchedules.set("SunRandomB",{type:eSchedule.SunRandom, offsetMinutes:30, randomMinutes:30})

function numberArray2HexString(d: Array<number>) {
    var s = "";
    for (let index = 0; index < d.length; index++) {
      var xx = d[index].toString(16);
      if (xx.length == 1) s += "0" + xx;
      else s += xx;
    }
    return s;
  }

export function processScheduler_RequestScheduler(ws: WebSocket, m: RequestScheduler) {
    console.info(`processScheduler_RequestScheduler`);
    switch (m.contentType()) {
        case uRequestScheduler.RequestSchedulerList: {
            console.info(`processScheduler_RequestScheduler->uRequestScheduler.RequestSchedulerList`);
            let b = new flatbuffers.Builder(1024);
            var data: number[]=[];
            exampleSchedules.forEach((v,k) => {
                data.push(ResponseSchedulerListItem.createResponseSchedulerListItem(b, b.createString(k), v.type))
            });
            let itemsOffset = ResponseSchedulerList.createItemsVector(b, data);

            b.finish(
                ResponseWrapper.createResponseWrapper(b, Responses.scheduler_ResponseScheduler,
                    ResponseScheduler.createResponseScheduler(b, uResponseScheduler.ResponseSchedulerList,
                        ResponseSchedulerList.createResponseSchedulerList(b, itemsOffset)
                    )
                )
            );
            ws.send(b.asUint8Array());
            break;
        }
        case uRequestScheduler.RequestSchedulerSave: {
            let b = new flatbuffers.Builder(1024);
            let ms = <RequestSchedulerSave>m.content(new RequestSchedulerSave())
            var p=ms.payload();
            switch(p.scheduleType()){
                case uSchedule.OneWeekIn15Minutes:
                    var ow=<OneWeekIn15Minutes>p.schedule(new OneWeekIn15Minutes())
                    var d=new Array<number>()
                    for(var i=0;i<84;i++)d.push(ow.data().v(i))
                    console.log(`Got OneWeekIn15Minutes "${p.name()}". Data bytes are ${numberArray2HexString(d)}`) 
                    exampleSchedules.set(p.name(),{type:eSchedule.OneWeekIn15Minutes, data:d})
                    break;
                case uSchedule.SunRandom:
                    var sr=<SunRandom>p.schedule(new SunRandom())
                    exampleSchedules.set(p.name(),{type:eSchedule.OneWeekIn15Minutes, offsetMinutes:sr.offsetMinutes(), randomMinutes:sr.randomMinutes()})
                    break;
            }
            b.finish(
                ResponseWrapper.createResponseWrapper(b, Responses.scheduler_ResponseScheduler,
                    ResponseScheduler.createResponseScheduler(b, uResponseScheduler.ResponseSchedulerSave,
                        ResponseSchedulerSave.createResponseSchedulerSave(b, b.createString(p.name()))
                    )
                )
            );
            ws.send(b.asUint8Array());
            
        }
        case uRequestScheduler.RequestSchedulerOpen: {
            let b = new flatbuffers.Builder(1024);
            let mo = <RequestSchedulerOpen>m.content(new RequestSchedulerOpen());
            var s=exampleSchedules.get(mo.name())
            if(s===undefined) return;
            switch(mo.type()){
                case eSchedule.OneWeekIn15Minutes:{      
                    console.log(`Send OneWeekIn15Minutes "${mo.name()}". Data bytes are ${numberArray2HexString(s.data)}`)              
                    b.finish(
                        ResponseWrapper.createResponseWrapper(b, Responses.scheduler_ResponseScheduler,
                            ResponseScheduler.createResponseScheduler(b, uResponseScheduler.ResponseSchedulerOpen,
                                ResponseSchedulerOpen.createResponseSchedulerOpen(b,
                                    Schedule.createSchedule(b, 
                                        b.createString(mo.name()),
                                        uSchedule.OneWeekIn15Minutes, 
                                        OneWeekIn15Minutes.createOneWeekIn15Minutes(b,
                                            OneWeekIn15MinutesData.createOneWeekIn15MinutesData(b, s.data)
                                        )
                                    )
                                )
                            )
                        )
                    );
                    ws.send(b.asUint8Array());
                    break
                }
                case eSchedule.SunRandom:{
                    b.finish(
                        ResponseWrapper.createResponseWrapper(b, Responses.scheduler_ResponseScheduler,
                            ResponseScheduler.createResponseScheduler(b, uResponseScheduler.ResponseSchedulerOpen,
                                ResponseSchedulerOpen.createResponseSchedulerOpen(b,
                                    Schedule.createSchedule(b, 
                                        b.createString(mo.name()),
                                        uSchedule.SunRandom,
                                        SunRandom.createSunRandom(b,s.offsetMinutes, s.randomMinutes)
                                    )
                                )
                            )
                        )
                    );
                    ws.send(b.asUint8Array());
                    break
                }
            }
            
            break;
        }
    }
}