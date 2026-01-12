#include "file_manager.h"
#include <unistd.h>
#include <fcntl.h>
#include <android/log.h>

#define LOG_TAG "FileManager"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

FileManager::FileManager() {
    LOGI("FileManager created");
}

FileManager::~FileManager() {
    clearFiles();
}

void FileManager::addFile(const std::string& id, const std::string& displayName,
                          const std::string& path, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SharedFile file;
    file.id = id;
    file.displayName = displayName;
    file.path = path;
    file.fd = -1;
    file.size = size;
    
    files_[id] = file;
    LOGI("Added file: %s (path: %s, size: %zu)", displayName.c_str(), path.c_str(), size);
}

void FileManager::addFileDescriptor(const std::string& id, const std::string& displayName,
                                     int fd, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SharedFile file;
    file.id = id;
    file.displayName = displayName;
    file.fd = fd;
    file.size = size;
    
    files_[id] = file;
    LOGI("Added file descriptor: %s (fd: %d, size: %zu)", displayName.c_str(), fd, size);
}

void FileManager::removeFile(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = files_.find(id);
    if (it != files_.end()) {
        if (it->second.fd >= 0) {
            close(it->second.fd);
        }
        files_.erase(it);
        LOGI("Removed file: %s", id.c_str());
    }
}

void FileManager::clearFiles() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : files_) {
        if (pair.second.fd >= 0) {
            close(pair.second.fd);
        }
    }
    files_.clear();
    LOGI("Cleared all files");
}

std::vector<SharedFile> FileManager::getFiles() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<SharedFile> result;
    for (const auto& pair : files_) {
        result.push_back(pair.second);
    }
    return result;
}

bool FileManager::getFile(const std::string& id, SharedFile& outFile) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = files_.find(id);
    if (it != files_.end()) {
        outFile = it->second;
        return true;
    }
    return false;
}

bool FileManager::openFile(const std::string& id, int& outFd, size_t& outSize, 
                           std::string& outName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = files_.find(id);
    if (it == files_.end()) {
        LOGE("File not found: %s", id.c_str());
        return false;
    }
    
    const SharedFile& file = it->second;
    outSize = file.size;
    outName = file.displayName;
    
    if (file.fd >= 0) {
        // Duplicate the file descriptor for serving
        outFd = dup(file.fd);
        if (outFd < 0) {
            LOGE("Failed to dup fd for file: %s", id.c_str());
            return false;
        }
        // Seek to beginning
        lseek(outFd, 0, SEEK_SET);
    } else if (!file.path.empty()) {
        // Open from path
        outFd = open(file.path.c_str(), O_RDONLY);
        if (outFd < 0) {
            LOGE("Failed to open file: %s", file.path.c_str());
            return false;
        }
    } else {
        LOGE("No valid fd or path for file: %s", id.c_str());
        return false;
    }
    
    return true;
}
