import { useEffect, useRef } from 'react'
import { HamburgerIcon } from '../icons/SvgIcons'

interface HamburgerMenuProps {
  isOpen: boolean
  onToggle: () => void
  onClose: () => void
  onOpenFile: () => void
  onReadClipboard: () => void
  onSaveToFile: () => void
  onCopyVietnamese: () => void
  onOpenNamesetChooser: () => void
  onOpenNamesetEntries: () => void
  onOpenNamesetsManager: () => void
  onOpenPhrases: () => void
  onOpenNames: () => void
  onReconvert: () => void
  onReloadDict: () => void
}

export function HamburgerMenu({
  isOpen,
  onToggle,
  onClose,
  onOpenFile,
  onReadClipboard,
  onSaveToFile,
  onCopyVietnamese,
  onOpenNamesetChooser,
  onOpenNamesetEntries,
  onOpenNamesetsManager,
  onOpenPhrases,
  onOpenNames,
  onReconvert,
  onReloadDict,
}: HamburgerMenuProps) {
  const containerRef = useRef<HTMLDivElement>(null)

  useEffect(() => {
    function handleClickOutside(event: MouseEvent) {
      if (containerRef.current && !containerRef.current.contains(event.target as Node)) {
        onClose()
      }
    }
    if (isOpen) {
      document.addEventListener('mousedown', handleClickOutside)
    }
    return () => {
      document.removeEventListener('mousedown', handleClickOutside)
    }
  }, [isOpen, onClose])

  return (
    <div className="hamburger-container" ref={containerRef}>
      <button
        className="hamburger-btn"
        onClick={onToggle}
        title="Menu (File, Settings, Namesets)"
      >
        <HamburgerIcon />
      </button>

      {isOpen && (
        <div className="dropdown-menu">
          <button
            className="menu-item"
            onClick={() => {
              onOpenFile()
              onClose()
            }}
          >
            <span>Open File…</span>
            <span className="shortcut">Ctrl+O</span>
          </button>

          <button
            className="menu-item"
            onClick={() => {
              onReadClipboard()
              onClose()
            }}
          >
            <span>Read from Clipboard</span>
            <span className="shortcut">Ctrl+V</span>
          </button>

          <button
            className="menu-item"
            onClick={() => {
              onSaveToFile()
              onClose()
            }}
          >
            <span>Save to File…</span>
            <span className="shortcut">Ctrl+S</span>
          </button>

          <button
            className="menu-item"
            onClick={() => {
              onCopyVietnamese()
              onClose()
            }}
          >
            <span>Copy Vietnamese</span>
            <span className="shortcut">Ctrl+Shift+C</span>
          </button>

          <div className="menu-separator" />

          <button
            className="menu-item"
            onClick={() => {
              onOpenNamesetChooser()
              onClose()
            }}
          >
            <span>Choose Nameset…</span>
          </button>

          <button
            className="menu-item"
            onClick={() => {
              onOpenNamesetEntries()
              onClose()
            }}
          >
            <span>Active Nameset Entries…</span>
          </button>

          <button
            className="menu-item"
            onClick={() => {
              onOpenNamesetsManager()
              onClose()
            }}
          >
            <span>Namesets Manager…</span>
          </button>

          <div className="menu-separator" />

          <button
            className="menu-item"
            onClick={() => {
              onOpenPhrases()
              onClose()
            }}
          >
            <span>Dictionary: Phrases…</span>
            <span className="shortcut">Ctrl+D</span>
          </button>

          <button
            className="menu-item"
            onClick={() => {
              onOpenNames()
              onClose()
            }}
          >
            <span>Dictionary: Global Names…</span>
            <span className="shortcut">Ctrl+Shift+D</span>
          </button>

          <div className="menu-separator" />

          <button
            className="menu-item"
            onClick={() => {
              onReconvert()
              onClose()
            }}
          >
            <span>Re-convert</span>
            <span className="shortcut">Ctrl+R</span>
          </button>

          <button
            className="menu-item"
            onClick={() => {
              onReloadDict()
              onClose()
            }}
          >
            <span>Reload Dictionary</span>
          </button>
        </div>
      )}
    </div>
  )
}

