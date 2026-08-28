#include "http_server.h"
#include "file_manager.h"
#include "auth_manager.h"
#include "web_frontend.h"
#include "multipart_parser.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <android/log.h>

#define LOG_TAG "HttpServer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

HttpServer::HttpServer()
    : serverSocket_(-1), running_(false), port_(0),
      fileManager_(nullptr), authManager_(nullptr), uploadCounter_(0) {
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

void HttpServer::setUploadDir(const std::string& dir) {
    std::lock_guard<std::mutex> lock(uploadMutex_);
    uploadDir_ = dir;

    // Make sure it exists - Kotlin creates it too, this is just belt and braces
    if (!dir.empty()) {
        mkdir(dir.c_str(), 0755);
    }
    LOGI("Upload directory set to %s", dir.c_str());
}

void HttpServer::setUploadCallback(UploadCallback callback) {
    std::lock_guard<std::mutex> lock(uploadMutex_);
    uploadCallback_ = std::move(callback);
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
    std::string leftoverBody;

    if (!parseRequest(clientSocket, method, path, headers, leftoverBody) || method.empty()) {
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
    } else if (method == "POST") {
        if (path == "/api/upload") {
            handleUpload(clientSocket, headers, leftoverBody);
        } else {
            sendJson(clientSocket, 404, "Not Found", "{\"ok\":false,\"error\":\"Unknown endpoint\"}");
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

bool HttpServer::parseRequest(int clientSocket, std::string& method, std::string& path,
                              std::unordered_map<std::string, std::string>& headers,
                              std::string& leftoverBody) {
    char buffer[BUFFER_SIZE];
    std::string requestData;

    // Read request headers
    while (requestData.find("\r\n\r\n") == std::string::npos &&
           requestData.size() < MAX_HEADER_SIZE) {
        ssize_t bytesRead = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesRead <= 0) {
            break;
        }
        // append() rather than += so binary body bytes survive embedded NULs
        requestData.append(buffer, static_cast<size_t>(bytesRead));
    }

    if (requestData.empty()) {
        return false;
    }

    // Split the header block from anything that already arrived of the body
    size_t headerEnd = requestData.find("\r\n\r\n");
    std::string headerBlock = (headerEnd == std::string::npos)
                                  ? requestData
                                  : requestData.substr(0, headerEnd + 2);
    leftoverBody = (headerEnd == std::string::npos)
                       ? ""
                       : requestData.substr(headerEnd + 4);

    // Parse request line
    std::istringstream stream(headerBlock);
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

    return !method.empty();
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

void HttpServer::sendJson(int clientSocket, int statusCode, const std::string& statusText,
                          const std::string& json) {
    std::unordered_map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    sendResponse(clientSocket, statusCode, statusText, headers, json);
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
        
        json << "{\"id\":\"" << jsonEscape(file.id) << "\","
             << "\"name\":\"" << jsonEscape(file.displayName) << "\","
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

void HttpServer::handleUpload(int clientSocket,
                              const std::unordered_map<std::string, std::string>& headers,
                              const std::string& leftoverBody) {
    std::string uploadDir;
    UploadCallback callback;
    {
        std::lock_guard<std::mutex> lock(uploadMutex_);
        uploadDir = uploadDir_;
        callback = uploadCallback_;
    }

    if (uploadDir.empty()) {
        sendJson(clientSocket, 503, "Service Unavailable",
                 "{\"ok\":false,\"error\":\"Upload storage is not ready on the device\"}");
        return;
    }

    // Content-Type must carry the multipart boundary
    auto contentTypeIt = headers.find("content-type");
    if (contentTypeIt == headers.end()) {
        sendJson(clientSocket, 400, "Bad Request",
                 "{\"ok\":false,\"error\":\"Missing Content-Type\"}");
        return;
    }

    const std::string& contentType = contentTypeIt->second;
    std::string contentTypeLower = contentType;
    std::transform(contentTypeLower.begin(), contentTypeLower.end(),
                   contentTypeLower.begin(), ::tolower);

    if (contentTypeLower.find("multipart/form-data") == std::string::npos) {
        sendJson(clientSocket, 415, "Unsupported Media Type",
                 "{\"ok\":false,\"error\":\"Expected multipart/form-data\"}");
        return;
    }

    size_t boundaryPos = contentTypeLower.find("boundary=");
    if (boundaryPos == std::string::npos) {
        sendJson(clientSocket, 400, "Bad Request",
                 "{\"ok\":false,\"error\":\"Missing multipart boundary\"}");
        return;
    }

    std::string boundary = contentType.substr(boundaryPos + strlen("boundary="));
    size_t semicolon = boundary.find(';');
    if (semicolon != std::string::npos) {
        boundary = boundary.substr(0, semicolon);
    }
    // Trim whitespace and optional quotes
    boundary.erase(0, boundary.find_first_not_of(" \t\""));
    size_t lastGood = boundary.find_last_not_of(" \t\"\r\n");
    if (lastGood != std::string::npos) {
        boundary.erase(lastGood + 1);
    }

    if (boundary.empty()) {
        sendJson(clientSocket, 400, "Bad Request",
                 "{\"ok\":false,\"error\":\"Empty multipart boundary\"}");
        return;
    }

    auto lengthIt = headers.find("content-length");
    if (lengthIt == headers.end()) {
        sendJson(clientSocket, 411, "Length Required",
                 "{\"ok\":false,\"error\":\"Content-Length required\"}");
        return;
    }

    unsigned long long declaredLength = 0;
    try {
        declaredLength = std::stoull(lengthIt->second);
    } catch (...) {
        sendJson(clientSocket, 400, "Bad Request",
                 "{\"ok\":false,\"error\":\"Invalid Content-Length\"}");
        return;
    }

    if (declaredLength > MAX_UPLOAD_SIZE ||
        declaredLength > static_cast<unsigned long long>(SIZE_MAX)) {
        sendJson(clientSocket, 413, "Payload Too Large",
                 "{\"ok\":false,\"error\":\"Upload is too large for this device\"}");
        return;
    }

    size_t contentLength = static_cast<size_t>(declaredLength);

    // curl and friends wait for this before sending a large body
    auto expectIt = headers.find("expect");
    if (expectIt != headers.end()) {
        std::string expect = expectIt->second;
        std::transform(expect.begin(), expect.end(), expect.begin(), ::tolower);
        if (expect.find("100-continue") != std::string::npos) {
            const char* cont = "HTTP/1.1 100 Continue\r\n\r\n";
            send(clientSocket, cont, strlen(cont), 0);
        }
    }

    // Uploads can take a while between chunks on a weak link
    struct timeval timeout;
    timeout.tv_sec = 120;
    timeout.tv_usec = 0;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    MultipartParser parser(boundary, uploadDir);
    bool ok = parser.parse(
        [clientSocket](char* buf, size_t n) -> long {
            return static_cast<long>(recv(clientSocket, buf, n, 0));
        },
        leftoverBody, contentLength);

    // Leave nothing unread on the socket, otherwise close() resets the
    // connection and the browser reports a network error instead of our status
    parser.drainRemaining(MAX_DRAIN_SIZE);

    if (!ok) {
        LOGE("Upload failed: %s", parser.error().c_str());
        sendJson(clientSocket, 400, "Bad Request",
                 "{\"ok\":false,\"error\":\"" + jsonEscape(parser.error()) + "\"}");
        return;
    }

    // Register each saved file so it shows up on the phone and in the web list
    std::ostringstream json;
    json << "{\"ok\":true,\"files\":[";

    bool first = true;
    for (const auto& file : parser.files()) {
        std::string id = nextUploadId();

        if (fileManager_) {
            fileManager_->addFile(id, file.fileName, file.path, file.size);
        }
        if (callback) {
            callback(id, file.fileName, file.path, file.size);
        }

        if (!first) json << ",";
        first = false;
        json << "{\"id\":\"" << jsonEscape(id) << "\","
             << "\"name\":\"" << jsonEscape(file.fileName) << "\","
             << "\"size\":" << file.size << "}";
    }

    json << "]}";

    LOGI("Upload complete: %zu file(s)", parser.files().size());
    sendJson(clientSocket, 200, "OK", json.str());
}

std::string HttpServer::nextUploadId() {
    unsigned long n = ++uploadCounter_;
    return "up-" + std::to_string(static_cast<long long>(time(nullptr))) + "-" + std::to_string(n);
}

std::string HttpServer::jsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);

    for (char c : value) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
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
