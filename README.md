# XenosTube

An open-source, self-hosted YouTube client and Python proxy backend for the Xbox 360.

XenosTube brings modern YouTube search and playback to homebrew-enabled Xbox 360 consoles without requiring proprietary files or outdated TLS/SSL workarounds. The Xbox 360 client connects over plain HTTP to a lightweight Python backend running on your PC, Mac, Linux machine, or Raspberry Pi, which fetches video feeds and serves Xbox 360-compatible H.264 + AAC (`.mp4`, <= 1080p 30fps) streams.

## Features
- **Clean-Room Homebrew Client:** Built with LibXenon (`xenon.elf`).
- **Plug-and-Play Configuration:** Point the console to any server via a simple `config.ini` file.
- **Lightweight Python Backend:** No Docker required—runs natively with FastAPI, `yt-dlp`, and FFmpeg.

---

## Quick Start

### 1. Start the Backend Server (PC / Mac / Linux)
1. Install **Python 3** and **FFmpeg** (`winget install Gyan.FFmpeg` on Windows).
2. Open the `server` folder and run `start_server.bat` (Windows) or run:
   ```bash
   pip install -r requirements.txt
   python server.py
