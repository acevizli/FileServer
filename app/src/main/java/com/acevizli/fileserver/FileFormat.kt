package com.acevizli.fileserver

/** Shared formatting helpers for the file lists */
object FileFormat {

    fun size(bytes: Long): String {
        if (bytes <= 0) return "0 B"
        val units = arrayOf("B", "KB", "MB", "GB")
        val digitGroups = (Math.log10(bytes.toDouble()) / Math.log10(1024.0)).toInt()
        val value = bytes / Math.pow(1024.0, digitGroups.toDouble())
        return String.format("%.1f %s", value, units[digitGroups.coerceAtMost(units.size - 1)])
    }

    fun icon(fileName: String): Int {
        return when (fileName.substringAfterLast('.', "").lowercase()) {
            "jpg", "jpeg", "png", "gif", "webp", "svg" -> R.drawable.ic_image
            "mp4", "mov", "avi", "mkv", "webm" -> R.drawable.ic_video
            "mp3", "wav", "flac", "aac", "ogg" -> R.drawable.ic_audio
            "pdf" -> R.drawable.ic_pdf
            "doc", "docx", "txt" -> R.drawable.ic_document
            "zip", "rar", "7z", "tar", "gz" -> R.drawable.ic_archive
            else -> R.drawable.ic_file
        }
    }
}
