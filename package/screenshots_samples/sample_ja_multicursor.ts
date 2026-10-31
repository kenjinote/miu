// miu v1.0.38 — マルチカーソル＆一括同時編集デモ (Ctrl+D / Ctrl+Alt+↑↓)
export interface GlassThemeConfig {
  readonly panelId: string;
  readonly glassAlpha: number;
  readonly blurRadius: number;
  readonly accentGlow: string;
}

// Ctrl+D で同じキーワードを連続選択し、複数箇所を同時に書き換え
export const MIU_WINDOW_PRESETS: Record<string, GlassThemeConfig> = {
  editorPane:   { panelId: "editor",   glassAlpha: 0.78, blurRadius: 32, accentGlow: "#38bdf8" },
  previewPane:  { panelId: "preview",  glassAlpha: 0.85, blurRadius: 24, accentGlow: "#00f5d4" },
  sidebarPane:  { panelId: "sidebar",  glassAlpha: 0.72, blurRadius: 28, accentGlow: "#c084fc" },
  terminalPane: { panelId: "terminal", glassAlpha: 0.80, blurRadius: 36, accentGlow: "#4ade80" },
  statusBar:    { panelId: "status",   glassAlpha: 0.90, blurRadius: 16, accentGlow: "#f472b6" },
  dialogPopup:  { panelId: "dialog",   glassAlpha: 0.88, blurRadius: 40, accentGlow: "#facc15" },
};

export function applyMicaBackdrop(target: GlassThemeConfig): string {
  const alphaPercent = Math.round(target.glassAlpha * 100);
  return `backdrop-filter: blur(${target.blurRadius}px); opacity: ${alphaPercent}%;`;
}
