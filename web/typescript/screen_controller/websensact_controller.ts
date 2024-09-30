import {ResponseWrapper, Responses } from "../../generated/flatbuffers/webmanager";
import BuildApps from "../../generated/sensact/sensactapps_copied_during_build";
import { ScreenController } from "./screen_controller";
import { TemplateResult, html, render } from "lit-html";
import { Ref, createRef, ref } from "lit-html/directives/ref.js";
import { ApplicationGroup, SensactApplication } from "../utils/sensactapps_base";

import bed from '../../svgs/solid/bed.svg?raw'
import layer_group from '../../svgs/solid/layer-group.svg?raw'
import lightbulb from '../../svgs/solid/lightbulb.svg?raw'
import { unsafeSVG } from "lit-html/directives/unsafe-svg.js";
import { GetLevelFromApplicationId, GetRoomFromApplicationId, GetTechnologyFromApplicationId } from "../utils/sensact";

import "../utils/extensions";
import { RequestCommand } from "../../generated/flatbuffers/websensact/request-command";
import { NotifyStatus } from "../../generated/flatbuffers/websensact/notify-status";
import { ResponseCommand } from "../../generated/flatbuffers/websensact/response-command";
import { ApplicationId } from "../../generated/flatbuffers/application-id";
import { ResponseStatus } from "../../generated/flatbuffers/websensact/response-status";

export class WebsensactController extends ScreenController{
    
    private btnSortTechnology() {
        var tech2apps = new Map<string, Array<SensactApplication>>()
        for (const app of this.sensactApps.values()) {
            var k=GetTechnologyFromApplicationId(app.applicationId);
            var arr=tech2apps.getOrAdd(k, ()=>new Array<SensactApplication>())
            arr.push(app);
        }
        var sortedMap = new Map([...tech2apps.entries()].sort((a,b)=>a[0].localeCompare(b[0])));
        var templates= new Array<TemplateResult<1>>();
        sortedMap.forEach((v,k)=>{
            var group = new ApplicationGroup(k, this.appManagement, v, k);
            templates.push(group.Template());
        });
        render(templates, this.mainElement.value!)
    }
    private btnSortRooms() {
        var level_room2apps = new Map<string, Array<SensactApplication>>()
        for (const app of this.sensactApps.values()) {
            var room_level=GetRoomFromApplicationId(app.applicationId)+"_"+GetLevelFromApplicationId(app.applicationId);
            var arr=level_room2apps.getOrAdd(room_level, ()=>new Array<SensactApplication>())
            arr.push(app);
        }
        var sortedMap = new Map([...level_room2apps.entries()].sort((a,b)=>a[0].localeCompare(b[0])));
        var templates= new Array<TemplateResult<1>>();
        sortedMap.forEach((v,k)=>{
            var group = new ApplicationGroup(k, this.appManagement, v, k);
            templates.push(group.Template());
        });
        render(templates, this.mainElement.value!)
    }
   
    private mainElement:Ref<HTMLElement>= createRef();
    public Template = () => html`
    <h1>Sensact Controls</h1>
        
    <div class="buttons">
        <button class="withsvg" @click=${() => this.btnSortRooms()}>${unsafeSVG(bed)}<span>Sort Rooms<span></button>
        <button class="withsvg" @click=${() => this.btnSortTechnology()}>${unsafeSVG(lightbulb)}<span>Sort Tech<span></button>
    </div>
    <section ${ref(this.mainElement)}></section>`
    
    private sensactApps:Map<string, SensactApplication>;


    public onMessage(messageWrapper: ResponseWrapper): void {
        switch (messageWrapper.responseType()) {
            case Responses.websensact_ResponseCommand:
                this.onResponseCommand(<ResponseCommand>messageWrapper.response(new ResponseCommand()));
                break;
            case Responses.websensact_NotifyStatus:
                this.onNotifyStatus(<NotifyStatus>messageWrapper.response(new NotifyStatus()));
                break;
            case Responses.websensact_ResponseStatus:
                this.onResponseStatus(<ResponseStatus>messageWrapper.response(new ResponseStatus()));
                break;
            default:
                break;
        }
    }
    private onResponseCommand(m: ResponseCommand) {
        console.log("Command confirmed");
    }
    private onNotifyStatus(m: NotifyStatus) {
        var app=this.sensactApps.get(ApplicationId[m.id()]);
        if(!app) return;
        app.UpdateState(m.status());
    }

    private onResponseStatus(m:ResponseStatus){
        for(var i=0; i<m.statesLength();i++){
            var app = this.sensactApps.get(ApplicationId[ m.states(i).id()])
            if(!app) return;
            app.UpdateState(m.states(i).status());
        }
    }
    
    
    onCreate(): void {
        this.appManagement.registerWebsocketMessageTypes(this, Responses.websensact_NotifyStatus, Responses.websensact_ResponseCommand, Responses.websensact_ResponseStatus);
        this.sensactApps=new Map<string, SensactApplication>(BuildApps().map(v=>[ApplicationId[v.applicationId], v])); 
       
    }

    private onStart_or_onRestart(){
        this.btnSortRooms();
    }
   
    onFirstStart(): void {
        this.onStart_or_onRestart();
    }
    onRestart(): void {
        this.onStart_or_onRestart();
    }
    onPause(): void {
    }

}
