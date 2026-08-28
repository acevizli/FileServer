package com.acevizli.fileserver

import android.text.format.DateUtils
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView

class PendingUploadsAdapter(
    private val onSave: (PendingUpload) -> Unit,
    private val onDiscard: (PendingUpload) -> Unit
) : ListAdapter<PendingUpload, PendingUploadsAdapter.ViewHolder>(DiffCallback) {

    class ViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val iconView: ImageView = view.findViewById(R.id.fileIcon)
        val nameView: TextView = view.findViewById(R.id.fileName)
        val detailView: TextView = view.findViewById(R.id.fileDetail)
        val saveButton: ImageButton = view.findViewById(R.id.saveButton)
        val discardButton: ImageButton = view.findViewById(R.id.discardButton)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_pending_upload, parent, false)
        return ViewHolder(view)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val item = getItem(position)

        holder.nameView.text = item.displayName
        holder.detailView.text = "${FileFormat.size(item.size)} • ${relativeTime(item.receivedAt)}"
        holder.iconView.setImageResource(FileFormat.icon(item.displayName))

        holder.saveButton.setOnClickListener { onSave(item) }
        holder.discardButton.setOnClickListener { onDiscard(item) }
    }

    private fun relativeTime(millis: Long): CharSequence =
        DateUtils.getRelativeTimeSpanString(
            millis, System.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS
        )

    companion object DiffCallback : DiffUtil.ItemCallback<PendingUpload>() {
        override fun areItemsTheSame(oldItem: PendingUpload, newItem: PendingUpload) =
            oldItem.id == newItem.id

        override fun areContentsTheSame(oldItem: PendingUpload, newItem: PendingUpload) =
            oldItem == newItem
    }
}
