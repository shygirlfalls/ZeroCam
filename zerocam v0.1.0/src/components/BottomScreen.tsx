import { useState, useRef, useCallback } from "react";
import {
  Camera,
  Eraser,
  FlipHorizontal,
  PaintBucket,
  PencilLine,
  RotateCcw,
  Video,
  ChevronDown,
  ZoomIn,
  ZoomOut,
  SwitchCamera,
  Sun,
  Contrast,
  Palette,
  Timer,
} from "lucide-react";
import { QUICK_EFFECTS, filterDef } from "../lib/filters";
import { useCyberCam } from "../state/CyberCamContext";

const DRAW_COLORS = ["#ff0000", "#00a2ff", "#00aa00", "#ff8c00", "#7b3cff", "#000000"];
const pad = (n: number) => String(n).padStart(2, "0");
const fmtClock = (ts: number) => {
  const d = new Date(ts);
  return `${pad(d.getHours())}:${pad(d.getMinutes())}`;
};

/* ===================== XP MENU DROPDOWN ===================== */
function MenuDropdown({ label, items }: { label: string; items: { label: string; onClick: () => void }[] }) {
  const [open, setOpen] = useState(false);
  return (
    <div className="relative">
      <button className="xp-menu-item px-2 flex items-center gap-1 h-full" onClick={() => setOpen(!open)}>
        {label} <ChevronDown size={8} />
      </button>
      {open && (
        <>
          <div className="fixed inset-0 z-[99]" onClick={() => setOpen(false)} />
          <div className="absolute left-0 top-full z-[100] xp-window min-w-[160px] shadow-xl py-1">
            {items.map((it, i) => (
              <button
                key={i}
                className="w-full text-left px-3 py-1 hover:bg-[#1559c5] hover:text-white text-[11px]"
                onClick={() => { it.onClick(); setOpen(false); }}
              >
                {it.label}
              </button>
            ))}
          </div>
        </>
      )}
    </div>
  );
}

/* ===================== XP SLIDER ===================== */
function XpSlider({ label, icon, value, min, max, step, onChange }: {
  label: string;
  icon: React.ReactNode;
  value: number;
  min: number;
  max: number;
  step: number;
  onChange: (v: number) => void;
}) {
  return (
    <div className="flex items-center gap-1 text-[10px]">
      <span className="shrink-0 w-4 flex justify-center">{icon}</span>
      <input
        type="range"
        min={min}
        max={max}
        step={step}
        value={value}
        onChange={(e) => onChange(parseFloat(e.target.value))}
        className="w-full h-2 accent-[#1559c5] cursor-pointer"
        title={`${label}: ${value.toFixed(1)}`}
      />
      <span className="shrink-0 w-6 text-right tabular-nums">{value.toFixed(1)}</span>
    </div>
  );
}

/* ===================== ON-SCREEN KEYBOARD ===================== */
const KB_ROWS = [
  ["1","2","3","4","5","6","7","8","9","0","-","_"],
  ["Q","W","E","R","T","Y","U","I","O","P"],
  ["A","S","D","F","G","H","J","K","L"],
  ["Z","X","C","V","B","N","M",".","/"],
];

