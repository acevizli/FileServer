#include "http_server.h"
#include "file_manager.h"
#include "auth_manager.h"
#include "web_frontend.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <android/log.h>

#define LOG_TAG "HttpServer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

HttpServer::HttpServer() 
    : serverSocket_(-1), running_(false), port_(0), 
      fileManager_(nullptr), authManager_(nullptr) {
    LOGI("HttpServer created");
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::setFileManager(FileManager* fm) {
    fileManager_ = fm;
}

void HttpServer::setAuthManager(AuthManager* am) {
    authManager_ = am;
}

bool HttpServer::start(int port) {
    if (running_) {
        LOGI("Server already running");
        return true;
    }
    
    // Create socket
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        LOGE("Failed to create socket: %s", strerror(errno));
        return false;
    }
    
    // Allow address reuse
    int opt = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOGE("Failed to set SO_REUSEADDR: %s", strerror(errno));
    }
    
    // Bind
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(serverSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOGE("Failed to bind socket: %s", strerror(errno));
        close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }
    
    // Listen
    if (listen(serverSocket_, 10) < 0) {
        LOGE("Failed to listen: %s", strerror(errno));
        close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }
    
    port_ = port;
    running_ = true;
    
    // Start accept thread
    acceptThread_ = std::thread(&HttpServer::acceptLoop, this);
    
    LOGI("Server started on port %d", port);
    return true;
}

void HttpServer::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Close server socket to unblock accept
    if (serverSocket_ >= 0) {
        shutdown(serverSocket_, SHUT_RDWR);
        close(serverSocket_);
        serverSocket_ = -1;
    }
    
    // Wait for thread
    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }
    
    LOGI("Server stopped");
}

bool HttpServer::isRunning() const {
    return running_;
}

int HttpServer::getPort() const {
    return port_;
}

void HttpServer::acceptLoop() {
    LOGI("Accept loop started");
    
    while (running_) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            if (running_) {
                LOGE("Accept failed: %s", strerror(errno));
            }
            continue;
        }
        
        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, INET_ADDRSTRLEN);
        LOGI("Connection from %s:%d", clientIp, ntohs(clientAddr.sin_port));
        
        // Handle in new thread (simple approach)
        std::thread([this, clientSocket]() {
            handleClient(clientSocket);
        }).detach();
    }
    
    LOGI("Accept loop ended");
}

void HttpServer::handleClient(int clientSocket) {
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = 30;
    timeout.tv_usec = 0;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    std::string method, path;
    std::unordered_map<std::string, std::string> headers;
    
    std::string body = parseRequest(clientSocket, method, path, headers);
    
    if (method.empty()) {
        close(clientSocket);
        return;
    }
    
    LOGI("Request: %s %s", method.c_str(), path.c_str());
    
    // Check authentication
    if (authManager_ && authManager_->hasCredentials()) {
        auto authIt = headers.find("authorization");
        if (authIt == headers.end() || !authManager_->validateCredentials(authIt->second)) {
            // Send 401 Unauthorized
            std::unordered_map<std::string, std::string> respHeaders;
            respHeaders["WWW-Authenticate"] = "Basic realm=\"" + authManager_->getAuthRealm() + "\"";
            respHeaders["Content-Type"] = "text/html; charset=utf-8";
            sendResponse(clientSocket, 401, "Unauthorized", respHeaders,
                        "<html><body><h1>401 Unauthorized</h1><p>Authentication required.</p></body></html>");
            close(clientSocket);
            return;
        }
    }
    
    // Route request
    if (method == "GET") {
        if (path == "/" || path == "/index.html") {
            std::string html = handleIndexPage();
            std::unordered_map<std::string, std::string> respHeaders;
            respHeaders["Content-Type"] = "text/html; charset=utf-8";
            sendResponse(clientSocket, 200, "OK", respHeaders, html);
        }
        else if (path == "/api/files") {
            std::string json = handleApiFiles();
            std::unordered_map<std::string, std::string> respHeaders;
            respHeaders["Content-Type"] = "application/json";
            sendResponse(clientSocket, 200, "OK", respHeaders, json);
        }
        else if (path.rfind("/download/", 0) == 0) {
            std::string fileId = path.substr(10); // Remove "/download/"
            if (!handleFileDownload(clientSocket, fileId)) {
                std::unordered_map<std::string, std::string> respHeaders;
                respHeaders["Content-Type"] = "text/html; charset=utf-8";
                sendResponse(clientSocket, 404, "Not Found", respHeaders,
                            "<html><body><h1>404 Not Found</h1></body></html>");
            }
        }
        else {
            // 404
            std::unordered_map<std::string, std::string> respHeaders;
            respHeaders["Content-Type"] = "text/html; charset=utf-8";
            sendResponse(clientSocket, 404, "Not Found", respHeaders,
                        "<html><body><h1>404 Not Found</h1></body></html>");
        }
    } else {
        // Method not allowed
        std::unordered_map<std::string, std::string> respHeaders;
        respHeaders["Content-Type"] = "text/html; charset=utf-8";
        sendResponse(clientSocket, 405, "Method Not Allowed", respHeaders,
                    "<html><body><h1>405 Method Not Allowed</h1></body></html>");
    }
    
    close(clientSocket);
}

