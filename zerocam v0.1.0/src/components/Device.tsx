import { useEffect, useRef, useState } from "react";
import { ChevronDown, ChevronLeft, ChevronRight, ChevronUp, Home, Power } from "lucide-react";
import { useCyberCam } from "../state/CyberCamContext";
import BottomScreen from "./BottomScreen";
import TopScreen from "./TopScreen";

function Screw({ className }: { className: string }) {
  return (
    <span
      className={`absolute h-[7px] w-[7px] rounded-full ${className}`}
      style={{
        background: "radial-gradient(circle at 35% 30%, #5a6074 0%, #22242e 55%, #0a0b10 100%)",
        boxShadow: "inset 0 1px 1px rgba(255,255,255,0.25), 0 1px 2px rgba(0,0,0,0.8)",
      }}
    >
      <span className="absolute left-1/2 top-1/2 h-px w-[4px] -translate-x-1/2 -translate-y-1/2 rotate-45 bg-black/70" />
    </span>
  );
}

function Shoulder({ side }: { side: "L" | "R" }) {
  const { press, pressed } = useCyberCam();
  const label = side === "L" ? "L2" : "R2";
  return (
    <button
      onClick={() => press(side)}
      data-pressed={pressed[side] ? "true" : undefined}
      className={`btn-chip absolute -top-[10px] z-0 flex h-[22px] w-[132px] items-center justify-center rounded-t-[10px] ${side === "L" ? "left-[52px]" : "right-[52px]"}`}
      style={{ background: "linear-gradient(180deg,#48556f,#12151f)" }}
      title={`${label} trigger — capture`}
    >
      <span className="font-orb text-[8px] font-black tracking-[0.26em] text-cyan-100/85">{label}</span>
    </button>
  );
}

function ZoomSlider() {
  const { settings, setZoom } = useCyberCam();
  const railRef = useRef<HTMLDivElement>(null);
  const [drag, setDrag] = useState(false);
  const ratio = (settings.zoom - 1) / 3;

  const update = (clientY: number) => {
    const r = railRef.current?.getBoundingClientRect();
    if (!r) return;
    const t = 1 - (clientY - r.top) / r.height;
    setZoom(1 + Math.max(0, Math.min(1, t)) * 3);
  };

  useEffect(() => {
    if (!drag) return;
    const mv = (e: PointerEvent) => update(e.clientY);
    const up = () => setDrag(false);
    window.addEventListener("pointermove", mv);
    window.addEventListener("pointerup", up);
    return () => {
      window.removeEventListener("pointermove", mv);
      window.removeEventListener("pointerup", up);
    };
  }, [drag]);

  return (
    <div className="absolute right-[13px] top-[86px] flex h-[190px] w-[36px] flex-col items-center">
      <span className="font-orb text-[7px] font-black tracking-[0.16em] text-cyan-200/80">ZOOM</span>
      <div className="relative mt-1 flex-1">
        <div ref={railRef} className="slider-rail absolute inset-y-0 left-1/2 w-[9px] -translate-x-1/2 rounded-full" />
        {Array.from({ length: 6 }).map((_, i) => (
          <span key={i} className="absolute left-1/2 h-px w-[15px] -translate-x-1/2 bg-black/60" style={{ top: `${(i / 5) * 100}%` }} />
        ))}
        <button
          onPointerDown={(e) => {
            e.preventDefault();
            setDrag(true);
            update(e.clientY);
          }}
          className="slider-knob absolute left-1/2 h-[20px] w-[26px] cursor-grab -translate-x-1/2 rounded-[5px] active:cursor-grabbing"
          style={{ top: `calc(${(1 - ratio) * 100}% - ${(1 - ratio) * 20}px)` }}
          title="Zoom slider"
        />
      </div>
      <span className="font-orb mt-1 text-[6.5px] font-bold tracking-[0.12em] text-cyan-200/45">1X</span>
    </div>
  );
}

