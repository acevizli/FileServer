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

    /** Where browser uploads are written on the device */
    external fun setUploadDir(path: String)

    /** Notified when the C++ side has finished writing an upload to disk */
    @Volatile
    var uploadListener: ((id: String, displayName: String, path: String, size: Long) -> Unit)? = null

    /**
     * Called from C++ on a server thread once an uploaded file is on disk.
     * The listener is responsible for hopping to the main thread.
     */
    @JvmStatic
    fun onFileUploaded(id: String, displayName: String, path: String, size: Long) {
        uploadListener?.invoke(id, displayName, path, size)
    }
}
