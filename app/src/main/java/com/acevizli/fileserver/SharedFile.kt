package com.acevizli.fileserver

import android.net.Uri

data class SharedFile(
    val id: String,
    val displayName: String,
    val uri: Uri,
    val size: Long,
    /** Set for files uploaded from a browser - absolute path on the device */
    val localPath: String? = null
) {
    /** True when the file arrived from the web UI rather than the Android file picker */
    val fromWeb: Boolean get() = localPath != null
}
