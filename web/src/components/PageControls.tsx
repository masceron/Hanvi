import { useState } from 'react'

interface PageControlsProps {
  currentPage: number
  totalPages: number
  onPageChange: (newPage: number) => void
}

export function PageControls({ currentPage, totalPages, onPageChange }: PageControlsProps) {
  const [prevPage, setPrevPage] = useState(currentPage)
  const [inputVal, setInputVal] = useState(String(currentPage + 1))

  if (prevPage !== currentPage) {
    setPrevPage(currentPage)
    setInputVal(String(currentPage + 1))
  }

  const hasPages = totalPages > 0
  const canPrev = hasPages && currentPage > 0
  const canNext = hasPages && currentPage < totalPages - 1

  function handleCommit() {
    const num = parseInt(inputVal, 10)
    if (!isNaN(num)) {
      const clamped = Math.max(0, Math.min(totalPages - 1, num - 1))
      onPageChange(clamped)
      setInputVal(String(clamped + 1))
    } else {
      setInputVal(String(currentPage + 1))
    }
  }

  return (
    <div className="page-control-bar">
      <div className="page-control-inner">
        <button
          className="btn page-nav-btn"
          disabled={!canPrev}
          onClick={() => onPageChange(currentPage - 1)}
          title="Previous Page (Left Arrow)"
        >
          Previous
        </button>

        <div className="page-counter">
          <input
            className="page-input"
            type="text"
            disabled={!hasPages}
            value={hasPages ? inputVal : '0'}
            onChange={(e) => setInputVal(e.target.value)}
            onBlur={handleCommit}
            onKeyDown={(e) => {
              if (e.key === 'Enter') {
                handleCommit()
                ;(e.target as HTMLInputElement).blur()
              }
            }}
          />
          <span className="page-slash">/</span>
          <span className="page-total">{hasPages ? totalPages : 0}</span>
        </div>

        <button
          className="btn page-nav-btn"
          disabled={!canNext}
          onClick={() => onPageChange(currentPage + 1)}
          title="Next Page (Right Arrow)"
        >
          Next
        </button>
      </div>
    </div>
  )
}