std::string HttpServer::parseRequest(int clientSocket, std::string& method, std::string& path,
                                     std::unordered_map<std::string, std::string>& headers) {
    char buffer[BUFFER_SIZE];
    std::string requestData;
    
    // Read request headers
    while (requestData.find("\r\n\r\n") == std::string::npos && 
           requestData.size() < MAX_HEADER_SIZE) {
        ssize_t bytesRead = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
        if (bytesRead <= 0) {
            break;
        }
        buffer[bytesRead] = '\0';
        requestData += buffer;
    }
    
    if (requestData.empty()) {
        return "";
    }
    
    // Parse request line
    std::istringstream stream(requestData);
    std::string requestLine;
    std::getline(stream, requestLine);
    
    // Remove trailing \r if present
    if (!requestLine.empty() && requestLine.back() == '\r') {
        requestLine.pop_back();
    }
    
    // Parse method and path
    std::istringstream lineStream(requestLine);
    std::string httpVersion;
    lineStream >> method >> path >> httpVersion;
    
    // Parse headers
    std::string headerLine;
    while (std::getline(stream, headerLine)) {
        if (!headerLine.empty() && headerLine.back() == '\r') {
            headerLine.pop_back();
        }
        if (headerLine.empty()) {
            break;
        }
        
        size_t colonPos = headerLine.find(':');
        if (colonPos != std::string::npos) {
            std::string key = headerLine.substr(0, colonPos);
            std::string value = headerLine.substr(colonPos + 1);
            
            // Trim and lowercase key
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            
            // Trim value
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            headers[key] = value;
        }
    }
    
    // Extract body if present
    size_t bodyStart = requestData.find("\r\n\r\n");
    if (bodyStart != std::string::npos) {
        return requestData.substr(bodyStart + 4);
    }
    
    return "";
}

void HttpServer::sendResponse(int clientSocket, int statusCode, const std::string& statusText,
                              const std::unordered_map<std::string, std::string>& headers,
                              const std::string& body) {
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
    
    // Add headers
    for (const auto& header : headers) {
        response << header.first << ": " << header.second << "\r\n";
    }
    
    // Content-Length
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body;
    
    std::string responseStr = response.str();
    send(clientSocket, responseStr.c_str(), responseStr.size(), 0);
}

void HttpServer::sendFileResponse(int clientSocket, int fd, size_t fileSize, 
                                   const std::string& mimeType) {
    std::ostringstream headers;
    headers << "HTTP/1.1 200 OK\r\n";
    headers << "Content-Type: " << mimeType << "\r\n";
    headers << "Content-Length: " << fileSize << "\r\n";
    headers << "Connection: close\r\n";
    headers << "\r\n";
    
    std::string headerStr = headers.str();
    send(clientSocket, headerStr.c_str(), headerStr.size(), 0);
    
    // Send file content
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    while ((bytesRead = read(fd, buffer, BUFFER_SIZE)) > 0) {
        ssize_t totalSent = 0;
        while (totalSent < bytesRead) {
            ssize_t sent = send(clientSocket, buffer + totalSent, bytesRead - totalSent, 0);
            if (sent <= 0) {
                return;
            }
            totalSent += sent;
        }
    }
}

