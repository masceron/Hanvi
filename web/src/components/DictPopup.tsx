import { useEffect } from 'react'
import type { Token } from '../core/types'
import { CloseIcon } from '../icons/SvgIcons'

interface DictPopupProps {
  token: Token | null
  onClose: () => void
}

export function DictPopup({ token, onClose }: DictPopupProps) {
  useEffect(() => {
    function handleKeyDown(e: KeyboardEvent) {
      if (e.key === 'Escape') {
        onClose()
      }
    }
    window.addEventListener('keydown', handleKeyDown)
    return () => window.removeEventListener('keydown', handleKeyDown)
  }, [onClose])

  if (!token) return null

  function copyText(text: string) {
    void navigator.clipboard.writeText(text)
  }

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div className="dict-popup" onClick={(e) => e.stopPropagation()}>
        <div className="popup-header">
          <span className="popup-title">Word Inspection</span>
          <button className="icon-btn close-btn" onClick={onClose} title="Close (Esc)">
            <CloseIcon />
          </button>
        </div>

        <div className="popup-body">
          <div className="entry-row">
            <span className="label">Chinese:</span>
            <span className="value cn-text" onClick={() => copyText(token.cn)} title="Click to copy">
              {token.cn}
            </span>
          </div>

          <div className="entry-row">
            <span className="label">Hán Việt:</span>
            <span className="value sv-text" onClick={() => copyText(token.sv)} title="Click to copy">
              {token.sv}
            </span>
          </div>

          <div className="entry-row">
            <span className="label">Vietnamese:</span>
            <span className="value vn-text" onClick={() => copyText(token.vn)} title="Click to copy">
              {token.vn}
            </span>
          </div>

          {token.isRule && (
            <div className="rule-badge-notice">
              <span>Grammar transformation rule matched</span>
            </div>
          )}
        </div>

        <div className="popup-footer">
          <button className="btn outline" onClick={() => copyText(`${token.cn} -> ${token.vn}`)}>
            Copy Pair
          </button>
          <button className="btn close-popup-btn" onClick={onClose}>
            Close
          </button>
        </div>
      </div>
    </div>
  )
}

