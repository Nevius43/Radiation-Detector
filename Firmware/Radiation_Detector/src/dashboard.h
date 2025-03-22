#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <Arduino.h>

// Returns the complete HTML page for the dashboard.
String getDashboardPage();

// Returns the radiation data as JSON for API access
String getRadiationDataJson();

// External declarations for chart data (defined in Radiation-Detector.cpp)
extern float chart1Buffer[];
extern float chart3Buffer[];
extern const int CHART1_SEGMENTS;
extern const int CHART3_SEGMENTS;

#endif
