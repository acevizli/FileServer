#pragma once

#include <string>

namespace WebFrontend {

inline std::string getIndexHtml() {
    return R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>File Server</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        :root {
            --bg-primary: #0f0f1a;
            --bg-secondary: #1a1a2e;
            --bg-card: #16213e;
            --accent: #e94560;
            --accent-hover: #ff6b6b;
            --text-primary: #eee;
            --text-secondary: #aaa;
            --border: #333;
            --success: #4ade80;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            background: linear-gradient(135deg, var(--bg-primary) 0%, var(--bg-secondary) 100%);
            min-height: 100vh;
            color: var(--text-primary);
            padding: 20px;
        }
        
        .container {
            max-width: 800px;
            margin: 0 auto;
        }
        
        header {
            text-align: center;
            padding: 40px 20px;
            background: linear-gradient(135deg, var(--bg-card) 0%, rgba(233, 69, 96, 0.1) 100%);
            border-radius: 20px;
            margin-bottom: 30px;
            border: 1px solid var(--border);
            box-shadow: 0 10px 40px rgba(0, 0, 0, 0.3);
        }
        
        .logo {
            font-size: 48px;
            margin-bottom: 10px;
        }
        
        h1 {
            font-size: 2rem;
            font-weight: 700;
            background: linear-gradient(90deg, var(--accent), var(--accent-hover));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
        }
        
        .subtitle {
            color: var(--text-secondary);
            margin-top: 8px;
        }
        
        .file-count {
            display: inline-block;
            background: var(--accent);
            color: white;
            padding: 4px 12px;
            border-radius: 20px;
            font-size: 0.85rem;
            margin-top: 15px;
        }
        
        .upload-card {
            background: var(--bg-card);
            border-radius: 16px;
            padding: 24px;
            margin-bottom: 30px;
            border: 1px solid var(--border);
        }

        .upload-card h2 {
            font-size: 1.1rem;
            margin-bottom: 16px;
        }

        .drop-zone {
            border: 2px dashed var(--border);
            border-radius: 12px;
            padding: 32px 20px;
            text-align: center;
            cursor: pointer;
            transition: all 0.2s ease;
        }

        .drop-zone:hover,
        .drop-zone.dragover {
            border-color: var(--accent);
            background: rgba(233, 69, 96, 0.08);
        }

        .drop-zone .icon {
            font-size: 40px;
            margin-bottom: 10px;
        }

        .drop-zone p {
            color: var(--text-secondary);
            font-size: 0.9rem;
        }

        .drop-zone .browse {
            color: var(--accent);
            font-weight: 600;
        }

        #fileInput {
            display: none;
        }

        .upload-queue {
            margin-top: 16px;
            display: none;
        }

        .queue-item {
            display: flex;
            align-items: center;
            gap: 12px;
            padding: 10px 0;
            border-top: 1px solid var(--border);
            font-size: 0.9rem;
        }

        .queue-item .qname {
            flex: 1;
            min-width: 0;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }

        .queue-item .qstatus {
            color: var(--text-secondary);
            font-size: 0.8rem;
            flex-shrink: 0;
        }

        .queue-item.done .qstatus { color: var(--success); }
        .queue-item.failed .qstatus { color: var(--accent); }

        .progress {
            height: 6px;
            background: var(--bg-secondary);
            border-radius: 3px;
            overflow: hidden;
            margin-top: 16px;
            display: none;
        }

        .progress-bar {
            height: 100%;
            width: 0%;
            background: linear-gradient(90deg, var(--accent), var(--accent-hover));
            transition: width 0.2s ease;
        }

        .upload-btn {
            background: linear-gradient(135deg, var(--accent) 0%, var(--accent-hover) 100%);
            color: white;
            border: none;
            padding: 12px 20px;
            border-radius: 8px;
            font-weight: 600;
            font-size: 0.95rem;
            cursor: pointer;
            width: 100%;
            margin-top: 16px;
            display: none;
        }

        .upload-btn:disabled {
            opacity: 0.6;
            cursor: not-allowed;
        }

        .files-header {
            display: none;
            align-items: center;
            gap: 12px;
            margin-bottom: 16px;
        }

        .files-header h2 {
            font-size: 1.1rem;
            flex: 1;
            margin: 0;
        }

        .files-header .hint {
            display: block;
            font-size: 0.8rem;
            font-weight: 400;
            color: var(--text-secondary);
            margin-top: 2px;
        }