std::string HttpServer::handleIndexPage() {
    return WebFrontend::getIndexHtml();
}

std::string HttpServer::handleApiFiles() {
    if (!fileManager_) {
        return "[]";
    }
    
    auto files = fileManager_->getFiles();
    
    std::ostringstream json;
    json << "[";
    
    bool first = true;
    for (const auto& file : files) {
        if (!first) json << ",";
        first = false;
        
        // Escape JSON strings
        std::string escapedName = file.displayName;
        // Simple escape for quotes
        size_t pos = 0;
        while ((pos = escapedName.find('"', pos)) != std::string::npos) {
            escapedName.replace(pos, 1, "\\\"");
            pos += 2;
        }
        
        json << "{\"id\":\"" << file.id << "\","
             << "\"name\":\"" << escapedName << "\","
             << "\"size\":" << file.size << "}";
    }
    
    json << "]";
    return json.str();
}

bool HttpServer::handleFileDownload(int clientSocket, const std::string& fileId) {
    if (!fileManager_) {
        return false;
    }
    
    int fd;
    size_t size;
    std::string name;
    
    if (!fileManager_->openFile(fileId, fd, size, name)) {
        return false;
    }
    
    std::string mimeType = getMimeType(name);
    
    // Build response with Content-Disposition for download
    std::ostringstream headers;
    headers << "HTTP/1.1 200 OK\r\n";
    headers << "Content-Type: " << mimeType << "\r\n";
    headers << "Content-Length: " << size << "\r\n";
    headers << "Content-Disposition: attachment; filename=\"" << name << "\"\r\n";
    headers << "Connection: close\r\n";
    headers << "\r\n";
    
    std::string headerStr = headers.str();
    send(clientSocket, headerStr.c_str(), headerStr.size(), 0);
    
    // Send file content
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    while ((bytesRead = read(fd, buffer, BUFFER_SIZE)) > 0) {
        ssize_t totalSent = 0;
        while (totalSent < bytesRead) {
            ssize_t sent = send(clientSocket, buffer + totalSent, bytesRead - totalSent, 0);
            if (sent <= 0) {
                close(fd);
                return true;
            }
            totalSent += sent;
        }
    }
    
    close(fd);
    return true;
}

std::string HttpServer::getMimeType(const std::string& filename) {
    // Extract extension
    size_t dotPos = filename.rfind('.');
    if (dotPos == std::string::npos) {
        return "application/octet-stream";
    }
    
    std::string ext = filename.substr(dotPos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Common MIME types
    static const std::unordered_map<std::string, std::string> mimeTypes = {
        {"html", "text/html"},
        {"htm", "text/html"},
        {"css", "text/css"},
        {"js", "application/javascript"},
        {"json", "application/json"},
        {"xml", "application/xml"},
        {"txt", "text/plain"},
        {"pdf", "application/pdf"},
        {"doc", "application/msword"},
        {"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        {"xls", "application/vnd.ms-excel"},
        {"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
        {"ppt", "application/vnd.ms-powerpoint"},
        {"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"png", "image/png"},
        {"gif", "image/gif"},
        {"webp", "image/webp"},
        {"svg", "image/svg+xml"},
        {"ico", "image/x-icon"},
        {"mp3", "audio/mpeg"},
        {"wav", "audio/wav"},
        {"ogg", "audio/ogg"},
        {"mp4", "video/mp4"},
        {"webm", "video/webm"},
        {"avi", "video/x-msvideo"},
        {"mkv", "video/x-matroska"},
        {"mov", "video/quicktime"},
        {"zip", "application/zip"},
        {"rar", "application/x-rar-compressed"},
        {"7z", "application/x-7z-compressed"},
        {"tar", "application/x-tar"},
        {"gz", "application/gzip"},
        {"apk", "application/vnd.android.package-archive"},
    };
    
    auto it = mimeTypes.find(ext);
    if (it != mimeTypes.end()) {
        return it->second;
    }
    
    return "application/octet-stream";
}
