#ifndef NETWORK_UTILS_HPP
#define NETWORK_UTILS_HPP

#define _CRT_SECURE_NO_WARNINGS // Fix for VS2022 warnings

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wininet.h> 
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

// Link necessary libraries automatically
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "ws2_32.lib")

namespace SimpleServer {

    // Helper: Initialize Winsock (marked inline to prevent linker errors)
    inline void initWinsock() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "WSAStartup failed: " << result << std::endl;
            exit(1);
        }
    }

    inline std::string loadHtmlFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return "<h1>404 - File Not Found</h1>";
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    inline void sendResponse(SOCKET clientSock, const std::string& body, const std::string& contentType) {
        std::string header = "HTTP/1.1 200 OK\r\n";
        header += "Content-Type: " + contentType + "\r\n";
        header += "Access-Control-Allow-Origin: *\r\n";
        header += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        header += "Connection: close\r\n\r\n";

        send(clientSock, header.c_str(), (int)header.size(), 0);
        send(clientSock, body.c_str(), (int)body.size(), 0);
    }

    inline std::string getQueryParam(const std::string& url, const std::string& key) {
        size_t keyPos = url.find(key + "=");
        if (keyPos == std::string::npos) return "";
        size_t valueStart = keyPos + key.length() + 1;
        size_t valueEnd = url.find('&', valueStart);
        if (valueEnd == std::string::npos) valueEnd = url.length();
        return url.substr(valueStart, valueEnd - valueStart);
    }

    // Real-Time Data Fetcher (HTTPS)
    inline std::string fetchURL(const std::string& url) {
        HINTERNET hInternet = InternetOpenA("WeatherApp/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) return "";

        HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE, 0);
        if (!hConnect) {
            InternetCloseHandle(hInternet);
            return "";
        }

        std::string response;
        char buffer[4096];
        DWORD bytesRead;

        while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            response.append(buffer, bytesRead);
        }

        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return response;
    }
}

#endif