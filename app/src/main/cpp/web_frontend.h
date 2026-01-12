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
            <p class="subtitle">Download shared files securely</p>
            <div class="file-count" id="fileCount">Loading...</div>
        </header>
        
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
        
        async function loadFiles() {
            try {
                const response = await fetch('/api/files');
                const files = await response.json();
                
                const container = document.getElementById('filesContainer');
                const countEl = document.getElementById('fileCount');
                
                countEl.textContent = files.length + ' file' + (files.length !== 1 ? 's' : '') + ' available';
                
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
                            <div class="file-name">${file.name}</div>
                            <div class="file-size">${formatFileSize(file.size)}</div>
                        </div>
                        <a href="/download/${file.id}" class="download-btn" download="${file.name}">
                            ⬇️ Download
                        </a>
                    </div>
                `).join('');
                
            } catch (error) {
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
        
        loadFiles();
    </script>
</body>
</html>)HTML";
}

} // namespace WebFrontend
