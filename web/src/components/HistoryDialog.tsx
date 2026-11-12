import { useCallback, useEffect, useState } from 'react'
import { core } from '../core/client'
import type { HistoryAction, NameSet } from '../core/types'
import { ConfirmDialog } from './ConfirmDialog'
import { Modal } from './Modal'

interface HistoryDialogProps {
  nameSets: NameSet[]
  onClose: (changed: boolean) => void
}

// Phrases hold several meanings separated by U+001F.
function shown(value: string | null) {
  return (value ?? '').split('\x1F').join(' / ')
}

function where(action: HistoryAction, nameSets: NameSet[]) {
  if (action.kind === 'phrase') return 'Phrase'
  if (action.kind === 'variant') return 'Variant'
  if (action.setId === -1) return 'Name'
  const set = nameSets.find((s) => s.id === action.setId)
  return set ? `Nameset “${set.title}”` : 'Deleted nameset'
}

function describe(action: HistoryAction) {
  switch (action.change) {
    case 'add':
      return `Added: ${shown(action.after)}`
    case 'delete':
      return `Deleted: ${shown(action.before)}`
    case 'reorder':
      return `Reordered: ${shown(action.after)}`
    case 'addMeaning':
      return `Added meaning: ${action.meaning}`
    case 'removeMeaning':
      return `Removed meaning: ${action.meaning}`
    case 'edit':
      return `${shown(action.before)} → ${shown(action.after)}`
  }
}

function stateText(action: HistoryAction) {
  switch (action.state) {
    case 'undoable':
      return ''
    case 'undone':
      return 'Undone'
    case 'changedLater':
      return `Changed again by #${action.changedBy}`
    case 'changedOutside':
      return 'Changed outside Hanvi'
    case 'nameSetDeleted':
      return 'Nameset deleted'
  }
}

function when(time: number) {
  const date = new Date(time * 1000)
  const today = new Date().toDateString() === date.toDateString()
  return today
    ? date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
    : date.toLocaleString([], { year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit' })
}

// Recent dictionary edits, newest first, each undoable while its entry still holds what the edit
// left there.
export function HistoryDialog({ nameSets, onClose }: HistoryDialogProps) {
  const [actions, setActions] = useState<HistoryAction[] | null>(null)
  const [selectedId, setSelectedId] = useState<number | null>(null)
  const [changed, setChanged] = useState(false)
  const [busy, setBusy] = useState(false)
  const [confirmingClear, setConfirmingClear] = useState(false)

  const reload = useCallback(
    () =>
      core.history().then((list) => {
        setActions(list)
        setSelectedId((id) => (list.some((a) => a.id === id) ? id : (list[0]?.id ?? null)))
      }),
    [],
  )

  useEffect(() => {
    void reload()
  }, [reload])

  const selected = actions?.find((a) => a.id === selectedId) ?? null

  async function undo(action: HistoryAction | null) {
    if (!action || action.state !== 'undoable' || busy) return
    setBusy(true)
    try {
      if ((await core.undo(action.id)) === 'undoable') setChanged(true)
      // Reloaded either way: the entry may have changed since the list was shown.
      await reload()
    } finally {
      setBusy(false)
    }
  }

  async function clear() {
    setBusy(true)
    try {
      await core.clearHistory()
      await reload()
    } finally {
      setBusy(false)
    }
  }

  function handleKey(e: React.KeyboardEvent) {
    if (!actions || actions.length === 0) return
    const index = actions.findIndex((a) => a.id === selectedId)
    if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      e.preventDefault()
      const next = Math.max(0, Math.min(actions.length - 1, index + (e.key === 'ArrowDown' ? 1 : -1)))
      setSelectedId(actions[next].id)
    } else if (e.key === 'Enter') {
      e.preventDefault()
      void undo(selected)
    }
  }

  useEffect(() => {
    document.querySelector('.history-dialog tr.selected')?.scrollIntoView({ block: 'nearest' })
  }, [selectedId])

  return (
    <>
      <Modal
        title="Recent Changes"
        className="history-dialog"
        onClose={() => onClose(changed)}
        footer={
          <>
            <span className="entries-count">
              {actions === null
                ? 'Loading…'
                : actions.length === 0
                  ? 'No changes yet.'
                  : 'The last 500 changes are kept.'}
            </span>
            <button
              className="btn btn-danger-outline"
              disabled={busy || !actions || actions.length === 0}
              onClick={() => setConfirmingClear(true)}
            >
              Clear History
            </button>
            <button
              className="btn btn-primary"
              disabled={busy || selected?.state !== 'undoable'}
              onClick={() => undo(selected)}
            >
              Undo
            </button>
            <button className="btn close-popup-btn" onClick={() => onClose(changed)}>
              Close
            </button>
          </>
        }
      >
        <div className="entries-list" tabIndex={0} onKeyDown={handleKey}>
          <table className="entries-table history-table">
            <thead>
              <tr>
                <th>#</th>
                <th>When</th>
                <th>Entry</th>
                <th>Change</th>
                <th>Status</th>
              </tr>
            </thead>
            <tbody>
              {actions?.map((action) => (
                <tr
                  key={action.id}
                  className={[action.id === selectedId ? 'selected' : '', action.state === 'undoable' ? '' : 'inactive']
                    .join(' ')
                    .trim()}
                  onClick={() => setSelectedId(action.id)}
                  onDoubleClick={() => undo(action)}
                >
                  <td>{action.id}</td>
                  <td className="nowrap">{when(action.time)}</td>
                  <td className="nowrap">
                    {where(action, nameSets)} <span className="cn-text">{action.original}</span>
                  </td>
                  <td className="history-change" title={describe(action)}>
                    {describe(action)}
                  </td>
                  <td className="nowrap">{stateText(action)}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </Modal>

      {confirmingClear && (
        <ConfirmDialog
          title="Clear history"
          confirmLabel="Clear"
          onConfirm={() => void clear()}
          onClose={() => setConfirmingClear(false)}
        >
          <p>
            Forget every recorded change? The dictionary stays as it is, but none of these changes can be undone
            afterwards.
          </p>
        </ConfirmDialog>
      )}
    </>
  )
}
