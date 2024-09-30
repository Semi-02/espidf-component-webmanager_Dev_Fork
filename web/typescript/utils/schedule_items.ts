import { html, TemplateResult } from "lit-html";
import {IAppManagement} from "./interfaces"
import { RequestSchedulerOpen } from "../../generated/flatbuffers/scheduler/request-scheduler-open";
import { eSchedule } from "../../generated/flatbuffers/scheduler/e-schedule";
import { OneWeekIn15Minutes } from "../../generated/flatbuffers/scheduler/one-week-in15-minutes";
import { RequestScheduler } from "../../generated/flatbuffers/scheduler/request-scheduler";
import { ResponseSchedulerOpen } from "../../generated/flatbuffers/scheduler/response-scheduler-open";
import { SunRandom } from "../../generated/flatbuffers/scheduler/sun-random";
import { uRequestScheduler } from "../../generated/flatbuffers/scheduler/u-request-scheduler";
import { uSchedule } from "../../generated/flatbuffers/scheduler/u-schedule";
import { Requests, Responses } from "../../generated/flatbuffers/webmanager";
import * as flatbuffers from 'flatbuffers';
import { RequestSchedulerSave } from "../../generated/flatbuffers/scheduler/request-scheduler-save";
import { Schedule } from "../../generated/flatbuffers/scheduler/schedule";
import { OneWeekIn15MinutesData } from "../../generated/flatbuffers/scheduler/one-week-in15-minutes-data";
import { iSunRandomDialogHandler, iWeeklyScheduleDialogHandler } from "../dialog_controller/weeklyschedule_dialog";
import { numberArray2HexString } from "./common";
import { RequestSchedulerDelete } from "../../generated/flatbuffers/scheduler/request-scheduler-delete";
import { SunRandomScheduleEditorDialog } from "../dialog_controller/SunRandomScheduleEditorDialog";
import { RequestSchedulerRename } from "../../generated/flatbuffers/scheduler/request-scheduler-rename";


export abstract class ScheduleItem {

    constructor(public readonly name:string, protected readonly type:string, protected readonly appManagement:IAppManagement) { }
    
    public OverallTemplate=()=>html`
    <tr>
        <td class="minwidth">${this.name}</td>
        <td class="minwidth">${this.type}</td>
        <td>${this.CoreEditTemplate()}</td>
    </tr>
    `
    protected abstract CoreEditTemplate:()=>TemplateResult<1>;
    public abstract OnResponseSchedulerOpen(m:ResponseSchedulerOpen):void;
    public abstract OnCreate():void;
    public abstract SaveToServer():void;
}

export class PredefinedSchedule extends ScheduleItem{
    public OnCreate(): void {
        throw new Error("PredefinedSchedule may not be created");
    }
    constructor(name:string, appManagement:IAppManagement){
        super(name, "Predefined", appManagement);
    }

    public OnResponseSchedulerOpen(m:ResponseSchedulerOpen):void{
        return;
    }

    protected CoreEditTemplate=()=>html``

    public SaveToServer():void{
        return
    }
}

export class SunRandomSchedule extends ScheduleItem implements iSunRandomDialogHandler{
    private offsetMinutes: number;
    private randomMinutes: number;
    constructor(name:string, appManagement:IAppManagement){
        super(name, "SunRandom", appManagement);
    }

    public SaveToServer():void{
        let b = new flatbuffers.Builder(1024);       
        this.appManagement.WrapAndFinishAndSend(b, 
            Requests.scheduler_RequestScheduler,
            RequestScheduler.createRequestScheduler(b, 
                uRequestScheduler.RequestSchedulerSave, 
                RequestSchedulerSave.createRequestSchedulerSave(b, 
                    Schedule.createSchedule(b,
                        b.createString(this.name), 
                        uSchedule.SunRandom,
                        SunRandom.createSunRandom(b,
                            this.offsetMinutes,
                            this.randomMinutes
                        )
                    )
                )
            ),
            [Responses.scheduler_ResponseScheduler]
        )
    }

    public OnResponseSchedulerOpen(m:ResponseSchedulerOpen):void{
       
        if(m.payload().scheduleType()!=uSchedule.SunRandom)
            return;
        if(m.payload().name()!=this.name){
            console.error("m.payload().name()!=this.name")
            return;
        }
        var rso = <SunRandom>m.payload().schedule(new SunRandom())
        this.offsetMinutes=rso.offsetMinutes();
        this.randomMinutes=rso.randomMinutes();
        this.appManagement.showDialog(new SunRandomScheduleEditorDialog(this.name, this.offsetMinutes, this.randomMinutes, this))
    }

    public OnCreate(): void {
        this.offsetMinutes=15
        this.randomMinutes=15
        this.SaveToServer()
    }

    public handleSunRandomDialog(ok: boolean, offsetMinutes: number, randomMinutes: number){
        console.info("In SunRandomSchedule: Dialog closed");
        if(!ok) return;
        this.offsetMinutes=offsetMinutes;
        this.randomMinutes=randomMinutes;
        this.SaveToServer()
    }

    private btnEditClicked() {
        let b = new flatbuffers.Builder(256);       
        this.appManagement.WrapAndFinishAndSend(b, 
            Requests.scheduler_RequestScheduler,
            RequestScheduler.createRequestScheduler(b, 
                uRequestScheduler.RequestSchedulerOpen, 
                RequestSchedulerOpen.createRequestSchedulerOpen(b, b.createString(this.name), eSchedule.SunRandom)
            ),
            [Responses.scheduler_ResponseScheduler]
        )
    }

