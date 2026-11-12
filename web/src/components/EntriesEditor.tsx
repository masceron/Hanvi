import { useCallback, useEffect, useLayoutEffect, useRef, useState } from 'react'
import { flushSync } from 'react-dom'
import { core } from '../core/client'
import { GLOBAL, Priority, type Entry, type NameSet } from '../core/types'
import { CloseIcon, SearchIcon } from '../icons/SvgIcons'
import { DictPopup } from './DictPopup'
import { Modal } from './Modal'

export type EntriesSource =
  | { kind: 'phrases' }
  | { kind: 'names' }
  | { kind: 'nameset'; nameSet: NameSet }

interface EntriesEditorProps {
  source: EntriesSource
  activeNameSet: NameSet | null
  onClose: (changed: boolean) => void
}

const FIRST_CHUNK = 200
const NEXT_CHUNK = 100
const MAX_WINDOW = 400
const SEARCH_DEBOUNCE_MS = 150

function describe(source: EntriesSource) {
  switch (source.kind) {
    case 'phrases':
      return { title: 'Dictionary: Phrases', setId: GLOBAL, priority: Priority.Phrase }
    case 'names':
      return { title: 'Dictionary: Global Names', setId: GLOBAL, priority: Priority.Name }
    case 'nameset':
      return { title: `Nameset: ${source.nameSet.title}`, setId: source.nameSet.id, priority: Priority.Name }
  }
}

// Phrases hold several meanings separated by U+001F; the first is the default.
const MEANING_SEPARATOR = '\x1F'

