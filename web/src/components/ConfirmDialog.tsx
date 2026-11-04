import type { ReactNode } from 'react'
import { Modal } from './Modal'

interface ConfirmDialogProps {
  title: string
  confirmLabel: string
  // Offered besides confirming, e.g. exporting before a replace.
  extraAction?: { label: string; onClick: () => void }
  onConfirm: () => void
  onClose: () => void
  children: ReactNode
}

export function ConfirmDialog({ title, confirmLabel, extraAction, onConfirm, onClose, children }: ConfirmDialogProps) {
  return (
    <Modal
      title={title}
      className="confirm-dialog"
      onClose={onClose}
      footer={(close) => (
        <>
          <button className="btn close-popup-btn" autoFocus onClick={close}>
            Cancel
          </button>
          {extraAction && (
            <button className="btn" onClick={extraAction.onClick}>
              {extraAction.label}
            </button>
          )}
          <button
            className="btn btn-danger"
            onClick={() => {
              onConfirm()
              close()
            }}
          >
            {confirmLabel}
          </button>
        </>
      )}
    >
      <div className="popup-body confirm-body">{children}</div>
    </Modal>
  )
}
