export function paginate(text: string, minLength: number): string[] {
  if (!text) return []
  const pages: string[] = []
  let cursor = 0
  const length = text.length

  while (cursor < length) {
    const targetEnd = cursor + minLength

    if (targetEnd >= length) {
      pages.push(text.slice(cursor))
      break
    }

    const cutoff = text.indexOf('\n', targetEnd)
    if (cutoff === -1) {
      pages.push(text.slice(cursor))
      break
    } else {
      pages.push(text.slice(cursor, cutoff + 1))
      cursor = cutoff + 1
    }
  }

  return pages
}

