package com.acevizli.fileserver

import android.Manifest
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.PackageManager
import android.net.Uri
import android.net.wifi.WifiManager
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.os.IBinder
import android.provider.OpenableColumns
import android.provider.Settings
import android.text.Editable
import android.text.TextWatcher
import android.view.View
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.LinearLayoutManager
import com.acevizli.fileserver.databinding.ActivityMainBinding
import java.net.Inet4Address
import java.net.NetworkInterface

class MainActivity : AppCompatActivity() {
    
    private lateinit var binding: ActivityMainBinding
    private var fileServerService: FileServerService? = null
    private var serviceBound = false
    
    private val sharedFilesAdapter = SharedFilesAdapter { file ->
        fileServerService?.removeFile(file)
        updateFilesList()
    }

    private val pendingUploadsAdapter = PendingUploadsAdapter(
        onSave = { upload ->
            val saved = fileServerService?.savePendingUpload(upload)
            updatePendingList()
            Toast.makeText(
                this,
                if (saved != null) "Saved to ${saved.absolutePath}" else "Could not save ${upload.displayName}",
                Toast.LENGTH_LONG
            ).show()
        },
        onDiscard = { upload ->
            fileServerService?.discardPendingUpload(upload)
            updatePendingList()
            Toast.makeText(this, "Discarded ${upload.displayName}", Toast.LENGTH_SHORT).show()
        }
    )
    
    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
            val localBinder = binder as FileServerService.LocalBinder
            fileServerService = localBinder.getService()
            serviceBound = true

            fileServerService?.onFilesChanged = { updateFilesList() }

            // Browser uploads land in quarantine and wait for the user to save them
            fileServerService?.onPendingUploadsChanged = { arrival ->
                updatePendingList()
                if (arrival != null) {
                    Toast.makeText(
                        this@MainActivity,
                        "Received ${arrival.displayName}",
                        Toast.LENGTH_SHORT
                    ).show()
                }
            }

