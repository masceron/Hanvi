import type { TabData } from '../core/types'
import { AddIcon, CloseIcon } from '../icons/SvgIcons'
import { HamburgerMenu } from './HamburgerMenu'

interface HeaderProps {
  tabs: TabData[]
  activeTabId: string
  isMenuOpen: boolean
  statusText: string
  isBusy: boolean
  onToggleMenu: () => void
  onCloseMenu: () => void
  onSelectTab: (id: string) => void
  onNewTab: () => void
  onCloseTab: (id: string) => void
  onOpenFile: () => void
  onReadClipboard: () => void
  onSaveToFile: () => void
  onCopyVietnamese: () => void
  onOpenNamesetChooser: () => void
  onReconvert: () => void
  onReloadDict: () => void
}

export function Header({
  tabs,
  activeTabId,
  isMenuOpen,
  statusText,
  isBusy,
  onToggleMenu,
  onCloseMenu,
  onSelectTab,
  onNewTab,
  onCloseTab,
  onOpenFile,
  onReadClipboard,
  onSaveToFile,
  onCopyVietnamese,
  onOpenNamesetChooser,
  onReconvert,
  onReloadDict,
}: HeaderProps) {
  return (
    <header className="tab-container">
      <HamburgerMenu
        isOpen={isMenuOpen}
        onToggle={onToggleMenu}
        onClose={onCloseMenu}
        onOpenFile={onOpenFile}
        onReadClipboard={onReadClipboard}
        onSaveToFile={onSaveToFile}
        onCopyVietnamese={onCopyVietnamese}
        onOpenNamesetChooser={onOpenNamesetChooser}
        onReconvert={onReconvert}
        onReloadDict={onReloadDict}
        onNewTab={onNewTab}
        onCloseTab={() => onCloseTab(activeTabId)}
      />

      <div className="tab-bar">
        {tabs.map((tab) => {
          const isActive = tab.id === activeTabId
          return (
            <div
              key={tab.id}
              className={`tab-item ${isActive ? 'selected' : ''}`}
              onClick={() => onSelectTab(tab.id)}
              onAuxClick={(e) => {
                if (e.button === 1) {
                  // Middle click closes tab
                  e.preventDefault()
                  onCloseTab(tab.id)
                }
              }}
              title={tab.fileName || tab.title}
            >
              <span className="tab-title">{tab.title}</span>
              <button
                className="tab-close-btn"
                onClick={(e) => {
                  e.stopPropagation()
                  onCloseTab(tab.id)
                }}
                title="Close Tab (Ctrl+W)"
              >
                <CloseIcon />
              </button>
            </div>
          )
        })}
      </div>

      <button className="add-tab-btn" onClick={onNewTab} title="New Tab (Ctrl+T)">
        <AddIcon />
      </button>

      <div className="header-status">
        {isBusy && <span className="busy-dot" />}
        <span className="status-label">{statusText}</span>
      </div>
    </header>
  )
}

