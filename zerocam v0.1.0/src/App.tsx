import { useEffect, useState } from "react";
import { CyberCamProvider } from "./state/CyberCamContext";
import Scene from "./components/Scene";
import Device from "./components/Device";

const DEVICE_W = 720;
const DEVICE_H = 440 + 26 + 520 + 60;

function Stage() {
  const [scale, setScale] = useState(1);

  useEffect(() => {
    const fit = () => {
      const s = Math.min(
        1.04,
        (window.innerWidth - 40) / DEVICE_W,
        (window.innerHeight - 40) / DEVICE_H,
      );
      setScale(Math.max(0.28, s));
    };
    fit();
    window.addEventListener("resize", fit);
    return () => window.removeEventListener("resize", fit);
  }, []);

  return (
    <div
      className="device-stage absolute inset-0 z-10 flex items-center"
      style={{ perspective: "1900px" }}
    >
      <div style={{ width: DEVICE_W * scale, height: DEVICE_H * scale }}>
        <div
          style={{
            width: DEVICE_W,
            height: DEVICE_H,
            transform: `scale(${scale})`,
            transformOrigin: "center center",
          }}
        >
          <Device />
        </div>
      </div>
    </div>
  );
}

export default function App() {
  return (
    <CyberCamProvider>
      <div className="clouds-bg relative h-dvh w-full select-none overflow-hidden font-[Tahoma,Verdana,sans-serif]">
        <Scene />
        <Stage />
      </div>
    </CyberCamProvider>
  );
}
