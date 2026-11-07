import { useRef } from "react";
import { BatteryFull, Camera, FlipHorizontal, Images, PencilLine, Video, Wifi } from "lucide-react";
import { filterDef } from "../lib/filters";
import { useCyberCam } from "../state/CyberCamContext";

const pad = (n: number) => String(n).padStart(2, "0");
const fmtClock = (ts: number) => {
  const d = new Date(ts);
  return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
};
const fmtRecord = (ms: number) => {
  const sec = Math.floor(ms / 1000);
  const m = Math.floor(sec / 60);
  const s = sec % 60;
  return `${pad(m)}:${pad(s)}`;
};

function XpBar({ title, right }: { title: string; right?: React.ReactNode }) {
  return (
    <div className="xp-titlebar absolute inset-x-0 top-0 z-20 flex h-6 items-center justify-between px-2 text-[11px] text-white">
      <span className="flex items-center gap-1 font-[Tahoma,Verdana,sans-serif] font-bold">
        <Camera size={12} /> {title}
      </span>
      <span className="flex items-center gap-2">{right}</span>
    </div>
  );
}

function GalleryOverlay() {
  const { media, gIndex, currentMedia } = useCyberCam();
  return (
    <div className="absolute inset-0 z-40 bg-[#c0c0c0] font-[Tahoma,Verdana,sans-serif] text-black">
      <div className="xp-titlebar flex h-6 items-center justify-between px-2 text-[11px] text-white">
        <span className="flex items-center gap-1 font-bold">
          <Images size={12} /> ZeroCam Gallery
        </span>
        <span>{media.length > 0 ? `${gIndex + 1}/${media.length}` : "0/0"}</span>
      </div>
      <div className="absolute inset-x-1 top-7 bottom-6 xp-panel bg-[#d4d0c8] p-2">
        <div className="xp-inset flex h-full items-center justify-center overflow-hidden bg-[#f4f4f4] p-1">
          {currentMedia ? (
            currentMedia.kind === "video" ? (
              <video
                key={currentMedia.id}
                src={currentMedia.blobUrl}
                className="max-h-full max-w-full object-contain"
                controls
                autoPlay
                loop
              />
            ) : (
              <img src={currentMedia.dataUrl ?? currentMedia.thumbUrl} alt={currentMedia.name} className="max-h-full max-w-full object-contain" />
            )
          ) : (
            <div className="text-center text-[12px] text-black/60">No saved files yet.</div>
          )}
        </div>
      </div>
      <div className="xp-statusbar absolute inset-x-0 bottom-0 flex h-5 items-center justify-between px-2 text-[11px]">
        <span>
          {currentMedia
            ? `${currentMedia.name}.${currentMedia.kind === "video" ? "webm" : "bmp"}`
            : "Empty memory"}
        </span>
        <span>{currentMedia ? `${currentMedia.w}×${currentMedia.h}` : ""}</span>
      </div>
    </div>
  );
}

