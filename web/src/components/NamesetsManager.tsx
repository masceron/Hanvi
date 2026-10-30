import { useCallback, useEffect, useRef, useState } from 'react'
import { core } from '../core/client'
import { Priority, type Entry, type NameSet } from '../core/types'
import { download } from '../download'
import { EntriesEditor } from './EntriesEditor'
import { Modal } from './Modal'

interface NamesetsManagerProps {
  activeNameSet: NameSet | null
  onClose: (changed: boolean) => void
}

// Nameset files hold one `original=translated` pair per line, as the desktop app reads and writes them.
function parseNameSetFile(text: string): Entry[] | string {
  const entries: Entry[] = []
  const lines = text.split(/\r?\n/)
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i]
    if (!line.trim()) continue
    const separator = line.indexOf('=')
    if (separator === -1) return `Format error at line ${i + 1}: missing '=' separator.`
    const original = line.slice(0, separator)
    const translated = line.slice(separator + 1)
    if (!original || !translated) return `Format error at line ${i + 1}: original or translated text is empty.`
    entries.push({ original, translated })
  }
  return entries
}

export function NamesetsManager({ activeNameSet, onClose }: NamesetsManagerProps) {
  const [sets, setSets] = useState<NameSet[]>([])
  const [counts, setCounts] = useState<Record<number, number>>({})
  const [changed, setChanged] = useState(false)
  const [message, setMessage] = useState<{ text: string; error: boolean } | null>(null)

  const [newTitle, setNewTitle] = useState('')
  const [renaming, setRenaming] = useState<{ id: number; title: string } | null>(null)
  const [confirmingDelete, setConfirmingDelete] = useState<number | null>(null)
  const [entriesOf, setEntriesOf] = useState<NameSet | null>(null)
  const fileInputRef = useRef<HTMLInputElement>(null)
  // Escape unmounts the rename box, which may blur it: that blur must not save.
  const renameCancelled = useRef(false)

  const reload = useCallback(
    () =>
      Promise.all([core.nameSets(), core.nameSetEntryCounts()]).then(([list, entryCounts]) => {
        setSets(list)
        setCounts(entryCounts)
      }),
    [],
  )

  useEffect(() => {
    void reload()
  }, [reload])

  function report(text: string, error = false) {
    setMessage({ text, error })
  }

  async function createSet() {
    const title = newTitle.trim()
    if (!title) return
    if ((await core.createNameSet(title)) === null) {
      report(`Could not create “${title}”: a nameset with that name already exists.`, true)
      return
    }
    setChanged(true)
    setNewTitle('')
    report(`Created “${title}”.`)
    await reload()
  }

  async function importSet(file: File) {
    const title = newTitle.trim()
    const parsed = parseNameSetFile(await file.text())
    if (typeof parsed === 'string') {
      report(`${parsed} Nothing was imported.`, true)
      return
    }
    if ((await core.importNameSet(title, parsed)) === null) {
      report('Could not import: the name might already exist, or the file contains duplicate entries. Nothing was imported.', true)
      return
    }
    setChanged(true)
    setNewTitle('')
    report(`Imported ${parsed.length} names into “${title}”.`)
    await reload()
  }

  function startRename(set: NameSet) {
    renameCancelled.current = false
    setRenaming({ id: set.id, title: set.title })
  }

  async function saveRename() {
    if (!renaming || renameCancelled.current) return
    const title = renaming.title.trim()
    setRenaming(null)
    const current = sets.find((s) => s.id === renaming.id)
    if (!title || title === current?.title) return
    if (!(await core.renameNameSet(renaming.id, title))) {
      report(`Could not rename to “${title}”: a nameset with that name already exists.`, true)
      return
    }
    setChanged(true)
    await reload()
  }

  async function deleteSet(id: number) {
    setConfirmingDelete(null)
    setSets((prev) => prev.filter((s) => s.id !== id))
    await core.deleteNameSet(id)
    setChanged(true)
    await reload()
  }

  async function exportSet(set: NameSet) {
    const entries = await core.listEntries(set.id, Priority.Name, '', 0, -1)
    download(`${set.title}.txt`, entries.map((e) => `${e.original}=${e.translated}`).join('\n') + '\n')
  }

  return (
    <>
      <Modal
        title="Namesets"
        className="namesets-manager"
        onClose={() => onClose(changed)}
        footer={
          <button className="btn close-popup-btn" onClick={() => onClose(changed)}>
            Close
          </button>
        }
      >
        <div className="namesets-create">
          <input
            placeholder="New nameset name…"
            value={newTitle}
            onChange={(e) => setNewTitle(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && void createSet()}
          />
          <button className="btn btn-primary" disabled={!newTitle.trim()} onClick={createSet}>
            Create
          </button>
          <button
            className="btn"
            disabled={!newTitle.trim()}
            title="Create the nameset from a text file of original=translated lines"
            onClick={() => fileInputRef.current?.click()}
          >
            Import from file…
          </button>
          <input
            ref={fileInputRef}
            type="file"
            accept=".txt,text/plain"
            hidden
            onChange={(e) => {
              const file = e.target.files?.[0]
              e.target.value = ''
              if (file) void importSet(file)
            }}
          />
        </div>

        {message && <div className={`namesets-message ${message.error ? 'error' : ''}`}>{message.text}</div>}

        <div className="nameset-list-container">
          {sets.length === 0 ? (
            <div className="empty-message">No namesets yet. Create one above.</div>
          ) : (
            <table className="nameset-table">
              <thead>
                <tr>
                  <th>Title</th>
                  <th style={{ width: 80, textAlign: 'center' }}>Entries</th>
                  <th style={{ textAlign: 'right' }}>Actions</th>
                </tr>
              </thead>
              <tbody>
                {sets.map((set) => (
                  <tr
                    key={set.id}
                    className={set.id === activeNameSet?.id ? 'active-row' : ''}
                    onDoubleClick={() => renaming?.id !== set.id && setEntriesOf(set)}
                  >
                    <td className="nameset-title-cell">
                      {renaming?.id === set.id ? (
                        <input
                          className="inline-edit"
                          value={renaming.title}
                          autoFocus
                          onChange={(e) => setRenaming({ id: set.id, title: e.target.value })}
                          onBlur={saveRename}
                          onKeyDown={(e) => {
                            if (e.key === 'Enter') e.currentTarget.blur() // saves via onBlur
                            if (e.key === 'Escape') {
                              e.stopPropagation()
                              renameCancelled.current = true
                              setRenaming(null)
                            }
                          }}
                        />
                      ) : (
                        <>
                          {set.title}
                          {set.id === activeNameSet?.id && <span className="active-tag">Active</span>}
                        </>
                      )}
                    </td>
                    <td style={{ textAlign: 'center' }}>{counts[set.id] ?? 0}</td>
                    <td className="nameset-actions">
                      {confirmingDelete === set.id ? (
                        <>
                          <span className="confirm-text">Delete all its names?</span>
                          <button className="btn btn-danger" onClick={() => deleteSet(set.id)}>
                            Delete
                          </button>
                          <button className="btn" onClick={() => setConfirmingDelete(null)}>
                            Cancel
                          </button>
                        </>
                      ) : (
                        <>
                          <button className="btn" onClick={() => setEntriesOf(set)}>
                            Entries
                          </button>
                          <button className="btn" onClick={() => startRename(set)}>
                            Rename
                          </button>
                          <button className="btn" title="Download as original=translated lines" onClick={() => exportSet(set)}>
                            Save
                          </button>
                          <button className="btn btn-danger-outline" onClick={() => setConfirmingDelete(set.id)}>
                            Delete
                          </button>
                        </>
                      )}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>
      </Modal>

      {entriesOf && (
        <EntriesEditor
          source={{ kind: 'nameset', nameSet: entriesOf }}
          activeNameSet={activeNameSet}
          onClose={(entriesChanged) => {
            setEntriesOf(null)
            if (entriesChanged) {
              setChanged(true)
              void reload()
            }
          }}
        />
      )}
    </>
  )
}
