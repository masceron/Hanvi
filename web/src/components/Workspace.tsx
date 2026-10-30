import { useMemo, useState } from 'react'
import type { Paragraph, TabData, Token } from '../core/types'
import { SearchIcon } from '../icons/SvgIcons'
import { FindBar } from './FindBar'
import { PageControls } from './PageControls'
import { TokenView } from './TokenView'

interface WorkspaceProps {
  tab: TabData | null
  hoveredTokenId: number | null
  activeTokenId: number | null
  isBusy: boolean
  onHoverToken: (id: number | null) => void
  onClickToken: (token: Token) => void
  onConvertText: (text: string) => void
  onPageChange: (page: number) => void
  onToggleEdit: () => void
  onCopyVietnamese: () => void
}

export function Workspace({
  tab,
  hoveredTokenId,
  activeTokenId,
  isBusy,
  onHoverToken,
  onClickToken,
  onConvertText,
  onPageChange,
  onToggleEdit,
  onCopyVietnamese,
}: WorkspaceProps) {
  const [isFindBarOpen, setIsFindBarOpen] = useState(false)
  const [searchQuery, setSearchQuery] = useState('')
  const [currentMatchIndex, setCurrentMatchIndex] = useState(0)
  const [draftText, setDraftText] = useState('')

  const rawDocs = tab?.document

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

  const isEditing = tab ? tab.isEditing || tab.pages.length === 0 : false
  const paragraphs: Paragraph[] = rawDocs ?? []

  return (
    <main className="workspace-splitter">
      {/* Left Column: Chinese (top) & Sino-Vietnamese (bottom) */}
      <section className="column-left">
        {/* Top Pane: Chinese */}
        <div className="pane-container pane-cn">
          <div className="pane-header">
            <span className="pane-title">Chinese Source</span>
            {tab && tab.pages.length > 0 && (
              <button className="btn-sm" onClick={onToggleEdit}>
                {tab.isEditing ? 'Cancel Edit' : 'Edit Source'}
              </button>
            )}
          </div>

          <div className="pane-content">
            {isEditing ? (
              <div className="edit-source-container">
                <textarea
                  className="source-textarea"
                  placeholder="Paste or type Chinese novel text here…"
                  value={draftText || tab?.rawText || ''}
                  onChange={(e) => setDraftText(e.target.value)}
                />
                <div className="edit-source-actions">
                  <button
                    className="btn btn-primary"
                    disabled={isBusy || !(draftText || tab?.rawText)}
                    onClick={() => {
                      onConvertText(draftText || tab?.rawText || '')
                      setDraftText('')
                    }}
                  >
                    {isBusy ? 'Converting…' : 'Convert & Align'}
                  </button>
                </div>
              </div>
            ) : (
              <TokenView
                paragraphs={paragraphs}
                role="cn"
                hoveredTokenId={hoveredTokenId}
                activeTokenId={activeTokenId}
                onHoverToken={onHoverToken}
                onClickToken={onClickToken}
              />
            )}
          </div>
        </div>

        {/* Divider */}
        <div className="pane-splitter-h" />

        {/* Bottom Pane: Sino-Vietnamese */}
        <div className="pane-container pane-sv">
          <div className="pane-header">
            <span className="pane-title">Sino-Vietnamese (Hán Việt)</span>
          </div>

          <div className="pane-content">
            <TokenView
              paragraphs={paragraphs}
              role="sv"
              hoveredTokenId={hoveredTokenId}
              activeTokenId={activeTokenId}
              onHoverToken={onHoverToken}
              onClickToken={onClickToken}
            />
          </div>
        </div>
      </section>

      {/* Main Divider */}
      <div className="workspace-splitter-v" />

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
              hoveredTokenId={hoveredTokenId}
              activeTokenId={activeTokenId}
              searchQuery={searchQuery}
              currentMatchIndex={currentMatchIndex}
              onHoverToken={onHoverToken}
              onClickToken={onClickToken}
            />
          </div>

          {/* Page navigation controls right under Vietnamese output */}
          <PageControls
            currentPage={tab?.currentPage ?? 0}
            totalPages={tab?.pages.length ?? 0}
            onPageChange={onPageChange}
          />
        </div>
      </section>
    </main>
  )
}
