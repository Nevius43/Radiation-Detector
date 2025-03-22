#include "dashboard.h"

// Declare the global variables defined in your main code.
extern float currentuSvHr;
extern float averageuSvHr;
extern float maxuSvHr;
extern float cumulativeDosemSv;

// External access to chart data
extern float chart1Buffer[];
extern float chart3Buffer[];
extern const int CHART1_SEGMENTS;
extern const int CHART3_SEGMENTS;

// Helper function to format radiation data as JSON
String getRadiationDataJson() {
  String json = "{";
  json += "\"current\":" + String(currentuSvHr) + ",";
  json += "\"average\":" + String(averageuSvHr) + ",";
  json += "\"maximum\":" + String(maxuSvHr) + ",";
  json += "\"cumulative\":" + String(cumulativeDosemSv) + ",";
  
  // Add Chart1 data (1-hour chart with 3-minute intervals)
  json += "\"hourly\":[";
  for (int i = 0; i < CHART1_SEGMENTS; i++) {
    if (i > 0) json += ",";
    json += String(chart1Buffer[i]);
  }
  json += "],";
  
  // Add Chart3 data (24-hour chart with 1-hour intervals)
  json += "\"daily\":[";
  for (int i = 0; i < CHART3_SEGMENTS; i++) {
    if (i > 0) json += ",";
    json += String(chart3Buffer[i]);
  }
  json += "]}";
  
  return json;
}

