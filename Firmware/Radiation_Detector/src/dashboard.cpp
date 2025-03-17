#include "dashboard.h"

// Declare the global variables defined in your main code.
extern float currentuSvHr;
extern float averageuSvHr;
extern float maxuSvHr;
extern float cumulativeDosemSv;

String getDashboardPage() {
  String page = "";
  page += "<!DOCTYPE html><html lang='en'><head>";
  page += "<meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  page += "<title>Radiation Detector Dashboard</title>";
  // Embedded CSS for modern, elegant styling.
  page += "<style>";
  page += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #f0f2f5; margin: 0; padding: 0; }";
  page += "header { background-color: #4CAF50; color: white; padding: 20px 10px; text-align: center; }";
  page += "h1 { margin: 0; font-size: 2em; }";
  page += ".container { display: flex; flex-direction: column; align-items: center; padding: 20px; }";
  page += ".cards { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; width: 100%; max-width: 1000px; margin: 20px 0; }";
  page += ".card { background-color: white; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); padding: 20px; text-align: center; transition: transform 0.3s; }";
  page += ".card:hover { transform: translateY(-5px); }";
  page += ".card h2 { margin: 0 0 10px 0; font-size: 1.5em; color: #333; }";
  page += ".card p { margin: 0; font-size: 2em; color: #4CAF50; }";
  page += ".buttons { margin-top: 30px; }";
  page += ".btn { display: inline-block; padding: 15px 25px; font-size: 1em; color: white; background-color: #007BFF; text-decoration: none; border-radius: 5px; transition: background-color 0.3s; margin: 0 10px; }";
  page += ".btn:hover { background-color: #0056b3; }";
  page += "footer { margin-top: 40px; font-size: 0.9em; color: #777; }";
  page += "</style>";
  page += "</head><body>";
  
  // Header section
  page += "<header><h1>Radiation Detector Dashboard</h1></header>";
  
  // Main container
  page += "<div class='container'>";
  
  // Cards section for sensor metrics
  page += "<div class='cards'>";
  
  // Card: Current Radiation
  page += "<div class='card'>";
  page += "<h2>Current</h2>";
  page += "<p>" + String(currentuSvHr, 2) + " uSv/h</p>";
  page += "</div>";
  
  // Card: Average Radiation
  page += "<div class='card'>";
  page += "<h2>Average</h2>";
  page += "<p>" + String(averageuSvHr, 2) + " uSv/h</p>";
  page += "</div>";
  
  // Card: Maximum Radiation
  page += "<div class='card'>";
  page += "<h2>Maximum</h2>";
  page += "<p>" + String(maxuSvHr, 2) + " uSv/h</p>";
  page += "</div>";
  
  // Card: Cumulative Dose
  page += "<div class='card'>";
  page += "<h2>Cumulative Dose</h2>";
  page += "<p>" + String(cumulativeDosemSv, 2) + " mSv</p>";
  page += "</div>";
  
  page += "</div>"; // end cards
  
  // Buttons: Refresh and OTA Update
  page += "<div class='buttons'>";
  page += "<a class='btn' href='/'>Refresh</a>";
  page += "<a class='btn' href='/update'>OTA Update</a>";
  page += "</div>";
  
  // Footer
  page += "<footer>&copy; 2025 Pablo Morán Peña. All rights reserved.</footer>";
  
  page += "</div>"; // end container
  
  page += "</body></html>";
  
  return page;
}
