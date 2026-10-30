import { useEffect, useState } from 'react'
import type { NameSet } from '../core/types'
import { CloseIcon, SearchIcon } from '../icons/SvgIcons'

interface NamesetChooserProps {
  isOpen: boolean
  nameSets: NameSet[]
  activeSetId: number
  onSelect: (id: number) => void
  onClose: () => void
}

export function NamesetChooser({
  isOpen,
  nameSets,
  activeSetId,
  onSelect,
  onClose,
}: NamesetChooserProps) {
  const [filter, setFilter] = useState('')

  useEffect(() => {
    function handleKeyDown(e: KeyboardEvent) {
      if (e.key === 'Escape') onClose()
    }
    if (isOpen) {
      window.addEventListener('keydown', handleKeyDown)
    }
    return () => window.removeEventListener('keydown', handleKeyDown)
  }, [isOpen, onClose])

  if (!isOpen) return null

  const filteredSets = nameSets.filter((s) =>
    s.title.toLowerCase().includes(filter.toLowerCase()),
  )

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div className="nameset-dialog" onClick={(e) => e.stopPropagation()}>
        <div className="popup-header">
          <span className="popup-title">Choose Nameset</span>
          <button className="icon-btn close-btn" onClick={onClose} title="Close">
            <CloseIcon />
          </button>
        </div>

        <div className="nameset-search-bar">
          <SearchIcon className="search-icon" />
          <input
            type="text"
            placeholder="Search nameset..."
            value={filter}
            autoFocus
            onChange={(e) => setFilter(e.target.value)}
          />
          {filter && (
            <button className="clear-btn" onClick={() => setFilter('')}>
              <CloseIcon />
            </button>
          )}
        </div>

        <div className="nameset-list-container">
          {filteredSets.length === 0 ? (
            <div className="empty-message">No namesets found</div>
          ) : (
            <table className="nameset-table">
              <thead>
                <tr>
                  <th>Title</th>
                  <th style={{ width: 80, textAlign: 'center' }}>ID</th>
                  <th style={{ width: 100, textAlign: 'right' }}>Action</th>
                </tr>
              </thead>
              <tbody>
                {filteredSets.map((set) => {
                  const isActive = set.id === activeSetId
                  return (
                    <tr key={set.id} className={isActive ? 'active-row' : ''}>
                      <td className="nameset-title-cell">
                        {set.title}
                        {isActive && <span className="active-tag">Active</span>}
                      </td>
                      <td style={{ textAlign: 'center', opacity: 0.7 }}>#{set.id}</td>
                      <td style={{ textAlign: 'right' }}>
                        <button
                          className={`btn ${isActive ? 'btn-active-set' : 'btn-choose-set'}`}
                          onClick={() => {
                            onSelect(set.id)
                            onClose()
                          }}
                        >
                          {isActive ? 'Current' : 'Choose'}
                        </button>
                      </td>
                    </tr>
                  )
                })}
              </tbody>
            </table>
          )}
        </div>

        <div className="nameset-footer">
          <button
            className="btn btn-danger-outline"
            onClick={() => {
              onSelect(-1)
              onClose()
            }}
          >
            Clear (No Nameset)
          </button>
          <button className="btn" onClick={onClose}>
            Close
          </button>
        </div>
      </div>
    </div>
  )
}

