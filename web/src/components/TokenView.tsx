import { Fragment } from 'react'
import type { Paragraph, Token } from '../core/types'

interface TokenViewProps {
  paragraphs: Paragraph[]
  role: 'cn' | 'sv' | 'vn'
  hoveredTokenId: number | null
  activeTokenId: number | null
  searchQuery?: string
  currentMatchIndex?: number
  onHoverToken: (id: number | null) => void
  onClickToken: (token: Token) => void
}

export function TokenView({
  paragraphs,
  role,
  hoveredTokenId,
  activeTokenId,
  searchQuery = '',
  currentMatchIndex = -1,
  onHoverToken,
  onClickToken,
}: TokenViewProps) {
  if (paragraphs.length === 0) {
    return (
      <div className="token-empty-state">
        <span>No text loaded</span>
      </div>
    )
  }

  const query = searchQuery.trim().toLowerCase()
  let matchCounter = 0

  return (
    <div className={`token-canvas role-${role}`}>
      {paragraphs.map((para, pIdx) => (
        <p key={pIdx} className="token-paragraph">
          {para.map((token) => {
            const isHovered = hoveredTokenId !== null && token.id === hoveredTokenId
            const isActive = activeTokenId !== null && token.id === activeTokenId

            let text = ''
            let hasSpaceBefore = false

            if (role === 'cn') {
              text = token.cn
              hasSpaceBefore = token.cnSpace
            } else if (role === 'sv') {
              text = token.sv
              hasSpaceBefore = token.svSpace
            } else {
              text = token.vn
              hasSpaceBefore = token.vnSpace
            }

            if (!text) return null

            // Search matching for Vietnamese view
            let isMatch = false
            let isCurrentMatch = false

            if (role === 'vn' && query && text.toLowerCase().includes(query)) {
              isMatch = true
              if (matchCounter === currentMatchIndex) {
                isCurrentMatch = true
              }
              matchCounter++
            }

            const classes = [
              'token',
              token.isRule ? 'token-rule' : '',
              isHovered ? 'hovered' : '',
              isActive ? 'active' : '',
              isMatch ? 'search-match' : '',
              isCurrentMatch ? 'search-match-active' : '',
            ]
              .filter(Boolean)
              .join(' ')

            return (
              <Fragment key={token.id}>
                {hasSpaceBefore && <span className="space"> </span>}
                <span
                  className={classes}
                  onMouseEnter={() => onHoverToken(token.id)}
                  onMouseLeave={() => onHoverToken(null)}
                  onClick={() => onClickToken(token)}
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
}

