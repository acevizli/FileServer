#include <jni.h>
#include <string>
#include <memory>
#include <android/log.h>

#include "http_server.h"
#include "file_manager.h"
#include "auth_manager.h"

#define LOG_TAG "NativeLib"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global instances
static std::unique_ptr<HttpServer> g_server;
static std::unique_ptr<FileManager> g_fileManager;
static std::unique_ptr<AuthManager> g_authManager;

static void ensureInitialized() {
    if (!g_fileManager) {
        g_fileManager = std::make_unique<FileManager>();
    }
    if (!g_authManager) {
        g_authManager = std::make_unique<AuthManager>();
    }
    if (!g_server) {
        g_server = std::make_unique<HttpServer>();
        g_server->setFileManager(g_fileManager.get());
        g_server->setAuthManager(g_authManager.get());
    }
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_acevizli_fileserver_NativeServer_startServer(JNIEnv* env, jobject /* this */, jint port) {
    LOGI("startServer called with port: %d", port);
    ensureInitialized();
    return g_server->start(port) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_acevizli_fileserver_NativeServer_stopServer(JNIEnv* env, jobject /* this */) {
    LOGI("stopServer called");
    if (g_server) {
        g_server->stop();
    }
}

JNIEXPORT jboolean JNICALL
Java_com_acevizli_fileserver_NativeServer_isServerRunning(JNIEnv* env, jobject /* this */) {
    if (g_server) {
        return g_server->isRunning() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_acevizli_fileserver_NativeServer_getServerPort(JNIEnv* env, jobject /* this */) {
    if (g_server) {
        return g_server->getPort();
    }
    return 0;
}

JNIEXPORT void JNICALL
Java_com_acevizli_fileserver_NativeServer_setCredentials(JNIEnv* env, jobject /* this */,
                                                          jstring username, jstring password) {
    ensureInitialized();
    
    const char* usernameChars = env->GetStringUTFChars(username, nullptr);
    const char* passwordChars = env->GetStringUTFChars(password, nullptr);
    
    g_authManager->setCredentials(usernameChars, passwordChars);
    
    env->ReleaseStringUTFChars(username, usernameChars);
    env->ReleaseStringUTFChars(password, passwordChars);
    
    LOGI("Credentials set for user: %s", usernameChars);
}

JNIEXPORT void JNICALL
Java_com_acevizli_fileserver_NativeServer_addFile(JNIEnv* env, jobject /* this */,
                                                   jstring id, jstring displayName,
                                                   jstring path, jlong size) {
    ensureInitialized();
    
    const char* idChars = env->GetStringUTFChars(id, nullptr);
    const char* nameChars = env->GetStringUTFChars(displayName, nullptr);
    const char* pathChars = env->GetStringUTFChars(path, nullptr);
    
    g_fileManager->addFile(idChars, nameChars, pathChars, static_cast<size_t>(size));
    
    env->ReleaseStringUTFChars(id, idChars);
    env->ReleaseStringUTFChars(displayName, nameChars);
    env->ReleaseStringUTFChars(path, pathChars);
}

JNIEXPORT void JNICALL
Java_com_acevizli_fileserver_NativeServer_addFileDescriptor(JNIEnv* env, jobject /* this */,
                                                             jstring id, jstring displayName,
                                                             jint fd, jlong size) {
    ensureInitialized();
    
    const char* idChars = env->GetStringUTFChars(id, nullptr);
    const char* nameChars = env->GetStringUTFChars(displayName, nullptr);
    
    g_fileManager->addFileDescriptor(idChars, nameChars, fd, static_cast<size_t>(size));
    
    env->ReleaseStringUTFChars(id, idChars);
    env->ReleaseStringUTFChars(displayName, nameChars);
}

JNIEXPORT void JNICALL
Java_com_acevizli_fileserver_NativeServer_removeFile(JNIEnv* env, jobject /* this */, jstring id) {
    if (!g_fileManager) return;
    
    const char* idChars = env->GetStringUTFChars(id, nullptr);
    g_fileManager->removeFile(idChars);
    env->ReleaseStringUTFChars(id, idChars);
}

JNIEXPORT void JNICALL
Java_com_acevizli_fileserver_NativeServer_clearFiles(JNIEnv* env, jobject /* this */) {
    if (g_fileManager) {
        g_fileManager->clearFiles();
    }
}

} // extern "C"