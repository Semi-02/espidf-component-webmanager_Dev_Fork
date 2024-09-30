import { Ref, createRef, ref } from "lit-html/directives/ref.js";
import { html } from "lit-html";
import { DialogController } from "./dialog_controller";
import { numberArray2HexString } from "../utils/common";

const weekdays = ["Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"];
const startHour=6;
enum MarkingMode{TOGGLE,ON,OFF};
export interface iWeeklyScheduleDialogHandler{
    handleWeeklyScheduleDialog(ok: boolean, referenceHandle:any, value: Array<number>);
}

export interface iSunRandomDialogHandler{
    handleSunRandomDialog(ok: boolean, offsetMinutes: number, randomMinutes: number);
}

export class WeeklyScheduleDialog extends DialogController {
    
    
    constructor(private header:string, private initialValue: Array<number>|null, private handler:iWeeklyScheduleDialogHandler, private referenceHandle:any){
        super()
    }

    private isSelecting = false;
    private markingMode:MarkingMode=MarkingMode.ON
    private tblBody:Ref<HTMLTableSectionElement>= createRef();

    protected okHandler() {
        
        var arr = new Array<number>(84);
        for(const d of [6,5,4,3,2,1,0]){
            for(var hour=0;hour<24;hour+=2){
                var value=0;
                for(var quarterhour=0;quarterhour<8;quarterhour++){
                    var sourceMarked=this.isSelected(d, ((hour+24-startHour)*4+quarterhour)%(4*24));
                    value=value | (sourceMarked?1:0);
                    value<<=1 //MSB ist dann immer die erste viertelstunde im 2h-Interval-byte
                }
                value>>=1
                arr[d*12+(hour/2)]=value;
            }
        }
        this.initialValue=arr;
        console.log(`Save clicked. Value=${numberArray2HexString(arr)}`)
        this.dialog.value!.close("Ok");
        this.handler.handleWeeklyScheduleDialog(true, this.referenceHandle, arr);
    }
   
    public Show(){
        if(this.initialValue){
            console.log(`Dialog opened. Value=${numberArray2HexString(this.initialValue)}`)
            for(var d=0;d<7;d++){
                for(var hour=0;hour<24;hour+=2){
                    var value=this.initialValue[d*12+(hour/2)];
                    for(var quarterhour=0;quarterhour<8;quarterhour++){
                        var shouldBeMarked=value & (0b10000000>>quarterhour)
                        this.setSelected(d, ((hour+24-startHour)*4+quarterhour)%(4*24), shouldBeMarked>0);
                    }
                }
            }
        }else{
            this.setAll(false);
        }
        var radios= <NodeListOf<HTMLInputElement>>document.getElementsByName("mode")
        radios[1].checked=true;
        this.dialog.value!.showModal();
    }

    private tdMousedown(e: MouseEvent) {
        //console.log(`mousedown @ ${(<HTMLElement>e.target).innerText}`)
        this.isSelecting = true;
        this.tdMouseenter(e)
        e.preventDefault(); // Verhindert die Textauswahl
    }

    

    private tdMouseenter(e: MouseEvent) {
        //console.log(`mouseenter @ ${(<HTMLElement>e.target).innerText}`)
        if (!this.isSelecting) return;
        switch(this.markingMode){
            case MarkingMode.TOGGLE:(<HTMLElement>e.target).classList.toggle('selected');break;
            case MarkingMode.ON:(<HTMLElement>e.target).classList.add('selected');break;
            case MarkingMode.OFF:(<HTMLElement>e.target).classList.remove('selected');break;
        }
    }

    private tdMouseup(e: MouseEvent) {
        //console.log(`mouseup @ ${(<HTMLElement>e.target).innerText}`)
        this.isSelecting = false;
    }

    private rdoChange(e:MouseEvent){
        var value:string =(<HTMLInputElement>e.target).value
        this.markingMode=MarkingMode[value];
    }

    private copy(sourceDay:number, destinationDays:Array<number>){
        
        for(var fifteen_minutes_slot=0;fifteen_minutes_slot<4*24;fifteen_minutes_slot++){
            var sourceMarked=this.isSelected(sourceDay, fifteen_minutes_slot);
            for(const d of destinationDays){
                this.setSelected(d, fifteen_minutes_slot, sourceMarked);
            }
        }
    }