export default function TopScreen() {
  const cam = useCyberCam();
  const {
    power,
    camStatus,
    settings,
    counting,
    flashKey,
    toast,
    toastKey,
    galleryOpen,
    media,
    now,
    recording,
    recordMs,
    canvasRef,
    drawLayerRef,
    topBoxRef,
  } = cam;
  const def = filterDef(settings.filter);
  const drawing = useRef(false);
  const last = useRef<{ x: number; y: number } | null>(null);

  const point = (e: React.PointerEvent<HTMLDivElement>) => {
    const r = e.currentTarget.getBoundingClientRect();
    return {
      x: ((e.clientX - r.left) / r.width) * 400,
      y: ((e.clientY - r.top) / r.height) * 240,
    };
  };

  const strokeTo = (x: number, y: number, move = false) => {
    const c = drawLayerRef.current;
    const ctx = c?.getContext("2d");
    if (!c || !ctx) return;
    ctx.lineCap = "round";
    ctx.lineJoin = "round";
    ctx.strokeStyle = settings.drawColor;
    ctx.lineWidth = 3;
    if (!move || !last.current) {
      ctx.beginPath();
      ctx.moveTo(x, y);
      ctx.lineTo(x + 0.01, y + 0.01);
      ctx.stroke();
    } else {
      ctx.beginPath();
      ctx.moveTo(last.current.x, last.current.y);
      ctx.lineTo(x, y);
      ctx.stroke();
    }
    last.current = { x, y };
  };

  return (
    <div ref={topBoxRef} className="absolute inset-0 overflow-hidden bg-[#bfcde0] font-[Tahoma,Verdana,sans-serif] text-black">
      <canvas ref={canvasRef} width={400} height={240} className={`absolute inset-0 h-full w-full ${power === "app" ? "opacity-100" : "opacity-0"}`} />
      <canvas ref={drawLayerRef} width={400} height={240} className="pointer-events-none absolute inset-0 h-full w-full opacity-0" />

      {power === "off" && (
        <div className="absolute inset-0 flex items-center justify-center bg-[#0c1b33] text-center text-white">
          <div>
            <div className="text-2xl font-bold tracking-wide">ZeroCam</div>
            <div className="mt-2 text-[12px] opacity-80">Press the power button to boot.</div>
          </div>
        </div>
      )}

      {power === "boot" && (
        <div className="absolute inset-0 bg-[#0d1d39] text-white">
          <div className="flex h-full flex-col items-center justify-center gap-4">
            <div className="text-3xl font-bold tracking-wide">ZeroCam</div>
            <div className="xp-inset h-5 w-48 overflow-hidden bg-white p-[2px]">
              <div className="h-full w-full bg-[linear-gradient(90deg,#2d67d8,#7db1ff,#2d67d8)]" style={{ animation: "marqueeX 1.6s linear infinite" }} />
            </div>
            <div className="text-[12px] opacity-80">Loading camera drivers…</div>
          </div>
        </div>
      )}

      {power === "home" && (
        <div className="absolute inset-0 bg-[linear-gradient(180deg,#6aa7ff,#1c63d5)] text-white">
          <div className="absolute inset-0 bg-[radial-gradient(circle_at_20%_20%,rgba(255,255,255,0.7),transparent_18%),radial-gradient(circle_at_60%_28%,rgba(255,255,255,0.55),transparent_24%),radial-gradient(circle_at_80%_18%,rgba(255,255,255,0.45),transparent_16%)]" />
          <div className="relative flex h-full flex-col items-center justify-center gap-3">
            <div className="rounded-md border border-white/40 bg-white/10 px-3 py-2 text-center shadow-lg backdrop-blur-sm">
              <div className="text-2xl font-bold">ZeroCam</div>
              <div className="text-[12px] opacity-90">Windows XP Camera Remix</div>
            </div>
            <div className="rounded-full border border-white/40 bg-white/15 px-3 py-1 text-[12px] shadow-md">
              Press A or START to launch
            </div>
            <div className="absolute bottom-3 right-3 text-[11px] opacity-90">{fmtClock(now)}</div>
          </div>
        </div>
      )}

      {power === "app" && (
        <>
          <XpBar
            title={`ZeroCam Preview${recording ? " - Recording" : ""}`}
            right={
              <>
                <span className="flex items-center gap-1"><Wifi size={10} /> LIVE</span>
                <BatteryFull size={12} />
              </>
            }
          />

          {settings.draw && !galleryOpen && (
            <div
              className="absolute inset-0 z-10 cursor-crosshair"
              onPointerDown={(e) => {
                const p = point(e);
                drawing.current = true;
                strokeTo(p.x, p.y, false);
              }}
              onPointerMove={(e) => {
                if (!drawing.current) return;
                const p = point(e);
                strokeTo(p.x, p.y, true);
              }}
              onPointerUp={() => {
                drawing.current = false;
                last.current = null;
              }}
              onPointerLeave={() => {
                drawing.current = false;
                last.current = null;
              }}
            />
          )}

          {camStatus === "requesting" && (
            <div className="absolute inset-0 z-20 flex items-center justify-center bg-white/65 text-[13px] font-bold text-[#17498b]">
              Requesting camera access…
            </div>
          )}

          {settings.hud && !galleryOpen && (
            <>
              <div className="xp-statusbar absolute inset-x-0 bottom-0 z-20 flex h-5 items-center justify-between px-2 text-[11px]">
                <span className="flex items-center gap-2">
                  <span>{def.name}</span>
                  <span>•</span>
                  <span>{settings.mode === "photo" ? "Photo" : "Video"}</span>
                  <span>•</span>
                  <span>{settings.zoom.toFixed(1)}x</span>
                  {settings.mirror && (
                    <span className="flex items-center gap-1">
                      <FlipHorizontal size={10} /> Mirror
                    </span>
                  )}
                  {settings.draw && (
                    <span className="flex items-center gap-1">
                      <PencilLine size={10} /> Draw
                    </span>
                  )}
                </span>
                <span className="flex items-center gap-2">
                  {settings.dateStamp && <span>Stamp On</span>}
                  <span>{fmtClock(now)}</span>
                </span>
              </div>

              <div className="absolute right-2 top-8 z-20 flex flex-col gap-1 text-[11px] text-[#0a2454]">
                {recording && (
                  <div className="xp-panel flex items-center gap-1 bg-[#ffe0e0] px-2 py-1 text-[#8b0000]">
                    <Video size={11} /> REC {fmtRecord(recordMs)}
                  </div>
                )}
                {counting != null && <div className="xp-panel bg-[#fff7d6] px-2 py-1">Timer: {counting}</div>}
                {camStatus === "denied" && <div className="xp-panel bg-[#eef4ff] px-2 py-1">Demo feed</div>}
              </div>
            </>
          )}

          {toast && !galleryOpen && (
            <div key={toastKey} className="absolute bottom-7 left-1/2 z-30 -translate-x-1/2 xp-panel bg-[#ffffe1] px-3 py-1 text-[11px] text-black shadow-lg">
              {toast}
            </div>
          )}

          {counting != null && (
            <div className="absolute inset-0 z-30 flex items-center justify-center bg-black/15">
              <div key={counting} className="text-[96px] font-bold text-white drop-shadow-[0_2px_4px_rgba(0,0,0,0.75)]" style={{ animation: "countPop 0.95s ease-out both" }}>
                {counting}
              </div>
            </div>
          )}

          {flashKey > 0 && <div key={flashKey} className="pointer-events-none absolute inset-0 z-30 bg-white" style={{ animation: "flashFade 0.35s ease-out both" }} />}
          {galleryOpen && <GalleryOverlay />}

          <div className="pointer-events-none absolute left-2 top-8 z-20 flex items-center gap-2 text-[11px] text-[#123b79]">
            <div className="xp-panel bg-white/70 px-2 py-1">{media.length} saved</div>
            <div className="xp-panel bg-white/70 px-2 py-1">{settings.facing === "user" ? "Inner cam" : "Outer cam"}</div>
          </div>
        </>
      )}
    </div>
  );
}
