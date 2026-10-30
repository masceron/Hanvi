import { useEffect, useRef, type ReactNode } from 'react'
import { CloseIcon } from '../icons/SvgIcons'

// Open modals, topmost last: Escape closes only the topmost one.
const stack: { current: () => void }[] = []

window.addEventListener('keydown', (e) => {
  if (e.key === 'Escape' && stack.length > 0) {
    e.preventDefault()
    e.stopImmediatePropagation()
    stack[stack.length - 1].current()
  }
})

interface ModalProps {
  title: string
  className: string
  onClose: () => void
  footer?: ReactNode
  children: ReactNode
}

export function Modal({ title, className, onClose, footer, children }: ModalProps) {
  const close = useRef(onClose)
  useEffect(() => {
    close.current = onClose
  })

  useEffect(() => {
    const entry = { current: () => close.current() }
    stack.push(entry)
    return () => {
      stack.splice(stack.indexOf(entry), 1)
    }
  }, [])

  return (
    <div className="modal-backdrop" onMouseDown={(e) => e.target === e.currentTarget && onClose()}>
      <div className={`modal ${className}`} role="dialog" aria-label={title}>
        <div className="popup-header">
          <span className="popup-title">{title}</span>
          <button className="icon-btn close-btn" onClick={onClose} title="Close (Esc)">
            <CloseIcon />
          </button>
        </div>
        {children}
        {footer && <div className="popup-footer">{footer}</div>}
      </div>
    </div>
  )
}
