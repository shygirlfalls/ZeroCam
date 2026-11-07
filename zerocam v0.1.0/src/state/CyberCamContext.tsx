import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from "react";
import {
  DEFAULT_SETTINGS,
  MAX_MEDIA,
  stampName,
  type ButtonId,
  type CamStatus,
  type FilterId,
  type MediaItem,
  type PowerState,
  type Settings,
  type TimerOpt,
} from "../lib/types";
import {
  applyLut,
  applyWarp,
  filterDef,
  FILTERS,
  getBestVideoMime,
  paintDateStamp,
  paintPlasma,
  paintScanlines,
  paintVhs,
} from "../lib/filters";
import { dataUrlToImageData, downloadBlob, encodeBMP } from "../lib/bmp";
import { sfx } from "../lib/sfx";

const VW = 400;
const VH = 240;
const LS_KEY = "zerocam-settings-v3";

interface FlyShot {
  url: string;
  dx: number;
  dy: number;
  rot: number;
  key: number;
}

interface ZeroCamApi {
  power: PowerState;
  camStatus: CamStatus;
  settings: Settings;
  media: MediaItem[];
  galleryOpen: boolean;
  gIndex: number;
  currentMedia: MediaItem | null;
  counting: number | null;
  recording: boolean;
  recordMs: number;
  flashKey: number;
  fly: FlyShot | null;
  toast: string | null;
  toastKey: number;
  pressed: Partial<Record<ButtonId, boolean>>;
  now: number;
  canvasRef: React.RefObject<HTMLCanvasElement | null>;
  drawLayerRef: React.RefObject<HTMLCanvasElement | null>;
  bottomCanvasRef: React.RefObject<HTMLCanvasElement | null>;
  topBoxRef: React.RefObject<HTMLDivElement | null>;
  thumbRef: React.RefObject<HTMLDivElement | null>;
  press: (b: ButtonId) => void;
  powerToggle: () => void;
  launch: () => void;
  retryCamera: () => void;
  setFilter: (id: FilterId) => void;
  cycleFilter: (dir: 1 | -1) => void;
  cycleTimer: () => void;
  setZoom: (z: number) => void;
  zoomBy: (delta: number) => void;
  toggleDateStamp: () => void;
  toggleHud: () => void;
  toggleMirror: () => void;
  toggleDraw: () => void;
  isEraser: boolean;
  setEraser: (on: boolean) => void;
  clearDrawing: () => void;
  setDrawColor: (hex: string) => void;
  toggleShutterSound: () => void;
  setMode: (mode: Settings["mode"]) => void;
  toggleFacing: () => void;
  openGallery: () => void;
  closeGallery: () => void;
  nextMedia: () => void;
  prevMedia: () => void;
  deleteCurrent: () => void;
  downloadMedia: (item: MediaItem, fmt?: "bmp" | "png" | "mp4" | "webm") => void;
  renameMedia: (id: string, newName: string) => void;
  renameOpen: boolean;
  setRenameOpen: (v: boolean) => void;
  setBrightness: (v: number) => void;
  setContrast: (v: number) => void;
  setSaturation: (v: number) => void;
  triggerPrimary: () => void;
}

const Ctx = createContext<ZeroCamApi | null>(null);

function loadSettings(): Settings {
  try {
    const raw = localStorage.getItem(LS_KEY);
    if (raw) return { ...DEFAULT_SETTINGS, ...JSON.parse(raw) };
  } catch { /* ignore */ }
  return DEFAULT_SETTINGS;
}

