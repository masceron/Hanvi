import { useEffect, useRef, useState } from 'react'
import { core } from '../core/client'
import { GLOBAL, Priority, type Lookup, type NameSet } from '../core/types'
import { CloseIcon } from '../icons/SvgIcons'
import { Modal } from './Modal'

interface DictPopupProps {
  // Chinese text being edited: a token, or a selection spanning several.
  text: string
  activeNameSet: NameSet | null
  onClose: (changed: boolean) => void
}

// Capitalization modes of the desktop popup: 0 lowercases everything, 1-3 capitalize that many
// leading words, 4 capitalizes every word.
const CAPITALIZE_MODES = [
  { mode: 0, label: 'None' },
  { mode: 1, label: '1' },
  { mode: 2, label: '2' },
  { mode: 3, label: '3' },
  { mode: 4, label: 'All' },
]

function capitalize(text: string, mode: number): string {
  return text
    .split(' ')
    .map((word, i) => {
      const lower = word.toLowerCase()
      const upper = mode === 4 || (mode > 0 && i < mode)
      return upper && lower ? lower[0].toUpperCase() + lower.slice(1) : lower
    })
    .join(' ')
}

function CapitalizeRow({ value, onChange }: { value: string; onChange: (text: string) => void }) {
  return (
    <div className="capitalize-row">
      <span className="capitalize-label">Capitalize</span>
      {CAPITALIZE_MODES.map(({ mode, label }) => (
        <button key={mode} className="btn btn-cap" disabled={!value} onClick={() => onChange(capitalize(value, mode))}>
          {label}
        </button>
      ))}
    </div>
  )
}

