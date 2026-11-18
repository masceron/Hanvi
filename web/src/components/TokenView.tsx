import { Fragment, memo, useRef, type ReactNode } from 'react'
import type { Paragraph, Token } from '../core/types'
import { editTarget, tokenOfSpan, type EditTarget, type Role } from './editTarget'

interface TokenViewProps {
  paragraphs: Paragraph[]
  role: Role
  searchQuery?: string
  currentMatchIndex?: number
  onHoverToken: (id: number | null) => void
  // token is null when the click missed every token.
  onClickToken: (
    token: Token | null,
    viaTouch: boolean,
    pIdx?: number,
    tIdx?: number,
    role?: Role,
  ) => void
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

  function spanAt(target: EventTarget): HTMLElement | null {
    return (target as HTMLElement).closest<HTMLElement>('[data-tid]')
  }

  function tokenAt(target: EventTarget): Token | null {
    const span = spanAt(target)
    return span && tokenOfSpan(span, paragraphs)
  }

  // The tooltip is set as the pointer reaches a token, rather than rendered on every span of every
  // pane; the browser reads it only when it shows the tooltip. Set every time, as React reuses the
  // spans for the next conversion and does not know about it.
  function hover(target: EventTarget) {
    const span = spanAt(target)
    const token = span && tokenOfSpan(span, paragraphs)
    if (span && token) {
      span.title = `Chinese: ${token.cn}\nHán Việt: ${token.sv}\nVietnamese: ${token.vn}${
        token.isRule ? '\n(Rule Matched)' : ''
      }`
    }
    onHoverToken(token?.id ?? null)
  }

  return (
    <div
      className={`token-canvas role-${role}`}
      onMouseOver={(e) => hover(e.target)}
      onMouseLeave={() => onHoverToken(null)}
      onPointerDown={(e) => (pointerType.current = e.pointerType)}
      onClick={(e) => {
        const span = spanAt(e.target)
        const pIdx = span && span.parentElement ? Number(span.parentElement.dataset.p) : undefined
        const tIdx = span ? Number(span.dataset.t) : undefined
        const token = span ? tokenOfSpan(span, paragraphs) : null
        onClickToken(token, pointerType.current === 'touch', pIdx, tIdx, role)
      }}
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
        <p key={pIdx} className="token-paragraph" data-p={pIdx}>
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

            const classes = isCurrentMatch
              ? 'token search-match search-match-active'
              : isMatch
                ? 'token search-match'
                : 'token'

            return (
              <Fragment key={tIdx}>
                {hasSpaceBefore && ' '}
                <span className={classes} data-tid={token.id} data-t={tIdx}>
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
