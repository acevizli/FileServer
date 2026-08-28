package com.acevizli.fileserver

/**
 * A file uploaded from the web UI that is sitting in the app's private quarantine
 * directory. It is not shared for download and not visible to other apps until
 * the user explicitly saves it to public storage.
 */
data class PendingUpload(
    val id: String,
    val displayName: String,
    /** Absolute path inside the quarantine directory */
    val path: String,
    val size: Long,
    val receivedAt: Long = System.currentTimeMillis()
)
