import * as fs from "node:fs"
import { CodeBuilder } from "../usersettings/typescript/utils/usersettings_base";
import path from "node:path";

export function X02(num: number, len = 2) { let str = num.toString(16); return "0".repeat(len - str.length) + str; }

export function writeFileCreateDirLazy(file: fs.PathOrFileDescriptor, data: string | NodeJS.ArrayBufferView, callback?: fs.NoParamCallback) {
  fs.mkdirSync(path.dirname(file.toString()), { recursive: true });
  if (callback) {
    fs.writeFile(file, data, callback);
  } else {
    fs.writeFileSync(file, data);
  }
}


export function createWriteStreamCreateDirLazy(pathLike: fs.PathLike): fs.WriteStream {
  fs.mkdirSync(path.dirname(pathLike.toString()), { recursive: true });
  return fs.createWriteStream(pathLike);
}



export class MyCodeBuilderImpl implements CodeBuilder {

  constructor(initialValue: string | null = null) {
    if (initialValue) {
      this.AppendLine(initialValue);
    }
  }

  public get Code() {
    return this.code;
  }
  private code = "";
  public AppendLine(line: string): void {
    this.code += line + "\r\n";
  }

  public Clear(initialValue: string | null = null) {
    this.code = "";
    if (initialValue) {
      this.AppendLine(initialValue);
    }
  }
}


export class SearchReplace {
  constructor(public search: string, public replaceFilePath: string) { }
}