function OnScreenKeyboard({ initial, onSave, onCancel }: { initial: string; onSave: (name: string) => void; onCancel: () => void }) {
  const [value, setValue] = useState(initial);

  const press = useCallback((ch: string) => {
    setValue(v => v + ch);
  }, []);

  return (
    <div className="absolute inset-0 z-[200] flex flex-col bg-[#c0c0c0] font-[Tahoma,Verdana,sans-serif] text-black">
      <div className="xp-titlebar flex h-6 shrink-0 items-center justify-between px-2 text-[11px] text-white font-bold">
        <span>Rename File</span>
        <button onClick={onCancel} className="px-1 text-white/80 hover:text-white">✕</button>
      </div>
      <div className="p-3">
        <div className="text-[11px] mb-1">Enter new name:</div>
        <div className="xp-inset flex items-center bg-white px-2 py-1 mb-2">
          <input
            type="text"
            value={value}
            onChange={(e) => setValue(e.target.value)}
            className="w-full bg-transparent text-[13px] outline-none font-bold"
            autoFocus
          />
        </div>
      </div>
      <div className="flex-1 flex flex-col gap-1 px-2 pb-1">
        {KB_ROWS.map((row, ri) => (
          <div key={ri} className="flex gap-1 justify-center">
            {row.map(ch => (
              <button key={ch} className="xp-btn flex-1 min-w-0 h-7 text-[11px] font-bold" onClick={() => press(ch)}>
                {ch}
              </button>
            ))}
          </div>
        ))}
        <div className="flex gap-1 justify-center">
          <button className="xp-btn flex-[2] h-7 text-[11px]" onClick={() => setValue(v => v.slice(0, -1))}>← Bksp</button>
          <button className="xp-btn flex-[5] h-7 text-[11px]" onClick={() => press(" ")}>Space</button>
          <button className="xp-btn flex-[2] h-7 text-[11px]" onClick={() => setValue("")}>Clear</button>
        </div>
      </div>
      <div className="flex gap-2 px-3 pb-3">
        <button className="xp-btn flex-1 h-7 font-bold text-[11px]" onClick={() => { if (value.trim()) onSave(value.trim()); }}>Save</button>
        <button className="xp-btn flex-1 h-7 text-[11px]" onClick={onCancel}>Cancel</button>
      </div>
    </div>
  );
}

/* ===================== BOOT ===================== */
function BootPanel() {
  return (
    <div className="absolute inset-0 bg-[#c0c0c0] font-[Tahoma,Verdana,sans-serif] text-black">
      <div className="xp-titlebar flex h-6 items-center px-2 text-[11px] text-white font-bold">ZeroCam Setup</div>
      <div className="flex h-[calc(100%-24px)] flex-col items-center justify-center gap-3 text-center text-[12px]">
        <div className="text-xl font-bold text-[#134a96]">ZeroCam</div>
        <div className="xp-inset h-5 w-48 overflow-hidden bg-white p-[2px]">
          <div className="h-full w-full bg-[linear-gradient(90deg,#2d67d8,#7db1ff,#2d67d8)]" style={{ animation: "marqueeX 1.6s linear infinite" }} />
        </div>
        <div>Initializing Paint workspace…</div>
      </div>
    </div>
  );
}

/* ===================== HOME ===================== */
function HomePanel() {
  const { launch, now } = useCyberCam();
  return (
    <div className="absolute inset-0 bg-[linear-gradient(180deg,#6aa7ff,#1c63d5)] font-[Tahoma,Verdana,sans-serif] text-white">
      <div className="absolute inset-0 bg-[radial-gradient(circle_at_20%_20%,rgba(255,255,255,0.7),transparent_18%),radial-gradient(circle_at_60%_28%,rgba(255,255,255,0.55),transparent_24%)]" />
      <div className="relative flex h-full flex-col">
        <div className="flex justify-between px-3 py-2 text-[11px]">
          <span>ZeroCam Desktop</span>
          <span>{fmtClock(now)}</span>
        </div>
        <div className="flex flex-1 items-center justify-center">
          <button onClick={launch} className="flex flex-col items-center gap-2 rounded-md border border-white/35 bg-white/10 px-5 py-4 shadow-lg backdrop-blur-sm hover:bg-white/15">
            <Camera size={38} />
            <div className="text-center"><div className="font-bold text-xl">Launch ZeroCam</div></div>
          </button>
        </div>
      </div>
    </div>
  );
}