    private btnDeleteClicked() {
        let b = new flatbuffers.Builder(256);       
        this.appManagement.WrapAndFinishAndSend(b, 
            Requests.scheduler_RequestScheduler,
            RequestScheduler.createRequestScheduler(b, 
                uRequestScheduler.RequestSchedulerDelete, 
                RequestSchedulerDelete.createRequestSchedulerDelete(b, b.createString(this.name))
            ),
            [Responses.scheduler_ResponseScheduler]
        )
    }

    private handleRenameDialog(ok:boolean, newName:string){
        if(!ok) return;
        let b = new flatbuffers.Builder(256);       
        this.appManagement.WrapAndFinishAndSend(b, 
            Requests.scheduler_RequestScheduler,
            RequestScheduler.createRequestScheduler(b, 
                uRequestScheduler.RequestSchedulerRename, 
                RequestSchedulerRename.createRequestSchedulerRename(b, b.createString(this.name), b.createString(newName))
            ),
            [Responses.scheduler_ResponseScheduler]
        )
    }

    private btnRenameClicked() {
        this.appManagement.showEnterFilenameDialog("Enter new name", (ok, value)=>this.handleRenameDialog(ok, value));
    }

    protected CoreEditTemplate=()=>html`<button @click=${()=>this.btnEditClicked()}>Edit</button><button @click=${()=>this.btnDeleteClicked()}>Delete</button><button @click=${()=>this.btnRenameClicked()}>Rename</button>`
}

export class OneWeekIn15MinutesSchedule extends ScheduleItem implements iWeeklyScheduleDialogHandler{
    private value:Array<number>=[]
    
    constructor(name:string, appManagement:IAppManagement){
        super(name, "OneWeekIn15Minutes", appManagement);
    }
    
       

    public OnResponseSchedulerOpen(m:ResponseSchedulerOpen|null):void{
        if(m.payload().scheduleType()!=uSchedule.OneWeekIn15Minutes)
            return;
        if(m.payload().name()!=this.name){
            console.error("m.payload().name()!=this.name")
            return;
        }
        this.value = new Array<number>(84);
        var rso = <OneWeekIn15Minutes>m.payload().schedule(new OneWeekIn15Minutes())
        for(var i=0;i<84;i++){this.value[i]=rso.data()!.v(i)!}
        this.appManagement.showWeeklyTimetableDialog(`Weekly Schedule ${this.name}`, this.value, this, null);
    }

    public OnCreate(): void {
        this.value = new Array<number>(84).fill(0xFF)
        this.SaveToServer()
    }

    public handleWeeklyScheduleDialog(ok: boolean, referenceHandle: any, value: Array<number>) {
        if(!ok) return
        console.info(`In OneWeekIn15MinutesSchedule: WeeklyScheduleDialog closed with ok, data = ${numberArray2HexString(value)}`)
        this.value=value
        this.SaveToServer()
    }

    public SaveToServer():void{
        let b = new flatbuffers.Builder(1024)
        this.appManagement.WrapAndFinishAndSend(b,
            Requests.scheduler_RequestScheduler,
            RequestScheduler.createRequestScheduler(b, 
                uRequestScheduler.RequestSchedulerSave, 
                RequestSchedulerSave.createRequestSchedulerSave(b, 
                    Schedule.createSchedule(b,
                        b.createString(this.name), 
                        uSchedule.OneWeekIn15Minutes,
                        OneWeekIn15Minutes.createOneWeekIn15Minutes(b,
                            OneWeekIn15MinutesData.createOneWeekIn15MinutesData(b, this.value)
                        )
                    )
                )
            ),
            [Responses.scheduler_ResponseScheduler]
        )
    }

    private btnEditClicked(e:MouseEvent){

        let b = new flatbuffers.Builder(256);       
        this.appManagement.WrapAndFinishAndSend(b,
            Requests.scheduler_RequestScheduler,
            RequestScheduler.createRequestScheduler(b, 
                uRequestScheduler.RequestSchedulerOpen, 
                RequestSchedulerOpen.createRequestSchedulerOpen(b, b.createString(this.name), eSchedule.OneWeekIn15Minutes)
            ),
            [Responses.scheduler_ResponseScheduler]
        )
    }

    private btnDeleteClicked(_e:MouseEvent) {
        let b = new flatbuffers.Builder(256);       
        this.appManagement.WrapAndFinishAndSend(b, 
            Requests.scheduler_RequestScheduler,
            RequestScheduler.createRequestScheduler(b, 
                uRequestScheduler.RequestSchedulerDelete, 
                RequestSchedulerDelete.createRequestSchedulerDelete(b, b.createString(this.name))
            ),
            [Responses.scheduler_ResponseScheduler]
        )
    }

    private handleRenameDialog(ok:boolean, newName:string){
        if(!ok) return;
        let b = new flatbuffers.Builder(256);       
        this.appManagement.WrapAndFinishAndSend(b, 
            Requests.scheduler_RequestScheduler,
            RequestScheduler.createRequestScheduler(b, 
                uRequestScheduler.RequestSchedulerRename, 
                RequestSchedulerRename.createRequestSchedulerRename(b, b.createString(this.name), b.createString(newName))
            ),
            [Responses.scheduler_ResponseScheduler]
        )
    }

    private btnRenameClicked(_e:MouseEvent) {
        this.appManagement.showEnterFilenameDialog("Enter new name", (ok, value)=>this.handleRenameDialog(ok, value));
    }

    protected CoreEditTemplate=()=>html`<button @click=${(e:MouseEvent)=>this.btnEditClicked(e)}>Edit</button><button @click=${(e:MouseEvent)=>this.btnDeleteClicked(e)}>Delete</button><button @click=${(e:MouseEvent)=>this.btnRenameClicked(e)}>Rename</button>`
}