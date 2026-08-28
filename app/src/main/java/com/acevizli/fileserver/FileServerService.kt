package com.acevizli.fileserver

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.media.MediaScannerConnection
import android.net.Uri
import android.os.Binder
import android.os.Build
import android.os.Handler
import android.os.Environment
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
        /** Public folder saved uploads are moved into: /storage/emulated/0/FileServer */
        const val SAVE_DIR_NAME = "FileServer"
        /** App-private quarantine folder browser uploads are written to first */
        const val QUARANTINE_DIR_NAME = "incoming"
    }

    private val binder = LocalBinder()
    private val sharedFiles = mutableListOf<SharedFile>()
    private val pendingUploads = mutableListOf<PendingUpload>()
    private val fileDescriptors = mutableMapOf<String, ParcelFileDescriptor>()
    private val mainHandler = Handler(Looper.getMainLooper())

    /** Notified on the main thread whenever the shared file list changes */
    var onFilesChanged: ((uploadedFile: SharedFile?) -> Unit)? = null

    /** Notified on the main thread whenever the pending-upload list changes */
    var onPendingUploadsChanged: ((newArrival: PendingUpload?) -> Unit)? = null

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
        clearOrphanedUploads()

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

    /**
     * Quarantine directory browser uploads are written into. Lives in the app's
     * private internal storage, so uploads are not visible to other apps, not
     * indexed by the media scanner, and never served back over HTTP. Files stay
     * here until the user explicitly saves them.
     */
    fun quarantineDir(): File {
        val dir = File(filesDir, QUARANTINE_DIR_NAME)
        if (!dir.exists()) {
            dir.mkdirs()
        }
        return dir
    }

    /**
     * Public folder saved uploads are moved into. Falls back to app-private
     * external storage when we do not hold all-files access.
     */
    fun saveDir(): File {
        val publicDir = File(Environment.getExternalStorageDirectory(), SAVE_DIR_NAME)
        if (publicDir.exists() || publicDir.mkdirs()) {
            return publicDir
        }
        Log.w(TAG, "Cannot use ${publicDir.absolutePath}, falling back to app storage")
        val base = getExternalFilesDir(null) ?: filesDir
        val dir = File(base, SAVE_DIR_NAME)
        if (!dir.exists()) {
            dir.mkdirs()
        }
        return dir
    }

    /**
     * A browser upload finished. It goes onto the pending list only: it is never
     * added to the shared (downloadable) files and never leaves the quarantine
     * directory until the user taps Save.
     */
    private fun onUploadReceived(id: String, name: String, path: String, size: Long) {
        val upload = PendingUpload(id, name, path, size)
        pendingUploads.add(upload)
        Log.i(TAG, "Quarantined upload: $name ($size bytes) -> $path")
        onPendingUploadsChanged?.invoke(upload)
    }

    /**
     * The pending list only lives in memory, so anything left in quarantine at
     * startup is from a previous session the user never acted on. Those files
     * would otherwise sit there unreachable, so drop them.
     */
    private fun clearOrphanedUploads() {
        val leftovers = quarantineDir().listFiles() ?: return
        var deleted = 0
        for (file in leftovers) {
            if (file.delete()) deleted++ else Log.w(TAG, "Could not delete orphan ${file.name}")
        }
        if (deleted > 0) {
            Log.i(TAG, "Cleared $deleted orphaned upload(s) from a previous session")
        }
    }

    fun getPendingUploads(): List<PendingUpload> = pendingUploads.toList()

    /**
     * Move a quarantined upload into public storage. Returns the destination
     * file, or null if the move failed.
     */
    fun savePendingUpload(upload: PendingUpload): File? {
        val source = File(upload.path)
        if (!source.exists()) {
            Log.e(TAG, "Pending upload missing on disk: ${upload.path}")
            pendingUploads.remove(upload)
            onPendingUploadsChanged?.invoke(null)
            return null
        }

        val dest = uniqueDestination(saveDir(), upload.displayName)
        val moved = try {
            // renameTo fails across mount points, so fall back to a copy
            if (source.renameTo(dest)) {
                true
            } else {
                source.copyTo(dest, overwrite = true)
                source.delete()
                true
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to save ${upload.displayName}", e)
            false
        }

        if (!moved) return null

        MediaScannerConnection.scanFile(this, arrayOf(dest.absolutePath), null, null)
        pendingUploads.remove(upload)
        onPendingUploadsChanged?.invoke(null)
        Log.i(TAG, "Saved upload: ${dest.absolutePath}")
        return dest
    }

    /** Saves every pending upload. Returns how many were written. */
    fun saveAllPendingUploads(): Int {
        var saved = 0
        for (upload in pendingUploads.toList()) {
            if (savePendingUpload(upload) != null) saved++
        }
        return saved
    }

    /** Deletes a quarantined upload without ever moving it to public storage */
    fun discardPendingUpload(upload: PendingUpload) {
        File(upload.path).delete()
        pendingUploads.remove(upload)
        onPendingUploadsChanged?.invoke(null)
        Log.i(TAG, "Discarded upload: ${upload.displayName}")
    }

    fun discardAllPendingUploads() {
        for (upload in pendingUploads.toList()) {
            File(upload.path).delete()
        }
        pendingUploads.clear()
        onPendingUploadsChanged?.invoke(null)
    }

    /** Avoids clobbering an existing file: report.pdf -> report (1).pdf */
    private fun uniqueDestination(dir: File, name: String): File {
        var candidate = File(dir, name)
        if (!candidate.exists()) return candidate

        val base = name.substringBeforeLast('.', name)
        val ext = name.substringAfterLast('.', "")
        var n = 1
        while (candidate.exists()) {
            val suffix = if (ext.isEmpty()) "" else ".$ext"
            candidate = File(dir, "$base ($n)$suffix")
            n++
        }
        return candidate
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

        // Uploads are written to the private quarantine dir, never public storage
        NativeServer.setUploadDir(quarantineDir().absolutePath)

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
