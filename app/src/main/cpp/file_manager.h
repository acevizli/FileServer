#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

struct SharedFile {
    std::string id;
    std::string displayName;
    std::string path;       // File path (for regular files)
    int fd;                 // File descriptor (for SAF files, -1 if not used)
    size_t size;
    
    SharedFile() : fd(-1), size(0) {}
};

class FileManager {
public:
    FileManager();
    ~FileManager();
    
    void addFile(const std::string& id, const std::string& displayName, 
                 const std::string& path, size_t size);
    void addFileDescriptor(const std::string& id, const std::string& displayName, 
                           int fd, size_t size);
    void removeFile(const std::string& id);
    void clearFiles();
    
    std::vector<SharedFile> getFiles() const;
    bool getFile(const std::string& id, SharedFile& outFile) const;
    
    // Read file content - caller must handle file descriptor duplication for SAF files
    bool openFile(const std::string& id, int& outFd, size_t& outSize, std::string& outName) const;
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, SharedFile> files_;
};
