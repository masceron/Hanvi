import { useState } from 'react'
import { Modal } from './Modal'

interface PasteDialogProps {
  onText: (text: string) => void
  onClose: () => void
}

// Where the page may not read the clipboard itself (always over plain HTTP, and wherever the
// user declines), the text is pasted here instead; pasting converts it straight away.
export function PasteDialog({ onText, onClose }: PasteDialogProps) {
  const [text, setText] = useState('')

  return (
    <Modal
      title="Paste Chinese text"
      className="paste-dialog"
      onClose={onClose}
      footer={(close) => (
        <>
          <button className="btn close-popup-btn" onClick={close}>
            Cancel
          </button>
          <button
            className="btn btn-primary"
            disabled={!text.trim()}
            onClick={() => {
              onText(text)
              close()
            }}
          >
            Convert
          </button>
        </>
      )}
    >
      {(close) => (
        <div className="popup-body">
          <p className="paste-hint">Paste the text here: long-press and choose Paste, or press Ctrl+V.</p>
          <textarea
            className="paste-area cn-text"
            autoFocus
            value={text}
            placeholder="Chinese text…"
            onChange={(e) => setText(e.target.value)}
            onPaste={(e) => {
              const pasted = e.clipboardData.getData('text/plain')
              if (!pasted.trim()) return
              e.preventDefault()
              onText(pasted)
              close()
            }}
          />
        </div>
      )}
    </Modal>
  )
}
