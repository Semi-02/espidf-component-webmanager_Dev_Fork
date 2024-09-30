
import "../style/app.css"
import { TemplateResult, html, render } from "lit-html";
import RouterMenu, { IRouteHandler, Route } from "./routermenu";
import { Ref, createRef, ref } from "lit-html/directives/ref.js";
import { IAppManagement, IWebsocketMessageListener } from "./utils/interfaces";
import { ResponseWrapper, NotifyLiveLogItem, Responses, Requests, RequestWrapper } from "../generated/flatbuffers/webmanager";
import { DialogController, FilenameDialog, OkCancelDialog, OkDialog, PasswordDialog } from "./dialog_controller/dialog_controller";
import { Severity, Html, severity2class, severity2symbol } from "./utils/common";
import { WS_URL } from "./constants";
import * as flatbuffers from "flatbuffers"
import { DefaultScreenController, ScreenController } from "./screen_controller/screen_controller";
import { CanMonitorScreenController } from "./screen_controller/canmonitor_controller";
import { WifimanagerController as WifimanagerScreenController } from "./screen_controller/wifimanager_controller";
import { runCarRace } from "./screen_controller/racinggame_controller";
import { FingerprintScreenController } from "./screen_controller/fingerprint_controller";
import { SystemScreenController } from "./screen_controller/systemscreen_controller";
import { UsersettingsController } from "./screen_controller/usersettings_controller";
import { WeeklyScheduleDialog, iWeeklyScheduleDialogHandler } from "./dialog_controller/weeklyschedule_dialog";
import { SchedulerScreenController } from "./screen_controller/scheduler_controller";
import { WebsensactController } from "./screen_controller/websensact_controller";


class Router2ContentAdapter implements IRouteHandler {
  constructor(public readonly child: ScreenController, public readonly app: AppController) { }
  handleRoute(params: RegExpMatchArray): void {
    console.log("Router2ContentAdapter->handleRoute")
    this.app.SetMain(this.child, params)
  }
}

const ANSI_ESCAPE = new RegExp("(\\x9B|\\x1B\\[)[0-?]*[ -\\/]*[@-~]");
const MAX_MESSAGE_COUNT = 20;


function AddScreenControllers(app: AppController): void {
  //app.AddScreenController("dashboard", new RegExp("^/$"), html`<span>&#127760;</span><span>Home</span>`, DefaultScreenController)
  app.AddScreenController("can", new RegExp("^/can$"), html`<span>🥽</span><span>Can Monitor</span>`, CanMonitorScreenController)
  app.AddScreenController("system", new RegExp("^/system$"), html`<span>🧰</span><span>System Settings</span>`, SystemScreenController)
  app.AddScreenController("properties", new RegExp("^/properties$"), html`<span>⌘</span><span>Properties</span>`, UsersettingsController)
  app.AddScreenController("wifiman", new RegExp("^/wifiman$"), html`<span>📶</span><span>Wifi Manager</span>`, WifimanagerScreenController)
  //app.AddScreenController("timeseries", /^\/timeseries(?:\/(?<id>\w*))?(?:\/(?<val>\d*))?$/, html`<span>📶</span><span>Timeseries</span>`, TimeseriesController)
  app.AddScreenController("finger", new RegExp("^/finger$"), html`<span>👉</span><span>Fingerprint</span>`, FingerprintScreenController)
  app.AddScreenController("schedule", new RegExp("^/schedule$"), html`<span>📅</span><span>Scheduler</span>`, SchedulerScreenController)
  app.AddScreenController("sensact", new RegExp("^/sensact$"), html`<span>🏠</span><span>Sensact</span>`, WebsensactController)
}

class BufferedMessage {
  constructor(public data: ArrayBuffer, public messagesToUnlock: Responses[] = [Responses.NONE]) { }
}

class AppController implements IAppManagement, IWebsocketMessageListener {

  private routes: Array<Route> = []

  private messageType2listener: Map<number, Array<IWebsocketMessageListener>>;
  private messagesToUnlock: Array<Responses> = [Responses.NONE];
  private socket: WebSocket | null = null;
  private modalSpinnerTimeoutHandle: number = -1;
  private messageBuffer?: BufferedMessage

  private messageCount = 0;
  private menu = new RouterMenu("/", this.routes);
  private mainContent: ScreenController=new DefaultScreenController(this);
  private mainRef: Ref<HTMLInputElement> = createRef();
  private scroller: Ref<HTMLDivElement> = createRef();
  private scroller_anchor: Ref<HTMLDivElement> = createRef();
  


  private modal: Ref<HTMLDivElement> = createRef();
  private dialog: Ref<HTMLDivElement> = createRef();
  private snackbarTimeout: number = -1;


