#pragma once
#include <cinttypes>
#include <cmath>


namespace sunsetsunrise
{
    // Sunrise and Sunset DawnType
    enum class eDawn:int32_t{ //Value is 1E9*sin(dawn angle)
        NORMAL    = (int32_t)(1E9*sin( -0.8333 * M_PI / 180.0)),
        CIVIL     = (int32_t)(1E9*sin( -6.0000 * M_PI / 180.0)),
        NAUTIC    = (int32_t)(1E9*sin(-12.0000 * M_PI / 180.0)),
        ASTRONOMIC= (int32_t)(1E9*sin(-18.0000 * M_PI / 180.0)),
    };

    uint32_t JulianDate(time_t unixSecs);
    // Time formula
    // Tdays is the number of days since Jan 1 2000, and replaces T as the Tropical Century. T = Tdays / 36525.0
    double TimeFormulaDouble(double *DK, double Tdays);
    void DuskTillDawn(double julianDay, double latitudeRadiant, double longitudeRadiant, eDawn dawn, double &sunriseHour, double &sunsetHour);
}

