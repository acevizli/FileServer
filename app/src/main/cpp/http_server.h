#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <jni.h>

class FileManager;
class AuthManager;

class HttpServer {
public:
    // Called when a browser upload has been written to disk
    using UploadCallback = std::function<void(const std::string& id, const std::string& name,
                                              const std::string& path, size_t size)>;

    HttpServer();
    ~HttpServer();

    bool start(int port);
    void stop();
    bool isRunning() const;
    int getPort() const;

    void setFileManager(FileManager* fm);
    void setAuthManager(AuthManager* am);

    // Directory browser uploads are written into
    void setUploadDir(const std::string& dir);
    void setUploadCallback(UploadCallback callback);

private:
    void acceptLoop();
    void handleClient(int clientSocket);

    bool parseRequest(int clientSocket, std::string& method, std::string& path,
                      std::unordered_map<std::string, std::string>& headers,
                      std::string& leftoverBody);
    void sendResponse(int clientSocket, int statusCode, const std::string& statusText,
                      const std::unordered_map<std::string, std::string>& headers,
                      const std::string& body);
    void sendJson(int clientSocket, int statusCode, const std::string& statusText,
                  const std::string& json);
    void sendFileResponse(int clientSocket, int fd, size_t fileSize, const std::string& mimeType);

    std::string handleIndexPage();
    std::string handleApiFiles();
    bool handleFileDownload(int clientSocket, const std::string& fileId);
    // Streams every shared file as one store-only (uncompressed) ZIP
    bool handleZipDownload(int clientSocket);
    void handleUpload(int clientSocket,
                      const std::unordered_map<std::string, std::string>& headers,
                      const std::string& leftoverBody);

    std::string getMimeType(const std::string& filename);
    static std::string jsonEscape(const std::string& value);
    std::string nextUploadId();

    int serverSocket_;
    std::atomic<bool> running_;
    std::atomic<int> port_;
    std::thread acceptThread_;

    FileManager* fileManager_;
    AuthManager* authManager_;

    mutable std::mutex uploadMutex_;
    std::string uploadDir_;
    UploadCallback uploadCallback_;
    std::atomic<unsigned long> uploadCounter_;

    static constexpr int BUFFER_SIZE = 8192;
    static constexpr int MAX_HEADER_SIZE = 16384;
    // Upper bound on a single upload request (4 GB). Kept as 64-bit because
    // size_t is only 32 bits on the 32-bit ABIs.
    static constexpr unsigned long long MAX_UPLOAD_SIZE = 4ULL * 1024 * 1024 * 1024;
    // How much of a rejected body we are willing to read before hanging up
    static constexpr size_t MAX_DRAIN_SIZE = 4 * 1024 * 1024;
};
