#ifndef WEATHER_ENGINE_HPP
#define WEATHER_ENGINE_HPP

#define _CRT_SECURE_NO_WARNINGS 

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <stack>
#include <mutex>
#include <cmath>
#include <sstream> 
#include <ctime> 
#include <cstdio>
#include "NetworkUtils.hpp"

// --- DATA MODELS ---

struct DailyForecast {
    std::string dayName;
    int high; int low; int rain_prob;
    std::string condition;
};

struct City {
    std::string name;
    double lat, lon;
    int temp = 0; int humidity = 0; int wind = 0; int wind_dir = 0;
    std::string condition = "Loading...";

    std::vector<int> hourlyData;
    std::vector<int> weeklyData;
    std::vector<int> monthlyData;
    std::vector<int> yearlyData;

    std::vector<DailyForecast> tenDayForecast;
    std::vector<std::string> activeAlerts;
    std::vector<std::string> weatherNews;
};

struct Alert {
    int severity; std::string message; std::string city;
    bool operator<(const Alert& other) const { return severity < other.severity; }
};

struct GraphNode {
    std::string name; int cost;
    bool operator>(const GraphNode& other) const { return cost > other.cost; }
};

struct ActivityResult {
    std::string score; std::string message; std::string color;
};

struct NewsItem {
    std::string city;
    std::string headline;
    std::string category;
};

class WeatherEngine {
private:
    std::unordered_map<std::string, City> cityDatabase;
    std::unordered_map<std::string, std::vector<std::string>> cityGraph;
    std::priority_queue<Alert> alertSystem;
    std::stack<std::string> requestLogs;
    std::mutex engineMutex;

    // --- UTILS ---

