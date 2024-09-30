
import * as flatbuffers from 'flatbuffers';
import { ApplicationId } from '../../generated/flatbuffers/application-id';
import { Command } from '../../generated/flatbuffers/command';
import { CommandMessage } from '../../generated/flatbuffers/websensact/command-message';


export async function sendCommandMessage(id: ApplicationId, cmd: Command, payload: Uint8Array) {
    let b = new flatbuffers.Builder(1024);
    let payloadOffset = CommandMessage.createPayloadVector(b, payload);
    CommandMessage.startCommandMessage(b);
    CommandMessage.addId(b, id);
    CommandMessage.addCmd(b, cmd);
    CommandMessage.addPayload(b, payloadOffset);
    let x = CommandMessage.endCommandMessage(b);
    b.finish(x);
    //let buf = b.asUint8Array();
}