        .download-all-btn {
            background: linear-gradient(135deg, var(--accent) 0%, var(--accent-hover) 100%);
            color: white;
            border: none;
            padding: 10px 16px;
            border-radius: 8px;
            font-weight: 600;
            font-size: 0.9rem;
            cursor: pointer;
            white-space: nowrap;
        }

        a.download-all-btn {
            text-decoration: none;
            display: inline-block;
        }

        .download-all-btn.secondary {
            background: transparent;
            border: 1px solid var(--accent);
            color: var(--accent);
        }

        .download-all-btn:disabled {
            opacity: 0.6;
            cursor: not-allowed;
        }

        .files-grid {
            display: flex;
            flex-direction: column;
            gap: 12px;
        }
        
        .file-card {
            background: var(--bg-card);
            border-radius: 12px;
            padding: 16px 20px;
            display: flex;
            align-items: center;
            gap: 16px;
            border: 1px solid var(--border);
            transition: all 0.3s ease;
            cursor: pointer;
        }
        
        .file-card:hover {
            transform: translateY(-2px);
            box-shadow: 0 8px 30px rgba(233, 69, 96, 0.15);
            border-color: var(--accent);
        }
        
        .file-icon {
            width: 48px;
            height: 48px;
            background: linear-gradient(135deg, var(--accent) 0%, var(--accent-hover) 100%);
            border-radius: 12px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 24px;
            flex-shrink: 0;
        }
        
        .file-info {
            flex: 1;
            min-width: 0;
        }
        
        .file-name {
            font-weight: 600;
            font-size: 1rem;
            color: var(--text-primary);
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }
        
        .file-size {
            color: var(--text-secondary);
            font-size: 0.85rem;
            margin-top: 4px;
        }
        
        .download-btn {
            background: linear-gradient(135deg, var(--accent) 0%, var(--accent-hover) 100%);
            color: white;
            border: none;
            padding: 10px 20px;
            border-radius: 8px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s ease;
            text-decoration: none;
            display: inline-flex;
            align-items: center;
            gap: 8px;
        }
        
        .download-btn:hover {
            transform: scale(1.05);
            box-shadow: 0 4px 20px rgba(233, 69, 96, 0.4);
        }
        
        .empty-state {
            text-align: center;
            padding: 60px 20px;
            background: var(--bg-card);
            border-radius: 16px;
            border: 1px dashed var(--border);
        }
        
        .empty-state .icon {
            font-size: 64px;
            margin-bottom: 20px;
            opacity: 0.5;
        }
        
        .empty-state p {
            color: var(--text-secondary);
        }
        
        .loading {
            text-align: center;
            padding: 40px;
            color: var(--text-secondary);
        }
        
        .spinner {
            width: 40px;
            height: 40px;
            border: 3px solid var(--border);
            border-top-color: var(--accent);
            border-radius: 50%;
            animation: spin 1s linear infinite;
            margin: 0 auto 20px;
        }
        
        @keyframes spin {
            to { transform: rotate(360deg); }
        }
        
        footer {
            text-align: center;
            padding: 30px;
            color: var(--text-secondary);
            font-size: 0.85rem;
        }
        
