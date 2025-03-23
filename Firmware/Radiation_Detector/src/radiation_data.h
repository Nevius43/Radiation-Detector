#ifndef RADIATION_DATA_H
#define RADIATION_DATA_H

// Export getRadiationDataJson function to be used by dashboard.cpp
String getRadiationDataJsonExport();

// Rename cumulativemSv to cumulativeDosemSv for dashboard.cpp
// This is an extern declaration of the variable defined in Radiation-Detector.cpp
extern float cumulativemSv;
// This creates an alias for the dashboard.cpp to use
#define cumulativeDosemSv cumulativemSv

// Also export other radiation data variables needed by dashboard.cpp
extern float currentuSvHr;
extern float averageuSvHr;
extern float maxuSvHr;
extern float chart1Buffer[];
extern float chart3Buffer[];

#endif // RADIATION_DATA_H 