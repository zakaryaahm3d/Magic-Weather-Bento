# Magic-Weather-Bento

**Magic Weather Bento** is a high-performance, C++ powered weather dashboard that merges low-level backend engineering with modern, glassmorphism-styled frontend design. Unlike standard weather apps, this project serves as a practical engine for **Data Structures and Algorithms (DSA)**, applying concepts like graph theory and heaps to solve real-world problems like safe route planning and activity forecasting.

<img width="1889" height="894" alt="image" src="https://github.com/user-attachments/assets/eafc213a-b0e3-469a-9571-30cf6a9dd1e4" />

## Key Features

### Frontend Experience
* **Magic Bento Grid**: A responsive, modular layout that adapts to different screen sizes, featuring a premium glassmorphism aesthetic.
* **3D Navigation**: An immersive, interactive 3D sphere menu for intuitive navigation between dashboard views.
* **Dynamic Atmospherics**: The interface automatically updates its background and color themes to reflect real-time weather conditions (e.g., storm clouds, sunny skies, or winter snow).
* **Rich Visualization**: Includes interactive Leaflet.js maps and Chart.js analytics for temperature trends across 24-hour, weekly, and monthly periods.

### ⚙️ Backend Engineering
* **Custom HTTP Server**: A bespoke server implementation built from the ground up using **Winsock2**, handling raw TCP/IP connections and HTTP request parsing without external web frameworks.
* **Smart Routing Engine**: A custom pathfinding module that determines the safest travel routes between cities based on weather conditions.
* **Lifestyle Analysis**: A logic engine that evaluates wind speed, precipitation, and temperature to provide suitability scores for outdoor activities like drone flying, cricket, or construction.
* **Live Data Pipeline**: Direct integration with the [Open-Meteo API](https://open-meteo.com/) via **WinINet** for real-time forecasting.

## Data Structures & Algorithms

This project is built to demonstrate the practical application of core computer science concepts:

| Algorithm / Structure | Application in Project |
| :--- | :--- |
| **Dijkstra’s Algorithm** | The core of the **Trip Planner**. It calculates the optimal route between cities by treating the map as a weighted graph, where "cost" is determined by distance and adverse weather conditions. |
| **Graph (Adjacency List)** | Represents the network of cities (Nodes) and highways (Edges) across the region, allowing for efficient traversal and connectivity checks. |
| **Max-Priority Queue** | Powers the **Alert System**. It ensures that critical warnings (Severe Thunderstorms, Heatwaves) are prioritized and displayed immediately over minor advisories. |
| **Hash Maps (`unordered_map`)** | Provides $O(1)$ access times for city data retrieval and caching, ensuring the dashboard remains responsive even with a large dataset. |
| **Stack (LIFO)** | Manages the server's request logging system, maintaining a history of the most recent API calls for debugging and analytics. |
| **Sorting Algorithms** | Efficiently processes city data to generate dynamic rankings, such as the "Top 5 Hottest Cities" widget. |

## Technology Stack

* **Core Language**: C++ (MSVC / C++17)
* **Networking APIs**: Windows Sockets 2 (`Winsock2`), Windows Internet (`WinINet`)
* **Interface**: HTML5, CSS3 (Advanced Animations), JavaScript (Vanilla ES6+)
* **Visualization Libraries**:
    * **Chart.js**: For rendering temperature and humidity graphs.
    * **Leaflet.js**: For rendering the interactive city map.
    * **FontAwesome**: For iconography.

## Project Structure

An overview of the codebase:

```text
magic-weather-bento/
├── main.cpp           
├── WeatherEngine.hpp    
├── NetworkUtils.hpp   
└── index.html                    
