import { useEffect, useRef, useState, type ReactNode } from 'react'
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
  footer?: ReactNode | ((close: () => void) => ReactNode)
  children: ReactNode
}

export function Modal({ title, className, onClose, footer, children }: ModalProps) {
  const [isClosing, setIsClosing] = useState(false)
  const isClosingRef = useRef(false)

  const handleClose = () => {
    if (isClosingRef.current) return
    isClosingRef.current = true
    setIsClosing(true)
    setTimeout(() => {
      onClose()
    }, 180)
  }

  const close = useRef(handleClose)
  useEffect(() => {
    close.current = handleClose
  })

  useEffect(() => {
    const entry = { current: () => close.current() }
    stack.push(entry)
    return () => {
      stack.splice(stack.indexOf(entry), 1)
    }
  }, [])

  return (
    <div
      className={`modal-backdrop ${isClosing ? 'modal-backdrop-exit' : ''}`}
      onMouseDown={(e) => e.target === e.currentTarget && handleClose()}
    >
      <div className={`modal ${className} ${isClosing ? 'modal-exit' : ''}`} role="dialog" aria-label={title}>
        <div className="popup-header">
          <span className="popup-title">{title}</span>
          <button className="icon-btn close-btn" onClick={handleClose} title="Close (Esc)">
            <CloseIcon />
          </button>
        </div>
        {children}
        {footer && (
          <div className="popup-footer">
            {typeof footer === 'function' ? footer(handleClose) : footer}
          </div>
        )}
      </div>
    </div>
  )
}
