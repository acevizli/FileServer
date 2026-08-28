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
        holder.sizeView.text = if (file.fromWeb) {
            "⬆ received • ${FileFormat.size(file.size)}"
        } else {
            FileFormat.size(file.size)
        }
        holder.iconView.setImageResource(FileFormat.icon(file.displayName))
        
        holder.removeButton.setOnClickListener {
            onRemove(file)
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
