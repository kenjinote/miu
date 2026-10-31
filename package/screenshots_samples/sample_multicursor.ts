// miu — Acrylic & Mica Frosted Glass Theme Engine
export interface BackdropConfig {
  readonly mode: "dark" | "light";
  readonly glassAlpha: number;
  readonly blurRadiusPx: number;
  readonly tintColor: string;
}

export function createThemePreset(isDark: boolean): BackdropConfig {
  const glassAlpha = isDark ? 0.78 : 0.84;
  return {
    mode: isDark ? "dark" : "light",
    glassAlpha: glassAlpha,
    blurRadiusPx: 32,
    tintColor: isDark ? "#0F172A" : "#F8FAFC",
  };
}

export function blendAcrylicLayer(baseAlpha: number): number {
  const glassAlpha = Math.min(1.0, Math.max(0.15, baseAlpha));
  const clamped = glassAlpha * 0.95;
  console.log(`[D2D] Acrylic glassAlpha=${glassAlpha.toFixed(2)} -> ${clamped.toFixed(2)}`);
  return glassAlpha;
}
