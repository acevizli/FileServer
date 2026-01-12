package com.acevizli.fileserver

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView

class SharedFilesAdapter(
    private val onRemove: (SharedFile) -> Unit
) : ListAdapter<SharedFile, SharedFilesAdapter.ViewHolder>(DiffCallback) {
    
    class ViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val iconView: ImageView = view.findViewById(R.id.fileIcon)
        val nameView: TextView = view.findViewById(R.id.fileName)
        val sizeView: TextView = view.findViewById(R.id.fileSize)
        val removeButton: ImageButton = view.findViewById(R.id.removeButton)
    }
    
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_shared_file, parent, false)
        return ViewHolder(view)
    }
    
    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val file = getItem(position)
        
        holder.nameView.text = file.displayName
        holder.sizeView.text = formatFileSize(file.size)
        holder.iconView.setImageResource(getFileIcon(file.displayName))
        
        holder.removeButton.setOnClickListener {
            onRemove(file)
        }
    }
    
    private fun formatFileSize(bytes: Long): String {
        if (bytes <= 0) return "0 B"
        val units = arrayOf("B", "KB", "MB", "GB")
        val digitGroups = (Math.log10(bytes.toDouble()) / Math.log10(1024.0)).toInt()
        val size = bytes / Math.pow(1024.0, digitGroups.toDouble())
        return String.format("%.1f %s", size, units[digitGroups.coerceAtMost(units.size - 1)])
    }
    
    private fun getFileIcon(fileName: String): Int {
        val ext = fileName.substringAfterLast('.', "").lowercase()
        return when (ext) {
            "jpg", "jpeg", "png", "gif", "webp", "svg" -> R.drawable.ic_image
            "mp4", "mov", "avi", "mkv", "webm" -> R.drawable.ic_video
            "mp3", "wav", "flac", "aac", "ogg" -> R.drawable.ic_audio
            "pdf" -> R.drawable.ic_pdf
            "doc", "docx", "txt" -> R.drawable.ic_document
            "zip", "rar", "7z", "tar", "gz" -> R.drawable.ic_archive
            else -> R.drawable.ic_file
        }
    }
    
    companion object DiffCallback : DiffUtil.ItemCallback<SharedFile>() {
        override fun areItemsTheSame(oldItem: SharedFile, newItem: SharedFile): Boolean {
            return oldItem.id == newItem.id
        }
        
        override fun areContentsTheSame(oldItem: SharedFile, newItem: SharedFile): Boolean {
            return oldItem == newItem
        }
    }
}
