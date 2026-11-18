import { useEffect, useState } from 'react'
import type { Token } from './core/types'
import type { Role } from './components/editTarget'

export interface FlatToken {
  pIdx: number
  tIdx: number
  token: Token
}

export interface TokenSelection {
  startIndex: number
  endIndex: number
  anchorIndex: number
  activeRole: Role
}

export function isMobileBrowser(): boolean {
  if (typeof window === 'undefined') return false

  // Query parameter override for debugging and manual testing on desktop:
  // e.g., http://localhost:5173/?mobile=1
  if (new URLSearchParams(window.location.search).get('mobile') === '1') return true

  // Modern Client Hints if supported by the browser
  // @ts-expect-error navigator.userAgentData is experimental
  if (navigator.userAgentData?.mobile) return true

  const ua = navigator.userAgent || ''
  // Common mobile User-Agent patterns
  if (/Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(ua)) return true

  // iPadOS on Safari reports as Macintosh with multi-touch
  if (/Macintosh/i.test(ua) && navigator.maxTouchPoints > 1) return true

  // Coarse pointer with touch device
  if (typeof window.matchMedia === 'function') {
    if (window.matchMedia('(pointer: coarse)').matches && navigator.maxTouchPoints > 0) {
      return true
    }
  }

  return false
}

export function useIsMobileBrowser(): boolean {
  const [isMobile, setIsMobile] = useState(isMobileBrowser)

  useEffect(() => {
    function handleResize() {
      setIsMobile(isMobileBrowser())
    }
    window.addEventListener('resize', handleResize)
    return () => window.removeEventListener('resize', handleResize)
  }, [])

  return isMobile
}