  showEnterFilenameDialog(messageText: string, pHandler?: (ok: boolean, value: string) => any): void {
    this.renderAndShowDialog(new FilenameDialog(messageText, pHandler));
  }
  showEnterPasswordDialog(messageText: string, pHandler?: (ok: boolean, value: string) => any): void {
    this.renderAndShowDialog(new PasswordDialog(messageText, pHandler));
  }
  showOKDialog(pSeverity: Severity, messageText: string, pHandler?: (ok: boolean, value: string) => any): void {
    this.renderAndShowDialog(new OkDialog(pSeverity, messageText, pHandler));
    
  }
  showOKCancelDialog(pSeverity: Severity, messageText: string, pHandler?: (ok: boolean, value: string) => any): void {
    this.renderAndShowDialog(new OkCancelDialog(pSeverity, messageText, pHandler));
  }

  showWeeklyTimetableDialog(heading:string, initialValue: Array<number>, handler:iWeeklyScheduleDialogHandler, referenceHandle:any):void{
    this.renderAndShowDialog(new WeeklyScheduleDialog(heading, initialValue, handler, referenceHandle));
  }

  showDialog<T extends DialogController>(type: T): void{
    this.renderAndShowDialog(type);
  }

  private renderAndShowDialog(d:DialogController){
    //this.dialog.value!.innerText="";
    render(d.Template(), this.dialog.value!)
    d.Show();
  }


  public SetMain(child: ScreenController, params: RegExpMatchArray) {
    this.mainContent.onPausePublic();
    this.mainContent = child
    //this.mainRef.value!.innerText=""
    render(this.mainContent.Template(), this.mainRef.value!)
    this.mainContent.onStartPublic();
    child.SetParameter(params)
  }

  public AddScreenController<T extends ScreenController>(url: string, urlPattern: RegExp, caption: TemplateResult<1>, type: { new(m: IAppManagement): T; }): ScreenController {
    let controllerObject = new type(this);
    var w = new Route(url, urlPattern, caption, new Router2ContentAdapter(controllerObject, this))
    this.routes.push(w)
    controllerObject.onCreate();
    return controllerObject
  }

 

  private setModal(state: boolean) {
    this.modal.value!.style.display = state ? "flex" : "none";
  }

  private modalSpinnerTimeout() {
    this.setModal(false);
    this.showOKDialog(Severity.ERROR, "Server did not respond");
  }

  public WrapAndFinishAndSend(b:flatbuffers.Builder, message_type:Requests,  message:flatbuffers.Offset, messagesToUnlock: Array<Responses> = [Responses.NONE], maxWaitingTimeMs: number = 3000){
    console.log(`Send Request ${Requests[message_type]} to server an expecting ${messagesToUnlock.map((v)=>Responses[v]).join(", ")} as answer`)
    b.finish(RequestWrapper.createRequestWrapper(b, message_type,message));
    var data=b.asUint8Array()
    this.sendWebsocketMessage(data, messagesToUnlock, maxWaitingTimeMs);
  }

  private sendWebsocketMessage(data: ArrayBuffer, messagesToUnlock: Array<Responses> = [Responses.NONE], maxWaitingTimeMs: number = 2000): void {
    if (this.socket!.readyState != this.socket!.OPEN) {
      console.info('sendWebsocketMessage --> not OPEN --> buffer')
      //still in connection state -->buffer
      if (this.messageBuffer) {
        this.showSnackbar(Severity.ERROR, `Websocket is still in CONNECTING state and there is already a message in buffer`)
        return;
      }
      this.messageBuffer = new BufferedMessage(data, messagesToUnlock);
      return;
    }
    this.messagesToUnlock = messagesToUnlock
    if (messagesToUnlock && messagesToUnlock[0] != Responses.NONE) {
      this.setModal(true)
      this.modalSpinnerTimeoutHandle = <number>(<unknown>setTimeout(() => this.modalSpinnerTimeout(), maxWaitingTimeMs)) //casting to make TypeScript happy
    }

    console.info(`sendWebsocketMessage --> OPEN --> send to server`)
    try {
      this.socket?.send(data)
    } catch (error: any) {
      this.setModal(false)
      if (this.modalSpinnerTimeoutHandle) {
        clearTimeout(this.modalSpinnerTimeoutHandle)
      }
      this.showOKDialog(Severity.ERROR, `Error while sending a request to server:${error}`)
    }
  }

  private onWebsocketData(data: ArrayBuffer) {
    if (data.byteLength == 4096) {
      //TODO: it is dumb idea to use the size of the message to test whether it is a raw timeseries message or not. But hopefully, Flatbuffer messages never get that big
      // this.timeseriesScreenController?.onTimeseriesMessage(data);
      // if(this.messagesToUnlock.includes(Responses.ResponseTimeseriesDummy)){
      //   clearTimeout(this.modalSpinnerTimeoutHandle);
      //   this.messagesToUnlock=[Responses.NONE];
      //   this.setModal(false);
      // }
      return
    }
    let arr = new Uint8Array(data)
    let bb = new flatbuffers.ByteBuffer(arr)
    let messageWrapper = ResponseWrapper.getRootAsResponseWrapper(bb)
    console.log(`A message of type "${Responses[messageWrapper.responseType()]}" [${messageWrapper.responseType()}] with length ${data.byteLength} has arrived.`)
    if (this.messagesToUnlock.includes(messageWrapper.responseType())) {
      clearTimeout(this.modalSpinnerTimeoutHandle)
      this.messagesToUnlock = [Responses.NONE]
      this.setModal(false)
    }
    this.messageType2listener.get(messageWrapper.responseType())?.forEach((v) => {
      v.onMessage(messageWrapper)
    })
  }

