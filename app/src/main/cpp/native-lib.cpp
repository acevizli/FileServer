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

// Cached for calling back into Kotlin from server threads
static JavaVM* g_vm = nullptr;
static jclass g_nativeServerClass = nullptr;
static jmethodID g_onFileUploadedMethod = nullptr;

static void notifyFileUploaded(const std::string& id, const std::string& name,
                               const std::string& path, size_t size) {
    if (!g_vm || !g_nativeServerClass || !g_onFileUploadedMethod) {
        return;
    }

    // Upload runs on a detached server thread, so attach it to the JVM first
    JNIEnv* env = nullptr;
    bool attached = false;

    if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            LOGE("Failed to attach thread for upload callback");
            return;
        }
        attached = true;
    }

    jstring jId = env->NewStringUTF(id.c_str());
    jstring jName = env->NewStringUTF(name.c_str());
    jstring jPath = env->NewStringUTF(path.c_str());

    env->CallStaticVoidMethod(g_nativeServerClass, g_onFileUploadedMethod,
                              jId, jName, jPath, static_cast<jlong>(size));

    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }

    env->DeleteLocalRef(jId);
    env->DeleteLocalRef(jName);
    env->DeleteLocalRef(jPath);

    if (attached) {
        g_vm->DetachCurrentThread();
    }
}

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
        g_server->setUploadCallback(notifyFileUploaded);
    }
}

jboolean startServer(JNIEnv* env, jobject /* this */, jint port) {
    LOGI("startServer called with port: %d", port);
    ensureInitialized();
    return g_server->start(port) ? JNI_TRUE : JNI_FALSE;
}

void stopServer(JNIEnv* env, jobject /* this */) {
    LOGI("stopServer called");
    if (g_server) {
        g_server->stop();
    }
}

jboolean isServerRunning(JNIEnv* env, jobject /* this */) {
    if (g_server) {
        return g_server->isRunning() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

jint getServerPort(JNIEnv* env, jobject /* this */) {
    if (g_server) {
        return g_server->getPort();
    }
    return 0;
}

void setCredentials(JNIEnv* env, jobject /* this */, jstring username, jstring password) {
    ensureInitialized();
    
    const char* usernameChars = env->GetStringUTFChars(username, nullptr);
    const char* passwordChars = env->GetStringUTFChars(password, nullptr);
    
    g_authManager->setCredentials(usernameChars, passwordChars);
    
    env->ReleaseStringUTFChars(username, usernameChars);
    env->ReleaseStringUTFChars(password, passwordChars);
    
    LOGI("Credentials set for user: %s", usernameChars);
}

void addFile(JNIEnv* env, jobject /* this */, jstring id, jstring displayName,
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

void addFileDescriptor(JNIEnv* env, jobject /* this */, jstring id, jstring displayName,
                                                        jint fd, jlong size) {
    ensureInitialized();
    
    const char* idChars = env->GetStringUTFChars(id, nullptr);
    const char* nameChars = env->GetStringUTFChars(displayName, nullptr);
    
    g_fileManager->addFileDescriptor(idChars, nameChars, fd, static_cast<size_t>(size));
    
    env->ReleaseStringUTFChars(id, idChars);
    env->ReleaseStringUTFChars(displayName, nameChars);
}

void removeFile(JNIEnv* env, jobject /* this */, jstring id) {
    if (!g_fileManager) return;
    
    const char* idChars = env->GetStringUTFChars(id, nullptr);
    g_fileManager->removeFile(idChars);
    env->ReleaseStringUTFChars(id, idChars);
}

void clearFiles(JNIEnv* env, jobject /* this */) {
    if (g_fileManager) {
        g_fileManager->clearFiles();
    }
}

void setUploadDir(JNIEnv* env, jobject /* this */, jstring dir) {
    ensureInitialized();

    const char* dirChars = env->GetStringUTFChars(dir, nullptr);
    g_server->setUploadDir(dirChars);
    LOGI("Upload dir: %s", dirChars);
    env->ReleaseStringUTFChars(dir, dirChars);
}

static const JNINativeMethod gMethods[] = {
    {"startServer", "(I)Z", (void *) startServer},
    {"stopServer",          "()V",                                (void *) stopServer},
    {"isServerRunning", "()Z",               (void *) isServerRunning},
    {"getServerPort",          "()I",               (void *) getServerPort},
    {"setCredentials", "(Ljava/lang/String;Ljava/lang/String;)V", (void *) setCredentials},
    {"addFile", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;J)V", (void *) addFile},
    {"addFileDescriptor", "(Ljava/lang/String;Ljava/lang/String;IJ)V", (void *) addFileDescriptor},
    {"removeFile", "(Ljava/lang/String;)V", (void *) removeFile},
    {"clearFiles", "()V", (void *) clearFiles},
    {"setUploadDir", "(Ljava/lang/String;)V", (void *) setUploadDir},
};

jint JNI_OnLoad(JavaVM *vm, void *) {
  JNIEnv * env = nullptr;
  if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR;
  }
  if (env == NULL) {
    return JNI_ERR;
  }
  jclass cls = env->FindClass("com/acevizli/fileserver/NativeServer");
  if (cls == NULL) {
    return JNI_ERR;
  }
  env->RegisterNatives(cls, gMethods, sizeof(gMethods) / sizeof(gMethods[0]));

  // Keep what we need to push upload events back to Kotlin
  g_vm = vm;
  g_nativeServerClass = static_cast<jclass>(env->NewGlobalRef(cls));
  g_onFileUploadedMethod = env->GetStaticMethodID(
      g_nativeServerClass, "onFileUploaded",
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;J)V");
  if (g_onFileUploadedMethod == NULL) {
    LOGE("onFileUploaded callback not found");
    env->ExceptionClear();
  }

  return JNI_VERSION_1_6;
}


