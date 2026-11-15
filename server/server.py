import socket
from fastapi import FastAPI
from fastapi.responses import PlainTextResponse, RedirectResponse
import yt_dlp

APP_NAME = "XenosTube"
VERSION = "1.0.0"
CODENAME = "Jashalla"

app = FastAPI(title=f"{APP_NAME} Backend", version=VERSION)

# Enforce H.264 (avc1) + AAC (mp4a) at <= 30fps for Xbox 360 compatibility
YDL_FORMAT = "best[ext=mp4][vcodec^=avc1][acodec^=mp4a][height<=1080][fps<=30]/best[ext=mp4]"

@app.get("/")
def root():
    return {"app": APP_NAME, "version": VERSION, "codename": CODENAME}

@app.get("/api/list", response_class=PlainTextResponse)
def list_videos(q: str, limit: int = 12):
    ydl_opts = {"quiet": True, "extract_flat": True}
    lines = []
    with yt_dlp.YoutubeDL(ydl_opts) as ydl:
        info = ydl.extract_info(f"ytsearch{limit}:{q}", download=False)
        for entry in info.get("entries", []):
            vid_id = entry.get("id", "")
            duration = int(entry.get("duration") or 0)
            title = entry.get("title", "Untitled").replace("|", "-").replace("\n", " ")[:55]
            lines.append(f"{vid_id}|{duration}|{title}")
    return "\n".join(lines)

@app.get("/api/play/{video_id}")
def play_video(video_id: str):
    ydl_opts = {"quiet": True, "format": YDL_FORMAT}
    with yt_dlp.YoutubeDL(ydl_opts) as ydl:
        info = ydl.extract_info(f"https://www.youtube.com/watch?v={video_id}", download=False)
        return RedirectResponse(url=info["url"])

if __name__ == "__main__":
    import uvicorn
    local_ip = socket.gethostbyname(socket.gethostname())
    print(f"\n=== {APP_NAME} v{VERSION} ({CODENAME}) ===")
    print(f">>> Set server_url=http://{local_ip}:8080 in your Xbox 360 config.ini <<<\n")
    uvicorn.run(app, host="0.0.0.0", port=8080)
