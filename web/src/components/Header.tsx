import { HamburgerMenu } from './HamburgerMenu'

interface HeaderProps {
  isMenuOpen: boolean
  statusText: string
  isBusy: boolean
  hasUpdate?: boolean
  onApplyUpdate?: () => void
  onDismissUpdate?: () => void
  onToggleMenu: () => void
  onCloseMenu: () => void
  onOpenFile: () => void
  onReadClipboard: () => void
  onSaveToFile: () => void
  onCopyVietnamese: () => void
  onOpenNamesetChooser: () => void
  onOpenNamesetEntries: () => void
  onOpenNamesetsManager: () => void
  onOpenPhrases: () => void
  onOpenNames: () => void
  onOpenHistory: () => void
  onReconvert: () => void
  onReloadDict: () => void
  onExportDict: () => void
  onImportDict: () => void
  onResetDict: () => void
  storageNote: string
}

// Documents are not tabbed: the browser's own tabs do that, and the open document's
// name goes in the browser tab title.
export function Header({
  isMenuOpen,
  statusText,
  isBusy,
  hasUpdate,
  onApplyUpdate,
  onDismissUpdate,
  onToggleMenu,
  onCloseMenu,
  onOpenFile,
  onReadClipboard,
  onSaveToFile,
  onCopyVietnamese,
  onOpenNamesetChooser,
  onOpenNamesetEntries,
  onOpenNamesetsManager,
  onOpenPhrases,
  onOpenNames,
  onOpenHistory,
  onReconvert,
  onReloadDict,
  onExportDict,
  onImportDict,
  onResetDict,
  storageNote,
}: HeaderProps) {
  return (
    <header className="app-header">
      <HamburgerMenu
        isOpen={isMenuOpen}
        onToggle={onToggleMenu}
        onClose={onCloseMenu}
        onOpenFile={onOpenFile}
        onReadClipboard={onReadClipboard}
        onSaveToFile={onSaveToFile}
        onCopyVietnamese={onCopyVietnamese}
        onOpenNamesetChooser={onOpenNamesetChooser}
        onOpenNamesetEntries={onOpenNamesetEntries}
        onOpenNamesetsManager={onOpenNamesetsManager}
        onOpenPhrases={onOpenPhrases}
        onOpenNames={onOpenNames}
        onOpenHistory={onOpenHistory}
        onReconvert={onReconvert}
        onReloadDict={onReloadDict}
        onExportDict={onExportDict}
        onImportDict={onImportDict}
        onResetDict={onResetDict}
        storageNote={storageNote}
      />

      <div className="header-status">
        {hasUpdate && (
          <div className="update-pill" role="status">
            <button
              type="button"
              className="update-reload-btn"
              onClick={onApplyUpdate}
              title="Reload to apply the new version"
            >
              Update ready · Reload
            </button>
            <button
              type="button"
              className="update-dismiss-btn"
              onClick={onDismissUpdate}
              aria-label="Dismiss update notification"
              title="Dismiss"
            >
              ×
            </button>
          </div>
        )}
        {isBusy && <span className="busy-dot" />}
        <span className="status-label">{statusText}</span>
      </div>
    </header>
  )
}