        @media (max-width: 600px) {
            .file-card {
                flex-wrap: wrap;
            }
            
            .download-btn {
                width: 100%;
                justify-content: center;
                margin-top: 10px;
            }
            
            h1 {
                font-size: 1.5rem;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <div class="logo">📁</div>
            <h1>File Server</h1>
            <p class="subtitle">Send files to your phone, or grab the ones it shares</p>
            <div class="file-count" id="fileCount">Loading...</div>
        </header>

        <div class="upload-card">
            <h2>⬆️ Send to phone</h2>
            <div class="drop-zone" id="dropZone">
                <div class="icon">📤</div>
                <p>Drop files here or <span class="browse">browse</span></p>
                <p style="margin-top: 6px; font-size: 0.8rem;">They get written to the phone's storage</p>
            </div>
            <input type="file" id="fileInput" multiple>
            <div class="upload-queue" id="uploadQueue"></div>
            <div class="progress" id="progress"><div class="progress-bar" id="progressBar"></div></div>
            <button class="upload-btn" id="uploadBtn">Upload</button>
        </div>

        <div class="files-header" id="filesHeader">
            <h2>⬇️ Shared by phone
                <span class="hint" id="filesHint"></span>
            </h2>
            <a class="download-all-btn" id="downloadZipBtn" href="/download-all.zip">Download All as ZIP</a>
            <button class="download-all-btn secondary" id="downloadAllBtn">Download Separately</button>
        </div>

        <div id="filesContainer" class="loading">
            <div class="spinner"></div>
            <p>Loading files...</p>
        </div>
        
        <footer>
            <p>FileServer for Android • Secure file sharing on local network</p>
        </footer>
    </div>

    <script>
        function formatFileSize(bytes) {
            if (bytes === 0) return '0 B';
            const k = 1024;
            const sizes = ['B', 'KB', 'MB', 'GB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
        }
        
        function getFileIcon(filename) {
            const ext = filename.split('.').pop().toLowerCase();
            const icons = {
                'pdf': '📄',
                'doc': '📝', 'docx': '📝',
                'xls': '📊', 'xlsx': '📊',
                'ppt': '📽️', 'pptx': '📽️',
                'jpg': '🖼️', 'jpeg': '🖼️', 'png': '🖼️', 'gif': '🖼️', 'webp': '🖼️', 'svg': '🖼️',
                'mp4': '🎬', 'mov': '🎬', 'avi': '🎬', 'mkv': '🎬', 'webm': '🎬',
                'mp3': '🎵', 'wav': '🎵', 'flac': '🎵', 'aac': '🎵', 'ogg': '🎵',
                'zip': '📦', 'rar': '📦', '7z': '📦', 'tar': '📦', 'gz': '📦',
                'txt': '📃',
                'html': '🌐', 'css': '🎨', 'js': '⚡',
                'apk': '📱',
            };
            return icons[ext] || '📄';
        }
        
        let sharedFiles = [];

        async function loadFiles() {
            try {
                const response = await fetch('/api/files');
                const files = await response.json();
                sharedFiles = files;
                
                const container = document.getElementById('filesContainer');
                const countEl = document.getElementById('fileCount');
                
                countEl.textContent = files.length + ' file' + (files.length !== 1 ? 's' : '') + ' available';

                const header = document.getElementById('filesHeader');
                header.style.display = files.length === 0 ? 'none' : 'flex';
                if (files.length > 0) {
                    const total = files.reduce((sum, f) => sum + f.size, 0);
                    document.getElementById('filesHint').textContent =
                        files.length + ' file' + (files.length !== 1 ? 's' : '') + ' • ' + formatFileSize(total);
                }

                if (files.length === 0) {
                    container.innerHTML = `
                        <div class="empty-state">
                            <div class="icon">📭</div>
                            <p>No files shared yet</p>
                            <p style="margin-top: 8px; font-size: 0.9rem;">Add files from the Android app to share them</p>
                        </div>
                    `;
                    return;
                }
                
                container.className = 'files-grid';
                container.innerHTML = files.map(file => `
                    <div class="file-card">
                        <div class="file-icon">${getFileIcon(file.name)}</div>
                        <div class="file-info">
                            <div class="file-name">${escapeHtml(file.name)}</div>
                            <div class="file-size">${formatFileSize(file.size)}</div>
                        </div>
                        <a href="/download/${encodeURIComponent(file.id)}" class="download-btn" download="${escapeHtml(file.name)}">
                            ⬇️ Download
                        </a>
                    </div>
                `).join('');
                
            } catch (error) {
                document.getElementById('filesHeader').style.display = 'none';
                console.error('Error loading files:', error);
                document.getElementById('filesContainer').innerHTML = `
                    <div class="empty-state">
                        <div class="icon">⚠️</div>
                        <p>Failed to load files</p>
                        <p style="margin-top: 8px; font-size: 0.9rem;">Please refresh the page</p>
                    </div>
                `;
            }
        }
        
        // ---- Upload ----

        const dropZone = document.getElementById('dropZone');
        const fileInput = document.getElementById('fileInput');
        const uploadBtn = document.getElementById('uploadBtn');
        const queueEl = document.getElementById('uploadQueue');
        const progressEl = document.getElementById('progress');
        const progressBar = document.getElementById('progressBar');

        let pending = [];
        let uploading = false;

        function renderQueue() {
            if (pending.length === 0) {
                queueEl.style.display = 'none';
                uploadBtn.style.display = 'none';
                return;
            }

            queueEl.style.display = 'block';
            uploadBtn.style.display = 'block';
            uploadBtn.textContent = 'Upload ' + pending.length + ' file' + (pending.length !== 1 ? 's' : '');

            queueEl.innerHTML = pending.map((f, i) => `
                <div class="queue-item" id="queueItem${i}">
                    <span>${getFileIcon(f.name)}</span>
                    <span class="qname">${escapeHtml(f.name)}</span>
                    <span class="qstatus" id="queueStatus${i}">${formatFileSize(f.size)}</span>
                </div>
            `).join('');
        }

        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }

        function setQueueState(state, message) {
            pending.forEach((f, i) => {
                const item = document.getElementById('queueItem' + i);
                const status = document.getElementById('queueStatus' + i);
                if (item) item.className = 'queue-item ' + state;
                if (status && message) status.textContent = message;
            });
        }

        function addFiles(fileList) {
            if (uploading) return;
            for (const file of fileList) {
                pending.push(file);
            }
            renderQueue();
        }

        dropZone.addEventListener('click', () => {
            if (!uploading) fileInput.click();
        });

        fileInput.addEventListener('change', () => {
            addFiles(fileInput.files);
            fileInput.value = '';
        });

        ['dragenter', 'dragover'].forEach(evt => {
            dropZone.addEventListener(evt, e => {
                e.preventDefault();
                dropZone.classList.add('dragover');
            });
        });

        ['dragleave', 'drop'].forEach(evt => {
            dropZone.addEventListener(evt, e => {
                e.preventDefault();
                dropZone.classList.remove('dragover');
            });
        });

        dropZone.addEventListener('drop', e => {
            if (e.dataTransfer && e.dataTransfer.files) {
                addFiles(e.dataTransfer.files);
            }
        });

        // Let the whole page accept a drop without navigating away
        window.addEventListener('dragover', e => e.preventDefault());
        window.addEventListener('drop', e => e.preventDefault());

        uploadBtn.addEventListener('click', () => {
            if (uploading || pending.length === 0) return;

            const form = new FormData();
            for (const file of pending) {
                form.append('files', file, file.name);
            }

            uploading = true;
            uploadBtn.disabled = true;
            progressEl.style.display = 'block';
            progressBar.style.width = '0%';
            setQueueState('', 'waiting...');

            const xhr = new XMLHttpRequest();
            xhr.open('POST', '/api/upload');

            xhr.upload.addEventListener('progress', e => {
                if (e.lengthComputable) {
                    const percent = Math.round((e.loaded / e.total) * 100);
                    progressBar.style.width = percent + '%';
                    setQueueState('', percent + '%');
                }
            });

            xhr.addEventListener('load', () => {
                uploading = false;
                uploadBtn.disabled = false;

                let result = null;
                try {
                    result = JSON.parse(xhr.responseText);
                } catch (e) {
                    // fall through to the generic error below
                }

                if (xhr.status === 200 && result && result.ok) {
                    progressBar.style.width = '100%';
                    setQueueState('done', 'saved ✓');
                    setTimeout(() => {
                        pending = [];
                        renderQueue();
                        progressEl.style.display = 'none';
                        loadFiles();
                    }, 1200);
                } else {
                    const message = (result && result.error) ? result.error : 'upload failed';
                    setQueueState('failed', message);
                }
            });

            xhr.addEventListener('error', () => {
                uploading = false;
                uploadBtn.disabled = false;
                setQueueState('failed', 'connection lost');
            });

            xhr.send(form);
        });

        // Browsers will not hand us a zip, so fetch each file in turn. They are
        // spaced out because a burst of downloads gets throttled or blocked.
        const downloadAllBtn = document.getElementById('downloadAllBtn');
        let downloadingAll = false;

        downloadAllBtn.addEventListener('click', async () => {
            if (downloadingAll || sharedFiles.length === 0) return;

            downloadingAll = true;
            downloadAllBtn.disabled = true;
            const original = downloadAllBtn.textContent;

            for (let i = 0; i < sharedFiles.length; i++) {
                const file = sharedFiles[i];
                downloadAllBtn.textContent = 'Downloading ' + (i + 1) + '/' + sharedFiles.length;

                const link = document.createElement('a');
                link.href = '/download/' + encodeURIComponent(file.id);
                link.download = file.name;
                document.body.appendChild(link);
                link.click();
                document.body.removeChild(link);

                await new Promise(resolve => setTimeout(resolve, 800));
            }

            downloadAllBtn.textContent = 'Done ✓';
            setTimeout(() => {
                downloadAllBtn.textContent = original;
                downloadAllBtn.disabled = false;
                downloadingAll = false;
            }, 1500);
        });

        loadFiles();
    </script>
</body>
</html>)HTML";
}

} // namespace WebFrontend