export function DictPopup({ text, activeNameSet, onClose }: DictPopupProps) {
  const [entry, setEntry] = useState<Lookup | null>(null)
  const [changed, setChanged] = useState(false)

  const [storedName, setStoredName] = useState<string | null>(null)
  const [name, setName] = useState('')
  const [useNameSet, setUseNameSet] = useState(false)

  const [phrases, setPhrases] = useState<string[]>([])
  const [newMeaning, setNewMeaning] = useState('')
  // Index of the chip being dragged; a ref too, as dragover fires faster than React re-renders.
  const [dragIndex, setDragIndex] = useState<number | null>(null)
  const dragging = useRef<number | null>(null)
  // Order as last saved, to tell whether a drag actually moved anything.
  const savedOrder = useRef<string[]>([])
  const currentOrder = useRef<string[]>([])
  useEffect(() => {
    currentOrder.current = phrases
  }, [phrases])

  useEffect(() => {
    void core.lookup(text).then((found) => {
      setEntry(found)
      setStoredName(found.name)
      setName(found.name ?? '')
      // Like the desktop: names go to the active nameset unless the shown name is a global one.
      setUseNameSet(activeNameSet !== null && (found.nameFromNameSet || found.name === null))
      setPhrases(found.phrases)
      savedOrder.current = found.phrases
    })
  }, [text, activeNameSet])

  const nameTarget = useNameSet && activeNameSet ? activeNameSet.id : GLOBAL
  const trimmedName = name.trim()

  // Each edit updates the popup first: the worker applies edits in order, so there is nothing to wait for.
  async function saveName() {
    if (!trimmedName || trimmedName === storedName) return
    setChanged(true)
    setStoredName(trimmedName)
    setName(trimmedName)
    await core.insertEntry(nameTarget, text, trimmedName, Priority.Name)
  }

  async function deleteName() {
    setChanged(true)
    setStoredName(null)
    setName('')
    await core.removeEntry(nameTarget, text, Priority.Name)
  }

  async function addMeaning() {
    const meaning = newMeaning.trim()
    if (!meaning) return
    setChanged(true)
    // The core puts a new meaning first and drops an existing copy of it.
    const updated = [meaning, ...phrases.filter((p) => p !== meaning)]
    setPhrases(updated)
    savedOrder.current = updated
    setNewMeaning('')
    await core.insertEntry(GLOBAL, text, meaning, Priority.Phrase)
  }

  async function removeMeaning(meaning: string) {
    setChanged(true)
    const updated = phrases.filter((p) => p !== meaning)
    setPhrases(updated)
    savedOrder.current = updated
    await core.removeMeaning(text, meaning)
  }

  async function deletePhrase() {
    setChanged(true)
    setPhrases([])
    savedOrder.current = []
    await core.removeEntry(GLOBAL, text, Priority.Phrase)
  }

  function startDrag(index: number) {
    dragging.current = index
    setDragIndex(index)
  }

  // Dragging a chip over another moves it there.
  function dragOver(index: number) {
    const from = dragging.current
    if (from === null || from === index) return
    setPhrases((prev) => {
      const updated = [...prev]
      const [moved] = updated.splice(from, 1)
      updated.splice(index, 0, moved)
      return updated
    })
    dragging.current = index
    setDragIndex(index)
  }

  // Saves the order once the chip is dropped (on drop, or on dragend when dropped elsewhere).
  async function finishDrag() {
    dragging.current = null
    setDragIndex(null)
    const order = currentOrder.current
    if (order.every((p, i) => p === savedOrder.current[i])) return
    savedOrder.current = order
    setChanged(true)
    await core.reorderMeanings(text, order)
  }

  return (
    <Modal
      title="Change name/phrase"
      className="dict-popup"
      onClose={() => onClose(changed)}
      footer={
        <button className="btn close-popup-btn" onClick={() => onClose(changed)}>
          Close
        </button>
      }
    >
      <div className="popup-body dict-editor">
        <div className="dict-original">{text}</div>

        <section className="dict-section">
          <h3>Sino-Vietnamese reading</h3>
          <div className="dict-sv">{entry?.svReading ?? '…'}</div>
        </section>

        <section className="dict-section">
          <h3>Name</h3>
          <input
            className="dict-input"
            value={name}
            placeholder="No name"
            disabled={!entry}
            onChange={(e) => setName(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && void saveName()}
          />
          <label className={`nameset-toggle ${activeNameSet ? '' : 'disabled'}`}>
            <input
              type="checkbox"
              checked={useNameSet}
              disabled={!activeNameSet}
              onChange={(e) => setUseNameSet(e.target.checked)}
            />
            {activeNameSet ? `Use current nameset (${activeNameSet.title})` : 'Use current nameset'}
          </label>
          <CapitalizeRow value={name} onChange={setName} />
          <div className="dict-actions">
            <button className="btn" disabled={!entry} onClick={() => setName(entry?.svReading ?? '')}>
              Use SV reading
            </button>
            <button className="btn btn-primary" disabled={!trimmedName || trimmedName === storedName} onClick={saveName}>
              Save
            </button>
            <button className="btn btn-danger-outline" disabled={storedName === null} onClick={deleteName}>
              Delete
            </button>
          </div>
        </section>

        <section className="dict-section">
          <h3>Phrase</h3>
          <div
            className="chip-list"
            onDragOver={(e) => dragging.current !== null && e.preventDefault()}
            onDrop={(e) => {
              e.preventDefault()
              void finishDrag()
            }}
          >
            {phrases.length === 0 && <span className="chip-empty">No meanings</span>}
            {phrases.map((meaning, i) => (
              <span
                key={meaning}
                className={`chip ${i === 0 ? 'chip-default' : ''} ${i === dragIndex ? 'dragging' : ''}`}
                draggable
                title={i === 0 ? 'Default meaning (drag to reorder)' : 'Drag to reorder'}
                onDragStart={(e) => {
                  e.dataTransfer.effectAllowed = 'move'
                  e.dataTransfer.setData('text/plain', meaning)
                  startDrag(i)
                }}
                onDragOver={() => dragOver(i)}
                onDragEnd={finishDrag}
              >
                {meaning}
                <button className="chip-remove" title="Remove this meaning" onClick={() => removeMeaning(meaning)}>
                  <CloseIcon width={12} height={12} />
                </button>
              </span>
            ))}
          </div>
          <input
            className="dict-input"
            value={newMeaning}
            placeholder="New meaning"
            disabled={!entry}
            onChange={(e) => setNewMeaning(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && void addMeaning()}
          />
          <CapitalizeRow value={newMeaning} onChange={setNewMeaning} />
          <div className="dict-actions">
            <button className="btn" disabled={!entry} onClick={() => setNewMeaning(entry?.svReading ?? '')}>
              Use SV reading
            </button>
            <button className="btn btn-primary" disabled={!newMeaning.trim()} onClick={addMeaning}>
              Add
            </button>
            <button className="btn btn-danger-outline" disabled={phrases.length === 0} onClick={deletePhrase}>
              Delete
            </button>
          </div>
        </section>
      </div>
    </Modal>
  )
}
