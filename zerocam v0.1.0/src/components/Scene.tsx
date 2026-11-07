const TICKER =
  "ZEROCAM /// WINDOWS XP + MSPAINT CAMERA UI /// LIVE EFFECTS /// MIRROR /// DRAW ON SCREEN /// PHOTO + VIDEO /// L2/R2 TRIGGERS /// 4·6·8·10 SEC TIMER /// DATE STAMP /// ZOOM 1X-4X /// ";

export default function Scene() {
  return (
    <div className="pointer-events-none absolute inset-0 z-0 overflow-hidden font-[Tahoma,Verdana,sans-serif]" aria-hidden>
      {/* ambient glows */}
      <div className="absolute -top-40 left-[8%] h-[34rem] w-[46rem] rounded-full bg-[radial-gradient(closest-side,rgba(255,255,255,0.13),transparent)] blur-2xl" />
      <div className="absolute -bottom-52 right-[2%] h-[36rem] w-[50rem] rounded-full bg-[radial-gradient(closest-side,rgba(255,255,255,0.11),transparent)] blur-2xl" />

      {/* perspective grid */}
      <div className="grid-floor" />

      {/* giant backdrop type — centered behind device */}
      <div className="absolute inset-0 flex items-center justify-center">
        <h1 className="whitespace-nowrap text-[18vw] font-bold leading-none text-white/[0.06]">
          ZEROCAM
        </h1>
      </div>

      {/* ticker */}
      <div className="absolute inset-x-0 bottom-0 border-t border-white/20 bg-[#083f9f]/80 py-1 backdrop-blur-md">
        <div className="flex w-max whitespace-nowrap" style={{ animation: "marqueeX 28s linear infinite" }}>
          {[0, 1].map((k) => (
            <span key={k} className="pr-2 text-[10px] font-bold tracking-widest text-white/60">
              {TICKER}
              {TICKER}
            </span>
          ))}
        </div>
      </div>
    </div>
  );
}
