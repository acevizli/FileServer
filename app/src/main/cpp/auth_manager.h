#pragma once

#include <string>
#include <mutex>

class AuthManager {
public:
    AuthManager();
    ~AuthManager() = default;
    
    void setCredentials(const std::string& username, const std::string& password);
    bool validateCredentials(const std::string& authHeader) const;
    bool hasCredentials() const;
    std::string getAuthRealm() const;
    
private:
    static std::string base64Decode(const std::string& encoded);
    static bool isBase64Char(unsigned char c);
    
    mutable std::mutex mutex_;
    std::string username_;
    std::string password_;
    std::string realm_;
};
