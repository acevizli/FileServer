package com.acevizli.fileserver

import android.net.Uri

data class SharedFile(
    val id: String,
    val displayName: String,
    val uri: Uri,
    val size: Long
)
