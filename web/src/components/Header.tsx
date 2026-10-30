import { HamburgerMenu } from './HamburgerMenu'

interface HeaderProps {
  isMenuOpen: boolean
  statusText: string
  isBusy: boolean
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
  onReconvert: () => void
  onReloadDict: () => void
}

// Documents are not tabbed: the browser's own tabs do that, and the open document's
// name goes in the browser tab title.
export function Header({
  isMenuOpen,
  statusText,
  isBusy,
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
  onReconvert,
  onReloadDict,
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
        onReconvert={onReconvert}
        onReloadDict={onReloadDict}
      />

      <div className="header-status">
        {isBusy && <span className="busy-dot" />}
        <span className="status-label">{statusText}</span>
      </div>
    </header>
  )
}
