import { ApplicationId } from "../../generated/flatbuffers/application-id";
import { Command } from "../../generated/flatbuffers/command";
import { ISensactContext } from "./interfaces";

export abstract class SensactApplication {
  
  protected state:number=0;
  constructor(public readonly applicationId: ApplicationId, public readonly ApplicationDescription: string, ctx:ISensactContext,) { }
  public abstract GetState():number;
  public abstract onCommand(c: Command, p: bigint);
}

export class OnOffApplication extends SensactApplication {
  
  public onCommand(c: Command, p: bigint) {
    if(c==Command.ON){
      console.info(`App ${ApplicationId[this.applicationId]} goes on.`);
      this.state=1;
    }else if(c==Command.OFF){
      console.info(`App ${ApplicationId[this.applicationId]} goes off.`);
      this.state=0;
    }
  }
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
  private direction=0;
  private position=10;
  
  public GetState(): number {
    return this.state;
  }
  public SetState(s:number): void {
    this.state=s;
  }

  public onCommand(c: Command, p: bigint) {
    if(c==Command.UP){
      console.info(`App ${ApplicationId[this.applicationId]} goes up.`);
      this.direction=-1;
    }else if(c==Command.STOP){
      console.info(`App ${ApplicationId[this.applicationId]} stops.`);
      this.direction=0;
    }else if(c==Command.DOWN){
      console.info(`App ${ApplicationId[this.applicationId]} goes down.`);
      this.direction=+1;
    }
    this.position+=this.direction
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