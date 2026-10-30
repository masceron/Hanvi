import { useCallback, useEffect, useRef, useState } from 'react'
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
  const [rows, setRows] = useState<Entry[]>([])
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
  const loadingMore = useRef(false)
  const listRef = useRef<HTMLDivElement>(null)
  const sentinelRef = useRef<HTMLDivElement>(null)
  const originalInputRef = useRef<HTMLInputElement>(null)

  useEffect(() => {
    const timer = window.setTimeout(() => setFilter(search.trim()), SEARCH_DEBOUNCE_MS)
    return () => window.clearTimeout(timer)
  }, [search])

  const reload = useCallback(async () => {
    const id = ++requestId.current
    const [items, count] = await Promise.all([
      core.listEntries(setId, priority, filter, 0, FIRST_CHUNK),
      filter ? Promise.resolve(null) : core.countEntries(setId, priority),
    ])
    if (id !== requestId.current) return
    setRows(items)
    setHasMore(items.length === FIRST_CHUNK)
    setTotal(count)
    setSelected(-1)
    setEditingRow(-1)
    listRef.current?.scrollTo(0, 0)
  }, [setId, priority, filter])

  useEffect(() => {
    void reload()
  }, [reload])

  const loadMore = useCallback(async () => {
    if (!hasMore || loadingMore.current) return
    loadingMore.current = true
    const id = requestId.current
    const items = await core.listEntries(setId, priority, filter, rows.length, NEXT_CHUNK)
    loadingMore.current = false
    if (id !== requestId.current) return
    setRows((prev) => [...prev, ...items])
    setHasMore(items.length === NEXT_CHUNK)
  }, [hasMore, setId, priority, filter, rows.length])

  // Fetch the next chunk when the end of the list scrolls into view.
  useEffect(() => {
    const sentinel = sentinelRef.current
    if (!sentinel) return
    const observer = new IntersectionObserver((seen) => seen[0].isIntersecting && void loadMore(), {
      root: listRef.current,
      rootMargin: '200px',
    })
    observer.observe(sentinel)
    return () => observer.disconnect()
  }, [loadMore])

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
    setRows((prev) => prev.filter((_, i) => i !== row))
    setTotal((t) => (t === null ? t : t - 1))
    setSelected(Math.min(row, rows.length - 2))
    await core.removeEntry(setId, entry.original, priority)
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
    setRows((prev) => prev.map((e, i) => (i === editingRow ? { ...e, translated: value } : e)))
    await core.insertEntry(setId, entry.original, value, priority)
  }

  // The popup may have changed or removed the phrase: show what is stored now.
  async function phrasePopupClosed(popupChanged: boolean) {
    const key = phraseKey
    setPhraseKey(null)
    if (!popupChanged || key === null) return
    markChanged()
    const translated = await core.getTranslation(key, Priority.Phrase)
    if (translated === null) {
      setRows((prev) => prev.filter((e) => e.original !== key))
      setTotal((t) => (t === null ? t : t - 1))
    } else {
      setRows((prev) => prev.map((e) => (e.original === key ? { ...e, translated } : e)))
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

  let countLabel: string
  if (rows.length === 0) countLabel = filter ? 'No matches' : '0 entries'
  else if (filter) countLabel = hasMore ? `${rows.length}+ matches (scroll for more)` : `${rows.length} matches`
  else if (total !== null && total > rows.length) countLabel = `Showing ${rows.length} of ${total} entries`
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
          <div ref={sentinelRef} className="entries-sentinel" />
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
