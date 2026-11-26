/** Returns true if the Unicode code point is a Chinese (CJK) ideograph. */
export function isChineseChar(cp: number): boolean {
  return (
    (cp >= 0x4e00 && cp <= 0x9fff) || // CJK Unified Ideographs
    (cp >= 0x3400 && cp <= 0x4dbf) || // CJK Unified Ideographs Extension A
    (cp >= 0xf900 && cp <= 0xfaff) || // CJK Compatibility Ideographs
    cp === 0x3007 || // Ideographic Number Zero (〇)
    (cp >= 0x20000 && cp <= 0x2a6df) || // Extension B
    (cp >= 0x2a700 && cp <= 0x2b73f) || // Extension C
    (cp >= 0x2b740 && cp <= 0x2b81f) || // Extension D
    (cp >= 0x2b820 && cp <= 0x2ceaf) || // Extension E
    (cp >= 0x2ceb0 && cp <= 0x2ebef) || // Extension F
    (cp >= 0x2ebf0 && cp <= 0x2ee5f) || // Extension I
    (cp >= 0x2f800 && cp <= 0x2fa1f) || // CJK Compatibility Ideographs Supplement
    (cp >= 0x30000 && cp <= 0x3134f) || // Extension G
    (cp >= 0x31350 && cp <= 0x323af) // Extension H
  )
}

/**
 * Counts the number of Chinese (CJK ideograph) characters in a string,
 * handling supplementary-plane surrogate pairs as single characters.
 */
export function countChineseChars(text: string): number {
  let count = 0
  for (let i = 0; i < text.length; i++) {
    const code = text.charCodeAt(i)
    let cp = code
    if (code >= 0xd800 && code <= 0xdbff && i + 1 < text.length) {
      const low = text.charCodeAt(i + 1)
      if (low >= 0xdc00 && low <= 0xdfff) {
        cp = 0x10000 + (((code & 0x3ff) << 10) | (low & 0x3ff))
        i++
      }
    }
    if (isChineseChar(cp)) count++
  }
  return count
}
