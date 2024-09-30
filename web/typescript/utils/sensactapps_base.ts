
import { TemplateResult, html } from 'lit-html';
import * as flatbuffers from 'flatbuffers';
import { Ref, createRef, ref } from 'lit-html/directives/ref.js';
import { classMap } from 'lit-html/directives/class-map.js';
import { ApplicationId } from "../../generated/flatbuffers/application-id";
import { IAppManagement } from './interfaces';
import { Requests, Responses } from '../../generated/flatbuffers/webmanager';
import { RequestStatus } from '../../generated/flatbuffers/websensact/request-status';
import { ValueUpdater } from './usersettings_base';

export enum ItemState {
  NODATA,
  SYNCHRONIZED,
  NONSYNCHRONIZED,
}

export interface SensactContext {

};

export class ApplicationGroup {
  constructor(public readonly DisplayName: string, private readonly appManagement: IAppManagement, public readonly Apps: Array<SensactApplication>, private readonly key: string | null = null) { }

  public get Key() {
    return this.key ?? this.DisplayName;
  }

  private panelOpen = false;
  private dataDirty = false;
  public spanArrowContainer: Ref<HTMLElement> = createRef();
  public divPanel: Ref<HTMLTableSectionElement> = createRef();
  public btnOpenClose: Ref<HTMLElement> = createRef();
  public btnUpdate: Ref<HTMLButtonElement> = createRef();
  public btnReset: Ref<HTMLButtonElement> = createRef();

  public Template = () => {

    var itemTemplates: Array<TemplateResult<1>> = [];
    for (const app of this.Apps) {
      itemTemplates.push(app.OverallTemplate());

    }
    return html`
    <div class="appgroup">
        <button ${ref(this.btnOpenClose)} @click=${(e) => this.onBtnOpenCloseClicked(e)} style="display: flex; ">
            <span ${ref(this.spanArrowContainer)}>▶</span>
            <span style="flex-grow: 1;">${this.DisplayName}</span>
            <input ${ref(this.btnUpdate)} @click=${(e: MouseEvent) => this.onBtnUpdateClicked(e)} type="button" value=" ⟳ Fetch Values from Server" />
            <input ${ref(this.btnReset)} type="button" value=" 🗑 Reset Values" />
        </button>
        <div ${ref(this.divPanel)}>
            <table>
                <thead>
                    <tr><th>Name</th><th>ID</th><th>Controls</th></tr>
                </thead>
                <tbody>${itemTemplates}</tbody>
            </table>
            
        </div>
    </div>
    `}

  private sendRequestGetApplicationStatus() {
    let b = new flatbuffers.Builder(1024);

    console.info(`sendRequestGetApplicationStatus`);
            
    var ids=new Array<ApplicationId>();
    this.Apps.forEach((v,k) => {
        ids.push(v.applicationId)
    });
      
    this.appManagement.WrapAndFinishAndSend(b,
      Requests.websensact_RequestStatus,
      RequestStatus.createRequestStatus(b, RequestStatus.createIdsVector(b, ids)),
      [Responses.websensact_ResponseStatus]
    );
  }


  private onBtnOpenCloseClicked(e: MouseEvent) {
    this.panelOpen = !this.panelOpen;
    this.btnOpenClose.value!.classList.toggle("active");
    if (this.panelOpen) {
      this.divPanel.value!.style.display = "block";
      this.spanArrowContainer.value!.textContent = "▼";
      this.sendRequestGetApplicationStatus();
    } else {
      this.divPanel.value!.style.display = "none";
      this.spanArrowContainer.value!.textContent = "▶";
    }
    e.stopPropagation();
  }

  private onBtnUpdateClicked(e: MouseEvent) {
    this.sendRequestGetApplicationStatus();
    e.stopPropagation()
  }

}
const LEN_OF_APPLICATIONID_PREFIX=14;
export abstract class SensactApplication {

  protected itemState: ItemState = ItemState.NODATA;
  public Flag: boolean = false; //for various use; eg. to check whether all Items got an update


  public abstract UpdateState(state32bit:number);

  constructor(public readonly applicationId: ApplicationId, public readonly ApplicationName: string, public readonly ApplicationDescription: string,) { }

  protected abstract CoreAppHtmlTemplate: () => TemplateResult<1>;

  
  public OverallTemplate = () => html`
  <tr class="app">
      <td>${this.ApplicationName}</td>
      <td>${ApplicationId[this.applicationId].slice(LEN_OF_APPLICATIONID_PREFIX)}</td>
      <td>${this.CoreAppHtmlTemplate()}</td>
  </tr>
  `

  public NoDataFromServerAvailable() {
    this.SetVisualState(ItemState.NODATA);
  }

  public ConfirmSuccessfulWrite() {
    this.SetVisualState(ItemState.SYNCHRONIZED);
  }



  protected SetVisualState(value: ItemState): void {
    /*
    this.inputElement.value!.className = "";
    this.inputElement.value!.classList.add("config-item");
    switch (value) {
      case ItemState.NODATA:
        this.inputElement.value!.classList.add("nodata");
        this.inputElement.value!.disabled = true;
        this.btnReset.value!.disabled = true;
        break;
      case ItemState.SYNCHRONIZED:
        this.inputElement.value!.classList.add("synchronized");
        this.inputElement.value!.disabled = false;
        this.btnReset.value!.disabled = true;
        break;
      case ItemState.NONSYNCHRONIZED:
        this.inputElement.value!.classList.add("nonsynchronized");
        this.inputElement.value!.disabled = false;
        this.btnReset.value!.disabled = false;
        break;
      default:
        break;
    }
        */
  }
}

