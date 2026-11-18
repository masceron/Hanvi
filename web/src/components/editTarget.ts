import type { Paragraph, Rule, Token } from '../core/types'

export type Role = 'cn' | 'sv' | 'vn'

export type EditTarget = { kind: 'text'; text: string } | { kind: 'rule'; rule: Rule }

// The token a TokenView span shows: the paragraph's index is on its <p>, the token's on the span.
export function tokenOfSpan(span: HTMLElement, paragraphs: Paragraph[]): Token {
  return paragraphs[Number(span.parentElement!.dataset.p)][Number(span.dataset.t)]
}

// What a right-click in a pane edits, as on the desktop: with text selected in that pane, a rule
// among the selected tokens, else the Chinese the selection covers; otherwise the clicked token.
export function editTarget(
  canvas: HTMLElement,
  paragraphs: Paragraph[],
  role: Role,
  clicked: Token | null,
): EditTarget | null {
  const selection = window.getSelection()
  if (selection && !selection.isCollapsed && selection.rangeCount > 0) {
    const range = selection.getRangeAt(0)
    if (canvas.contains(range.commonAncestorContainer)) {
      const pieces = selectedPieces(canvas, paragraphs, range)
      const rule = pieces.find((piece) => piece.token.rule)?.token.rule
      if (rule) return { kind: 'rule', rule }
      const text = pieces.map((piece) => chineseOf(piece, role)).join('')
      if (text) return { kind: 'text', text }
    }
  }

  if (!clicked) return null
  if (clicked.rule) return { kind: 'rule', rule: clicked.rule }
  return clicked.cn ? { kind: 'text', text: clicked.cn } : null
}

export function editTargetOfTokens(tokens: Token[]): EditTarget | null {
  if (!tokens || tokens.length === 0) return null
  if (tokens.length === 1) {
    const token = tokens[0]
    if (token.rule) return { kind: 'rule', rule: token.rule }
    return token.cn ? { kind: 'text', text: token.cn } : null
  }
  const rule = tokens.find((t) => t.rule)?.rule
  if (rule) return { kind: 'rule', rule }
  const text = tokens.map((t) => t.cn).join('')
  return text ? { kind: 'text', text } : null
}

interface Piece {
  token: Token
  // Selected part of the token's text in this pane.
  start: number
  end: number
}

function selectedPieces(canvas: HTMLElement, paragraphs: Paragraph[], range: Range): Piece[] {
  const pieces: Piece[] = []
  for (const span of canvas.querySelectorAll<HTMLElement>('[data-tid]')) {
    if (!range.intersectsNode(span)) continue
    const textNode = span.firstChild
    const length = span.textContent?.length ?? 0
    const start = range.startContainer === textNode ? range.startOffset : 0
    const end = range.endContainer === textNode ? range.endOffset : length
    if (end > start) {
      pieces.push({ token: tokenOfSpan(span, paragraphs), start, end })
    }
  }
  return pieces
}

function chineseOf({ token, start, end }: Piece, role: Role): string {
  if (role === 'cn') return token.cn.slice(start, end)
  if (role === 'sv') {
    // One Hán Việt syllable per character: keep the characters whose syllables are selected.
    const words = token.sv.split(' ')
    if (words.length === token.cn.length) {
      let text = ''
      let offset = 0
      words.forEach((word, i) => {
        if (offset + word.length > start && offset < end) text += token.cn[i]
        offset += word.length + 1
      })
      return text
    }
  }
  return token.cn
}
