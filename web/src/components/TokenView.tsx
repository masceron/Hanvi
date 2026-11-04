import { Fragment, memo, useRef, type ReactNode } from 'react'
import type { Paragraph, Token } from '../core/types'
import { editTarget, type EditTarget, type Role } from './editTarget'

interface TokenViewProps {
  paragraphs: Paragraph[]
  role: Role
  searchQuery?: string
  currentMatchIndex?: number
  onHoverToken: (id: number | null) => void
  // token is null when the click missed every token.
  onClickToken: (token: Token | null, viaTouch: boolean) => void
  onEditRequest: (target: EditTarget) => void
  // Shown instead of the text while there is none.
  empty?: ReactNode
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
  empty,
}: TokenViewProps) {
  // Pointer type of the last press, as click events do not tell touch from mouse everywhere.
  const pointerType = useRef('')

  if (paragraphs.length === 0) {
    return <div className="token-empty-state">{empty}</div>
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
      onPointerDown={(e) => (pointerType.current = e.pointerType)}
      onClick={(e) => onClickToken(tokenAt(e.target), pointerType.current === 'touch')}
      onContextMenu={(e) => {
        const clicked = tokenAt(e.target)
        const target = editTarget(e.currentTarget, paragraphs, role, clicked)
        if (!target) return
        e.preventDefault()
        if (clicked) onClickToken(clicked, false)
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