export class OnOffApplication extends SensactApplication {
  private inputElement: Ref<HTMLInputElement> = createRef()
  constructor(applicationId: ApplicationId, applicationName: string, applicationDescription: string,) { super(applicationId, applicationName, applicationDescription) }

  private oninput() {
    if (this.inputElement.value!.checked) {
      //x.SendONCommand(this.applicationId, 0);
    } else {
      //x.SendOFFCommand(this.applicationId, 0);
    }
    console.log(`onoff ${this.applicationId} ${this.inputElement.value!.checked}`);
  }

  export async function sendCommandMessage(id: ApplicationId, cmd: Command, payload: bigint) {
    console.log(`sendCommandMessage id=${id} cmd=${cmd}`)
    let b = new flatbuffers.Builder(1024);
    this.appManagement.WrapAndFinishAndSend(b,
        Requests.websensact_RequestCommand,
        RequestCommandMessage.createRequestCommandMessage(b, id, cmd, payload ),
        [Responses.websensact_ResponseCommand]
    );
    //let buf = b.asUint8Array();
}
  protected CoreAppHtmlTemplate = () => html`
       <input ${ref(this.inputElement)} @input=${() => this.oninput()} class="toggle" type="checkbox"></input>`

}
/*
Ein Blinds-Timer öffnet und schließt die verbundenen Rolläden 
+öffnen: beim CIVIL_SUNRISE + Offset +  Zufallsüberlagerung
es können mehrere Rolläden registriert werden
sie alle erhalten den gleichen SURISE-Type und den gleichen Offset, aber eine individuelle Zufallsüberlagerung
Implementierung in c++:
Beim Hochfahren/Initialisieren werden für alle Rolläden die nächsten Zeitpunkte zum Öffnen und zum Schließen berechnet und abgelegt als epoch seconds
Wenn diese Zeiten dann individuell erreicht werden, wird der passende Befehl an den Rolladen losgesendet und direkt der nächste Termin zum Öffnen/Schließen am Folgetag berechnet
*/
export class BlindsTimerApplication extends SensactApplication {
  private inputElement: Ref<HTMLInputElement> = createRef()
  constructor(applicationId: ApplicationId, applicationKey: string, applicationDescription: string,) { super(applicationId, applicationKey, applicationDescription) }

  protected CoreAppHtmlTemplate = () => html`
       <input ${ref(this.inputElement)} @input=${() => this.oninput()} type="checkbox"></input>`


  private oninput() {
    if (this.inputElement.value!.checked) {
      //x.SendONCommand(this.applicationId, 0);
    } else {
      //x.SendOFFCommand(this.applicationId, 0);
    }
    console.log(`blindstimer ${this.applicationId} ${this.inputElement.value!.checked}`);

  }


}

export class BlindApplication extends SensactApplication {

  private upElement: Ref<HTMLInputElement> = createRef()
  private stopElement: Ref<HTMLInputElement> = createRef()
  private downElement: Ref<HTMLInputElement> = createRef()

  constructor(applicationId: ApplicationId, applicationKey: string, applicationDescription: string,) { super(applicationId, applicationKey, applicationDescription) }

  onStop() {
    //x.SendSTOPCommand(this.applicationId);
    console.log(`blind_stop ${this.applicationId}`);
  }

  onUp() {
    console.log(`blind_up ${this.applicationId}`);
  }

  onDown() {
    //x.SendDOWNCommand(this.applicationId, 1);
    console.log(`blind_down ${this.applicationId}`);
  }

  protected CoreAppHtmlTemplate = () => html`
  <button ${ref(this.upElement)} @input=${() => this.onUp()}>▲</button>
  <button ${ref(this.stopElement)} @input=${() => this.onStop()}>▮</button>
  <button ${ref(this.downElement)} @input=${() => this.onDown()}>▼</button>
  `
}


export class SinglePwmApplication extends SensactApplication {
  private onOffElement: Ref<HTMLInputElement> = createRef()
  private sliderElement: Ref<HTMLInputElement> = createRef()
  constructor(applicationId: ApplicationId, applicationKey: string, applicationDescription: string,) { super(applicationId, applicationKey, applicationDescription) }
  private oninput() {
    if (this.onOffElement.value!.checked) {
      //x.SendONCommand(this.applicationId, 0);
    } else {
      //x.SendOFFCommand(this.applicationId, 0);
    }
    console.log(`SinglePwmApplication ${this.applicationId} ${this.onOffElement.value!.checked}`);
  }

  private onslide() {
    //x.SendSET_VERTICAL_TARGETCommand(this.applicationId, parseInt(slider.value));
    console.log(`singlepwm_slider ${this.applicationId} ${this.sliderElement.value!.valueAsNumber}`);
  }
  protected CoreAppHtmlTemplate = () => html`
  <input ${ref(this.onOffElement)} @input=${() => this.oninput()} class="toggle" type="checkbox"></input>
  <input ${ref(this.sliderElement)} @input=${() => this.onslide()} type="range" min="1" max="100" value="50">
  `


}