/* ===================== GALLERY ===================== */
function GalleryPanel() {
  const cam = useCyberCam();
  const { media, currentMedia, nextMedia, prevMedia, deleteCurrent, closeGallery, downloadMedia } = cam;
  return (
    <div className="absolute inset-0 bg-[#c0c0c0] font-[Tahoma,Verdana,sans-serif] text-black">
      <div className="xp-titlebar flex h-6 items-center justify-between px-2 text-[11px] text-white font-bold">
        <span>ZeroCam Gallery</span>
        <span>{media.length > 0 ? `${cam.gIndex + 1}/${media.length}` : "0/0"}</span>
      </div>
      <div className="grid h-[calc(100%-44px)] grid-cols-[1fr_120px] gap-2 p-2">
        <div className="xp-inset flex items-center justify-center overflow-hidden bg-[#e0e0e0] p-1">
          {currentMedia ? (
            currentMedia.kind === "video" ? (
              <video src={currentMedia.blobUrl} className="max-h-full max-w-full object-contain" autoPlay loop muted />
            ) : (
              <img src={currentMedia.dataUrl ?? currentMedia.thumbUrl} alt={currentMedia.name} className="max-h-full max-w-full object-contain" />
            )
          ) : (
            <div className="text-[12px] text-black/60">Session memory is empty.</div>
          )}
        </div>
        <div className="flex flex-col gap-1.5 text-[11px]">
          <button className="xp-btn" onClick={prevMedia}>Previous</button>
          <button className="xp-btn" onClick={nextMedia}>Next</button>
          <button className="xp-btn" onClick={() => currentMedia && downloadMedia(currentMedia, currentMedia.kind === "video" ? "webm" : "png")}>Save Copy</button>
          <button className="xp-btn text-red-700" onClick={deleteCurrent}>Delete</button>
          <div className="mt-auto"><button className="xp-btn w-full font-bold" onClick={closeGallery}>Close</button></div>
        </div>
      </div>
    </div>
  );
}

