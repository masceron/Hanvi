import { Fragment, memo } from 'react'
import type { Paragraph, Token } from '../core/types'
import { editTarget, type EditTarget, type Role } from './editTarget'

interface TokenViewProps {
  paragraphs: Paragraph[]
  role: Role
  searchQuery?: string
  currentMatchIndex?: number
  onHoverToken: (id: number | null) => void
  // null when the click missed every token.
  onClickToken: (token: Token | null) => void
  onEditRequest: (target: EditTarget) => void
}

// Hover and selection highlighting is done by Workspace with a stylesheet keyed on
// data-tid, so moving the mouse never re-renders the tokens.
export const TokenView = memo(function TokenView({
  paragraphs,
  role,
  searchQuery = '',
  currentMatchIndex = -1,
  onHoverToken,
  onClickToken,
  onEditRequest,
}: TokenViewProps) {
  if (paragraphs.length === 0) {
    return (
      <div className="token-empty-state">
        <span>Paste Chinese text (Ctrl+V) or open a file (Ctrl+O)</span>
      </div>
    )
  }

  const query = searchQuery.trim().toLowerCase()
  let matchCounter = 0

  function tokenAt(target: EventTarget): Token | null {
    const el = (target as HTMLElement).closest<HTMLElement>('[data-tid]')
    if (!el) return null
    return paragraphs[Number(el.dataset.p)][Number(el.dataset.t)]
  }

  return (
    <div
      className={`token-canvas role-${role}`}
      onMouseOver={(e) => onHoverToken(tokenAt(e.target)?.id ?? null)}
      onMouseLeave={() => onHoverToken(null)}
      onClick={(e) => onClickToken(tokenAt(e.target))}
      onContextMenu={(e) => {
        const clicked = tokenAt(e.target)
        const target = editTarget(e.currentTarget, paragraphs, role, clicked)
        if (!target) return
        e.preventDefault()
        if (clicked) onClickToken(clicked)
        onEditRequest(target)
      }}
    >
      {paragraphs.map((para, pIdx) => (
        <p key={pIdx} className="token-paragraph">
          {para.map((token, tIdx) => {
            const text = token[role]
            if (!text) return null

            const hasSpaceBefore = role === 'cn' ? token.cnSpace : role === 'sv' ? token.svSpace : token.vnSpace

            // Search matching for Vietnamese view
            let isMatch = false
            let isCurrentMatch = false
            if (role === 'vn' && query && text.toLowerCase().includes(query)) {
              isMatch = true
              isCurrentMatch = matchCounter === currentMatchIndex
              matchCounter++
            }

            const classes = [
              'token',
              isMatch ? 'search-match' : '',
              isCurrentMatch ? 'search-match-active' : '',
            ]
              .filter(Boolean)
              .join(' ')

            return (
              <Fragment key={tIdx}>
                {hasSpaceBefore && ' '}
                <span
                  className={classes}
                  data-tid={token.id}
                  data-p={pIdx}
                  data-t={tIdx}
                  title={`Chinese: ${token.cn}\nHán Việt: ${token.sv}\nVietnamese: ${token.vn}${
                    token.isRule ? '\n(Rule Matched)' : ''
                  }`}
                >
                  {text}
                </span>
              </Fragment>
            )
          })}
        </p>
      ))}
    </div>
  )
})
