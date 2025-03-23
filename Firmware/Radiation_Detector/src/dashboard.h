#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <Arduino.h>
#include "radiation_data.h" // Include our new header for radiation data exports

// Returns the complete HTML page for the dashboard.
String getDashboardPage();

// Chart size constants
#define CHART1_SIZE 20
#define CHART3_SIZE 24

// These are declared in Radiation-Detector.cpp
// Not using extern here to avoid conflicts with static declarations

#endif
