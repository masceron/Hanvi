import { useEffect, useMemo, useRef, useState } from 'react'
import type { DocumentState, Paragraph, Token } from '../core/types'
import { SearchIcon } from '../icons/SvgIcons'
import { FindBar } from './FindBar'
import { PageControls } from './PageControls'
import { Splitter } from './Splitter'
import { TokenView } from './TokenView'
import type { EditTarget } from './editTarget'

const NO_PARAGRAPHS: Paragraph[] = []

// Pane proportions. The desktop splits extra width 1:4 on top of each side's preferred size,
// which leaves the left column at about 30% of a typical window; Chinese and Hán Việt share it evenly.
const DEFAULT_LAYOUT = { left: 0.3, top: 0.5 }
const LAYOUT_KEY = 'hanvi.layout'
// Smallest pane size, in pixels, the dividers can be dragged to.
const MIN_COLUMN = 200
const MIN_ROW = 100

type Layout = typeof DEFAULT_LAYOUT

// The divider positions are a per-browser convenience: storage may be unavailable, and then
// the defaults are used.
function loadLayout(): Layout {
  try {
    const stored = JSON.parse(localStorage.getItem(LAYOUT_KEY) ?? 'null')
    if (typeof stored?.left === 'number' && typeof stored?.top === 'number') return stored
  } catch {
    // ignore
  }
  return DEFAULT_LAYOUT
}

function saveLayout(layout: Layout) {
  try {
    localStorage.setItem(LAYOUT_KEY, JSON.stringify(layout))
  } catch {
    // ignore
  }
}

interface WorkspaceProps {
  doc: DocumentState
  activeTokenId: number | null
  onClickToken: (token: Token | null) => void
  onEditRequest: (target: EditTarget) => void
  onPageChange: (page: number) => void
  onCopyVietnamese: () => void
}

