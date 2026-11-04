import { core } from './core/client'

// Browser storage is best-effort by default: Chrome and Firefox may evict it when the disk runs
// low, and Safari clears a site's storage after 7 days of browser use without a visit. Persistent
// storage is exempt from both, though clearing site data still removes it. Chrome and Safari grant
// it by their own heuristics (bookmarks, installed apps, engagement); Firefox asks the user.
let requested = false

// Asks, once per page load, that the browser keep the stored dictionary.
export async function requestPersistence() {
  if (requested) return
  requested = true
  try {
    if (!(await navigator.storage.persisted())) await navigator.storage.persist()
  } catch {
    // No StorageManager (insecure context): nothing is stored anyway.
  }
}

async function isPersisted(): Promise<boolean> {
  try {
    return await navigator.storage.persisted()
  } catch {
    return false
  }
}

// Where the dictionary lives, for the menu.
export async function describeStorage(): Promise<string> {
  const { available, customized } = await core.storageInfo()
  if (!available) return 'This browser cannot store the dictionary: edits last until the tab is closed.'
  if (!customized) return 'Using the bundled dictionary.'
  if (await isPersisted()) return 'Your dictionary is stored in this browser until you clear its site data.'
  return 'Your dictionary is stored in this browser, which may clear it to free space. Export a backup.'
}
