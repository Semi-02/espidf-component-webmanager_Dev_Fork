import { ApplicationId } from "../../generated/flatbuffers/application-id";
import { ISensactContext } from "./interfaces";

export abstract class SensactApplication {
  protected state:number=0;
  constructor(public readonly applicationId: ApplicationId, public readonly ApplicationDescription: string, ctx:ISensactContext,) { }
  public abstract GetState():number;
}

export class OnOffApplication extends SensactApplication {
  constructor(applicationId: ApplicationId, applicationDescription: string,ctx:ISensactContext,) { super(applicationId, applicationDescription,ctx) }
  public GetState(): number {
    return this.state;
  }
  public SetState(s:number): void {
    this.state=s==0?0:1;
  }
}

export class BlindsTimerApplication extends SensactApplication {
  
  constructor(applicationId: ApplicationId, applicationDescription: string,ctx:ISensactContext,) { super(applicationId, applicationDescription,ctx) }
  public GetState(): number {
    return this.state;
  }
  public SetState(s:number): void {
    this.state=s;
  }
}

export class BlindApplication extends SensactApplication {
  constructor(applicationId: ApplicationId, applicationDescription: string,ctx:ISensactContext,) { super(applicationId, applicationDescription,ctx) }
  public GetState(): number {
    return this.state;
  }
  public SetState(s:number): void {
    this.state=s;
  }
}

export class SinglePwmApplication extends SensactApplication {
  public GetState(): number {
    return this.state;
  }
  public SetState(s:number): void {
    this.state=s;
  }
}