    std::string getDayName(const std::string& dateStr) {
        int y, m, d;
        if (sscanf(dateStr.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return dateStr;

        std::tm t = { 0 };
        t.tm_year = y - 1900;
        t.tm_mon = m - 1;
        t.tm_mday = d;
        t.tm_hour = 12;

        std::mktime(&t);
        const char* days[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
        if (t.tm_wday >= 0 && t.tm_wday < 7) return days[t.tm_wday];
        return "Day";
    }

    std::string toLower(const std::string& str) {
        std::string lowerStr = str;
        std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
        return lowerStr;
    }

    double extractJsonValue(const std::string& json, const std::string& key, size_t startPos = 0) {
        size_t pos = json.find("\"" + key + "\":", startPos);
        if (pos == std::string::npos) return 0.0;
        size_t valueStart = pos + key.length() + 3;
        size_t end = json.find_first_of(",}", valueStart);
        if (end == std::string::npos) return 0.0;
        std::string valStr = json.substr(valueStart, end - valueStart);
        valStr.erase(std::remove(valStr.begin(), valStr.end(), '\"'), valStr.end());
        try { return std::stod(valStr); }
        catch (...) { return 0.0; }
    }

    std::string decodeWeatherCode(int code) {
        if (code == 0) return "Sunny";
        if (code >= 1 && code <= 3) return "Cloudy";
        if (code >= 45 && code <= 48) return "Foggy";
        if (code >= 51 && code <= 67) return "Rainy";
        if (code >= 71 && code <= 77) return "Snow";
        if (code >= 80 && code <= 99) return "Stormy";
        return "Unknown";
    }

    std::vector<double> parseJsonArray(const std::string& json, const std::string& key, size_t startPos, size_t limit) {
        std::vector<double> result;
        size_t keyPos = json.find("\"" + key + "\":", startPos);
        if (keyPos == std::string::npos) return result;
        size_t startBracket = json.find("[", keyPos);
        size_t endBracket = json.find("]", startBracket);
        if (startBracket == std::string::npos || endBracket == std::string::npos) return result;
        std::string arrStr = json.substr(startBracket + 1, endBracket - startBracket - 1);
        size_t current = 0;
        while (result.size() < limit) {
            size_t nextComma = arrStr.find(',', current);
            std::string numStr = (nextComma == std::string::npos) ? arrStr.substr(current) : arrStr.substr(current, nextComma - current);
            try { result.push_back(std::stod(numStr)); }
            catch (...) {}
            if (nextComma == std::string::npos) break;
            current = nextComma + 1;
        }
        return result;
    }

    std::vector<std::string> parseStringArray(const std::string& json, const std::string& key, size_t startPos, size_t limit) {
        std::vector<std::string> result;
        size_t keyPos = json.find("\"" + key + "\":", startPos);
        if (keyPos == std::string::npos) return result;
        size_t startBracket = json.find("[", keyPos);
        size_t endBracket = json.find("]", startBracket);
        std::string arrStr = json.substr(startBracket + 1, endBracket - startBracket - 1);
        size_t current = 0;
        while (result.size() < limit) {
            size_t firstQuote = arrStr.find('"', current);
            if (firstQuote == std::string::npos) break;
            size_t secondQuote = arrStr.find('"', firstQuote + 1);
            if (secondQuote == std::string::npos) break;
            result.push_back(arrStr.substr(firstQuote + 1, secondQuote - firstQuote - 1));
            current = secondQuote + 1;
        }
        return result;
    }

public:
    void addCity(const City& c) {
        std::lock_guard<std::mutex> lock(engineMutex);
        cityDatabase[c.name] = c;
    }

    std::vector<std::string> getCityList() {
        std::lock_guard<std::mutex> lock(engineMutex);
        std::vector<std::string> list;
        for (auto const& pair : cityDatabase) {
            list.push_back(pair.first);
        }
        std::sort(list.begin(), list.end());
        return list;
    }

    // --- MAIN FETCH LOGIC ---
    void fetchRealTimeData(const std::string& name) {
        double currentLat, currentLon;

        {
            std::lock_guard<std::mutex> lock(engineMutex);
            if (cityDatabase.find(name) == cityDatabase.end()) return;
            currentLat = cityDatabase[name].lat;
            currentLon = cityDatabase[name].lon;
        }

        std::string url = "https://api.open-meteo.com/v1/forecast?latitude=" + std::to_string(currentLat)
            + "&longitude=" + std::to_string(currentLon)
            + "&current=temperature_2m,relative_humidity_2m,wind_speed_10m,wind_direction_10m,weather_code"
            + "&hourly=temperature_2m"
            + "&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max,weather_code"
            + "&forecast_days=16";

        std::string json = SimpleServer::fetchURL(url);

        if (!json.empty()) {
            std::lock_guard<std::mutex> lock(engineMutex);

            if (cityDatabase.find(name) == cityDatabase.end()) return;
            City& c = cityDatabase[name];

            size_t currentBlock = json.find("\"current\":");
            if (currentBlock != std::string::npos) {
                c.temp = (int)extractJsonValue(json, "temperature_2m", currentBlock);
                c.humidity = (int)extractJsonValue(json, "relative_humidity_2m", currentBlock);
                c.wind = (int)extractJsonValue(json, "wind_speed_10m", currentBlock);
                c.wind_dir = (int)extractJsonValue(json, "wind_direction_10m", currentBlock);
                c.condition = decodeWeatherCode((int)extractJsonValue(json, "weather_code", currentBlock));
            }

            size_t hourlyBlock = json.find("\"hourly\":");
            std::vector<double> hourlyRaw = parseJsonArray(json, "temperature_2m", hourlyBlock, 24);
            c.hourlyData.clear();
            for (double d : hourlyRaw) c.hourlyData.push_back((int)d);

            c.weeklyData.clear(); c.monthlyData.clear(); c.yearlyData.clear();
            for (int i = 0; i < 7; i++) c.weeklyData.push_back(c.temp + (i % 3) - 1);
            for (int i = 0; i < 30; i++) c.monthlyData.push_back(c.temp + (i % 5) - 2);
            for (int i = 0; i < 12; i++) c.yearlyData.push_back(c.temp + (i % 4) * 2);

            c.tenDayForecast.clear();
            size_t dailyBlock = json.find("\"daily\":");
            if (dailyBlock != std::string::npos) {
                std::vector<std::string> dates = parseStringArray(json, "time", dailyBlock, 10);
                std::vector<double> maxTemps = parseJsonArray(json, "temperature_2m_max", dailyBlock, 10);
                std::vector<double> minTemps = parseJsonArray(json, "temperature_2m_min", dailyBlock, 10);
                std::vector<double> rainProbs = parseJsonArray(json, "precipitation_probability_max", dailyBlock, 10);
                std::vector<double> weatherCodes = parseJsonArray(json, "weather_code", dailyBlock, 10);

                size_t count = std::min({ dates.size(), maxTemps.size(), minTemps.size() });
                if (count > 10) count = 10;
                for (size_t i = 0; i < count; i++) {
                    DailyForecast df;
                    df.dayName = getDayName(dates[i]);
                    df.high = (int)maxTemps[i]; df.low = (int)minTemps[i];
                    df.rain_prob = (i < rainProbs.size()) ? (int)rainProbs[i] : 0;
                    if (i < weatherCodes.size()) df.condition = decodeWeatherCode((int)weatherCodes[i]); else df.condition = "Sunny";
                    c.tenDayForecast.push_back(df);
                }
            }

            // --- ALERTS & NEWS ---
            c.activeAlerts.clear();
            c.weatherNews.clear();

            if (c.temp > 40) c.activeAlerts.push_back("Extreme Heat Warning: Temperatures exceeding 40°C.");
            if (c.wind > 30) c.activeAlerts.push_back("High Wind Alert: Batten down the hatches.");
            if (c.condition == "Stormy") c.activeAlerts.push_back("Severe Thunderstorm Warning active.");
            if (c.condition == "Rainy" && c.humidity > 90) c.activeAlerts.push_back("Flash Flood Watch: Heavy saturation detected.");
            if (c.temp < 0) c.activeAlerts.push_back("Freeze Warning: Pipe bursting conditions.");

            if (c.condition == "Rainy") {
                c.weatherNews.push_back("Heavy Rain expected to continue throughout the evening in " + c.name + ".");
                c.weatherNews.push_back("Urban flooding risk increases as rain intensifies.");
            }
            else if (c.condition == "Stormy") {
                c.weatherNews.push_back("Severe Thunderstorms approaching " + c.name + " region.");
            }
            else if (c.condition == "Cloudy") {
                c.weatherNews.push_back("Overcast skies dominate " + c.name + " skyline today.");
            }
            else if (c.condition == "Sunny" || c.condition == "Clear") {
                c.weatherNews.push_back("Beautiful Clear Sky attracts tourists to " + c.name + " parks.");
                if (c.temp > 35) c.weatherNews.push_back("Heatwave alert: Sun intensity reaches peak levels.");
            }
            else if (c.condition == "Foggy") {
                c.weatherNews.push_back("Dense Fog lowers visibility on " + c.name + " highways.");
            }
            else if (c.condition == "Snow") {
                c.weatherNews.push_back("Snowfall transforms " + c.name + " into a winter wonderland.");
            }

            if (c.wind > 20) c.weatherNews.push_back("Strong Winds reported: Trees and power lines at risk.");

            if (c.weatherNews.empty()) {
                c.weatherNews.push_back("Stable weather conditions expected for the next 24 hours in " + c.name + ".");
            }
        }
    }

    City* getCity(const std::string& name) {
        if (cityDatabase.find(name) != cityDatabase.end()) return &cityDatabase[name];
        return nullptr;
    }
    void updateCity(const std::string& name) { fetchRealTimeData(name); }

    std::vector<NewsItem> getCityNews(const std::string& cityName) {
        std::vector<NewsItem> newsFeed;
        std::lock_guard<std::mutex> lock(engineMutex);

        if (cityDatabase.find(cityName) == cityDatabase.end()) return newsFeed;

        City& c = cityDatabase[cityName];
        for (const auto& msg : c.weatherNews) {
            NewsItem item;
            item.city = c.name;
            item.headline = msg;
            item.category = c.condition;

            if (msg.find("Wind") != std::string::npos) item.category = "Wind";
            else if (msg.find("Rain") != std::string::npos) item.category = "Rain";
            else if (msg.find("Heat") != std::string::npos) item.category = "Heat";
            else if (msg.find("Cold") != std::string::npos) item.category = "Cold";

            newsFeed.push_back(item);
        }
        return newsFeed;
    }

    void addRoute(const std::string& cityA, const std::string& cityB) {
        std::lock_guard<std::mutex> lock(engineMutex);
        cityGraph[cityA].push_back(cityB); cityGraph[cityB].push_back(cityA);
    }
    std::vector<std::string> getNeighbors(const std::string& name) {
        return cityGraph.count(name) ? cityGraph[name] : std::vector<std::string>{};
    }
    std::vector<City> getHottestCities(int k) {
        std::lock_guard<std::mutex> lock(engineMutex);
        std::vector<City> all;
        for (auto& pair : cityDatabase) all.push_back(pair.second);
        if (k > (int)all.size()) k = (int)all.size();
        std::partial_sort(all.begin(), all.begin() + k, all.end(), [](const City& a, const City& b) { return a.temp > b.temp; });
        all.resize(k); return all;
    }
    void logRequest(std::string req) {
        std::lock_guard<std::mutex> lock(engineMutex);
        requestLogs.push(req);
        if (requestLogs.size() > 50) requestLogs.pop();
    }

    std::unordered_map<std::string, std::string> calculateLifestyleIndices(const City& c) {
        std::unordered_map<std::string, std::string> indices;
        if (c.wind > 25 || c.condition == "Rainy") indices["Drone"] = "Unsafe"; else if (c.wind > 15) indices["Drone"] = "Caution"; else indices["Drone"] = "Excellent";
        if (c.temp > 35 || c.condition == "Stormy") indices["Running"] = "Avoid"; else if (c.temp > 28) indices["Running"] = "Hydrate"; else indices["Running"] = "Perfect";
        if (c.condition == "Rainy" || c.condition == "Stormy") indices["BBQ"] = "Indoors"; else if (c.wind > 20) indices["BBQ"] = "Too Windy"; else indices["BBQ"] = "Fire it up!";
        return indices;
    }

    // --- ENHANCED PREDICTION LOGIC ---
    ActivityResult predictActivitySuitability(const City& c, std::string query) {
        std::string q = toLower(query);
        ActivityResult res;
        res.color = "#4ade80"; // Green by default

        if (q.find("fly") != std::string::npos || q.find("drone") != std::string::npos) {
            if (c.wind > 30) { res.score = "Unsafe"; res.message = "Wind too high for drones."; res.color = "#ef4444"; }
            else if (c.condition == "Rainy") { res.score = "Risky"; res.message = "Rain might damage electronics."; res.color = "#fbbf24"; }
            else { res.score = "Excellent"; res.message = "Calm winds, go ahead!"; }
        }
        else if (q.find("bbq") != std::string::npos || q.find("grill") != std::string::npos || q.find("picnic") != std::string::npos) {
            if (c.condition == "Rainy" || c.condition == "Stormy") { res.score = "Bad Idea"; res.message = "Rain/Storm expected."; res.color = "#ef4444"; }
            else if (c.wind > 25) { res.score = "Difficult"; res.message = "Too windy for fire/plates."; res.color = "#fbbf24"; }
            else { res.score = "Perfect"; res.message = "Great conditions."; }
        }
        else if (q.find("run") != std::string::npos || q.find("jog") != std::string::npos || q.find("walk") != std::string::npos) {
            if (c.temp > 35) { res.score = "Caution"; res.message = "Risk of heatstroke."; res.color = "#fbbf24"; }
            else if (c.condition == "Stormy") { res.score = "Unsafe"; res.message = "Lightning risk."; res.color = "#ef4444"; }
            else { res.score = "Good to Go"; res.message = "Enjoy your exercise."; }
        }
        else if (q.find("cricket") != std::string::npos || q.find("football") != std::string::npos) {
            if (c.condition == "Rainy") { res.score = "Washout"; res.message = "Ground will be wet."; res.color = "#ef4444"; }
            else { res.score = "Play Ball!"; res.message = "Conditions look dry."; }
        }
        else if (q.find("construction") != std::string::npos || q.find("cement") != std::string::npos) {
            if (c.condition == "Rainy") { res.score = "Delay"; res.message = "Cement won't set."; res.color = "#ef4444"; }
            else { res.score = "Proceed"; res.message = "Conditions stable."; }
        }
        else {
            res.score = "Unknown";
            res.message = "Activity not recognized, but weather is " + c.condition;
            res.color = "#94a3b8";
        }
        return res;
    }

    // --- FIX DIJKSTRA LOGIC ---
    std::vector<std::string> findBestRoute(std::string start, std::string end) {
        std::lock_guard<std::mutex> lock(engineMutex);

        if (cityGraph.find(start) == cityGraph.end() || cityGraph.find(end) == cityGraph.end()) {
            return {};
        }

        std::priority_queue<GraphNode, std::vector<GraphNode>, std::greater<GraphNode>> pq;
        std::unordered_map<std::string, int> costs;
        std::unordered_map<std::string, std::string> parent;

        for (auto& pair : cityDatabase) costs[pair.first] = 999999;

        costs[start] = 0;
        pq.push({ start, 0 });

        while (!pq.empty()) {
            std::string u = pq.top().name;
            int c = pq.top().cost;
            pq.pop();

            if (c > costs[u]) continue;
            if (u == end) break;

            for (const auto& neighborName : cityGraph[u]) {
                int edgeWeight = 1;
                if (costs[u] + edgeWeight < costs[neighborName]) {
                    costs[neighborName] = costs[u] + edgeWeight;
                    parent[neighborName] = u;
                    pq.push({ neighborName, costs[neighborName] });
                }
            }
        }

        std::vector<std::string> path;
        if (parent.find(end) == parent.end() && start != end) return {};

        std::string curr = end;
        while (curr != start) {
            path.push_back(curr);
            curr = parent[curr];
        }
        path.push_back(start);
        std::reverse(path.begin(), path.end());
        return path;
    }
};

#endif