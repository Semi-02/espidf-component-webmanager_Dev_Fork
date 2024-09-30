import { RequestTimeseries, RequestWrapper, Requests, ResponseWrapper, Responses, TimeGranularity } from "../../generated/flatbuffers/webmanager";
import { ScreenController } from "./screen_controller";
import * as flatbuffers from 'flatbuffers';
import * as chartist from "chartist"
import { html, render } from "lit-html";
import { Ref, createRef, ref } from "lit-html/directives/ref.js";


export class TimeseriesController extends ScreenController {
    private chartSeconds?:chartist.LineChart;
    private chartDiv:Ref<HTMLDivElement>=createRef()
    private paramPara:Ref<HTMLParagraphElement>=createRef()
    public Template = () => html`<h1>Timeseries Sconds</h1><p ${ref(this.paramPara)}>Value=</p><div ${ref(this.chartDiv)}></div>`

    private ParamsTemplate = ()=>html`id=${this.match.groups!["id"]} val=${this.match.groups!["val"]}`
    match: RegExpMatchArray;

    onMessage(_messageWrapper: ResponseWrapper): void {
        throw new Error("TimeseriesController does not expect 'normal' messages");
    }


    private sendRequestTimeseries() {
        let b = new flatbuffers.Builder(1024);
        this.appManagement.WrapAndFinishAndSend(b,
            Requests.RequestTimeseries,
            RequestTimeseries.createRequestTimeseries(b, 0, 0),
            [Responses.ResponseTimeseriesDummy]
        );
    }

    private updateDate(date: Date, granularity: TimeGranularity) {
        switch (granularity) {
            case TimeGranularity.TEN_SECONDS:
                date.setSeconds(date.getSeconds() + 10);
                break;
            case TimeGranularity.ONE_MINUTE:
                date.setMinutes(date.getMinutes() + 1);
                break;
            case TimeGranularity.ONE_HOUR:
                date.setHours(date.getHours() + 1);
                break;
            case TimeGranularity.ONE_DAY:
                date.setDate(date.getDate() + 1); //Welche Chancen/Grenzen sehen Sie für den Transfer in ihr eigenes Fach?
                break;
            default:
                break;
        }
    }



    onTimeseriesMessage(data: ArrayBuffer): void {
        var dataView = new DataView(data);
        var epochSeconds = dataView.getBigInt64(0);
        var granularity:TimeGranularity = (Number(dataView.getBigInt64(8)));
        console.log(`Start epochSeconds= ${epochSeconds} Granularity=${granularity}`);
        var date = new Date(Number(epochSeconds) * 1000);
        var labels: Array<string> = [];
        var series: Array<Array<number>> = [[], [], [], []];

        var dataOffsetByte = 64;
        while (dataOffsetByte < 4096 / 2) {
            if((dataOffsetByte-64)%256==0){
                labels.push(date.toLocaleString());
            }
            else{
                labels.push("");
            }
            series[0].push(dataView.getInt16(dataOffsetByte));
            dataOffsetByte += 2;
            series[1].push(dataView.getInt16(dataOffsetByte));
            dataOffsetByte += 2;
            series[2].push(dataView.getInt16(dataOffsetByte));
            dataOffsetByte += 2;
            series[3].push(dataView.getInt16(dataOffsetByte));
            dataOffsetByte += 2;
            this.updateDate(date, granularity);
        }

        if(granularity==TimeGranularity.TEN_SECONDS){
            this.chartSeconds!.update({labels, series});
        }
    }

    private options: chartist.LineChartOptions<chartist.AxisOptions, chartist.AxisOptions> = {
        axisX:{
            scaleMinSpace:40
        },
        showArea:false,
        width: '100%',
        height:"200px",

    };

    onCreate(): void {
        

        
    }

    onFirstStart(): void {
        this.chartSeconds=new chartist.LineChart(
            this.chartDiv.value!,
            {
                labels: [1, 2, 3, 4, 5, 6, 7, 8],
                series: [[5, 9, 7, 8, 5, 3, 5, 4]]
            },
            this.options);
        //this.sendRequestTimeseries();
    }
    onRestart(): void {
        this.chartSeconds=new chartist.LineChart(
            this.chartDiv.value!,
            {
                labels: [1, 2, 3, 4, 5, 6, 7, 8],
                series: [[5, 9, 7, 8, 5, 3, 5, 4]]
            },
            this.options);
        //this.sendRequestTimeseries();
    }
    onPause(): void {
    }

    SetParameter(match:RegExpMatchArray):void{
        this.match=match;
        render(this.ParamsTemplate(), this.paramPara.value!)
    }

}