function CirclePad() {
  const [off, setOff] = useState({ x: 0, y: 0 });
  const drag = useRef(false);
  const origin = useRef({ x: 0, y: 0 });
  return (
    <div className="relative flex h-[96px] w-[96px] items-center justify-center rounded-full" style={{ background: "radial-gradient(circle at 50% 42%, #232634 0%, #12141c 70%)", boxShadow: "inset 0 3px 10px rgba(0,0,0,0.85), 0 1px 0 rgba(255,255,255,0.06)" }}>
      <div
        onPointerDown={(e) => {
          drag.current = true;
          origin.current = { x: e.clientX - off.x, y: e.clientY - off.y };
          (e.target as HTMLElement).setPointerCapture(e.pointerId);
        }}
        onPointerMove={(e) => {
          if (!drag.current) return;
          const dx = Math.max(-9, Math.min(9, e.clientX - origin.current.x));
          const dy = Math.max(-9, Math.min(9, e.clientY - origin.current.y));
          setOff({ x: dx, y: dy });
        }}
        onPointerUp={() => {
          drag.current = false;
          setOff({ x: 0, y: 0 });
        }}
        className="joy-knob h-[62px] w-[62px] cursor-grab touch-none rounded-full transition-transform duration-150 active:cursor-grabbing"
        style={{ transform: `translate(${off.x}px, ${off.y}px)`, transitionDuration: drag.current ? "0ms" : "220ms" }}
      />
    </div>
  );
}

function DPad() {
  const { press, pressed } = useCyberCam();
  const dirs = [
    { id: "DU" as const, icon: ChevronUp, cls: "left-1/2 top-[3px] -translate-x-1/2", title: "Zoom in" },
    { id: "DD" as const, icon: ChevronDown, cls: "bottom-[3px] left-1/2 -translate-x-1/2", title: "Zoom out" },
    { id: "DL" as const, icon: ChevronLeft, cls: "left-[3px] top-1/2 -translate-y-1/2", title: "Previous effect" },
    { id: "DR" as const, icon: ChevronRight, cls: "right-[3px] top-1/2 -translate-y-1/2", title: "Next effect" },
  ];
  return (
    <div className="relative h-[96px] w-[96px]">
      <div className="btn-round absolute left-1/2 top-1/2 h-[30px] w-[90px] -translate-x-1/2 -translate-y-1/2 rounded-[6px]" />
      <div className="btn-round absolute left-1/2 top-1/2 h-[90px] w-[30px] -translate-x-1/2 -translate-y-1/2 rounded-[6px]" />
      <div className="absolute left-1/2 top-1/2 h-[26px] w-[26px] -translate-x-1/2 -translate-y-1/2 rounded-full" style={{ background: "radial-gradient(circle at 40% 32%, #2b2f3e, #10121a)" }} />
      {dirs.map((d) => (
        <button key={d.id} title={d.title} onClick={() => press(d.id)} className={`absolute z-10 flex h-[30px] w-[30px] items-center justify-center text-cyan-200/50 transition ${pressed[d.id] ? "scale-90 text-cyan-100" : "hover:text-cyan-100"} ${d.cls}`}>
          <d.icon size={15} />
        </button>
      ))}
    </div>
  );
}

function FaceButtons() {
  const { press, pressed } = useCyberCam();
  const btns = [
    { id: "X" as const, pos: "left-1/2 top-0 -translate-x-1/2", hint: "Timer" },
    { id: "Y" as const, pos: "left-0 top-1/2 -translate-y-1/2", hint: "HUD" },
    { id: "A" as const, pos: "right-0 top-1/2 -translate-y-1/2", hint: "Gallery" },
    { id: "B" as const, pos: "bottom-0 left-1/2 -translate-x-1/2", hint: "Draw" },
  ];
  return (
    <div className="relative h-[116px] w-[116px]">
      {btns.map((b) => (
        <div key={b.id} className={`absolute ${b.pos}`}>
          <button onClick={() => press(b.id)} data-pressed={pressed[b.id] ? "true" : undefined} className="btn-round h-[38px] w-[38px] rounded-full" title={`${b.id} — ${b.hint}`} />
          <span className="font-orb pointer-events-none absolute left-1/2 top-1/2 -translate-x-1/2 -translate-y-1/2 text-[9px] font-black text-cyan-100/80">{b.id}</span>
        </div>
      ))}
    </div>
  );
}