export function CyberCamProvider({ children }: { children: ReactNode }) {
  const [power, setPower] = useState<PowerState>("app");
  const [camStatus, setCamStatus] = useState<CamStatus>("idle");
  const [settings, setSettings] = useState<Settings>(loadSettings);
  const [media, setMedia] = useState<MediaItem[]>([]);
  const [galleryOpen, setGalleryOpen] = useState(false);
  const [gIndex, setGIndex] = useState(0);
  const [counting, setCounting] = useState<number | null>(null);
  const [recording, setRecording] = useState(false);
  const [recordMs, setRecordMs] = useState(0);
  const [flashKey, setFlashKey] = useState(0);
  const [fly, setFly] = useState<FlyShot | null>(null);
  const [toast, setToast] = useState<string | null>(null);
  const [toastKey, setToastKey] = useState(0);
  const [pressed, setPressed] = useState<Partial<Record<ButtonId, boolean>>>({});
  const [now, setNow] = useState(Date.now());
  const [retry, setRetry] = useState(0);
  const [isEraser, setEraserState] = useState(false);
  const [renameOpen, setRenameOpen] = useState(false);

  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const drawLayerRef = useRef<HTMLCanvasElement | null>(null);
  const bottomCanvasRef = useRef<HTMLCanvasElement | null>(null);
  const topBoxRef = useRef<HTMLDivElement | null>(null);
  const thumbRef = useRef<HTMLDivElement | null>(null);

  const videoRef = useRef<HTMLVideoElement | null>(null);
  const streamRef = useRef<MediaStream | null>(null);
  const tmpRef = useRef<HTMLCanvasElement | null>(null);
  const tinyRef = useRef<HTMLCanvasElement | null>(null);
  const plasmaRef = useRef<HTMLCanvasElement | null>(null);
  const countTimerRef = useRef<number | null>(null);
  const bootTimerRef = useRef<number | null>(null);
  const mediaRecorderRef = useRef<MediaRecorder | null>(null);
  const recorderStreamRef = useRef<MediaStream | null>(null);
  const recordChunksRef = useRef<Blob[]>([]);
  const recordTimerRef = useRef<number | null>(null);
  const recordStartRef = useRef(0);

  const snap = useRef({ power, camStatus, settings, recording });
  useEffect(() => { snap.current = { power, camStatus, settings, recording }; });

  useEffect(() => {
    try { localStorage.setItem(LS_KEY, JSON.stringify(settings)); } catch { /* ignore */ }
  }, [settings]);

  useEffect(() => {
    const id = window.setInterval(() => setNow(Date.now()), 1000);
    return () => window.clearInterval(id);
  }, []);

  const showToast = useCallback((msg: string) => {
    setToast(msg);
    setToastKey((k) => k + 1);
  }, []);

  const clearDrawing = useCallback(() => {
    const c = drawLayerRef.current;
    const ctx = c?.getContext("2d");
    if (!c || !ctx) return;
    ctx.clearRect(0, 0, c.width, c.height);
  }, []);

  const stopStream = useCallback(() => {
    streamRef.current?.getTracks().forEach((t) => t.stop());
    streamRef.current = null;
  }, []);

  const cleanupRecordingTimer = useCallback(() => {
    if (recordTimerRef.current != null) {
      window.clearInterval(recordTimerRef.current);
      recordTimerRef.current = null;
    }
  }, []);

  const stopRecording = useCallback(() => {
    cleanupRecordingTimer();
    const rec = mediaRecorderRef.current;
    if (rec && rec.state !== "inactive") rec.stop();
  }, [cleanupRecordingTimer]);

  useEffect(() => {
    if (power !== "app") {
      stopStream();
      setCamStatus("idle");
      return;
    }
    let cancelled = false;
    const start = async () => {
      setCamStatus("requesting");
      try {
        const stream = await navigator.mediaDevices.getUserMedia({
          video: { facingMode: { ideal: settings.facing }, width: { ideal: 1280 }, height: { ideal: 720 } },
          audio: false,
        });
        if (cancelled) {
          stream.getTracks().forEach((t) => t.stop());
          return;
        }
        stopStream();
        streamRef.current = stream;
        if (!videoRef.current) videoRef.current = document.createElement("video");
        videoRef.current.srcObject = stream;
        videoRef.current.muted = true;
        videoRef.current.playsInline = true;
        await videoRef.current.play().catch(() => undefined);
        setCamStatus("live");
      } catch {
        if (!cancelled) {
          setCamStatus("denied");
          showToast("CAMERA BLOCKED — DEMO ONLINE");
        }
      }
    };
    void start();
    return () => { cancelled = true; };
  }, [power, settings.facing, retry, stopStream, showToast]);

  useEffect(() => {
    let raf = 0;
    const t0 = performance.now();
    let plasmaTick = 0;

    const loop = (ms: number) => {
      raf = requestAnimationFrame(loop);
      const { power: pw, camStatus: cs, settings: st } = snap.current;
      const disp = canvasRef.current;
      if (!disp || pw !== "app") return;
      const dctx = disp.getContext("2d");
      if (!dctx) return;
      const def = filterDef(st.filter);

      if (!tmpRef.current) {
        tmpRef.current = document.createElement("canvas");
        tmpRef.current.width = VW;
        tmpRef.current.height = VH;
      }
      const tmp = tmpRef.current;
      const tctx = tmp.getContext("2d", { willReadFrequently: true })!;
      const vid = videoRef.current;
      const t = (ms - t0) / 1000;
      tctx.clearRect(0, 0, VW, VH);

      if (cs === "live" && vid && vid.readyState >= 2 && vid.videoWidth > 0) {
        const sw = vid.videoWidth;
        const sh = vid.videoHeight;
        const ratio = VW / VH;
        let cw = sw;
        let ch = sw / ratio;
        if (ch > sh) { ch = sh; cw = sh * ratio; }
        cw /= st.zoom;
        ch /= st.zoom;
        const cx = (sw - cw) / 2;
        const cy = (sh - ch) / 2;

        const bcs = `brightness(${st.brightness}) contrast(${st.contrast}) saturate(${st.saturation})`;
        tctx.filter = def.css ? `${def.css} ${bcs}` : bcs;
        if (st.mirror) {
          tctx.save();
          tctx.scale(-1, 1);
          tctx.drawImage(vid, cx, cy, cw, ch, -VW, 0, VW, VH);
          tctx.restore();
        } else {
          tctx.drawImage(vid, cx, cy, cw, ch, 0, 0, VW, VH);
        }
        tctx.filter = "none";
      } else if (cs === "denied") {
        plasmaTick++;
        if (!plasmaRef.current) {
          plasmaRef.current = document.createElement("canvas");
          plasmaRef.current.width = VW / 2;
          plasmaRef.current.height = VH / 2;
        }
        const pc = plasmaRef.current;
        const pctx = pc.getContext("2d")!;
        if (plasmaTick % 2 === 0) paintPlasma(pctx, pc.width, pc.height, t);
        if (st.mirror) {
          tctx.save();
          tctx.scale(-1, 1);
          tctx.drawImage(pc, 0, 0, pc.width, pc.height, -VW, 0, VW, VH);
          tctx.restore();
        } else {
          tctx.drawImage(pc, 0, 0, pc.width, pc.height, 0, 0, VW, VH);
        }
      }

      if (def.pixelate && def.pixelate > 1) {
        if (!tinyRef.current) tinyRef.current = document.createElement("canvas");
        const tiny = tinyRef.current;
        tiny.width = Math.max(1, Math.floor(VW / def.pixelate));
        tiny.height = Math.max(1, Math.floor(VH / def.pixelate));
        const ictx = tiny.getContext("2d")!;
        ictx.clearRect(0, 0, tiny.width, tiny.height);
        ictx.drawImage(tmp, 0, 0, tiny.width, tiny.height);
        tctx.clearRect(0, 0, VW, VH);
        tctx.imageSmoothingEnabled = false;
        tctx.drawImage(tiny, 0, 0, tiny.width, tiny.height, 0, 0, VW, VH);
        tctx.imageSmoothingEnabled = true;
      }

      if (def.lut || st.filter === 'warp') {
        const img = tctx.getImageData(0, 0, VW, VH);
        if (def.lut) applyLut(img, def.lut);
        if (st.filter === 'warp') applyWarp(img, t);
        tctx.putImageData(img, 0, 0);
      }

      dctx.clearRect(0, 0, VW, VH);
      dctx.drawImage(tmp, 0, 0);
      if (drawLayerRef.current) dctx.drawImage(drawLayerRef.current, 0, 0);
      if (st.dateStamp) paintDateStamp(dctx, VW, VH, Date.now());
      if (def.overlay === "scan") paintScanlines(dctx, VW, VH);
      if (def.overlay === "vhs") paintVhs(dctx, VW, VH, t);

      // Render to bottom screen interactive preview
      const bctx = bottomCanvasRef.current?.getContext("2d");
      if (bctx) {
        bctx.clearRect(0, 0, VW, VH);
        bctx.drawImage(tmp, 0, 0);
        if (drawLayerRef.current) bctx.drawImage(drawLayerRef.current, 0, 0);
      }
    };

    raf = requestAnimationFrame(loop);
    return () => cancelAnimationFrame(raf);
  }, []);

  const buildFly = useCallback((url: string, key: number) => {
    const topBox = topBoxRef.current?.getBoundingClientRect();
    const thumb = thumbRef.current?.getBoundingClientRect();
    if (!topBox || !thumb) return;
    const dx = thumb.left + thumb.width / 2 - (topBox.left + topBox.width / 2);
    const dy = thumb.top + thumb.height / 2 - (topBox.top + topBox.height / 2);
    setFly({ url, dx, dy, rot: (Math.random() - 0.5) * 18, key });
    window.setTimeout(() => setFly((f) => (f && f.key === key ? null : f)), 720);
  }, []);

  const pushMedia = useCallback((item: MediaItem, flyUrl?: string) => {
    setMedia((prev) => {
      const next = [item, ...prev];
      if (next.length > MAX_MEDIA) {
        const removed = next.pop();
        if (removed?.kind === "video" && removed.blobUrl) URL.revokeObjectURL(removed.blobUrl);
      }
      return next;
    });
    setGIndex(0);
    if (flyUrl) buildFly(flyUrl, item.ts);
  }, [buildFly]);

  const doCapture = useCallback(() => {
    const disp = canvasRef.current;
    if (!disp) return;
    const st = snap.current.settings;
    const out = document.createElement("canvas");
    out.width = VW * st.scale; out.height = VH * st.scale;
    const octx = out.getContext("2d")!;
    octx.imageSmoothingEnabled = st.scale === 1;
    octx.drawImage(disp, 0, 0, out.width, out.height);
    const ts = Date.now();
    const dataUrl = out.toDataURL("image/jpeg", 0.92);
    pushMedia({
      id: `${ts}-${Math.random().toString(36).slice(2, 7)}`, kind: "photo", name: stampName(ts, "photo"),
      thumbUrl: dataUrl, dataUrl, mime: "image/jpeg", w: out.width, h: out.height,
      filter: st.filter, mirror: st.mirror, dateStamp: st.dateStamp, ts
    }, dataUrl);
    if (st.shutterSound) sfx.shutter();
    setFlashKey((k) => k + 1);
  }, [pushMedia]);

  const startRecording = useCallback(() => {
    const disp = canvasRef.current;
    if (!disp || typeof MediaRecorder === "undefined") return;
    try {
      const stream = disp.captureStream(30);
      const mime = getBestVideoMime();
      const rec = new MediaRecorder(stream, { mimeType: mime });
      recorderStreamRef.current = stream;
      mediaRecorderRef.current = rec;
      recordChunksRef.current = [];
      recordStartRef.current = Date.now();
      setRecording(true);
      showToast("REC START");
      sfx.ui(1240);
      rec.ondataavailable = (e) => { if (e.data.size > 0) recordChunksRef.current.push(e.data); };
      rec.onstop = () => {
        const duration = Math.max(1, Math.round((Date.now() - recordStartRef.current) / 1000));
        const mimeType = rec.mimeType || 'video/mp4';
        const blob = new Blob(recordChunksRef.current, { type: mimeType });
        const blobUrl = URL.createObjectURL(blob);
        const thumbUrl = disp.toDataURL("image/jpeg", 0.86);
        const st = snap.current.settings;
        const ts = Date.now();
        pushMedia({
          id: `${ts}-${Math.random().toString(36).slice(2, 7)}`, kind: "video", name: stampName(ts, "video"),
          thumbUrl, blobUrl, mime: mimeType, w: VW, h: VH, filter: st.filter, mirror: st.mirror, dateStamp: st.dateStamp, ts, duration
        }, thumbUrl);
        recorderStreamRef.current?.getTracks().forEach((t) => t.stop());
        recorderStreamRef.current = null;
        mediaRecorderRef.current = null;
        cleanupRecordingTimer();
        setRecording(false);
        setRecordMs(0);
        showToast("REC SAVED");
      };
      rec.start(250);
      recordTimerRef.current = window.setInterval(() => setRecordMs(Date.now() - recordStartRef.current), 250);
    } catch { showToast("RECORDER ERROR"); sfx.error(); }
  }, [cleanupRecordingTimer, pushMedia, showToast]);

  const runTimedAction = useCallback((action: () => void) => {
    const st = snap.current.settings;
    if (countTimerRef.current != null) return;
    if (st.timer > 0) {
      let n = st.timer;
      setCounting(n);
      if (st.shutterSound) sfx.countTick(false);
      countTimerRef.current = window.setInterval(() => {
        n -= 1;
        if (n <= 0) {
          window.clearInterval(countTimerRef.current!);
          countTimerRef.current = null;
          setCounting(null);
          if (snap.current.settings.shutterSound) sfx.countTick(true);
          action();
        } else {
          setCounting(n);
          if (snap.current.settings.shutterSound) sfx.countTick(false);
        }
      }, 1000);
    } else { action(); }
  }, []);

  const triggerPrimary = useCallback(() => {
    if (snap.current.settings.mode === "video") {
      if (mediaRecorderRef.current && mediaRecorderRef.current.state !== "inactive") stopRecording();
      else runTimedAction(startRecording);
    } else { runTimedAction(doCapture); }
  }, [doCapture, runTimedAction, startRecording, stopRecording]);

  const powerToggle = useCallback(() => {
    sfx.unlock();
    setFly(null);
    if (power === "off") {
      sfx.power(); setPower("boot"); setGalleryOpen(false);
      if (bootTimerRef.current) window.clearTimeout(bootTimerRef.current);
      bootTimerRef.current = window.setTimeout(() => { setPower("home"); sfx.jingle(); }, 2000);
    } else {
      stopRecording(); stopStream(); setPower("off"); setGalleryOpen(false); clearDrawing();
    }
  }, [power, stopRecording, stopStream, clearDrawing]);

  const api = useMemo<ZeroCamApi>(() => ({
    power, camStatus, settings, media, galleryOpen, gIndex,
    currentMedia: media[Math.min(gIndex, media.length - 1)] || null,
    counting, recording, recordMs, flashKey, fly, toast, toastKey, pressed, now,
    canvasRef, drawLayerRef, bottomCanvasRef, topBoxRef, thumbRef,
    press: (b: ButtonId) => {
      sfx.unlock();
      setPressed((p) => ({ ...p, [b]: true }));
      window.setTimeout(() => setPressed((p) => ({ ...p, [b]: false })), 150);
      if (power === "off" || power === "boot") return;
      if (power === "home") { if (b === "A" || b === "START") { sfx.launch(); setPower("app"); } return; }
      if (galleryOpen) {
        if (b === "L" || b === "DL") setGIndex(i => (i - 1 + media.length) % media.length);
        if (b === "R" || b === "DR") setGIndex(i => (i + 1) % media.length);
        if (b === "B" || b === "START") { setGalleryOpen(false); sfx.whoosh(false); }
        return;
      }
      switch (b) {
        case "L": case "R": triggerPrimary(); break;
        case "A": setGIndex(0); setGalleryOpen(true); sfx.whoosh(true); break;
        case "X": setSettings(p => ({ ...p, timer: ([0, 4, 6, 8, 10] as TimerOpt[])[(([0, 4, 6, 8, 10] as TimerOpt[]).indexOf(p.timer) + 1) % 5] })); sfx.ui(920); break;
        case "Y": setSettings(p => ({ ...p, hud: !p.hud })); sfx.toggle(true); break;
        case "B": setSettings(p => ({ ...p, draw: !p.draw })); sfx.toggle(true); break;
        case "SELECT": setSettings(p => ({ ...p, facing: p.facing === "user" ? "environment" : "user" })); sfx.ui(1200); break;
        case "DU": setSettings(p => ({ ...p, zoom: Math.min(4, p.zoom + 0.25) })); sfx.move(); break;
        case "DD": setSettings(p => ({ ...p, zoom: Math.max(1, p.zoom - 0.25) })); sfx.move(); break;
        case "DL": setSettings(p => { const idx = FILTERS.findIndex(f => f.id === p.filter); return { ...p, filter: FILTERS[(idx - 1 + FILTERS.length) % FILTERS.length].id }; }); sfx.move(); break;
        case "DR": setSettings(p => { const idx = FILTERS.findIndex(f => f.id === p.filter); return { ...p, filter: FILTERS[(idx + 1) % FILTERS.length].id }; }); sfx.move(); break;
        case "START": setPower("home"); sfx.whoosh(false); break;
      }
    },
    powerToggle, launch: () => { sfx.launch(); setPower("app"); }, retryCamera: () => setRetry(r => r + 1),
    setFilter: (id) => { setSettings(p => ({ ...p, filter: id })); sfx.move(); },
    cycleFilter: (dir) => { setSettings(p => { const idx = FILTERS.findIndex(f => f.id === p.filter); return { ...p, filter: FILTERS[(idx + dir + FILTERS.length) % FILTERS.length].id }; }); sfx.move(); },
    cycleTimer: () => { setSettings(p => ({ ...p, timer: ([0, 4, 6, 8, 10] as TimerOpt[])[(([0, 4, 6, 8, 10] as TimerOpt[]).indexOf(p.timer) + 1) % 5] })); sfx.ui(920); },
    setZoom: (z) => setSettings(p => ({ ...p, zoom: Math.max(1, Math.min(4, z)) })),
    zoomBy: (d) => setSettings(p => ({ ...p, zoom: Math.max(1, Math.min(4, p.zoom + d)) })),
    toggleDateStamp: () => { setSettings(p => ({ ...p, dateStamp: !p.dateStamp })); sfx.toggle(true); },
    toggleHud: () => { setSettings(p => ({ ...p, hud: !p.hud })); sfx.toggle(true); },
    toggleMirror: () => { setSettings(p => ({ ...p, mirror: !p.mirror })); sfx.toggle(true); },
    toggleDraw: () => { setSettings(p => ({ ...p, draw: !p.draw })); sfx.toggle(true); },
    isEraser, setEraser: (on) => { setEraserState(on); if (on) setSettings(p => ({ ...p, draw: true })); },
    clearDrawing, setDrawColor: (c) => { setSettings(p => ({ ...p, drawColor: c })); sfx.ui(1320); },
    renameOpen, setRenameOpen,
    setBrightness: (v) => setSettings(p => ({ ...p, brightness: Math.max(0.5, Math.min(2, v)) })),
    setContrast: (v) => setSettings(p => ({ ...p, contrast: Math.max(0.5, Math.min(2, v)) })),
    setSaturation: (v) => setSettings(p => ({ ...p, saturation: Math.max(0, Math.min(2, v)) })),
    toggleShutterSound: () => { setSettings(p => ({ ...p, shutterSound: !p.shutterSound })); sfx.ui(1000); },
    setMode: (m) => { setSettings(p => ({ ...p, mode: m })); sfx.ui(m === "video" ? 680 : 860); },
    toggleFacing: () => { setSettings(p => ({ ...p, facing: p.facing === "user" ? "environment" : "user" })); sfx.ui(1200); },
    openGallery: () => { setGIndex(0); setGalleryOpen(true); sfx.whoosh(true); },
    closeGallery: () => { setGalleryOpen(false); sfx.whoosh(false); },
    nextMedia: () => { setGIndex(i => (i + 1) % media.length); sfx.move(); },
    prevMedia: () => { setGIndex(i => (i - 1 + media.length) % media.length); sfx.move(); },
    deleteCurrent: () => {
      if (!media.length) return;
      setMedia(prev => prev.filter((_, i) => i !== gIndex));
      setGIndex(i => Math.max(0, i - 1));
      sfx.back(); showToast("DELETED");
    },
    renameMedia: (id, newName) => {
      if (!newName || !newName.trim()) return;
      setMedia(prev => prev.map(m => m.id === id ? { ...m, name: newName.trim() } : m));
      sfx.ui(1320); showToast("FILE RENAMED");
    },
    downloadMedia: (item, fmt) => {
      if (item.kind === "video") {
        const a = document.createElement("a"); a.href = item.blobUrl || "";
        a.download = `${item.name}.${item.mime.includes("mp4") ? "mp4" : "webm"}`;
        a.click();
      } else if (fmt === "bmp") {
        dataUrlToImageData(item.dataUrl || "").then(img => downloadBlob(encodeBMP(img), `${item.name}.bmp`));
      } else {
        const a = document.createElement("a"); a.href = item.dataUrl || ""; a.download = `${item.name}.png`; a.click();
      }
      sfx.ui(1330);
    },
    triggerPrimary
  }), [power, camStatus, settings, media, galleryOpen, gIndex, counting, recording, recordMs, flashKey, fly, toast, toastKey, pressed, now, clearDrawing, triggerPrimary, powerToggle, isEraser, renameOpen]);

  return <Ctx.Provider value={api}>{children}</Ctx.Provider>;
}

export function useCyberCam() {
  const ctx = useContext(Ctx);
  if (!ctx) throw new Error("useCyberCam must be used within CyberCamProvider");
  return ctx;
}
