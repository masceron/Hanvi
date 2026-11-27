// Whether conversions segment by word frequency (the core's Segmentation::Frequency) rather than
// taking the longest entry from the left. Kept per browser, like the appearance.
const STORAGE_KEY = 'hanvi.segmentation'

export function loadFrequencySegmentation(): boolean {
  try {
    return localStorage.getItem(STORAGE_KEY) === 'frequency'
  } catch {
    return false
  }
}

export function saveFrequencySegmentation(enabled: boolean) {
  try {
    localStorage.setItem(STORAGE_KEY, enabled ? 'frequency' : 'greedy')
  } catch {
    // ignore storage errors
  }
}
