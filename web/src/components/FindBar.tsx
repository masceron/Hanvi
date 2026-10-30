import { useEffect, useRef } from 'react'
import { ChevronLeftIcon, ChevronRightIcon, CloseIcon } from '../icons/SvgIcons'

interface FindBarProps {
  isOpen: boolean
  searchQuery: string
  onSearchChange: (q: string) => void
  matchCount: number
  currentMatchIndex: number
  onNext: () => void
  onPrevious: () => void
  onClose: () => void
}

export function FindBar({
  isOpen,
  searchQuery,
  onSearchChange,
  matchCount,
  currentMatchIndex,
  onNext,
  onPrevious,
  onClose,
}: FindBarProps) {
  const inputRef = useRef<HTMLInputElement>(null)

  useEffect(() => {
    if (isOpen) {
      inputRef.current?.focus()
      inputRef.current?.select()
    }
  }, [isOpen])

  useEffect(() => {
    function handleKeyDown(e: KeyboardEvent) {
      if (!isOpen) return
      if (e.key === 'Escape') {
        onClose()
      } else if (e.key === 'Enter') {
        if (e.shiftKey) onPrevious()
        else onNext()
      }
    }
    window.addEventListener('keydown', handleKeyDown)
    return () => window.removeEventListener('keydown', handleKeyDown)
  }, [isOpen, onClose, onNext, onPrevious])

  if (!isOpen) return null

  const matchText =
    searchQuery === ''
      ? ''
      : matchCount === 0
        ? 'No matches'
        : `${currentMatchIndex + 1} of ${matchCount}`

  return (
    <div className="findbar" onClick={(e) => e.stopPropagation()}>
      <input
        ref={inputRef}
        type="text"
        placeholder="Find in page..."
        value={searchQuery}
        onChange={(e) => onSearchChange(e.target.value)}
      />
      <span className="matches-label">{matchText}</span>
      <button
        className="findbar-btn"
        disabled={matchCount === 0}
        onClick={onPrevious}
        title="Previous match (Shift+Enter)"
      >
        <ChevronLeftIcon />
      </button>
      <button
        className="findbar-btn"
        disabled={matchCount === 0}
        onClick={onNext}
        title="Next match (Enter)"
      >
        <ChevronRightIcon />
      </button>
      <button className="findbar-btn close-find-btn" onClick={onClose} title="Close (Esc)">
        <CloseIcon />
      </button>
    </div>
  )
}

