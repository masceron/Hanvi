import type { Paragraph, Rule, Token } from './types'

// A converted document as convert() in web/wasm/bindings.cpp packs it: `ints` holds the paragraph
// count, then per paragraph its token count and per token: id, flags, then the lengths of cn, sv, vn
// and, with HAS_RULE, of the rule's four strings. `chars` holds those strings back to back.
export interface PackedDocument {
  ints: Uint32Array<ArrayBuffer>
  chars: string
}

const CN_SPACE = 1
const SV_SPACE = 2
const VN_SPACE = 4
const IS_RULE = 8
const HAS_RULE = 16

const utf16 = new TextDecoder('utf-16le')

// Copies the module's views of wasm memory, which the next call into the module may invalidate.
export function copyPacked(view: { ints: Uint32Array; chars: Uint16Array }): PackedDocument {
  return { ints: view.ints.slice(), chars: utf16.decode(view.chars) }
}

export function decodeDocument({ ints, chars }: PackedDocument): Paragraph[] {
  let i = 0
  let c = 0
  const next = () => {
    const end = c + ints[i++]
    const text = chars.slice(c, end)
    c = end
    return text
  }

  const paragraphs: Paragraph[] = new Array(ints[i++])
  for (let p = 0; p < paragraphs.length; p++) {
    const tokens: Token[] = new Array(ints[i++])
    for (let t = 0; t < tokens.length; t++) {
      const id = ints[i++]
      const flags = ints[i++]
      const token: Token = {
        id,
        cn: next(),
        sv: next(),
        vn: next(),
        cnSpace: (flags & CN_SPACE) !== 0,
        svSpace: (flags & SV_SPACE) !== 0,
        vnSpace: (flags & VN_SPACE) !== 0,
        isRule: (flags & IS_RULE) !== 0,
      }
      if (flags & HAS_RULE) {
        const rule: Rule = {
          originalStart: next(),
          originalEnd: next(),
          translationStart: next(),
          translationEnd: next(),
        }
        token.rule = rule
      }
      tokens[t] = token
    }
    paragraphs[p] = tokens
  }
  return paragraphs
}