String getDashboardPage() {
  String page = "";
  page += "<!DOCTYPE html><html lang='en'><head>";
  page += "<meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  page += "<title>Radiation Detector Dashboard</title>";
  // Add Chart.js for visualizations
  page += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";
  // Embedded CSS for modern, elegant styling.
  page += "<style>";
  page += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #f0f2f5; margin: 0; padding: 0; }";
  page += "header { background-color: #4CAF50; color: white; padding: 20px 10px; text-align: center; margin-bottom: 20px; }";
  page += "h1 { margin: 0; font-size: 2em; }";
  page += ".container { display: flex; flex-direction: column; align-items: center; padding: 20px; }";
  page += ".cards { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; width: 100%; max-width: 1000px; margin: 20px 0; }";
  page += ".card { background-color: white; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); padding: 20px; text-align: center; transition: transform 0.3s; }";
  page += ".card:hover { transform: translateY(-5px); }";
  page += ".card h2 { margin: 0 0 10px 0; font-size: 1.5em; color: #333; }";
  page += ".card p { margin: 0; font-size: 2em; color: #4CAF50; }";
  page += ".chart-container { width: 100%; max-width: 1000px; margin: 20px 0; background-color: white; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); padding: 20px; }";
  page += ".chart-row { display: flex; flex-wrap: wrap; gap: 20px; width: 100%; max-width: 1000px; }";
  page += ".chart-card { flex: 1; min-width: 300px; background-color: white; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); padding: 20px; }";
  page += ".buttons { margin-top: 30px; }";
  page += ".btn { display: inline-block; padding: 15px 25px; font-size: 1em; color: white; background-color: #007BFF; text-decoration: none; border-radius: 5px; transition: background-color 0.3s; margin: 0 10px; }";
  page += ".btn:hover { background-color: #0056b3; }";
  page += ".gauge-container { position: relative; width: 200px; height: 200px; margin: 0 auto; }";
  page += "footer { margin-top: 40px; font-size: 0.9em; color: #777; }";
  page += ".radiation-level { text-align: center; margin-top: 10px; font-weight: bold; }";
  page += ".radiation-safe { color: #4CAF50; }";
  page += ".radiation-moderate { color: #FFC107; }";
  page += ".radiation-high { color: #FF5722; }";
  page += ".radiation-extreme { color: #F44336; }";
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
  page += "<p id='current-radiation'>" + String(currentuSvHr, 2) + " uSv/h</p>";
  page += "</div>";
  
  // Card: Average Radiation
  page += "<div class='card'>";
  page += "<h2>Average</h2>";
  page += "<p id='average-radiation'>" + String(averageuSvHr, 2) + " uSv/h</p>";
  page += "</div>";
  
  // Card: Maximum Radiation
  page += "<div class='card'>";
  page += "<h2>Maximum</h2>";
  page += "<p id='maximum-radiation'>" + String(maxuSvHr, 2) + " uSv/h</p>";
  page += "</div>";
  
  // Card: Cumulative Dose
  page += "<div class='card'>";
  page += "<h2>Cumulative Dose</h2>";
  page += "<p id='cumulative-dose'>" + String(cumulativeDosemSv, 2) + " mSv</p>";
  page += "</div>";
  
  page += "</div>"; // end cards

  // Chart section
  page += "<div class='chart-row'>";
  
  // Gauge chart for current radiation level
  page += "<div class='chart-card'>";
  page += "<h2>Radiation Level</h2>";
  page += "<div class='gauge-container'><canvas id='gauge-chart'></canvas></div>";
  
  // Add radiation level indicator
  page += "<div id='radiation-level' class='radiation-level radiation-safe'>Safe Level</div>";
  page += "</div>";

  // Line chart for Chart1 data (1 hour)
  page += "<div class='chart-card'>";
  page += "<h2>Hourly Radiation (3-min intervals)</h2>";
  page += "<canvas id='hourly-chart'></canvas>";
  page += "</div>";
  
  page += "</div>"; // end chart row

  // Second chart row
  page += "<div class='chart-row'>";
  
  // Line chart for Chart3 data (24 hours)
  page += "<div class='chart-card'>";
  page += "<h2>Daily Radiation (1-hour intervals)</h2>";
  page += "<canvas id='daily-chart'></canvas>";
  page += "</div>";
  
  page += "</div>"; // end second chart row
  
  // Add data endpoint for AJAX refreshing
  page += "<div style='display:none;' id='chart-data'>";
  page += getRadiationDataJson();
  page += "</div>";
  
  // Buttons: Refresh and OTA Update
  page += "<div class='buttons'>";
  page += "<a class='btn' href='javascript:void(0)' onclick='refreshData()'>Refresh Data</a>";
  page += "<a class='btn' href='/update'>OTA Update</a>";
  page += "</div>";
  
  // Footer
  page += "<footer>&copy; 2025 Pablo Morán Peña. All rights reserved.</footer>";
  
  page += "</div>"; // end container

  // JavaScript for charts and auto-refresh
  page += "<script>";
  // Function to get chart data and update the UI
  page += "let hourlyChart, dailyChart, gaugeChart;";
  page += "function initCharts() {";
  // Parse the chart data
  page += "  const chartDataElement = document.getElementById('chart-data');";
  page += "  const chartData = JSON.parse(chartDataElement.textContent);";
  
  // Create labels for hourly chart (3-minute intervals)
  page += "  const hourlyLabels = [];";
  page += "  for (let i = 0; i < " + String(CHART1_SEGMENTS) + "; i++) {";
  page += "    const mins = (i * 3) % 60;";
  page += "    const hours = Math.floor((i * 3) / 60);";
  page += "    hourlyLabels.push(`${hours}h:${mins.toString().padStart(2, '0')}m`);";
  page += "  }";
  
  // Create labels for daily chart (1-hour intervals)
  page += "  const dailyLabels = [];";
  page += "  for (let i = 0; i < " + String(CHART3_SEGMENTS) + "; i++) {";
  page += "    dailyLabels.push(`${i}h`);";
  page += "  }";
  
  // Create hourly chart
  page += "  const hourlyCtx = document.getElementById('hourly-chart').getContext('2d');";
  page += "  hourlyChart = new Chart(hourlyCtx, {";
  page += "    type: 'line',";
  page += "    data: {";
  page += "      labels: hourlyLabels,";
  page += "      datasets: [{";
  page += "        label: 'µSv/h',";
  page += "        data: chartData.hourly,";
  page += "        borderColor: '#4CAF50',";
  page += "        backgroundColor: 'rgba(76, 175, 80, 0.1)',";
  page += "        fill: true,";
  page += "        tension: 0.3";
  page += "      }]";
  page += "    },";
  page += "    options: {";
  page += "      responsive: true,";
  page += "      plugins: {";
  page += "        legend: { position: 'top' }";
  page += "      },";
  page += "      scales: {";
  page += "        y: { beginAtZero: true }";
  page += "      }";
  page += "    }";
  page += "  });";
  
  // Create daily chart
  page += "  const dailyCtx = document.getElementById('daily-chart').getContext('2d');";
  page += "  dailyChart = new Chart(dailyCtx, {";
  page += "    type: 'line',";
  page += "    data: {";
  page += "      labels: dailyLabels,";
  page += "      datasets: [{";
  page += "        label: 'µSv/h',";
  page += "        data: chartData.daily,";
  page += "        borderColor: '#2196F3',";
  page += "        backgroundColor: 'rgba(33, 150, 243, 0.1)',";
  page += "        fill: true,";
  page += "        tension: 0.3";
  page += "      }]";
  page += "    },";
  page += "    options: {";
  page += "      responsive: true,";
  page += "      plugins: {";
  page += "        legend: { position: 'top' }";
  page += "      },";
  page += "      scales: {";
  page += "        y: { beginAtZero: true }";
  page += "      }";
  page += "    }";
  page += "  });";
  
  // Create gauge chart
  page += "  const gaugeCtx = document.getElementById('gauge-chart').getContext('2d');";
  page += "  gaugeChart = new Chart(gaugeCtx, {";
  page += "    type: 'doughnut',";
  page += "    data: {";
  page += "      labels: ['Current', 'Remaining'],";
  page += "      datasets: [{";
  page += "        data: [chartData.current, 10 - Math.min(chartData.current, 10)],";
  page += "        backgroundColor: [getRadiationColor(chartData.current), '#ECEFF1'],";
  page += "        borderWidth: 0";
  page += "      }]";
  page += "    },";
  page += "    options: {";
  page += "      responsive: true,";
  page += "      circumference: 180,";
  page += "      rotation: 270,";
  page += "      cutout: '70%',";
  page += "      plugins: {";
  page += "        legend: { display: false },";
  page += "        tooltip: { enabled: false }";
  page += "      }";
  page += "    }";
  page += "  });";
  
  // Update radiation level text
  page += "  updateRadiationLevelText(chartData.current);";
  page += "}";
  
  // Function to determine color based on radiation level
  page += "function getRadiationColor(value) {";
  page += "  if (value < 0.5) return '#4CAF50';"; // Safe
  page += "  if (value < 2.5) return '#FFC107';"; // Moderate
  page += "  if (value < 5) return '#FF5722';";   // High
  page += "  return '#F44336';";                 // Extreme
  page += "}";
  
  // Function to update radiation level text
  page += "function updateRadiationLevelText(value) {";
  page += "  const levelElement = document.getElementById('radiation-level');";
  page += "  levelElement.className = 'radiation-level';";
  page += "  if (value < 0.5) {";
  page += "    levelElement.textContent = 'Safe Level';";
  page += "    levelElement.classList.add('radiation-safe');";
  page += "  } else if (value < 2.5) {";
  page += "    levelElement.textContent = 'Moderate Level';";
  page += "    levelElement.classList.add('radiation-moderate');";
  page += "  } else if (value < 5) {";
  page += "    levelElement.textContent = 'High Level';";
  page += "    levelElement.classList.add('radiation-high');";
  page += "  } else {";
  page += "    levelElement.textContent = 'Extreme Level';";
  page += "    levelElement.classList.add('radiation-extreme');";
  page += "  }";
  page += "}";
  
  // Function to refresh data via AJAX
  page += "function refreshData() {";
  page += "  fetch('/api/data')";
  page += "    .then(response => response.json())";
  page += "    .then(data => {";
  // Update the values
  page += "      document.getElementById('current-radiation').textContent = data.current.toFixed(2) + ' uSv/h';";
  page += "      document.getElementById('average-radiation').textContent = data.average.toFixed(2) + ' uSv/h';";
  page += "      document.getElementById('maximum-radiation').textContent = data.maximum.toFixed(2) + ' uSv/h';";
  page += "      document.getElementById('cumulative-dose').textContent = data.cumulative.toFixed(2) + ' mSv';";
  
  // Update charts
  page += "      hourlyChart.data.datasets[0].data = data.hourly;";
  page += "      hourlyChart.update();";
  page += "      dailyChart.data.datasets[0].data = data.daily;";
  page += "      dailyChart.update();";
  
  // Update gauge chart
  page += "      gaugeChart.data.datasets[0].data = [data.current, 10 - Math.min(data.current, 10)];";
  page += "      gaugeChart.data.datasets[0].backgroundColor = [getRadiationColor(data.current), '#ECEFF1'];";
  page += "      gaugeChart.update();";
  
  // Update radiation level text
  page += "      updateRadiationLevelText(data.current);";
  page += "    })";
  page += "    .catch(error => console.error('Error refreshing data:', error));";
  page += "}";
  
  // Initialize charts on page load
  page += "document.addEventListener('DOMContentLoaded', initCharts);";
  
  // Auto-refresh every 30 seconds
  page += "setInterval(refreshData, 30000);";
  
  page += "</script>";
  
  page += "</body></html>";
  
  return page;
}