/* ===================== MAIN PANEL ===================== */
function MainPanel() {
  const cam = useCyberCam();
  const {
    settings, media, currentMedia, bottomCanvasRef, drawLayerRef, recording,
    setEraser, isEraser, toggleMirror, toggleDraw, clearDrawing, setDrawColor,
    setFilter, openGallery, downloadMedia, renameMedia, toggleHud, toggleFacing,
    triggerPrimary, renameOpen, setRenameOpen, setBrightness, setContrast, setSaturation,
    cycleTimer,
  } = cam;

  const [effectsOpen, setEffectsOpen] = useState(true);
  const drawing = useRef(false);
  const last = useRef<{ x: number; y: number } | null>(null);

  const getPos = (e: React.PointerEvent<HTMLCanvasElement>) => {
    const r = e.currentTarget.getBoundingClientRect();
    return { x: ((e.clientX - r.left) / r.width) * 400, y: ((e.clientY - r.top) / r.height) * 240 };
  };

  const draw = (x: number, y: number, move = false) => {
    const c = drawLayerRef.current;
    const ctx = c?.getContext("2d");
    if (!c || !ctx) return;
    ctx.lineCap = "round";
    ctx.lineJoin = "round";
    ctx.lineWidth = isEraser ? 12 : 4;
    ctx.globalCompositeOperation = isEraser ? "destination-out" : "source-over";
    ctx.strokeStyle = settings.drawColor;
    if (!move || !last.current) {
      ctx.beginPath(); ctx.moveTo(x, y); ctx.lineTo(x + 0.01, y + 0.01); ctx.stroke();
    } else {
      ctx.beginPath(); ctx.moveTo(last.current.x, last.current.y); ctx.lineTo(x, y); ctx.stroke();
    }
    last.current = { x, y };
  };

  return (
    <div className="absolute inset-0 bg-[#c0c0c0] font-[Tahoma,Verdana,sans-serif] text-black flex flex-col">
      {/* Rename keyboard overlay */}
      {renameOpen && currentMedia && (
        <OnScreenKeyboard
          initial={currentMedia.name}
          onSave={(name) => { renameMedia(currentMedia.id, name); setRenameOpen(false); }}
          onCancel={() => setRenameOpen(false)}
        />
      )}

      {/* Title Bar */}
      <div className="xp-titlebar flex h-6 shrink-0 items-center justify-between px-2 text-[11px] text-white font-bold">
        <span>ZeroCam - [{currentMedia?.name ?? "Untitled"}]</span>
        <span>{fmtClock(cam.now)}</span>
      </div>

      {/* Menu Bar */}
      <div className="flex h-6 shrink-0 items-center gap-1 border-b border-black/25 bg-[#ece9d8] px-1 text-[11px]">
        <MenuDropdown label="File" items={[
          { label: "Save Current Shot", onClick: () => currentMedia && downloadMedia(currentMedia, "png") },
          { label: "Exit to Home", onClick: () => cam.press("START") }
        ]} />
        <MenuDropdown label="Edit" items={[
          { label: `Mirror: ${settings.mirror ? "ON" : "OFF"}`, onClick: toggleMirror },
          { label: `Draw Mode: ${settings.draw ? "ON" : "OFF"}`, onClick: toggleDraw },
          { label: "Clear Workspace", onClick: clearDrawing },
          { label: "Rename File…", onClick: () => { if (currentMedia) setRenameOpen(true); } }
        ]} />
        <MenuDropdown label="Extras" items={[
          { label: `HUD: ${settings.hud ? "Visible" : "Hidden"}`, onClick: toggleHud },
          { label: `Camera: ${settings.facing === "user" ? "InnerCam" : "OuterCam"}`, onClick: toggleFacing },
          { label: "Retry Camera Signal", onClick: () => cam.retryCamera() }
        ]} />
        <button className="xp-menu-item px-3 h-full font-bold text-[#134a96]" onClick={openGallery}>Gallery</button>
      </div>

      {/* Main Workspace */}
      <div className="flex-1 min-h-0 flex gap-1 p-1">
        {/* Left Toolbar */}
        <div className="xp-panel w-[50px] flex flex-col items-center gap-1 py-1 px-1 shrink-0 overflow-y-auto">
          <button className="xp-tool" data-on={settings.mode === "photo"} onClick={() => cam.setMode("photo")} title="Photo"><Camera size={14} /></button>
          <button className="xp-tool" data-on={settings.mode === "video"} onClick={() => cam.setMode("video")} title="Video"><Video size={14} /></button>
          <button className="xp-tool" data-on={settings.mirror} onClick={toggleMirror} title="Mirror"><FlipHorizontal size={14} /></button>
          <div className="h-px w-full bg-black/20" />
          <button className="xp-tool" data-on={settings.draw && !isEraser} onClick={() => { toggleDraw(); setEraser(false); }} title="Pencil"><PencilLine size={14} /></button>
          <button className="xp-tool" data-on={isEraser} onClick={() => setEraser(true)} title="Eraser"><Eraser size={14} /></button>
          <button className="xp-tool" onClick={clearDrawing} title="Clear"><RotateCcw size={14} /></button>
          <div className="h-px w-full bg-black/20" />
          <button className="xp-tool" onClick={() => cam.zoomBy(0.5)} title="Zoom In"><ZoomIn size={14} /></button>
          <button className="xp-tool" onClick={() => cam.zoomBy(-0.5)} title="Zoom Out"><ZoomOut size={14} /></button>
          <div className="mt-1 grid grid-cols-2 gap-1">
            {DRAW_COLORS.map(c => (
              <button key={c} className="w-4 h-4 border border-black/40" style={{ background: c, boxShadow: settings.drawColor === c ? '0 0 0 1px #fff, 0 0 0 2px #000' : 'none' }} onClick={() => setDrawColor(c)} />
            ))}
          </div>
        </div>

        {/* Canvas + Controls */}
        <div className="flex-1 flex flex-col min-w-0">
          <div className="flex-1 xp-inset bg-black relative overflow-hidden">
            <canvas
              ref={bottomCanvasRef}
              width={400} height={240}
              className="absolute inset-0 w-full h-full bg-black cursor-crosshair"
              onPointerDown={(e) => {
                if (!settings.draw) toggleDraw();
                const p = getPos(e); drawing.current = true; draw(p.x, p.y, false);
              }}
              onPointerMove={(e) => { if (!drawing.current) return; const p = getPos(e); draw(p.x, p.y, true); }}
              onPointerUp={() => { drawing.current = false; last.current = null; }}
              onPointerLeave={() => { drawing.current = false; last.current = null; }}
            />
            {recording && (
              <div className="absolute top-2 right-2 flex items-center gap-1 xp-panel bg-[#ffe0e0] px-2 py-1 text-red-800 text-[10px] font-bold">
                <div className="w-2 h-2 rounded-full bg-red-600 animate-pulse" /> REC
              </div>
            )}
            <button className="absolute bottom-2 right-2 xp-btn flex items-center gap-1 px-2 py-1 text-[10px] font-bold" onClick={toggleFacing} title="Switch camera">
              <SwitchCamera size={12} />
              {settings.facing === "user" ? "INNER" : "OUTER"}
            </button>
          </div>

          {/* Bottom Panel: Bucket toggle + effects OR adjustments */}
          <div className="xp-panel mt-1 p-1 flex items-center gap-1">
            <button
              className="xp-inset w-7 h-7 flex items-center justify-center shrink-0 cursor-pointer"
              onClick={() => setEffectsOpen(v => !v)}
              title={effectsOpen ? "Hide effects / Show adjustments" : "Show effects"}
            >
              <PaintBucket size={14} />
            </button>

            {effectsOpen ? (
              /* Effects swatches */
              <div className="flex-1 flex flex-wrap gap-1 overflow-hidden">
                {QUICK_EFFECTS.map(id => {
                  const f = filterDef(id);
                  return (
                    <button
                      key={id}
                      className="w-5 h-5 border border-black/30"
                      style={{ background: f.swatch, transform: settings.filter === id ? 'scale(1.15)' : 'none', zIndex: settings.filter === id ? 10 : 1 }}
                      onClick={() => setFilter(id)}
                      title={f.name}
                    />
                  );
                })}
              </div>
            ) : (
              /* Adjustments: timer, brightness, contrast, saturation */
              <div className="flex-1 flex flex-col gap-1 px-1">
                <div className="flex items-center gap-2 text-[10px]">
                  <button className="xp-btn h-6 px-2 flex items-center gap-1 text-[10px]" onClick={cycleTimer} title="Timer">
                    <Timer size={12} /> {settings.timer === 0 ? "Off" : `${settings.timer}s`}
                  </button>
                  <span className="text-[9px] text-black/50">Brightness / Contrast / Saturation</span>
                </div>
                <XpSlider label="Brightness" icon={<Sun size={10} />} value={settings.brightness} min={0.5} max={2} step={0.1} onChange={setBrightness} />
                <XpSlider label="Contrast" icon={<Contrast size={10} />} value={settings.contrast} min={0.5} max={2} step={0.1} onChange={setContrast} />
                <XpSlider label="Saturation" icon={<Palette size={10} />} value={settings.saturation} min={0} max={2} step={0.1} onChange={setSaturation} />
              </div>
            )}

            <button className="xp-btn h-7 text-[10px] px-2 font-bold whitespace-nowrap shrink-0" onClick={triggerPrimary}>
              {settings.mode === 'photo' ? 'CAPTURE' : recording ? 'STOP' : 'REC'}
            </button>
          </div>
        </div>
      </div>

      {/* Status Bar */}
      <div className="xp-statusbar h-5 shrink-0 flex items-center justify-between px-2 text-[10px]">
        <span className="flex items-center gap-3">
          <span>{settings.mode === 'photo' ? 'Photo' : 'Video'}</span>
          <span>Zoom: {settings.zoom.toFixed(1)}x</span>
          <span>B:{settings.brightness.toFixed(1)} C:{settings.contrast.toFixed(1)} S:{settings.saturation.toFixed(1)}</span>
        </span>
        <span>Mem: {media.length}/60</span>
      </div>
    </div>
  );
}

/* ===================== ROOT ===================== */
export default function BottomScreen() {
  const { power, galleryOpen } = useCyberCam();
  return (
    <div className="absolute inset-0 overflow-hidden bg-[#c0c0c0]">
      {power === "off" && <div className="absolute inset-0 flex items-center justify-center bg-[#263b59] text-white">ZeroCam is Off</div>}
      {power === "boot" && <BootPanel />}
      {power === "home" && <HomePanel />}
      {power === "app" && (galleryOpen ? <GalleryPanel /> : <MainPanel />)}
    </div>
  );
}
