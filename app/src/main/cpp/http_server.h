#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <jni.h>

class FileManager;
class AuthManager;

class HttpServer {
public:
    HttpServer();
    ~HttpServer();
    
    bool start(int port);
    void stop();
    bool isRunning() const;
    int getPort() const;
    
    void setFileManager(FileManager* fm);
    void setAuthManager(AuthManager* am);
    
private:
    void acceptLoop();
    void handleClient(int clientSocket);
    
    std::string parseRequest(int clientSocket, std::string& method, std::string& path, 
                             std::unordered_map<std::string, std::string>& headers);
    void sendResponse(int clientSocket, int statusCode, const std::string& statusText,
                      const std::unordered_map<std::string, std::string>& headers,
                      const std::string& body);
    void sendFileResponse(int clientSocket, int fd, size_t fileSize, const std::string& mimeType);
    
    std::string handleIndexPage();
    std::string handleApiFiles();
    bool handleFileDownload(int clientSocket, const std::string& fileId);
    
    std::string getMimeType(const std::string& filename);
    
    int serverSocket_;
    std::atomic<bool> running_;
    std::atomic<int> port_;
    std::thread acceptThread_;
    
    FileManager* fileManager_;
    AuthManager* authManager_;
    
    static constexpr int BUFFER_SIZE = 8192;
    static constexpr int MAX_HEADER_SIZE = 16384;
};