export default function Device() {
  const cam = useCyberCam();
  const { power, pressed, powerToggle } = cam;
  const tiltRef = useRef<HTMLDivElement>(null);
  const powered = power !== "off";

  useEffect(() => {
    const el = tiltRef.current;
    if (!el) return;
    const onMove = (e: PointerEvent) => {
      const rx = -(e.clientY / window.innerHeight - 0.5) * 4.5;
      const ry = (e.clientX / window.innerWidth - 0.5) * 6;
      el.style.transform = `rotateX(${rx + 2}deg) rotateY(${ry}deg)`;
    };
    window.addEventListener("pointermove", onMove);
    return () => window.removeEventListener("pointermove", onMove);
  }, []);

  return (
    <div ref={tiltRef} style={{ transformStyle: "preserve-3d", transition: "transform 0.22s ease-out" }}>
      <div className="relative" style={{ animation: "floaty 7s ease-in-out infinite" }}>
        <div className="relative z-10">
          <Shoulder side="L" />
          <Shoulder side="R" />
          <div className={`shell-plastic relative h-[440px] w-[720px] rounded-t-[46px] rounded-b-[12px] transition duration-700 ${powered ? "" : "brightness-[0.72]"}`} style={{ transform: "rotateX(-2.5deg)", transformOrigin: "bottom center" }}>
            <Screw className="left-[14px] top-[14px]" />
            <Screw className="right-[14px] top-[14px]" />

            <div className="absolute left-1/2 top-[12px] flex -translate-x-1/2 items-center gap-3">
              <span className={`h-[5px] w-[5px] rounded-full transition ${powered ? "bg-sky-300 shadow-[0_0_6px_#7dd3fc]" : "bg-black/60"}`} />
              <span className="relative flex h-[18px] w-[18px] items-center justify-center rounded-full" style={{ background: "radial-gradient(circle at 38% 32%, #3a3f52 0%, #12141c 60%, #05060a 100%)", boxShadow: "inset 0 1px 2px rgba(255,255,255,0.15), 0 1px 4px rgba(0,0,0,0.7)" }}>
                <span className={`h-[7px] w-[7px] rounded-full ${cam.camStatus === "live" ? "bg-sky-300 shadow-[0_0_8px_#7dd3fc] animate-[ledPulse_1.6s_ease-in-out_infinite]" : "bg-[#05070d]"}`} style={{ color: "#7dd3fc" }} />
              </span>
              <span className={`h-[5px] w-[5px] rounded-full transition ${cam.recording ? "bg-red-500 shadow-[0_0_8px_#ef4444] animate-[blinkSoft_1s_steps(2)_infinite]" : powered ? "bg-lime-300 shadow-[0_0_6px_#bef264]" : "bg-black/60"}`} />
            </div>

            <div className="speaker-slits absolute left-[26px] top-1/2 h-[150px] w-[13px] -translate-y-1/2 rounded-full opacity-80" />
            <div className="speaker-slits absolute left-[44px] top-1/2 h-[150px] w-[13px] -translate-y-1/2 rounded-full opacity-80" />

            <div className="bezel absolute left-1/2 top-[38px] h-[382px] w-[calc(100%-150px)] -translate-x-1/2 rounded-[14px]">
              <div className="absolute inset-[10px] overflow-hidden rounded-[7px] bg-black">
                <TopScreen />
              </div>
            </div>

            <ZoomSlider />
            <div className="font-zen absolute bottom-[10px] left-[26px] text-[9px] tracking-[0.34em] text-cyan-100/25">ZERO·CAM</div>
            <span className="absolute bottom-[12px] right-[26px] h-[8px] w-[22px] rounded-full" style={{ background: cam.recording ? "linear-gradient(90deg,#f87171,#ef4444)" : powered ? "linear-gradient(90deg,#60a5fa,#bfdbfe)" : "#101219", boxShadow: powered ? `0 0 12px ${cam.recording ? "rgba(239,68,68,0.8)" : "rgba(96,165,250,0.8)"}` : "none", animation: powered ? "ledPulse 2.6s ease-in-out infinite" : "none", color: cam.recording ? "#ef4444" : "#60a5fa" }} />
          </div>
        </div>

        <div className="relative z-0 -my-[3px] flex h-[26px] items-center justify-center">
          <div className="h-[18px] w-[150px] rounded-full" style={{ background: "linear-gradient(180deg,#2c3040,#0c0e15)", boxShadow: "0 2px 8px rgba(0,0,0,0.8), inset 0 1px 0 rgba(255,255,255,0.12)" }} />
          <div className="mx-6 h-[10px] w-[220px] rounded-full" style={{ background: "linear-gradient(180deg,#14161f,#05060a)" }} />
          <div className="h-[18px] w-[150px] rounded-full" style={{ background: "linear-gradient(180deg,#2c3040,#0c0e15)", boxShadow: "0 2px 8px rgba(0,0,0,0.8), inset 0 1px 0 rgba(255,255,255,0.12)" }} />
        </div>

        <div className={`shell-plastic relative z-10 h-[520px] w-[720px] rounded-t-[10px] rounded-b-[48px] transition duration-700 ${powered ? "" : "brightness-[0.72]"}`}>
          <Screw className="bottom-[14px] left-[14px]" />
          <Screw className="bottom-[14px] right-[14px]" />

          <div className="absolute left-[10px] top-[70px] flex h-[120px] w-[14px] flex-col items-center">
            <span className="font-orb text-[6px] font-bold tracking-[0.1em] text-cyan-200/35">VOL</span>
            <div className="slider-rail mt-1 w-[5px] flex-1 rounded-full" />
          </div>

          <div className="flex h-full items-stretch gap-3 px-[26px] pb-[26px] pt-[22px]">
            <div className="flex w-[104px] flex-col items-center justify-center gap-9 pt-2">
              <CirclePad />
              <DPad />
            </div>

            <div className="flex min-w-0 flex-1 flex-col items-center">
              <div className="bezel relative w-full flex-1 rounded-[14px]">
                <div className="absolute inset-[10px] overflow-hidden rounded-[7px] bg-black">
                  <BottomScreen />
                </div>
              </div>
              <div className="mt-2.5 flex h-[34px] items-center gap-5">
                <button onClick={() => cam.press("SELECT")} data-pressed={pressed.SELECT ? "true" : undefined} className="btn-chip flex h-[17px] items-center rounded-full px-4" title="Switch camera">
                  <span className="font-orb text-[7px] font-black tracking-[0.24em] text-cyan-200/70">SELECT</span>
                </button>
                <div className="flex h-[30px] w-[30px] items-center justify-center rounded-full" style={{ background: "radial-gradient(circle at 38% 30%, #2b2f3e, #12141d)", boxShadow: "inset 0 1px 2px rgba(255,255,255,0.15), 0 2px 6px rgba(0,0,0,0.6)" }}>
                  <Home size={12} className="text-cyan-200/50" />
                </div>
                <button onClick={() => cam.press("START")} data-pressed={pressed.START ? "true" : undefined} className="btn-chip flex h-[17px] items-center rounded-full px-4" title="Exit to home">
                  <span className="font-orb text-[7px] font-black tracking-[0.24em] text-cyan-200/70">START</span>
                </button>
              </div>
            </div>

            <div className="flex w-[104px] flex-col items-center justify-center gap-10 pt-4">
              <FaceButtons />
              <div className="flex flex-col items-center gap-1.5">
                <span className={`h-[7px] w-[7px] rounded-full ${cam.recording ? "bg-red-400 shadow-[0_0_8px_#f87171]" : powered ? "bg-sky-300 shadow-[0_0_8px_#7dd3fc]" : "bg-black/60"}`} />
                <span className="font-orb text-[6.5px] font-bold tracking-[0.2em] text-cyan-200/35">{cam.recording ? "REC" : "LIVE"}</span>
              </div>
            </div>
          </div>

          <button onClick={powerToggle} className="group absolute -bottom-[2px] right-[76px] flex flex-col items-center" title="Power [P]">
            <span className={`btn-round flex h-[42px] w-[42px] items-center justify-center rounded-full ${!powered ? "animate-[ringPulse_1.8s_ease-out_infinite]" : ""}`}>
              <Power size={16} className={powered ? "text-cyan-300" : "text-cyan-200/60 group-hover:text-cyan-200"} />
            </span>
            <span className="font-orb mt-1 text-[6.5px] font-bold tracking-[0.26em] text-cyan-200/35">POWER</span>
          </button>

          <div className="absolute bottom-[22px] left-1/2 flex -translate-x-1/2 items-center gap-2 opacity-40">
            <span className="h-[4px] w-[4px] rounded-full bg-black shadow-[inset_0_1px_2px_rgba(0,0,0,1)]" />
            <span className="h-[4px] w-[4px] rounded-full bg-black shadow-[inset_0_1px_2px_rgba(0,0,0,1)]" />
            <span className="h-[4px] w-[4px] rounded-full bg-black shadow-[inset_0_1px_2px_rgba(0,0,0,1)]" />
          </div>
        </div>

        {cam.fly && (
          <img
            key={cam.fly.key}
            src={cam.fly.url}
            alt="capture"
            className="pointer-events-none absolute left-1/2 top-[220px] z-[70] h-[108px] w-[180px] rounded-[4px] border-2 border-sky-300 object-cover shadow-[0_0_30px_rgba(125,211,252,0.6)]"
            style={{ ["--fx" as string]: `${cam.fly.dx}px`, ["--fy" as string]: `${cam.fly.dy}px`, ["--fr" as string]: `${cam.fly.rot}deg`, animation: "flyShot 0.72s cubic-bezier(0.5,0.05,0.3,1) both" }}
          />
        )}

        <div className={`pointer-events-none absolute -bottom-16 left-1/2 h-24 w-[560px] -translate-x-1/2 rounded-full blur-3xl transition duration-1000 ${powered ? "bg-sky-400/16" : "bg-black/0"}`} />
      </div>
    </div>
  );
}
