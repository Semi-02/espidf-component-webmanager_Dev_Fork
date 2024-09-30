import { Ref, createRef, ref } from "lit-html/directives/ref.js";
import { RequestDeleteFinger, RequestWrapper, Requests, ResponseWrapper, Responses } from "../../generated/flatbuffers/webmanager";
import { ScreenController } from "./screen_controller";
import * as flatbuffers from 'flatbuffers';
import { TemplateResult, html, render } from "lit-html";


import calendarPlus from '../../svgs/regular/calendar-plus.svg?raw'
import rotate from '../../svgs/solid/rotate.svg?raw'
import { unsafeSVG } from "lit-html/directives/unsafe-svg.js";
import { ResponseScheduler } from "../../generated/flatbuffers/scheduler/response-scheduler";
import { uResponseScheduler } from "../../generated/flatbuffers/scheduler/u-response-scheduler";
import { ResponseSchedulerList } from "../../generated/flatbuffers/scheduler/response-scheduler-list";
import { ResponseSchedulerListItem } from "../../generated/flatbuffers/scheduler/response-scheduler-list-item";
import { RequestScheduler } from "../../generated/flatbuffers/scheduler/request-scheduler";
import { uRequestScheduler } from "../../generated/flatbuffers/scheduler/u-request-scheduler";
import { eSchedule } from "../../generated/flatbuffers/scheduler/e-schedule";
import { RequestSchedulerList } from "../../generated/flatbuffers/scheduler/request-scheduler-list";
import { ResponseSchedulerOpen } from "../../generated/flatbuffers/scheduler/response-scheduler-open";
import {ScheduleItem, OneWeekIn15MinutesSchedule, SunRandomSchedule, PredefinedSchedule} from "../utils/schedule_items"
import {CreateNewScheduleDialog} from "../dialog_controller/CreateNewScheduleDialog.ts"
import { RequestSchedulerDelete } from "../../generated/flatbuffers/scheduler/request-scheduler-delete.ts";


export class SchedulerScreenController extends ScreenController {
    private btnNew() {
        this.appManagement.showDialog(new CreateNewScheduleDialog(Array.from(this.name2item.keys()), this.appManagement, (ok, item)=>{
            if(!ok) return;
            item.OnCreate()
            this.name2item.set(item.name, item);
            var itemTemplates:Array<TemplateResult<1>>=[];
            for(const i of this.name2item.values()){
                itemTemplates.push(i.OverallTemplate())
            }
            render(itemTemplates, this.tBodySchedules!.value);
            
        }));
    }

    private btnUpdate(){
        this.onStart_or_onRestart_or_onUpdateClicked();
    }

    private tBodySchedules:Ref<HTMLTableSectionElement>= createRef();
    private name2item:Map<string, ScheduleItem>=new Map<string, ScheduleItem>();

    public Template = () =>{
        return html`
    <h1>Schedule Definitions</h1>
        
        <div class="buttons">
            <button class="withsvg" @click=${() => this.btnNew()}>${unsafeSVG(calendarPlus)}<span>New<span></button>
            <button class="withsvg" @click=${() => this.btnUpdate()}>${unsafeSVG(rotate)}<span>Update List<span></button>
        </div>
        

        <table class="schedules">
            <thead>
            <tr>
                <th>Name</th>
                <th>Type</th>
                <th>Edit</th>
            </tr>
            </thead>
            <tbody ${ref(this.tBodySchedules)}>
            </tbody>
        </table>
        `}

    //jede Tabellenzeile hat einen Button "Rename" und einen Button "Delete"
    //im Property-Speicher des ESP32 wird abgelegt, welche Bezeichnung zu welcher internen Nummer gehört
    //Das Anlegen eines Eintrages findet ausschließlich über die Automatische Nummerierung statt
    //in der Tabelle wird auch die interne Speichernummer angezeigt



    public sendRequestDeleteSchedule(name: string) {
        let b = new flatbuffers.Builder(1024);
        this.appManagement.WrapAndFinishAndSend(b,
            Requests.scheduler_RequestScheduler,
            RequestScheduler.createRequestScheduler(b,
                uRequestScheduler.RequestSchedulerDelete,
                RequestSchedulerDelete.createRequestSchedulerDelete(b, b.createString(name))
            ),
            [Responses.scheduler_ResponseScheduler]
        );
    }

    onMessage(messageWrapper: ResponseWrapper): void {
        if(messageWrapper.responseType() != Responses.scheduler_ResponseScheduler)
            return;
        let m = <ResponseScheduler>messageWrapper.response(new ResponseScheduler());
        console.log(`Received "${uResponseScheduler[m.contentType()]}"-message in scheduler`)
        switch (m.contentType()) {
            case uResponseScheduler.ResponseSchedulerList:{
                var itemTemplates:Array<TemplateResult<1>>=[];
                var list = <ResponseSchedulerList>m.content(new ResponseSchedulerList())
                for(var i=0;i<list.itemsLength();i++){
                    var item = list.items(i);
                    this.processItem(item, itemTemplates);
                }
                render(itemTemplates, this.tBodySchedules!.value);
                break;
            }
            case uResponseScheduler.ResponseSchedulerOpen:{
                var open = <ResponseSchedulerOpen>m.content(new ResponseSchedulerOpen())
                if(!this.name2item.has(open.payload().name())){
                    console.warn(`OpenMessage for ${open.payload().name()}, but this is not contained in [${Array.from(this.name2item.keys()).join(",")}].`)
                }else{
                    var o =this.name2item.get(open.payload().name())
                    o.OnResponseSchedulerOpen(open);
                }
                break;
            }
            default:
                break;
        }
        
    }
    processItem(item: ResponseSchedulerListItem, itemTemplates: TemplateResult<1>[]) {
        var elem:ScheduleItem=null;
        switch (item.type()) {
            case eSchedule.OneWeekIn15Minutes:{
                elem= new OneWeekIn15MinutesSchedule(item.name(), this.appManagement)
                break;
            }
            case eSchedule.SunRandom:{
                elem= new SunRandomSchedule(item.name(), this.appManagement)
                break;
            }
            case eSchedule.Predefined:{
                elem= new PredefinedSchedule(item.name(), this.appManagement)
                break;
            }
            default:
                return;
        }
        this.name2item.set(item.name(), elem);
        itemTemplates.push(elem.OverallTemplate())
    }

    private onStart_or_onRestart_or_onUpdateClicked(){
        let b = new flatbuffers.Builder(256);       
        this.appManagement.WrapAndFinishAndSend(b,
            Requests.scheduler_RequestScheduler,
            RequestScheduler.createRequestScheduler(b, 
                uRequestScheduler.RequestSchedulerList, 
                RequestSchedulerList.createRequestSchedulerList(b)
            ),
            [Responses.scheduler_ResponseScheduler]
        );
    }
    
    onCreate(): void {
        this.appManagement.registerWebsocketMessageTypes(this, Responses.scheduler_ResponseScheduler);
    }


    onFirstStart(): void {
        this.onStart_or_onRestart_or_onUpdateClicked()
    }
    onRestart(): void {
        this.onStart_or_onRestart_or_onUpdateClicked()
    }
    onPause(): void {
    }

}
