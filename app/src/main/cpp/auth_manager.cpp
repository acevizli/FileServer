#include "auth_manager.h"
#include <android/log.h>
#include <algorithm>

#define LOG_TAG "AuthManager"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

AuthManager::AuthManager() : realm_("FileServer") {
    LOGI("AuthManager created");
}

void AuthManager::setCredentials(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    username_ = username;
    password_ = password;
    LOGI("Credentials set for user: %s", username.c_str());
}

bool AuthManager::hasCredentials() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !username_.empty() && !password_.empty();
}

std::string AuthManager::getAuthRealm() const {
    return realm_;
}

bool AuthManager::validateCredentials(const std::string& authHeader) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if credentials are set
    if (username_.empty() && password_.empty()) {
        // No auth required
        return true;
    }
    
    // Expect: "Basic <base64>"
    const std::string prefix = "Basic ";
    if (authHeader.length() < prefix.length() ||
        authHeader.substr(0, prefix.length()) != prefix) {
        LOGE("Invalid auth header format");
        return false;
    }
    
    std::string encoded = authHeader.substr(prefix.length());
    // Trim whitespace
    encoded.erase(0, encoded.find_first_not_of(" \t\r\n"));
    encoded.erase(encoded.find_last_not_of(" \t\r\n") + 1);
    
    std::string decoded = base64Decode(encoded);
    
    // Expected format: "username:password"
    size_t colonPos = decoded.find(':');
    if (colonPos == std::string::npos) {
        LOGE("Invalid decoded credentials format");
        return false;
    }
    
    std::string user = decoded.substr(0, colonPos);
    std::string pass = decoded.substr(colonPos + 1);
    
    bool valid = (user == username_ && pass == password_);
    if (!valid) {
        LOGI("Authentication failed for user: %s", user.c_str());
    }
    return valid;
}

bool AuthManager::isBase64Char(unsigned char c) {
    return (isalnum(c) || c == '+' || c == '/');
}

std::string AuthManager::base64Decode(const std::string& encoded) {
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string decoded;
    int in_len = encoded.size();
    int i = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    
    while (in_len-- && encoded[in_] != '=' && isBase64Char(encoded[in_])) {
        char_array_4[i++] = encoded[in_++];
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                char_array_4[i] = base64_chars.find(char_array_4[i]);
            }
            
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            
            for (i = 0; i < 3; i++) {
                decoded += char_array_3[i];
            }
            i = 0;
        }
    }
    
    if (i) {
        for (int j = i; j < 4; j++) {
            char_array_4[j] = 0;
        }
        
        for (int j = 0; j < 4; j++) {
            char_array_4[j] = base64_chars.find(char_array_4[j]);
        }
        
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        
        for (int j = 0; j < i - 1; j++) {
            decoded += char_array_3[j];
        }
    }
    
    return decoded;
}