export function Workspace({
  doc,
  activeTokenId,
  onClickToken,
  onEditRequest,
  onPageChange,
  onCopyVietnamese,
}: WorkspaceProps) {
  const [isFindBarOpen, setIsFindBarOpen] = useState(false)
  const [searchQuery, setSearchQuery] = useState('')
  const [currentMatchIndex, setCurrentMatchIndex] = useState(0)
  const [hoveredTokenId, setHoveredTokenId] = useState<number | null>(null)
  const [layout, setLayout] = useState(loadLayout)

  function resize(change: Partial<Layout>) {
    setLayout((current) => {
      const updated = { ...current, ...change }
      saveLayout(updated)
      return updated
    })
  }

  const rawDocs = doc.document

  // Count matches in Vietnamese view
  const matchCount = useMemo(() => {
    if (!searchQuery.trim() || !rawDocs || rawDocs.length === 0) return 0
    const query = searchQuery.trim().toLowerCase()
    let count = 0
    for (const para of rawDocs) {
      for (const tok of para) {
        if (tok.vn && tok.vn.toLowerCase().includes(query)) {
          count++
        }
      }
    }
    return count
  }, [searchQuery, rawDocs])

  function handleNextMatch() {
    if (matchCount === 0) return
    setCurrentMatchIndex((prev) => (prev + 1) % matchCount)
  }

  function handlePrevMatch() {
    if (matchCount === 0) return
    setCurrentMatchIndex((prev) => (prev - 1 + matchCount) % matchCount)
  }

  // A new page starts at the top in every pane. currentPage changes together with the
  // converted document, so this runs once the new page is rendered.
  const workspaceRef = useRef<HTMLElement>(null)
  useEffect(() => {
    workspaceRef.current?.querySelectorAll('.pane-content').forEach((pane) => pane.scrollTo(0, 0))
  }, [doc.currentPage])

  // The highlighted token (clicked, or moved to with the arrow keys) is kept in view in every
  // pane: a pane where it is not comfortably visible scrolls to centre it.
  useEffect(() => {
    if (activeTokenId === null) return
    workspaceRef.current?.querySelectorAll('.pane-content').forEach((pane) => {
      const token = pane.querySelector(`.token[data-tid="${activeTokenId}"]`)
      if (!token) return
      const view = pane.getBoundingClientRect()
      const box = token.getBoundingClientRect()
      const margin = box.height
      if (box.top < view.top + margin || box.bottom > view.bottom - margin) {
        token.scrollIntoView({ block: 'center' })
      }
    })
  }, [activeTokenId])

  const paragraphs = rawDocs ?? NO_PARAGRAPHS

  // Highlights the hovered and selected token in every pane. Only this rule changes on hover:
  // the panes keep their DOM, and no font metrics change, so text never shifts.
  const highlighted = [hoveredTokenId, activeTokenId]
    .filter((id) => id !== null)
    .map((id) => `.token-canvas .token[data-tid="${id}"]`)
  const highlightCss = highlighted.length
    ? `${highlighted.join(', ')} { color: var(--accent-hover-token); background-color: var(--highlight-token-bg); }`
    : ''

  return (
    <main className="workspace-splitter" ref={workspaceRef}>
      <style>{highlightCss}</style>
      {/* Left Column: Chinese (top) & Sino-Vietnamese (bottom) */}
      <section className="column-left" style={{ flexBasis: `calc((100% - 6px) * ${layout.left})` }}>
        {/* Top Pane: Chinese */}
        <div className="pane-container pane-cn" style={{ flexBasis: `calc((100% - 6px) * ${layout.top})` }}>
          <div className="pane-header">
            <span className="pane-title">Chinese Source</span>
          </div>

          <div className="pane-content">
            <TokenView
              paragraphs={paragraphs}
              role="cn"
              onHoverToken={setHoveredTokenId}
              onClickToken={onClickToken}
              onEditRequest={onEditRequest}
            />
          </div>
        </div>

        <Splitter direction="rows" minSize={MIN_ROW} onResize={(top) => resize({ top })} />

        {/* Bottom Pane: Sino-Vietnamese */}
        <div className="pane-container pane-sv">
          <div className="pane-header">
            <span className="pane-title">Sino-Vietnamese (Hán Việt)</span>
          </div>

          <div className="pane-content">
            <TokenView
              paragraphs={paragraphs}
              role="sv"
              onHoverToken={setHoveredTokenId}
              onClickToken={onClickToken}
              onEditRequest={onEditRequest}
            />
          </div>
        </div>
      </section>

      <Splitter direction="columns" minSize={MIN_COLUMN} onResize={(left) => resize({ left })} />

      {/* Right Column: Vietnamese & Page Navigation */}
      <section className="column-right">
        <div className="pane-container pane-vn">
          <div className="pane-header">
            <span className="pane-title">Vietnamese Translation</span>
            <div className="pane-header-actions">
              <button
                className="btn-icon-label"
                onClick={() => setIsFindBarOpen((prev) => !prev)}
                title="Find in page (Ctrl+F)"
              >
                <SearchIcon />
                <span>Find</span>
              </button>
              <button className="btn-sm" onClick={onCopyVietnamese} title="Copy Vietnamese text">
                Copy
              </button>
            </div>
          </div>

          <div className="pane-content relative-container">
            <FindBar
              isOpen={isFindBarOpen}
              searchQuery={searchQuery}
              onSearchChange={(q) => {
                setSearchQuery(q)
                setCurrentMatchIndex(0)
              }}
              matchCount={matchCount}
              currentMatchIndex={currentMatchIndex}
              onNext={handleNextMatch}
              onPrevious={handlePrevMatch}
              onClose={() => setIsFindBarOpen(false)}
            />

            <TokenView
              paragraphs={paragraphs}
              role="vn"
              searchQuery={searchQuery}
              currentMatchIndex={currentMatchIndex}
              onHoverToken={setHoveredTokenId}
              onClickToken={onClickToken}
              onEditRequest={onEditRequest}
            />
          </div>

          {/* Page navigation controls right under Vietnamese output */}
          <PageControls
            currentPage={doc.currentPage}
            totalPages={doc.pages.length}
            onPageChange={onPageChange}
          />
        </div>
      </section>
    </main>
  )
}
