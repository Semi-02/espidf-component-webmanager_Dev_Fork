
import { IHtmlRenderer } from './interfaces';
import { html, render } from 'lit-html';

export class Counter extends EventTarget implements IHtmlRenderer{
    private clicks=0;

    constructor(){
        super()
        
    }
    RenderStatic(c: HTMLElement): void {
        render(this.Template, c);
    }
    public Template= html`<button @click="${() => this.clicks++}">Fired ${() => this.clicks} arrows</button>`
}
