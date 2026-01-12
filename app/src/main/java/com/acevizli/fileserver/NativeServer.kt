package com.acevizli.fileserver

/**
 * Native server interface - JNI bridge to C++ HTTP server
 */
object NativeServer {
    
    init {
        System.loadLibrary("fileserver")
    }
    
    external fun startServer(port: Int): Boolean
    external fun stopServer()
    external fun isServerRunning(): Boolean
    external fun getServerPort(): Int
    
    external fun setCredentials(username: String, password: String)
    
    external fun addFile(id: String, displayName: String, path: String, size: Long)
    external fun addFileDescriptor(id: String, displayName: String, fd: Int, size: Long)
    external fun removeFile(id: String)
    external fun clearFiles()
}
