export type ThemeId = 'dark' | 'sepia' | 'light'
export type FontChoice = 'default' | 'sans' | 'serif'

export interface AppearanceSettings {
  theme: ThemeId
  font: FontChoice
  fontSize: number
  lineHeight: number
}

export const DEFAULT_APPEARANCE: AppearanceSettings = {
  theme: 'dark',
  font: 'default',
  fontSize: 16,
  lineHeight: 1.65,
}

export const FONT_SIZE_MIN = 12
export const FONT_SIZE_MAX = 28
export const LINE_HEIGHT_MIN = 1.2
export const LINE_HEIGHT_MAX = 2.2

const STORAGE_KEY = 'hanvi.appearance'

const THEME_META_COLORS: Record<ThemeId, string> = {
  dark: '#1c1b22',
  sepia: '#e6d8be',
  light: '#f1f2f6',
}

function clamp(value: number, min: number, max: number) {
  return Math.min(max, Math.max(min, value))
}

function isThemeId(value: unknown): value is ThemeId {
  return value === 'dark' || value === 'sepia' || value === 'light'
}

function isFontChoice(value: unknown): value is FontChoice {
  return value === 'default' || value === 'sans' || value === 'serif'
}

export function loadAppearance(): AppearanceSettings {
  try {
    const raw = JSON.parse(localStorage.getItem(STORAGE_KEY) ?? 'null')
    if (!raw || typeof raw !== 'object') return DEFAULT_APPEARANCE
    return {
      theme: isThemeId(raw.theme) ? raw.theme : DEFAULT_APPEARANCE.theme,
      font: isFontChoice(raw.font) ? raw.font : DEFAULT_APPEARANCE.font,
      fontSize:
        typeof raw.fontSize === 'number' && Number.isFinite(raw.fontSize)
          ? Math.round(clamp(raw.fontSize, FONT_SIZE_MIN, FONT_SIZE_MAX))
          : DEFAULT_APPEARANCE.fontSize,
      lineHeight:
        typeof raw.lineHeight === 'number' && Number.isFinite(raw.lineHeight)
          ? Math.round(clamp(raw.lineHeight, LINE_HEIGHT_MIN, LINE_HEIGHT_MAX) * 20) / 20
          : DEFAULT_APPEARANCE.lineHeight,
    }
  } catch {
    return DEFAULT_APPEARANCE
  }
}

export function saveAppearance(settings: AppearanceSettings) {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(settings))
  } catch {
    // ignore storage errors
  }
}

export function applyAppearance(settings: AppearanceSettings) {
  const root = document.documentElement
  root.dataset.theme = settings.theme

  const vnFont =
    settings.font === 'serif'
      ? "Georgia, 'Noto Serif', 'Times New Roman', serif"
      : settings.font === 'sans'
        ? "'Noto Sans', 'Segoe UI', system-ui, sans-serif"
        : "Tahoma, 'Segoe UI', sans-serif"
  const svFont =
    settings.font === 'serif'
      ? "Georgia, 'Noto Serif', 'Times New Roman', serif"
      : settings.font === 'sans'
        ? "'Noto Sans', 'Segoe UI', system-ui, sans-serif"
        : 'Tahoma, sans-serif'
  const cnFont =
    settings.font === 'serif'
      ? "'Noto Serif SC', 'Songti SC', 'SimSun', serif"
      : "'Noto Sans SC', 'Microsoft YaHei', sans-serif"

  root.style.setProperty('--reader-font-vn', vnFont)
  root.style.setProperty('--reader-font-sv', svFont)
  root.style.setProperty('--reader-font-cn', cnFont)
  root.style.setProperty('--reader-font-size-vn', `${settings.fontSize}px`)
  root.style.setProperty('--reader-font-size-cn', `${settings.fontSize + 2}px`)
  root.style.setProperty('--reader-font-size-sv', `${Math.max(11, settings.fontSize - 1)}px`)
  root.style.setProperty('--reader-line-height', String(settings.lineHeight))

  const meta = document.querySelector<HTMLMetaElement>('meta[name="theme-color"]')
  if (meta) meta.content = THEME_META_COLORS[settings.theme]
}
