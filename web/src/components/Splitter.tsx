interface SplitterProps {
  // 'columns' is a vertical bar between side-by-side panes; 'rows' a horizontal bar between stacked ones.
  direction: 'columns' | 'rows'
  // Smallest size, in pixels, either neighbouring pane may be dragged to.
  minSize: number
  // Called while dragging with the share of the parent the pane before the bar should take.
  onResize: (fraction: number) => void
}

// A draggable divider between the two panes around it, which must be its siblings.
export function Splitter({ direction, minSize, onResize }: SplitterProps) {
  const columns = direction === 'columns'

  function startDrag(e: React.PointerEvent<HTMLDivElement>) {
    if (e.button !== 0) return
    e.preventDefault()
    const bar = e.currentTarget
    const parent = bar.parentElement!
    bar.setPointerCapture(e.pointerId)
    document.body.classList.add(columns ? 'resizing-columns' : 'resizing-rows')

    const move = (event: PointerEvent) => {
      const rect = parent.getBoundingClientRect()
      const barSize = columns ? bar.offsetWidth : bar.offsetHeight
      const total = (columns ? rect.width : rect.height) - barSize
      const position = (columns ? event.clientX - rect.left : event.clientY - rect.top) - barSize / 2
      onResize(Math.min(Math.max(position, minSize), total - minSize) / total)
    }
    const end = () => {
      bar.removeEventListener('pointermove', move)
      bar.removeEventListener('pointerup', end)
      bar.removeEventListener('pointercancel', end)
      document.body.classList.remove('resizing-columns', 'resizing-rows')
    }
    bar.addEventListener('pointermove', move)
    bar.addEventListener('pointerup', end)
    bar.addEventListener('pointercancel', end)
  }

  return (
    <div
      className={columns ? 'workspace-splitter-v' : 'pane-splitter-h'}
      role="separator"
      aria-orientation={columns ? 'vertical' : 'horizontal'}
      onPointerDown={startDrag}
    />
  )
}
