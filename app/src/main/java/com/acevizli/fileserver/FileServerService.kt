package com.acevizli.fileserver

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.net.Uri
import android.os.Binder
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.ParcelFileDescriptor
import android.util.Log
import androidx.core.app.NotificationCompat
import java.io.File
import java.util.UUID

class FileServerService : Service() {

    companion object {
        private const val TAG = "FileServerService"
        private const val NOTIFICATION_ID = 1
        private const val CHANNEL_ID = "file_server_channel"
        const val DEFAULT_PORT = 8080
        const val UPLOAD_DIR_NAME = "uploads"
    }

    private val binder = LocalBinder()
    private val sharedFiles = mutableListOf<SharedFile>()
    private val fileDescriptors = mutableMapOf<String, ParcelFileDescriptor>()
    private val mainHandler = Handler(Looper.getMainLooper())

    /** Notified on the main thread whenever the shared file list changes */
    var onFilesChanged: ((uploadedFile: SharedFile?) -> Unit)? = null

    var isRunning = false
        private set
    
    var serverPort = DEFAULT_PORT
        private set
    
    inner class LocalBinder : Binder() {
        fun getService(): FileServerService = this@FileServerService
    }
    
    override fun onBind(intent: Intent?): IBinder {
        return binder
    }
    
    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()

        // C++ calls this from a server thread once an upload is on disk
        NativeServer.uploadListener = { id, name, path, size ->
            mainHandler.post { onUploadReceived(id, name, path, size) }
        }
    }

    override fun onDestroy() {
        NativeServer.uploadListener = null
        stopServer()
        super.onDestroy()
    }

    /** Directory browser uploads land in */
    fun uploadDir(): File {
        val base = getExternalFilesDir(null) ?: filesDir
        val dir = File(base, UPLOAD_DIR_NAME)
        if (!dir.exists()) {
            dir.mkdirs()
        }
        return dir
    }

    private fun onUploadReceived(id: String, name: String, path: String, size: Long) {
        val file = SharedFile(id, name, Uri.fromFile(File(path)), size, localPath = path)
        sharedFiles.add(file)

        Log.i(TAG, "Received upload: $name ($size bytes) -> $path")

        if (isRunning) {
            updateNotification()
        }
        onFilesChanged?.invoke(file)
    }
    
    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "File Server",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "File server status notification"
            }
            
            val notificationManager = getSystemService(NotificationManager::class.java)
            notificationManager.createNotificationChannel(channel)
        }
    }
    
    private fun createNotification(): Notification {
        val intent = Intent(this, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_SINGLE_TOP
        }
        val pendingIntent = PendingIntent.getActivity(
            this, 0, intent,
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )
        
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("File Server Running")
            .setContentText("Serving ${sharedFiles.size} files on port $serverPort")
            .setSmallIcon(R.drawable.ic_server)
            .setOngoing(true)
            .setContentIntent(pendingIntent)
            .build()
    }
    
    fun setCredentials(username: String, password: String) {
        NativeServer.setCredentials(username, password)
        Log.i(TAG, "Credentials set")
    }
    
    fun startServer(port: Int): Boolean {
        if (isRunning) {
            Log.w(TAG, "Server already running")
            return true
        }
        
        serverPort = port

        // Tell the native side where to write browser uploads
        NativeServer.setUploadDir(uploadDir().absolutePath)

        // Add all files to native server
        for (file in sharedFiles) {
            addFileToNative(file)
        }

        val success = NativeServer.startServer(port)
        if (success) {
            isRunning = true
            startForeground(NOTIFICATION_ID, createNotification())
            Log.i(TAG, "Server started on port $port")
        } else {
            Log.e(TAG, "Failed to start server")
        }
        
        return success
    }
    
    fun stopServer() {
        if (!isRunning) return
        
        NativeServer.stopServer()
        isRunning = false
        
        stopForeground(STOP_FOREGROUND_REMOVE)
        Log.i(TAG, "Server stopped")
    }
    
    fun addFile(uri: Uri, displayName: String, size: Long): SharedFile {
        val id = UUID.randomUUID().toString()
        val file = SharedFile(id, displayName, uri, size)
        sharedFiles.add(file)
        
        if (isRunning) {
            addFileToNative(file)
            updateNotification()
        }
        
        Log.i(TAG, "Added file: $displayName (id: $id)")
        return file
    }
    
    private fun addFileToNative(file: SharedFile) {
        try {
            val path = file.localPath
            if (path != null) {
                // Uploaded file already on our own storage - serve it straight from the path
                NativeServer.addFile(file.id, file.displayName, path, file.size)
                return
            }

            // Open file descriptor for content:// URIs
            val pfd = contentResolver.openFileDescriptor(file.uri, "r")
            if (pfd != null) {
                fileDescriptors[file.id] = pfd
                NativeServer.addFileDescriptor(file.id, file.displayName, pfd.detachFd(), file.size)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to open file: ${file.displayName}", e)
        }
    }

    fun removeFile(file: SharedFile) {
        sharedFiles.remove(file)

        // Close file descriptor
        fileDescriptors[file.id]?.close()
        fileDescriptors.remove(file.id)

        if (isRunning) {
            NativeServer.removeFile(file.id)
            updateNotification()
        }

        Log.i(TAG, "Removed file: ${file.displayName}")
    }
    
    fun getSharedFiles(): List<SharedFile> = sharedFiles.toList()
    
    private fun updateNotification() {
        val notificationManager = getSystemService(NotificationManager::class.java)
        notificationManager.notify(NOTIFICATION_ID, createNotification())
    }
    
    fun clearFiles() {
        for (pfd in fileDescriptors.values) {
            pfd.close()
        }
        fileDescriptors.clear()
        sharedFiles.clear()
        
        if (isRunning) {
            NativeServer.clearFiles()
            updateNotification()
        }
    }
}
