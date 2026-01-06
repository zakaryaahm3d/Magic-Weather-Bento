#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define NOMINMAX

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <thread>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wininet.lib")

#include "WeatherEngine.hpp"
#include "NetworkUtils.hpp"

using namespace std;

WeatherEngine engine;

// Helper to decode URL params manually (simple version)
string urlDecode(string str) {
    string ret;
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '%') {
            if (i + 2 < str.length()) {
                int value;
                sscanf(str.substr(i + 1, 2).c_str(), "%x", &value);
                ret += static_cast<char>(value);
                i += 2;
            }
        }
        else if (str[i] == '+') {
            ret += ' ';
        }
        else {
            ret += str[i];
        }
    }
    return ret;
}

void handleClient(SOCKET clientSock) {
    char buffer[4096];
    int bytesReceived = recv(clientSock, buffer, 4096, 0);

    if (bytesReceived <= 0) {
        closesocket(clientSock);
        return;
    }

    string request(buffer, bytesReceived);
    stringstream ss(request);
    string method, url;
    ss >> method >> url;

    // --- ROUTING LOGIC ---
    if (url == "/" || url == "/index.html") {
        string html = SimpleServer::loadHtmlFile("index.html");
        SimpleServer::sendResponse(clientSock, html, "text/html");
    }
    else if (url.find("/news") != string::npos) {
        string city = SimpleServer::getQueryParam(url, "city");
        city = urlDecode(city);
        if (city.empty()) city = "Topi";

        vector<NewsItem> news = engine.getCityNews(city);

        stringstream json;
        json << "[";
        for (size_t i = 0; i < news.size(); i++) {
            json << "{";
            json << "\"city\": \"" << news[i].city << "\",";
            json << "\"headline\": \"" << news[i].headline << "\",";
            json << "\"category\": \"" << news[i].category << "\"";
            json << "}" << (i < news.size() - 1 ? "," : "");
        }
        json << "]";
        SimpleServer::sendResponse(clientSock, json.str(), "application/json");
    }
    else if (url == "/cities") {
        vector<string> cities = engine.getCityList();
        stringstream json;
        json << "[";
        for (size_t i = 0; i < cities.size(); i++) {
            json << "\"" << cities[i] << "\"" << (i < cities.size() - 1 ? "," : "");
        }
        json << "]";
        SimpleServer::sendResponse(clientSock, json.str(), "application/json");
    }
    else if (url.find("/predict") != string::npos) {
        string city = SimpleServer::getQueryParam(url, "city");
        string activity = SimpleServer::getQueryParam(url, "activity");

        city = urlDecode(city);
        activity = urlDecode(activity);

        engine.updateCity(city);
        City* c = engine.getCity(city);

        stringstream json;
        if (c) {
            ActivityResult res = engine.predictActivitySuitability(*c, activity);
            json << "{ \"score\": \"" << res.score << "\", \"message\": \"" << res.message << "\", \"color\": \"" << res.color << "\" }";
        }
        else {
            json << "{ \"score\": \"Unknown\", \"message\": \"City not found.\", \"color\": \"#94a3b8\" }";
        }
        SimpleServer::sendResponse(clientSock, json.str(), "application/json");
    }
    else if (url.find("/route") != string::npos) {
        string start = SimpleServer::getQueryParam(url, "start");
        string end = SimpleServer::getQueryParam(url, "end");

        start = urlDecode(start);
        end = urlDecode(end);

        engine.updateCity(start);
        engine.updateCity(end);

        vector<string> path = engine.findBestRoute(start, end);

        stringstream json;
        json << "{ \"path\": [";
        for (size_t i = 0; i < path.size(); i++) {
            json << "\"" << path[i] << "\"" << (i < path.size() - 1 ? "," : "");
        }
        json << "] }";
        SimpleServer::sendResponse(clientSock, json.str(), "application/json");
    }
    else if (url.find("/data") != string::npos) {
        string cityName = SimpleServer::getQueryParam(url, "city");
        cityName = urlDecode(cityName);
        if (cityName.empty()) cityName = "Topi";

        engine.updateCity(cityName);
        City* c = engine.getCity(cityName);

        if (c) {
            stringstream json;
            json << "{";
            json << "\"city\": \"" << c->name << "\",";
            json << "\"lat\": " << c->lat << ",";
            json << "\"lon\": " << c->lon << ",";
            json << "\"current\": {";
            json << "\"temperature_2d\": " << c->temp << ",";
            json << "\"wind_speed_10m\": " << c->wind << ",";
            json << "\"relative_humidity_2d\": " << c->humidity << ",";
            json << "\"rain\": 0,";
            json << "\"aqi\": 45,";
            json << "\"wind_dir\": " << c->wind_dir << ",";
            json << "\"condition\": \"" << c->condition << "\"";
            json << "},";

            auto lifestyle = engine.calculateLifestyleIndices(*c);
            json << "\"lifestyle\": {";
            json << "\"drone\": \"" << lifestyle["Drone"] << "\",";
            json << "\"running\": \"" << lifestyle["Running"] << "\",";
            json << "\"bbq\": \"" << lifestyle["BBQ"] << "\"";
            json << "},";

            json << "\"hourly\": ["; for (size_t i = 0; i < c->hourlyData.size(); i++) json << c->hourlyData[i] << (i < c->hourlyData.size() - 1 ? "," : ""); json << "],";
            json << "\"weekly\": ["; for (size_t i = 0; i < c->weeklyData.size(); i++) json << c->weeklyData[i] << (i < c->weeklyData.size() - 1 ? "," : ""); json << "],";
            json << "\"monthly\": ["; for (size_t i = 0; i < c->monthlyData.size(); i++) json << c->monthlyData[i] << (i < c->monthlyData.size() - 1 ? "," : ""); json << "],";
            json << "\"yearly\": ["; for (size_t i = 0; i < c->yearlyData.size(); i++) json << c->yearlyData[i] << (i < c->yearlyData.size() - 1 ? "," : ""); json << "],";

            json << "\"forecast\": [";
            for (size_t i = 0; i < c->tenDayForecast.size(); i++) {
                json << "{ \"day\": \"" << c->tenDayForecast[i].dayName
                    << "\", \"high\": " << c->tenDayForecast[i].high
                    << ", \"low\": " << c->tenDayForecast[i].low
                    << ", \"rain_prob\": " << c->tenDayForecast[i].rain_prob
                    << ", \"cond\": \"" << c->tenDayForecast[i].condition << "\" }";
                if (i < c->tenDayForecast.size() - 1) json << ",";
            }
            json << "],";

            json << "\"alerts\": [";
            for (size_t i = 0; i < c->activeAlerts.size(); i++) {
                json << "\"" << c->activeAlerts[i] << "\"" << (i < c->activeAlerts.size() - 1 ? "," : "");
            }
            json << "],";

            vector<string> neighbors = engine.getNeighbors(cityName);
            json << "\"neighbors\": [";
            for (size_t i = 0; i < neighbors.size(); i++) {
                json << "\"" << neighbors[i] << "\"" << (i < neighbors.size() - 1 ? "," : "");
            }
            json << "],";

            vector<City> hottest = engine.getHottestCities(5);
            json << "\"hottest_cities\": [";
            for (size_t i = 0; i < hottest.size(); i++) {
                json << "{\"name\": \"" << hottest[i].name << "\", \"temp\": " << hottest[i].temp << "}" << (i < hottest.size() - 1 ? "," : "");
            }
            json << "]";

            json << "}";
            SimpleServer::sendResponse(clientSock, json.str(), "application/json");
        }
        else {
            SimpleServer::sendResponse(clientSock, "{}", "application/json");
        }
    }
    else {
        SimpleServer::sendResponse(clientSock, "404 Not Found", "text/plain");
    }

    closesocket(clientSock);
}

