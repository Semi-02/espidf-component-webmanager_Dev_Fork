#pragma once
#include <stdio.h>
#include <cinttypes>
#include <cmath>
#include <ctime>
#include <chrono>
#include "sunsetsunrise.hh"
namespace sunsetsunrise
{
    constexpr double pi{M_PI};
    constexpr double RAD{M_PI / 180.0};
    constexpr double pi2{2.0f * M_PI};
    constexpr double JD2000 = 2451545.0;

   

    uint32_t JulianDate(time_t unixSecs)
    {
        return (unixSecs / 86400.0) + 2440587.5;
    }

    double InPi(double x)
    {
        int n = (int)(x / pi2);
        x = x - n * pi2;
        if (x < 0)
            x += pi2;
        return x;
    }
    
    double eps(double T) // Neigung der Erdachse
    {
        return RAD*(23.43929111 + (-46.8150*(T/36525.0f) - 0.00059*(T/36525.0f)*(T/36525.0f) + 0.001813*(T/36525.0f)*(T/36525.0f)*(T/36525.0f))/3600.0);
    }


    // Time formula
    // Tdays is the number of days since Jan 1 2000, and replaces T as the Tropical Century. T = Tdays / 36525.0
    double TimeFormulaDouble(double *DK, double Tdays)
    {

        double RA_Mean = 18.71506921 + (2400.0513369 / 36525.0) * Tdays;//+(2.5862e-5 - 1.72e-9*T)*T*T;; // we keep only first order value as T is between 0.20 and 0.30
        double M = InPi((pi2 * 0.993133) + (pi2 * 99.997361 / 36525.0) * Tdays);
        double L = InPi((pi2 * 0.7859453) + M + (6893.0 * sin(M) + 72.0 * sin(M + M) + (6191.2 / 36525.0) * Tdays) * (pi2 / 1296.0e3));

        //double e = eps(Tdays);
        //double cos_eps=cos(e);
        //double sin_eps=sin(e);
        double cos_eps = 0.91750f;     // precompute cos(eps) ->okkl!
        double sin_eps = 0.39773f;     // precompute sin(eps) -->okkl
	    double RA = atan(tan(L)*cos_eps);

        if (RA < 0.0f)
            RA += pi;
        if (L > pi)
            RA += pi;
        RA = RA * (24.0f / pi2);
        *DK = asin(sin_eps * sin(L));

        RA_Mean = 24.0 * InPi(pi2 * RA_Mean / 24.0) / pi2;
        double dRA = dRA = RA_Mean - RA;
        if (dRA < -12.0)
            dRA += 24.0;
        if (dRA > 12.0)
            dRA -= 24.0;

        dRA = dRA * 1.0027379;
        return dRA;
    }

    void DuskTillDawn(double julianDay, double latitudeRadiant, double longitudeRadiant, eDawn dawn, double &sunriseHour, double &sunsetHour)
    {

        double Tdays = julianDay - JD2000; // number of days since Jan 1 2000

        // ex 2458977 (2020 May 7) - 2451545 -> 7432 -> 0,2034
        double DK{0};
        const double sin_h = (double)dawn/(double)1E9;
        const float Zeitzone = 1; 
        double Zeitgleichung = TimeFormulaDouble(&DK, Tdays);
       
        double Zeitdifferenz = 12.0*acos((sin_h - sin(latitudeRadiant) * sin(DK)) / (cos(latitudeRadiant) * cos(DK)))/pi;
        // printf("Zeitgleichung: DK:%f return %f; Zeitdifferenz=%f; sin_h=%f; sin(lat)=%f\n", DK, Zeitgleichung, Zeitdifferenz, sin_h, sin(latitudeRadiant));
        double AufgangOrtszeit = 12.0 - Zeitdifferenz - Zeitgleichung;
        double UntergangOrtszeit = 12.0f + Zeitdifferenz - Zeitgleichung;
        double AufgangWeltzeit = AufgangOrtszeit - longitudeRadiant/(RAD * 15.0f);
        double UntergangWeltzeit = UntergangOrtszeit - longitudeRadiant/(RAD * 15.0f);
        double Aufgang = AufgangWeltzeit + Zeitzone + (1 / 120.0f); // In Stunden, with rounding to nearest minute (1/60 * .5)
        double Untergang = UntergangWeltzeit + Zeitzone;

        sunriseHour = std::fmod(Aufgang, 24.0f); // force 0 <= x < 24.0
        sunsetHour = std::fmod(Untergang, 24.0f);      
    }
}
/*
int main()
{
    double today = 2460318.5;
    double latDeg = 52.0965;
    double lonDeg = 7.6171;
    double Aufgang;
    double Untergang;
    sunsetsunrise::DuskTillDawn(today, latDeg * sunsetsunrise::RAD, lonDeg * sunsetsunrise::RAD, sunsetsunrise::DAWN_NORMAL, Aufgang, Untergang);
    int AufgangStunden = (int)Aufgang;
    int AufgangMinuten = (int)(60.0f * std::fmod(Aufgang, 1.0f));

    int UntergangStunden = (int)Untergang;
    int UntergangMinuten = (int)(60.0f * std::fmod(Untergang, 1.0f));
    printf("Aufgang: %d:%d, Untergang %d:%d", AufgangStunden, AufgangMinuten, UntergangStunden, UntergangMinuten);
    return 0;
}
*/