    private setAll(selected:boolean) {
        
        for(var d=0;d<weekdays.length;d++){
            for(var fifteen_minutes_slot=0;fifteen_minutes_slot<4*24;fifteen_minutes_slot++){
                this.setSelected(d, fifteen_minutes_slot, selected);
            }
        }
    }

    private isSelected(day_zero_based:number, fifteen_minutes_slot:number){
        const r=this.tblBody.value!.rows[day_zero_based]
        return r.cells[fifteen_minutes_slot+1].classList.contains("selected")
    }

    private setSelected(day_zero_based:number, fifteen_minutes_slot:number, value:boolean){
        const r=this.tblBody.value!.rows[day_zero_based]
        if(value){
            r.cells[fifteen_minutes_slot+1].classList.add("selected");
        }else{
            r.cells[fifteen_minutes_slot+1].classList.remove("selected");
        }
    }

    protected cancelHandler() {
        this.dialog.value!.close('Cancel')
        this.handler.handleWeeklyScheduleDialog(false, this.referenceHandle, null);
    }

    

    
    

    public Template = () => {
        const weekdayTemplate = (day_name, day_index) => html`${[...Array(96)].map((name, num) =>
            html`<td @mousedown=${(e: MouseEvent) => this.tdMousedown(e)} @mouseenter=${(e: MouseEvent) => this.tdMouseenter(e)} @mouseup=${(e: MouseEvent) => this.tdMouseup(e)}></td>`
        )}`
        const rowTemplates = [];
        weekdays.map((day_name, day_index) =>  {
            rowTemplates.push(html`<tr><td>${day_name}</td>${weekdayTemplate(day_name, day_index)}</tr>`)
        })


        return html`
    <dialog ${ref(this.dialog)}>
        <header>
            <span>${this.header}</span>
            <button @click=${()=>this.dialog.value!.close("cancelled")} type="button">&times;</button>
        </header>
        <main>

                <fieldset>
                    <legend>Marking Mode</legend>
                    <label><input @change=${(e:MouseEvent) => this.rdoChange(e)} type="radio" name="mode" value="TOGGLE" ?checked=${this.markingMode == MarkingMode.TOGGLE} />Toggle</label>
                    <label><input @change=${(e:MouseEvent) => this.rdoChange(e)} type="radio" name="mode" value="ON" ?checked=${this.markingMode == MarkingMode.ON}/>On</label>
                    <label><input @change=${(e:MouseEvent) => this.rdoChange(e)} type="radio" name="mode" value="OFF" ?checked=${this.markingMode == MarkingMode.OFF}/>Off</label>
                </fieldset>
                <fieldset>
                    <legend>Comfort Copy</legend>
                    <input @click=${(e:MouseEvent) => this.copy(0, [1,2,3,4])} type="button" value="Mo➔Di-Fr" />
                    <input @click=${(e:MouseEvent) => this.copy(0, [1,2,3,4,5,6])} type="button" value="Mo➔Di-So" />
                    <input @click=${(e:MouseEvent) => this.copy(5, [6])} type="button" value="Sa➔So" />
                </fieldset>
                <fieldset>
                    <legend>Comfort Fill</legend>
                    <input @click=${(e:MouseEvent) => this.setAll(true)} type="button" value="Fill All" />
                    <input @click=${(e:MouseEvent) => this.setAll(false)} type="button" value="Clear All" />
                </fieldset>

            <table class="weekschedule">
                <thead>
                <tr>
                    <th></th>
                    ${[...Array(24)].map((v,i) => html`<th colspan=4>${(i+startHour)%24}:00</th>`)}
                </tr>
                </thead>
                <tbody ${ref(this.tblBody)}>
                ${rowTemplates}
                </tbody>
            </table>
        </main>
        <footer><input @click=${() => this.okHandler()} type="button" value="OK"></input><input @click=${() => this.cancelHandler()} type="button" value="Cancel"></input></footer>
    </dialog>
    `}



}
