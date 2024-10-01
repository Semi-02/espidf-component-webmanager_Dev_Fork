import { ConfigGroup, StringItem, IntegerItem, BooleanItem, EnumItem } from '../../typescript/utils/usersettings_base';


export default class{
    public static Build():ConfigGroup[]{
        let i1:StringItem;
        let i2:StringItem;
        return [
            new ConfigGroup("Group1",
            [
                i1=new StringItem("Name G1I1 String", "foo", /.*/, "G1_1_S"),
                i2=new StringItem("Name G1I2 String", "BAR", /.*/, "G1_2_S"),
            ]),
            new ConfigGroup("Group2",
            [
                new StringItem("G2I1 String", "BAR", /.*/, "G2_1_S"),
                new IntegerItem("G2I2 Int", 1, 0, 100000, 1, "G2_2_I"),
                new BooleanItem("G2I3 Bool", true, "G2_3_B"),
                new EnumItem("G2I4 Enum", ["Hund", "Katze", "Maus", "Ente"], "G2_4_E"),
            ]),
            new ConfigGroup("webmanager",[
                new StringItem("ssid", "Woeste123", /.*/),
                new StringItem("password", "wrong", /.*/),
            ])
        ];
    }
}