import { Ref, createRef, ref } from "lit-html/directives/ref.js";
import { html } from "lit-html";
import { DialogController } from "./dialog_controller";
import { iSunRandomDialogHandler } from "./weeklyschedule_dialog";

export class SunRandomScheduleEditorDialog extends DialogController {

    protected inputOffset: Ref<HTMLInputElement> = createRef();
    protected inputRandom: Ref<HTMLInputElement> = createRef();

    constructor(protected nameOfSchedule: string, private offsetMinutes: number, private randomMinutes: number, protected handler: iSunRandomDialogHandler) {
        super();
    }

    protected cancelHandler() {
        this.dialog.value!.close('Cancel');
        this.handler.handleSunRandomDialog(false, 0, 0);
    }

    protected okHandler() {
        this.dialog.value!.close('Ok');
        this.handler.handleSunRandomDialog(true, this.inputOffset.value!.valueAsNumber, this.inputRandom.value!.valueAsNumber);
    }

    protected backdropClickedHandler(e: MouseEvent) {
        if (e.target === this.dialog) {
            this.cancelHandler();
        }
    }

    public Template() {
        return html`
    <dialog @cancel=${() => this.cancelHandler()} @click=${(e: MouseEvent) => this.backdropClickedHandler(e)} ${ref(this.dialog)}>
        <header>
            <span>Edit SunRandom Schedule "${this.nameOfSchedule}"</span>
            <button @click=${() => this.cancelHandler} type="button">&times;</button>
        </header>
        <main>
            <label><input ${ref(this.inputOffset)} type="number" value=${this.offsetMinutes} />Offset [minutes]</label>
            <label><input ${ref(this.inputRandom)} type="number" value=${this.randomMinutes} />Random [minutes]</label>
        </main>
        <footer><input @click=${() => this.okHandler()} type="button" value="OK"></input><input @click=${() => this.cancelHandler()} type="button" value="Cancel"></input></footer>
    </dialog>`;
    }

}
