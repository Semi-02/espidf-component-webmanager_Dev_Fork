import { TemplateResult, html } from "lit-html";
import { ResponseWrapper } from "../../generated/flatbuffers/webmanager";
import { IAppManagement, IWebsocketMessageListener } from "../utils/interfaces";

export enum ControllerState {
    CREATED,
    STARTED,
    PAUSED,
}

export abstract class ScreenController implements IWebsocketMessageListener {
    private state=ControllerState.CREATED
    constructor(protected appManagement: IAppManagement) {}
    public abstract onCreate(): void;
    public onStartPublic(){
        switch (this.state) {
            case ControllerState.CREATED:
                this.onFirstStart();
                this.state=ControllerState.STARTED;
                break;
            case ControllerState.STARTED:
                break;
            case ControllerState.PAUSED:
                this.onRestart();
                this.state=ControllerState.STARTED;
                break;
            default:
                break;
        }
    }
    public onPausePublic(){
        switch (this.state) {
            case ControllerState.CREATED:
                break;
            case ControllerState.STARTED:
                this.onPause();
                this.state=ControllerState.PAUSED;
                break;
            case ControllerState.PAUSED:
                break;
            default:
                break;
        }
    }
    protected abstract onFirstStart(): void;
    protected abstract onRestart(): void;
    abstract onPause(): void;
    abstract onMessage(messageWrapper:ResponseWrapper):void;
    abstract Template():TemplateResult<1>
    SetParameter(_params:RegExpMatchArray):void{}
}

export class DefaultScreenController extends ScreenController {
    
    public Template = () => html`<span>DefaultScreenController</span>`
    onMessage(messageWrapper: ResponseWrapper): void {
        messageWrapper
    }
    onCreate(): void {

    }
    onFirstStart(): void {

    }
    onRestart(): void {

    }
    onPause(): void {

    }
}