export function EntriesEditor({ source, activeNameSet, onClose }: EntriesEditorProps) {
  const { title, setId, priority } = describe(source)
  const isPhrases = priority === Priority.Phrase

  const [search, setSearch] = useState('')
  const [filter, setFilter] = useState('')
  // Like the desktop editor, only a window of up to MAX_WINDOW entries, starting at `offset`, is
  // loaded: scrolling fetches the next chunk and drops what is far out of view on the other side.
  const [view, setView] = useState<{ offset: number; rows: Entry[] }>({ offset: 0, rows: [] })
  const { offset, rows } = view
  const [hasMore, setHasMore] = useState(false)
  const [total, setTotal] = useState<number | null>(null)
  const [selected, setSelected] = useState(-1)
  const [changed, setChanged] = useState(false)

  const [newOriginal, setNewOriginal] = useState('')
  const [newTranslation, setNewTranslation] = useState('')
  // Names are edited in place; phrases open the dictionary popup.
  const [editingRow, setEditingRow] = useState(-1)
  const [editValue, setEditValue] = useState('')
  const [phraseKey, setPhraseKey] = useState<string | null>(null)
  // Escape unmounts the inline editor, which may blur it: that blur must not save.
  const editCancelled = useRef(false)

  const requestId = useRef(0)
  const loading = useRef(false)
  // The view as last set, for async loads that finish after other changes.
  const viewRef = useRef(view)
  // The first visible row and its distance from the top of the list, kept in place while rows are
  // added or dropped above it.
  const scrollAnchor = useRef<{ key: string; top: number } | null>(null)
  const listRef = useRef<HTMLDivElement>(null)
  const topSentinelRef = useRef<HTMLDivElement>(null)
  const bottomSentinelRef = useRef<HTMLDivElement>(null)
  const originalInputRef = useRef<HTMLInputElement>(null)

  useEffect(() => {
    const timer = window.setTimeout(() => setFilter(search.trim()), SEARCH_DEBOUNCE_MS)
    return () => window.clearTimeout(timer)
  }, [search])

  const updateView = useCallback((next: { offset: number; rows: Entry[] }) => {
    viewRef.current = next
    setView(next)
  }, [])

  // Rows moved by `by` places; the selection and the inline editor follow their row, or end
  // when it was dropped.
  const shiftRows = useCallback((by: number, size: number) => {
    const shift = (row: number) => (row === -1 || row + by < 0 || row + by >= size ? -1 : row + by)
    setSelected(shift)
    setEditingRow((row) => {
      if (row !== -1 && shift(row) === -1) editCancelled.current = true
      return shift(row)
    })
  }, [])

  // Called right before a flushSync update, so no scroll can come between measuring and adjusting.
  const holdScrollPosition = useCallback(() => {
    const list = listRef.current
    if (!list) return
    const listTop = list.getBoundingClientRect().top
    for (const row of list.querySelectorAll<HTMLElement>('tbody tr')) {
      const rect = row.getBoundingClientRect()
      if (rect.bottom > listTop) {
        scrollAnchor.current = { key: row.dataset.key!, top: rect.top - listTop }
        return
      }
    }
  }, [])

  useLayoutEffect(() => {
    const anchor = scrollAnchor.current
    const list = listRef.current
    scrollAnchor.current = null
    if (!anchor || !list) return
    const row = list.querySelector(`tr[data-key="${CSS.escape(anchor.key)}"]`)
    if (row) list.scrollTop += row.getBoundingClientRect().top - list.getBoundingClientRect().top - anchor.top
  }, [view])

  const reload = useCallback(async () => {
    const id = ++requestId.current
    const [items, count] = await Promise.all([
      core.listEntries(setId, priority, filter, 0, FIRST_CHUNK),
      filter ? Promise.resolve(null) : core.countEntries(setId, priority),
    ])
    if (id !== requestId.current) return
    scrollAnchor.current = null
    updateView({ offset: 0, rows: items })
    setHasMore(items.length === FIRST_CHUNK)
    setTotal(count)
    setSelected(-1)
    setEditingRow(-1)
    listRef.current?.scrollTo(0, 0)
  }, [setId, priority, filter, updateView])

  useEffect(() => {
    void reload()
  }, [reload])

  const loadDown = useCallback(async () => {
    if (!hasMore || loading.current) return
    loading.current = true
    const id = requestId.current
    const start = viewRef.current.offset + viewRef.current.rows.length
    try {
      const items = await core.listEntries(setId, priority, filter, start, NEXT_CHUNK)
      if (id !== requestId.current) return
      const { offset, rows } = viewRef.current
      const joined = [...rows, ...items]
      const dropped = Math.max(0, joined.length - MAX_WINDOW)
      holdScrollPosition()
      flushSync(() => {
        updateView({ offset: offset + dropped, rows: joined.slice(dropped) })
        shiftRows(-dropped, joined.length - dropped)
        setHasMore(items.length === NEXT_CHUNK)
      })
    } finally {
      loading.current = false
    }
  }, [hasMore, setId, priority, filter, holdScrollPosition, updateView, shiftRows])

  const loadUp = useCallback(async () => {
    const before = viewRef.current.offset
    if (before === 0 || loading.current) return
    loading.current = true
    const id = requestId.current
    const count = Math.min(NEXT_CHUNK, before)
    try {
      const items = await core.listEntries(setId, priority, filter, before - count, count)
      if (id !== requestId.current) return
      const { offset, rows } = viewRef.current
      const joined = [...items, ...rows]
      const kept = joined.slice(0, MAX_WINDOW)
      holdScrollPosition()
      flushSync(() => {
        updateView({ offset: items.length === 0 ? 0 : offset - items.length, rows: kept })
        shiftRows(items.length, kept.length)
        if (kept.length < joined.length) setHasMore(true)
      })
    } finally {
      loading.current = false
    }
  }, [setId, priority, filter, holdScrollPosition, updateView, shiftRows])

  // Fetch a chunk when either end of the window scrolls into view. Observing again after every
  // change keeps loading while an end stays in view.
  useEffect(() => {
    const top = topSentinelRef.current
    const bottom = bottomSentinelRef.current
    if (!top || !bottom) return
    const observer = new IntersectionObserver(
      (seen) => {
        for (const entry of seen) {
          if (entry.isIntersecting) void (entry.target === top ? loadUp() : loadDown())
        }
      },
      { root: listRef.current, rootMargin: '200px' },
    )
    observer.observe(top)
    observer.observe(bottom)
    return () => observer.disconnect()
  }, [loadUp, loadDown, view])

  function markChanged() {
    setChanged(true)
  }

  async function addEntry() {
    const original = newOriginal.trim()
    let value = newTranslation.trim()
    if (!original || !value) return
    if (isPhrases) {
      const meanings = [...new Set(value.split(/[/,]/).map((m) => m.trim()).filter(Boolean))]
      if (meanings.length === 0) return
      value = meanings.join(MEANING_SEPARATOR)
    }
    markChanged()
    await core.insertEntry(setId, original, value, priority)
    setNewOriginal('')
    setNewTranslation('')
    await reload()
    originalInputRef.current?.focus()
  }

  async function deleteEntry(row: number) {
    const entry = rows[row]
    if (!entry) return
    markChanged()
    const remaining = rows.filter((_, i) => i !== row)
    updateView({ offset, rows: remaining })
    setTotal((t) => (t === null ? t : t - 1))
    setSelected(Math.min(row, remaining.length - 1))
    await core.removeEntry(setId, entry.original, priority)
    if (remaining.length === 0 && (offset > 0 || hasMore)) await reload()
  }

  function editEntry(row: number) {
    const entry = rows[row]
    if (!entry) return
    setSelected(row)
    if (isPhrases) {
      setPhraseKey(entry.original)
    } else {
      editCancelled.current = false
      setEditingRow(row)
      setEditValue(entry.translated)
    }
  }

  async function saveInlineEdit() {
    const entry = rows[editingRow]
    const value = editValue.trim()
    setEditingRow(-1)
    if (editCancelled.current || !entry || !value || value === entry.translated) return
    markChanged()
    const current = viewRef.current
    updateView({ ...current, rows: current.rows.map((e) => (e.original === entry.original ? { ...e, translated: value } : e)) })
    await core.insertEntry(setId, entry.original, value, priority)
  }

  // The popup may have changed or removed the phrase: show what is stored now.
  async function phrasePopupClosed(popupChanged: boolean) {
    const key = phraseKey
    setPhraseKey(null)
    if (!popupChanged || key === null) return
    markChanged()
    const translated = await core.getTranslation(key, Priority.Phrase)
    const current = viewRef.current
    if (translated === null) {
      updateView({ ...current, rows: current.rows.filter((e) => e.original !== key) })
      setTotal((t) => (t === null ? t : t - 1))
    } else {
      updateView({ ...current, rows: current.rows.map((e) => (e.original === key ? { ...e, translated } : e)) })
    }
  }

  function handleListKey(e: React.KeyboardEvent) {
    if (editingRow !== -1) return
    if (e.key === 'ArrowDown') {
      e.preventDefault()
      setSelected((s) => Math.min(s + 1, rows.length - 1))
    } else if (e.key === 'ArrowUp') {
      e.preventDefault()
      setSelected((s) => Math.max(s - 1, 0))
    } else if (e.key === 'Delete' && selected !== -1) {
      e.preventDefault()
      void deleteEntry(selected)
    } else if (e.key === 'Enter' && selected !== -1) {
      e.preventDefault()
      editEntry(selected)
    }
  }

  useEffect(() => {
    listRef.current?.querySelector('tr.selected')?.scrollIntoView({ block: 'nearest' })
  }, [selected])

  const range = `${offset + 1} – ${offset + rows.length}`
  let countLabel: string
  if (rows.length === 0) countLabel = filter ? 'No matches' : '0 entries'
  else if (filter && hasMore) countLabel = `Matches ${range} (scroll for more)`
  else if (filter) countLabel = offset > 0 ? `Matches ${range}` : `${rows.length} matches`
  else if (total !== null && total > rows.length) countLabel = `Entries ${range} of ${total}`
  else countLabel = `${total ?? rows.length} entries`

  return (
    <>
      <Modal
        title={title}
        className="entries-editor"
        onClose={() => onClose(changed)}
        footer={
          <>
            <span className="entries-count">{countLabel}</span>
            <button className="btn" disabled={selected === -1} onClick={() => editEntry(selected)}>
              Edit
            </button>
            <button className="btn btn-danger-outline" disabled={selected === -1} onClick={() => deleteEntry(selected)}>
              Delete
            </button>
            <button className="btn close-popup-btn" onClick={() => onClose(changed)}>
              Close
            </button>
          </>
        }
      >
        <div className="nameset-search-bar">
          <SearchIcon className="search-icon" />
          <input
            type="text"
            placeholder="Search original or translation…"
            value={search}
            autoFocus
            onChange={(e) => setSearch(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === 'Escape' && search) {
                e.stopPropagation()
                setSearch('')
              }
            }}
          />
          {search && (
            <button className="clear-btn" onClick={() => setSearch('')}>
              <CloseIcon />
            </button>
          )}
        </div>

        <div className="entries-list" ref={listRef} tabIndex={0} onKeyDown={handleListKey}>
          <div ref={topSentinelRef} className="entries-sentinel" />
          <table className="entries-table">
            <thead>
              <tr>
                <th>Original (Chinese)</th>
                <th>Translation (Vietnamese)</th>
              </tr>
            </thead>
            <tbody>
              {rows.map((entry, i) => (
                <tr
                  key={entry.original}
                  data-key={entry.original}
                  className={i === selected ? 'selected' : ''}
                  onClick={() => setSelected(i)}
                  onDoubleClick={() => editEntry(i)}
                >
                  <td className="cn-text">{entry.original}</td>
                  <td title={isPhrases ? entry.translated.split(MEANING_SEPARATOR).join('\n') : entry.translated}>
                    {i === editingRow ? (
                      <input
                        className="inline-edit"
                        value={editValue}
                        autoFocus
                        onChange={(e) => setEditValue(e.target.value)}
                        onBlur={saveInlineEdit}
                        onKeyDown={(e) => {
                          if (e.key === 'Enter') e.currentTarget.blur() // saves via onBlur
                          if (e.key === 'Escape') {
                            e.stopPropagation()
                            editCancelled.current = true
                            setEditingRow(-1)
                          }
                        }}
                      />
                    ) : isPhrases ? (
                      entry.translated.split(MEANING_SEPARATOR).join(' / ')
                    ) : (
                      entry.translated
                    )}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
          <div ref={bottomSentinelRef} className="entries-sentinel" />
        </div>

        <div className="entries-add">
          <input
            ref={originalInputRef}
            className="cn-text"
            placeholder="Original (Chinese)…"
            value={newOriginal}
            onChange={(e) => setNewOriginal(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && (e.currentTarget.nextElementSibling as HTMLElement).focus()}
          />
          <input
            placeholder={isPhrases ? 'Translation (use / for multiple meanings)…' : 'Translation (Vietnamese)…'}
            value={newTranslation}
            onChange={(e) => setNewTranslation(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && void addEntry()}
          />
          <button className="btn btn-primary" disabled={!newOriginal.trim() || !newTranslation.trim()} onClick={addEntry}>
            Add
          </button>
        </div>
      </Modal>

      {phraseKey !== null && (
        <DictPopup text={phraseKey} activeNameSet={activeNameSet} onClose={phrasePopupClosed} />
      )}
    </>
  )
}
