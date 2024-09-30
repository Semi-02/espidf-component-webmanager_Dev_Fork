import { writeFileCreateDirLazy } from "./gulpfile_utils";
import path from "node:path";

import fs from "node:fs";
import * as gulp from "gulp";
import * as P from "./paths";
import { SENSACT_COMPONENT_GENERATED_PATH, OPTION_SENSACT } from "./gulpfile_config";

export function copy_flatbuffer_sources_from_sensact_generated(cb: gulp.TaskFunctionCallback) {

  if(OPTION_SENSACT){
    fs.cpSync(path.join(SENSACT_COMPONENT_GENERATED_PATH, "common", "applicationIds.fbs.inc"), path.join(P.GENERATED_SENSACT_FBS, "applicationIds.fbs.inc"), { recursive: true });
    fs.cpSync(path.join(SENSACT_COMPONENT_GENERATED_PATH, "common", "commandTypes.fbs.inc"), path.join(P.GENERATED_SENSACT_FBS, "commandTypes.fbs.inc"), { recursive: true });
  }else{
    fs.cpSync(path.join(P.FLATBUFFERS, "applicationIds.fbs.inc.empty"), path.join(P.GENERATED_SENSACT_FBS, "applicationIds.fbs.inc"), { recursive: true });
    fs.cpSync(path.join(P.FLATBUFFERS, "commandTypes.fbs.inc.empty"), path.join(P.GENERATED_SENSACT_FBS, "commandTypes.fbs.inc"), { recursive: true });
  }
  cb();
}

export function fill_template_sendcommands(cb: gulp.TaskFunctionCallback) {
  var content = fs.readFileSync(P.TEMPLATE_SEND_COMMAND_IMPLEMENTATION).toString();
  if(OPTION_SENSACT){
    content = content.replace("//TEMPLATE_HERE", fs.readFileSync(path.join(SENSACT_COMPONENT_GENERATED_PATH, "common", "sendCommandImplementation.ts.inc")).toString());
  }
  writeFileCreateDirLazy(path.join(P.GENERATED_SENSACT_TS, "sendCommandImplementation.ts"), content);
  writeFileCreateDirLazy(path.join(P.DEST_SENSACT_TYPESCRIPT_WEBUI, "sendCommandImplementation_copied_during_build.ts"), content);
  cb();
}

export function fill_template_sensactapps(cb: gulp.TaskFunctionCallback) {
  var content = fs.readFileSync(P.TEMPLATE_SENSACT_APPS).toString();
  if(OPTION_SENSACT){
    content = content.replace("//TEMPLATE_HERE", fs.readFileSync(path.join(SENSACT_COMPONENT_GENERATED_PATH, "common", "sensactapps.ts.inc")).toString());
  }
  writeFileCreateDirLazy(path.join(P.GENERATED_SENSACT_TS, "sensactapps.ts"), content);
  writeFileCreateDirLazy(path.join(P.DEST_SENSACT_TYPESCRIPT_WEBUI, "sensactapps_copied_during_build.ts"), content);
  cb();
}
