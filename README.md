# 📁 FileServer

A simple Android HTTP file server that moves files both ways between your phone and any device on the same local network.

## Why I Built This

**I hate sharing files from my Android device to my Mac.** AirDrop doesn't work, cloud uploads are slow, and USB cables are annoying.

So I built this simple HTTP file server that runs on Android. Just select files, start the server, and access them from any browser on the same Wi-Fi. No apps needed on the receiving end - just a browser.

It works in both directions: the browser can also drop files onto the page and they get written to the phone's storage.

## Features

- 📱 **Pure Android** - No server-side requirements
- 🔐 **Basic Authentication** - Password-protected access via browser's built-in login prompt
- 🌐 **Any Browser** - Works with Safari, Chrome, Firefox, etc.
- 📂 **Any File Type** - Share photos, videos, documents, APKs, anything
- ↔️ **Two-Way Transfer** - Drag files onto the web page to write them to the phone
- 🎨 **Modern Web UI** - Dark-themed file browser with download buttons
- ⚡ **Native Performance** - HTTP server written in C++ for speed
- 🔄 **Background Service** - Keeps running even when you switch apps

## How It Works

```
┌─────────────┐   download (GET)      ┌─────────────┐
│   Android   │ ◄─────────────────────│    Mac /    │
│   Phone     │ ─────────────────────►│   Browser   │
└─────────────┘   upload (POST)       └─────────────┘
                   same Wi-Fi
```

**Phone → Browser**

1. **Select files** you want to share
2. **Set a username/password**
3. **Start the server** - shows your IP address
4. **Open browser** on your Mac/PC → go to `http://<phone-ip>:8080`
5. **Login** and **download** your files

**Browser → Phone**

1. With the server running, open the same page
2. **Drag files onto the drop zone** (or click to browse) and hit Upload
3. The C++ server streams them straight to disk on the phone, under
   `Android/data/com.acevizli.fileserver/files/uploads/`
4. They appear in the app's file list marked `⬆ received`, and stay downloadable from the web page

Uploads are streamed to disk as they arrive, so file size is not limited by the phone's RAM.

### HTTP API

| Method | Path | Purpose |
| --- | --- | --- |
| `GET` | `/` | Web UI |
| `GET` | `/api/files` | JSON list of shared files |
| `GET` | `/download/<id>` | Download a file |
| `POST` | `/api/upload` | `multipart/form-data` upload, returns `{"ok":true,"files":[...]}` |

```bash
# Upload from the command line
curl -u user:pass -F "files=@movie.mp4" http://<phone-ip>:8080/api/upload
```

## Screenshots

*Coming soon*

## Tech Stack

- **Android/Kotlin** - UI, file picker, foreground service
- **C++ (NDK)** - HTTP server with socket programming
- **JNI** - Bridge between Kotlin and C++
- **No external dependencies** - Pure socket programming, no libraries

## Building

```bash
# Clone
git clone https://github.com/acevizli/FileServer.git
cd FileServer

# Build
./gradlew assembleDebug

# APK will be at: app/build/outputs/apk/debug/app-debug.apk
```

## TODO

- [ ] 🔒 **Make it more secure** - Add HTTPS support with self-signed certificates
- [ ] 🗂️ Folder sharing support
- [ ] 📥 Option to save uploads straight to the public Downloads folder
- [ ] 🎨 Customizable web UI themes

## Security Note

> ⚠️ This app uses HTTP Basic Authentication over unencrypted HTTP. This is fine for trusted local networks, but **do not use on public Wi-Fi**. Your credentials are Base64-encoded (not encrypted) and could be intercepted.

Future versions will add HTTPS support for secure transfers.