  public registerWebsocketMessageTypes(listener: IWebsocketMessageListener, ...messageTypes: number[]): () => any {
    messageTypes.forEach((messageType) => {
      let arr = this.messageType2listener.get(messageType)
      if (!arr) {
        arr = []
        this.messageType2listener.set(messageType, arr)
      }
      arr.push(listener)
    })
    return () => {
      this.unregister(listener)
    }
  }

  unregister(listener: IWebsocketMessageListener) {
    console.info('unregister')
    this.messageType2listener.forEach((v) => {
      v.filter((l) => {
        l != listener
      })
    })
  }
  public log(text: string) {
    this.logInternal("I" + text);
  }

  public showSnackbar(severity: Severity, text: string, timeout=3000) {
    if (this.snackbarTimeout >= 0) {
      clearInterval(this.snackbarTimeout);
    }
    var snackbar = document.getElementById("snackbar");
    snackbar.innerText = "";
    Html(snackbar, "span", [], [severity2class(severity)], severity2symbol(severity));
    Html(snackbar, "span", [], [], text);
    snackbar.style.visibility = "visible";
    snackbar.style.animation = "fadein 0.5s, fadeout 0.5s 2.5s";
    this.snackbarTimeout = <any>setTimeout(() => {
      snackbar.style.visibility = "hidden";
      snackbar.style.animation = "";
      this.snackbarTimeout = -1;
    }, timeout);
  }

  private logInternal(message: string) {
    let msg = document.createElement('p');
    if (message.startsWith("I")) {
      msg.className = 'info';
    } else if (message.startsWith("W")) {
      msg.className = 'warn';
    } else {
      msg.className = 'error';
    }
    msg.innerText = message.replace(ANSI_ESCAPE, "");
    this.scroller.value!.insertBefore(msg, this.scroller_anchor.value!);
    this.messageCount++;
    if (this.messageCount > MAX_MESSAGE_COUNT) {
      this.scroller.value!.removeChild(this.scroller.value!.firstChild!);
    }
  }

  onMessage(messageWrapper: ResponseWrapper): void {
    let li = <NotifyLiveLogItem>messageWrapper.response(new NotifyLiveLogItem());
    this.logInternal(<string>li.text());
  }

  constructor() {
    this.messageType2listener = new Map<number, [IWebsocketMessageListener]>;
    
    this.registerWebsocketMessageTypes(this, Responses.NotifyLiveLogItem);
  }

  private easteregg(){
    document.body.innerText="";
    document.body.innerHTML="<canvas id='c'>"
    var el = <HTMLCanvasElement>document.getElementById("c")!
    runCarRace(el);
  }



  public Startup(screenControllersBuilder:(app: AppController)=>void ) {
    screenControllersBuilder(this);
    const Template = html`
        <header><h1 @click=${()=>this.easteregg()}>WebManager</h1></header>
        <nav>${this.menu.Template()}<a href="javascript:void(0);" @click=${() => this.menu.ToggleHamburgerMenu()}><i>≡</i></a></nav>
        <main ${ref(this.mainRef)}></main>
        <footer><div ${ref(this.scroller)}><div ${ref(this.scroller_anchor)}></div></div></footer>
        <div ${ref(this.modal)} class="modal"><span class="loader"></span></div>
        <div id="snackbar">Some text some message..</div>
        <div ${ref(this.dialog)}></div>`
    render(Template, document.body);
    
    console.log(`Connecting to ${WS_URL}`)
    this.socket = new WebSocket(WS_URL)
    this.socket.binaryType = 'arraybuffer'
    this.socket.onopen = (_event) => {
      console.log(`Websocket is connected. ${this.messageBuffer ? "There is a message in buffer" : ""}`)
      if (this.messageBuffer) {
        this.sendWebsocketMessage(this.messageBuffer.data, this.messageBuffer.messagesToUnlock);
      }
      this.messageBuffer = undefined;
    }
    this.socket.onerror = (event: Event) => {
      var message = `Websocket error ${event}`
      console.error(message)
      this.showSnackbar(Severity.ERROR, message)
    }
    this.socket.onmessage = (event: MessageEvent<any>) => {
      this.onWebsocketData(event.data)
    }
    this.socket.onclose = (event) => {
      if (event.code == 1000) {
        console.info('The Websocket connection has been closed normally. But why????')
        return
      }
      var message = `Websocket has been closed ${event}`
      console.error(message)
      this.showSnackbar(Severity.ERROR, message)
    }
    this.menu.check();
  }

}
//<p>Before Head</p>${this.head.Template}<p>Before Nav</p>${this.menu.Template()}<p>Before Counter</p>${this.counter.Template}<p>Before Main</p>${this.mainContent.Template}

var app: AppController;
document.addEventListener("DOMContentLoaded", () => {
  console.log("DOMContentLoaded event -->starting app!")
  app = new AppController();
  app.Startup(AddScreenControllers)
});