            updateUI()
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            fileServerService?.onFilesChanged = null
            fileServerService?.onPendingUploadsChanged = null
            fileServerService = null
            serviceBound = false
        }
    }
    
    private val filePickerLauncher = registerForActivityResult(
        ActivityResultContracts.OpenMultipleDocuments()
    ) { uris ->
        if (uris.isNotEmpty()) {
            for (uri in uris) {
                addFileFromUri(uri)
            }
            updateFilesList()
        }
    }
    
    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val allGranted = permissions.all { it.value }
        if (!allGranted) {
            Toast.makeText(this, "Some permissions were denied", Toast.LENGTH_SHORT).show()
        }
    }
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        
        setupUI()
        requestPermissions()
        bindService()
    }
    
    override fun onDestroy() {
        if (serviceBound) {
            fileServerService?.onFilesChanged = null
            fileServerService?.onPendingUploadsChanged = null
            unbindService(serviceConnection)
            serviceBound = false
        }
        super.onDestroy()
    }
    
    private fun setupUI() {
        // RecyclerView setup
        // Both lists expand to their full height and the page scrolls as a whole,
        // so the RecyclerViews must not try to scroll (or consume drags) themselves
        binding.filesRecyclerView.layoutManager = LinearLayoutManager(this)
        binding.filesRecyclerView.adapter = sharedFilesAdapter
        binding.filesRecyclerView.isNestedScrollingEnabled = false
        binding.filesRecyclerView.setHasFixedSize(false)

        binding.pendingRecyclerView.layoutManager = LinearLayoutManager(this)
        binding.pendingRecyclerView.adapter = pendingUploadsAdapter
        binding.pendingRecyclerView.isNestedScrollingEnabled = false
        binding.pendingRecyclerView.setHasFixedSize(false)

        binding.saveAllButton.setOnClickListener {
            val count = fileServerService?.saveAllPendingUploads() ?: 0
            updatePendingList()
            val dir = fileServerService?.saveDir()?.absolutePath
            Toast.makeText(this, "Saved $count file(s) to $dir", Toast.LENGTH_LONG).show()
        }

        binding.discardAllButton.setOnClickListener {
            fileServerService?.discardAllPendingUploads()
            updatePendingList()
            Toast.makeText(this, "Discarded all received files", Toast.LENGTH_SHORT).show()
        }
        
        // Server toggle button
        binding.serverToggle.setOnClickListener {
            toggleServer()
        }
        
        // Add files button
        binding.addFilesButton.setOnClickListener {
            filePickerLauncher.launch(arrayOf("*/*"))
        }
        
        // Clear files button
        binding.clearFilesButton.setOnClickListener {
            fileServerService?.clearFiles()
            updateFilesList()
        }
        
        // Credentials text watchers
        val credentialsWatcher = object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
            override fun afterTextChanged(s: Editable?) {
                updateCredentials()
            }
        }
        
        binding.usernameInput.addTextChangedListener(credentialsWatcher)
        binding.passwordInput.addTextChangedListener(credentialsWatcher)
        
        // Set default port
        binding.portInput.setText(FileServerService.DEFAULT_PORT.toString())
        
        // Display IP address
        updateIpAddress()
    }
    
    private fun requestPermissions() {
        val permissions = mutableListOf<String>()
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            permissions.add(Manifest.permission.READ_MEDIA_IMAGES)
            permissions.add(Manifest.permission.READ_MEDIA_VIDEO)
            permissions.add(Manifest.permission.READ_MEDIA_AUDIO)
            permissions.add(Manifest.permission.POST_NOTIFICATIONS)
        } else {
            permissions.add(Manifest.permission.READ_EXTERNAL_STORAGE)
        }
        
        val permissionsToRequest = permissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        
        if (permissionsToRequest.isNotEmpty()) {
            permissionLauncher.launch(permissionsToRequest.toTypedArray())
        }

        requestAllFilesAccess()
    }

    /**
     * Writing uploads into /storage/emulated/0/FileServer needs all-files access
     * on Android 11+; without it the service falls back to app-private storage.
     */
    private fun requestAllFilesAccess() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return
        if (Environment.isExternalStorageManager()) return

        Toast.makeText(
            this,
            "Allow all-files access so web uploads can be saved to /FileServer",
            Toast.LENGTH_LONG
        ).show()
        try {
            startActivity(
                Intent(
                    Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    Uri.parse("package:$packageName")
                )
            )
        } catch (e: Exception) {
            startActivity(Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION))
        }
    }
    
    private fun bindService() {
        val intent = Intent(this, FileServerService::class.java)
        startService(intent)
        bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE)
    }
    
    private fun toggleServer() {
        val service = fileServerService ?: return
        
        if (service.isRunning) {
            service.stopServer()
        } else {
            // Validate credentials
            val username = binding.usernameInput.text.toString()
            val password = binding.passwordInput.text.toString()
            
            if (username.isEmpty() || password.isEmpty()) {
                Toast.makeText(this, "Please enter username and password", Toast.LENGTH_SHORT).show()
                return
            }
            
            // Get port
            val port = binding.portInput.text.toString().toIntOrNull() ?: FileServerService.DEFAULT_PORT
            
            // Set credentials and start
            service.setCredentials(username, password)
            
            if (service.startServer(port)) {
                Toast.makeText(this, "Server started", Toast.LENGTH_SHORT).show()
            } else {
                Toast.makeText(this, "Failed to start server", Toast.LENGTH_SHORT).show()
            }
        }
        
        updateUI()
    }
    
    private fun updateCredentials() {
        val username = binding.usernameInput.text.toString()
        val password = binding.passwordInput.text.toString()
        
        if (username.isNotEmpty() && password.isNotEmpty()) {
            fileServerService?.setCredentials(username, password)
        }
    }
    
    private fun addFileFromUri(uri: Uri) {
        // Take persistable permission
        try {
            contentResolver.takePersistableUriPermission(
                uri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION
            )
        } catch (e: Exception) {
            // Permission might not be persistable
        }
        
        // Get file info
        var displayName = "Unknown"
        var size = 0L
        
        contentResolver.query(uri, null, null, null, null)?.use { cursor ->
            if (cursor.moveToFirst()) {
                val nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                val sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE)
                
                if (nameIndex >= 0) {
                    displayName = cursor.getString(nameIndex)
                }
                if (sizeIndex >= 0) {
                    size = cursor.getLong(sizeIndex)
                }
            }
        }
        
        fileServerService?.addFile(uri, displayName, size)
    }
    
    private fun updateFilesList() {
        val files = fileServerService?.getSharedFiles() ?: emptyList()
        sharedFilesAdapter.submitList(files.toList())
        
        // Update empty state
        binding.emptyState.visibility = if (files.isEmpty()) View.VISIBLE else View.GONE
        binding.filesRecyclerView.visibility = if (files.isEmpty()) View.GONE else View.VISIBLE
    }
    
    private fun updateUI() {
        val service = fileServerService
        val isRunning = service?.isRunning == true
        
        // Update server toggle button
        binding.serverToggle.text = if (isRunning) "Stop Server" else "Start Server"
        binding.serverToggle.setBackgroundColor(
            ContextCompat.getColor(this, if (isRunning) R.color.stop_color else R.color.start_color)
        )
        
        // Update status
        if (isRunning) {
            val ip = getLocalIpAddress()
            val port = service?.serverPort ?: FileServerService.DEFAULT_PORT
            binding.serverStatus.text = "Running at http://$ip:$port"
            binding.serverStatus.setTextColor(ContextCompat.getColor(this, R.color.status_running))
            binding.uploadPath.text =
                "Uploads are held privately until you save them to ${service?.saveDir()?.absolutePath}"
        } else {
            binding.serverStatus.text = "Not running"
            binding.serverStatus.setTextColor(ContextCompat.getColor(this, R.color.status_stopped))
            binding.uploadPath.text = ""
        }
        
        // Disable inputs when running
        binding.usernameInput.isEnabled = !isRunning
        binding.passwordInput.isEnabled = !isRunning
        binding.portInput.isEnabled = !isRunning
        
        updateFilesList()
        updatePendingList()
    }

    /** The received-from-web card only exists while something is waiting */
    private fun updatePendingList() {
        val pending = fileServerService?.getPendingUploads().orEmpty()
        pendingUploadsAdapter.submitList(pending)
        binding.pendingCard.visibility = if (pending.isEmpty()) View.GONE else View.VISIBLE
        binding.pendingTitle.text = "Received From Web (${pending.size})"
    }

    private fun updateIpAddress() {
        val ip = getLocalIpAddress()
        binding.ipAddress.text = "Your IP: $ip"
    }
    
    private fun getLocalIpAddress(): String {
        try {
            // Try to get WiFi IP first
            val wifiManager = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
            val wifiInfo = wifiManager.connectionInfo
            val ipAddress = wifiInfo.ipAddress
            
            if (ipAddress != 0) {
                return String.format(
                    "%d.%d.%d.%d",
                    ipAddress and 0xff,
                    ipAddress shr 8 and 0xff,
                    ipAddress shr 16 and 0xff,
                    ipAddress shr 24 and 0xff
                )
            }
            
            // Fallback to network interfaces
            for (networkInterface in NetworkInterface.getNetworkInterfaces()) {
                for (address in networkInterface.inetAddresses) {
                    if (!address.isLoopbackAddress && address is Inet4Address) {
                        return address.hostAddress ?: continue
                    }
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        
        return "Unknown"
    }
}