void initRealCities() {
    cout << "Initializing Massive City Database..." << endl;

    // 1. Add Cities (Nodes)
    engine.addCity({ "Topi", 34.07, 72.63 });
    engine.addCity({ "Islamabad", 33.68, 73.04 });
    engine.addCity({ "Lahore", 31.55, 74.34 });
    engine.addCity({ "Karachi", 24.86, 67.01 });
    engine.addCity({ "Peshawar", 34.01, 71.56 });
    engine.addCity({ "Quetta", 30.18, 67.00 });
    engine.addCity({ "Multan", 30.20, 71.47 });
    engine.addCity({ "Faisalabad", 31.42, 73.09 });
    engine.addCity({ "Rawalpindi", 33.60, 73.04 });
    engine.addCity({ "Hyderabad", 25.39, 68.35 });
    engine.addCity({ "Sialkot", 32.49, 74.52 });
    engine.addCity({ "Abbottabad", 34.16, 73.22 });
    engine.addCity({ "Murree", 33.90, 73.39 });

    // 2. Establish Fully Connected Backbone (Edges)
    // This ensures almost every city can reach every other city via logical paths.

    // --- NORTH NETWORK ---
    engine.addRoute("Peshawar", "Topi");
    engine.addRoute("Peshawar", "Islamabad"); // M1
    engine.addRoute("Topi", "Islamabad");
    engine.addRoute("Topi", "Abbottabad");
    engine.addRoute("Abbottabad", "Murree");
    engine.addRoute("Abbottabad", "Islamabad"); // Hazara Motorway link
    engine.addRoute("Murree", "Islamabad");
    engine.addRoute("Islamabad", "Rawalpindi"); // Twin cities
    engine.addRoute("Peshawar", "Rawalpindi");

    // --- CENTRAL NETWORK (PUNJAB) ---
    engine.addRoute("Islamabad", "Lahore"); // M2
    engine.addRoute("Islamabad", "Faisalabad"); // M2 -> M4
    engine.addRoute("Islamabad", "Sialkot"); // Sialkot-Lahore Motorway link
    engine.addRoute("Rawalpindi", "Lahore");
    engine.addRoute("Lahore", "Sialkot");
    engine.addRoute("Lahore", "Faisalabad"); // M3
    engine.addRoute("Lahore", "Multan"); // M3 -> M4 / M5 start
    engine.addRoute("Faisalabad", "Multan"); // M4

    // --- SOUTH NETWORK (SINDH & BALOCHISTAN) ---
    engine.addRoute("Multan", "Hyderabad"); // N5 / Motorway logical link
    engine.addRoute("Multan", "Karachi"); // Long haul logical link
    engine.addRoute("Hyderabad", "Karachi"); // M9

    // --- WESTERN LINKS (QUETTA) ---
    engine.addRoute("Quetta", "Karachi"); // N25
    engine.addRoute("Quetta", "Multan"); // N70
    engine.addRoute("Quetta", "Hyderabad"); // Via Sukkur (logical)
    engine.addRoute("Quetta", "Peshawar"); // N50 (Long route, but valid for graph)

    // Initial load to prevent empty state
    engine.updateCity("Topi");
}

int main() {
    SimpleServer::initWinsock();
    initRealCities();

    SOCKET serverSock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "Bind failed!" << endl;
        return 1;
    }
    listen(serverSock, 10);

    cout << "DSA Weather Server running on http://localhost:8080" << endl;
    while (true) {
        SOCKET clientSock = accept(serverSock, nullptr, nullptr);
        if (clientSock != INVALID_SOCKET) {
            thread(handleClient, clientSock).detach();
        }
    }
    return 0;
}