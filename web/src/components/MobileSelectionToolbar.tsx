import { useEffect, useRef, useState, type RefObject } from 'react'
import { ChevronLeftIcon, ChevronRightIcon, CloseIcon } from '../icons/SvgIcons'
import type { FlatToken, TokenSelection } from '../mobile'

interface MobileSelectionToolbarProps {
  selection: TokenSelection | null
  flatTokens: FlatToken[]
  workspaceRef: RefObject<HTMLElement | null>
  onExpandLeft: () => void
  onExpandRight: () => void
  onEdit: () => void
  onClose: () => void
}

const TOOLBAR_WIDTH = 205
const TOOLBAR_HEIGHT = 38

export function MobileSelectionToolbar({
  selection,
  flatTokens,
  workspaceRef,
  onExpandLeft,
  onExpandRight,
  onEdit,
  onClose,
}: MobileSelectionToolbarProps) {
  const [pos, setPos] = useState<{ top: number; left: number; visible: boolean } | null>(null)
  const toolbarRef = useRef<HTMLDivElement>(null)

  useEffect(() => {
    function update() {
      if (!selection || !workspaceRef.current || flatTokens.length === 0) {
        setPos(null)
        return
      }
      const workspaceEl = workspaceRef.current
      const anchorItem = flatTokens[selection.anchorIndex] ?? flatTokens[selection.startIndex]
      if (!anchorItem) {
        setPos(null)
        return
      }

      // Find canvas matching active role, or fallback to visible canvas
      const canvas =
        workspaceEl.querySelector(`.token-canvas.role-${selection.activeRole}`) ||
        workspaceEl.querySelector('.token-canvas.role-vn') ||
        workspaceEl.querySelector('.token-canvas')

      if (!canvas) return

      const anchorSpan = canvas.querySelector<HTMLElement>(
        `p[data-p="${anchorItem.pIdx}"] span[data-t="${anchorItem.tIdx}"]`,
      )

      if (!anchorSpan) return

      const rect = anchorSpan.getBoundingClientRect()
      if (rect.width === 0 && rect.height === 0) {
        return
      }

      // Hide if the anchor token is scrolled off screen
      const isVisible = rect.bottom >= 44 && rect.top <= window.innerHeight - 36

      const centerX = (rect.left + rect.right) / 2
      const posX = Math.max(
        8,
        Math.min(window.innerWidth - TOOLBAR_WIDTH - 8, centerX - TOOLBAR_WIDTH / 2),
      )

      // Keep position constant relative to the initial anchor token
      let posY = rect.top - TOOLBAR_HEIGHT - 8
      if (posY < 52) {
        posY = rect.bottom + 8
      }
      if (posY + TOOLBAR_HEIGHT > window.innerHeight - 36) {
        posY = Math.max(52, window.innerHeight - TOOLBAR_HEIGHT - 40)
      }

      setPos({ top: posY, left: posX, visible: isVisible })
    }

    update()
    const rafId = requestAnimationFrame(update)

    window.addEventListener('resize', update)
    window.addEventListener('scroll', update, true)

    return () => {
      cancelAnimationFrame(rafId)
      window.removeEventListener('resize', update)
      window.removeEventListener('scroll', update, true)
    }
  }, [selection, flatTokens, workspaceRef])

  if (!selection || !pos || !pos.visible) return null

  const numTokens = selection.endIndex - selection.startIndex + 1
  const canExpandLeft = selection.startIndex > 0
  const canExpandRight = selection.endIndex < flatTokens.length - 1

  return (
    <div
      ref={toolbarRef}
      className="mobile-selection-toolbar"
      style={{
        position: 'fixed',
        top: `${pos.top}px`,
        left: `${pos.left}px`,
        zIndex: 1000,
      }}
      onPointerDown={(e) => e.stopPropagation()}
      onClick={(e) => e.stopPropagation()}
    >
      <button
        type="button"
        className="mobile-toolbar-btn mobile-toolbar-arrow mobile-toolbar-arrow-left"
        disabled={!canExpandLeft}
        onClick={(e) => {
          e.preventDefault()
          e.stopPropagation()
          onExpandLeft()
        }}
        title="Expand selection left"
        aria-label="Expand selection left"
      >
        <ChevronLeftIcon />
      </button>

      <button
        type="button"
        className="mobile-toolbar-btn mobile-toolbar-edit"
        onClick={(e) => {
          e.preventDefault()
          e.stopPropagation()
          onEdit()
        }}
        title="Edit selected text"
        aria-label="Edit selected text"
      >
        <span>Edit</span>
        {numTokens > 1 && <span className="mobile-toolbar-count">({numTokens})</span>}
      </button>

      <button
        type="button"
        className="mobile-toolbar-btn mobile-toolbar-arrow mobile-toolbar-arrow-right"
        disabled={!canExpandRight}
        onClick={(e) => {
          e.preventDefault()
          e.stopPropagation()
          onExpandRight()
        }}
        title="Expand selection right"
        aria-label="Expand selection right"
      >
        <ChevronRightIcon />
      </button>

      <button
        type="button"
        className="mobile-toolbar-btn mobile-toolbar-close"
        onClick={(e) => {
          e.preventDefault()
          e.stopPropagation()
          onClose()
        }}
        title="Clear selection"
        aria-label="Clear selection"
      >
        <CloseIcon />
      </button>
    </div>
  )
}
