interface BottomBarProps {
  progressPercent: number
  statusMessage: string
  activeNamesetTitle: string
  charPerPage: number
  onOpenNamesetChooser: () => void
  onCharPerPageChange: (chars: number) => void
}

export function BottomBar({
  progressPercent,
  statusMessage,
  activeNamesetTitle,
  charPerPage,
  onOpenNamesetChooser,
  onCharPerPageChange,
}: BottomBarProps) {
  return (
    <footer className="bottom-bar">
      <div className="bottom-left">
        <div className="progress-bar-container">
          <div
            className="progress-bar-fill"
            style={{ width: `${Math.min(100, Math.max(0, progressPercent))}%` }}
          />
        </div>
        <span className="bottom-status-text">{statusMessage}</span>
      </div>

      <div className="bottom-right">
        <button
          className="btn current-nameset-btn"
          onClick={onOpenNamesetChooser}
          title="Active novel nameset (click to change)"
        >
          {activeNamesetTitle || 'None'}
        </button>

        <div className="char-per-page-container" title="Characters per page">
          <label htmlFor="char-per-page" className="char-label">
            Chars/page:
          </label>
          <input
            id="char-per-page"
            className="char-per-page-input"
            type="number"
            min={1000}
            max={50000}
            step={1000}
            value={charPerPage}
            onChange={(e) => {
              const val = parseInt(e.target.value, 10)
              if (!isNaN(val) && val >= 500) {
                onCharPerPageChange(val)
              }
            }}
          />
        </div>
      </div>
    </footer>
  )
}

