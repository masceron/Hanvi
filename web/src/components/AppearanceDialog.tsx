import {
  DEFAULT_APPEARANCE,
  FONT_SIZE_MAX,
  FONT_SIZE_MIN,
  LINE_HEIGHT_MAX,
  LINE_HEIGHT_MIN,
  type AppearanceSettings,
  type FontChoice,
  type ThemeId,
} from '../appearance'
import { Modal } from './Modal'

interface AppearanceDialogProps {
  settings: AppearanceSettings
  onChange: (next: AppearanceSettings) => void
  onClose: () => void
}

const THEMES: { id: ThemeId; label: string; swatch: string; text: string }[] = [
  { id: 'dark', label: 'Dark', swatch: '#2d2d2d', text: '#ffffff' },
  { id: 'sepia', label: 'Sepia', swatch: '#f4ecd8', text: '#3b2f20' },
  { id: 'light', label: 'Light', swatch: '#ffffff', text: '#18181b' },
]

const FONTS: { id: FontChoice; label: string }[] = [
  { id: 'default', label: 'Default' },
  { id: 'sans', label: 'Sans' },
  { id: 'serif', label: 'Serif' },
]

export function AppearanceDialog({ settings, onChange, onClose }: AppearanceDialogProps) {
  const isDefault =
    settings.theme === DEFAULT_APPEARANCE.theme &&
    settings.font === DEFAULT_APPEARANCE.font &&
    settings.fontSize === DEFAULT_APPEARANCE.fontSize &&
    settings.lineHeight === DEFAULT_APPEARANCE.lineHeight

  function update(patch: Partial<AppearanceSettings>) {
    onChange({ ...settings, ...patch })
  }

  return (
    <Modal
      title="Appearance"
      className="appearance-dialog"
      onClose={onClose}
      footer={(close) => (
        <>
          <button
            type="button"
            className="btn appearance-reset-btn"
            disabled={isDefault}
            onClick={() => onChange(DEFAULT_APPEARANCE)}
          >
            Reset
          </button>
          <button type="button" className="btn close-popup-btn" onClick={close}>
            Close
          </button>
        </>
      )}
    >
      <div className="popup-body appearance-body">
        <div className="appearance-group">
          <span className="appearance-label">Theme</span>
          <div className="appearance-segmented">
            {THEMES.map((t) => (
              <button
                key={t.id}
                type="button"
                className={`appearance-option ${settings.theme === t.id ? 'active' : ''}`}
                onClick={() => update({ theme: t.id })}
              >
                <span
                  className="appearance-swatch"
                  style={{ backgroundColor: t.swatch, color: t.text }}
                  aria-hidden="true"
                >
                  A
                </span>
                <span>{t.label}</span>
              </button>
            ))}
          </div>
        </div>

        <div className="appearance-group">
          <span className="appearance-label">Font</span>
          <div className="appearance-segmented">
            {FONTS.map((f) => (
              <button
                key={f.id}
                type="button"
                className={`appearance-option ${settings.font === f.id ? 'active' : ''}`}
                onClick={() => update({ font: f.id })}
              >
                {f.label}
              </button>
            ))}
          </div>
        </div>

        <div className="appearance-group">
          <div className="appearance-row-header">
            <label htmlFor="appearance-font-size" className="appearance-label">
              Font size
            </label>
            <span className="appearance-value">{settings.fontSize}px</span>
          </div>
          <div className="appearance-slider-row">
            <button
              type="button"
              className="btn btn-sm"
              disabled={settings.fontSize <= FONT_SIZE_MIN}
              onClick={() => update({ fontSize: Math.max(FONT_SIZE_MIN, settings.fontSize - 1) })}
              aria-label="Decrease font size"
            >
              −
            </button>
            <input
              id="appearance-font-size"
              type="range"
              min={FONT_SIZE_MIN}
              max={FONT_SIZE_MAX}
              step={1}
              value={settings.fontSize}
              onChange={(e) => update({ fontSize: Number(e.target.value) })}
            />
            <button
              type="button"
              className="btn btn-sm"
              disabled={settings.fontSize >= FONT_SIZE_MAX}
              onClick={() => update({ fontSize: Math.min(FONT_SIZE_MAX, settings.fontSize + 1) })}
              aria-label="Increase font size"
            >
              +
            </button>
          </div>
        </div>

        <div className="appearance-group">
          <div className="appearance-row-header">
            <label htmlFor="appearance-line-height" className="appearance-label">
              Line spacing
            </label>
            <span className="appearance-value">{settings.lineHeight.toFixed(2)}</span>
          </div>
          <div className="appearance-slider-row">
            <button
              type="button"
              className="btn btn-sm"
              disabled={settings.lineHeight <= LINE_HEIGHT_MIN}
              onClick={() =>
                update({
                  lineHeight: Math.max(LINE_HEIGHT_MIN, Math.round((settings.lineHeight - 0.05) * 20) / 20),
                })
              }
              aria-label="Decrease line spacing"
            >
              −
            </button>
            <input
              id="appearance-line-height"
              type="range"
              min={LINE_HEIGHT_MIN}
              max={LINE_HEIGHT_MAX}
              step={0.05}
              value={settings.lineHeight}
              onChange={(e) => update({ lineHeight: Number(e.target.value) })}
            />
            <button
              type="button"
              className="btn btn-sm"
              disabled={settings.lineHeight >= LINE_HEIGHT_MAX}
              onClick={() =>
                update({
                  lineHeight: Math.min(LINE_HEIGHT_MAX, Math.round((settings.lineHeight + 0.05) * 20) / 20),
                })
              }
              aria-label="Increase line spacing"
            >
              +
            </button>
          </div>
        </div>

        <div className="appearance-preview">
          <div className="role-cn">伴随着房门被推开的响动，少年抬头望去。</div>
          <div className="role-vn">
            Cùng với tiếng vang phòng cửa bị đẩy ra, thiếu niên ngẩng đầu nhìn lại.
          </div>
        </div>
      </div>
    </Modal>
  )
}
