function decodeUplink(input) {
    var data = {};
    if (input.fPort != 2) {
        return { errors: ["unknown FPort"] }
    }
    if (input.bytes.length != 8) {
        return { errors: ["No Sensor"] }
    }
    data.batteryVoltage_mV = (input.bytes[0] << 8 | input.bytes[1]) & 0x3FFF;
    data.distance_mm = input.bytes[2] << 8 | input.bytes[3];
    data.temperature_degCelsius = (input.bytes[5] << 8 | input.bytes[6]) / 10;

    return { data: data };
}