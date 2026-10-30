import type { SVGProps } from 'react'

export function HamburgerIcon(props: SVGProps<SVGSVGElement>) {
  return (
    <svg width="16" height="16" viewBox="0 0 16 16" fill="none" stroke="currentColor" {...props}>
      <line x1="2.5" y1="4" x2="13.5" y2="4" strokeWidth="1.6" strokeLinecap="round" />
      <line x1="2.5" y1="8" x2="13.5" y2="8" strokeWidth="1.6" strokeLinecap="round" />
      <line x1="2.5" y1="12" x2="13.5" y2="12" strokeWidth="1.6" strokeLinecap="round" />
    </svg>
  )
}

export function CloseIcon(props: SVGProps<SVGSVGElement>) {
  return (
    <svg width="16" height="16" viewBox="0 0 16 16" fill="none" stroke="currentColor" {...props}>
      <line x1="3.5" y1="3.5" x2="12.5" y2="12.5" strokeWidth="1.8" strokeLinecap="round" />
      <line x1="12.5" y1="3.5" x2="3.5" y2="12.5" strokeWidth="1.8" strokeLinecap="round" />
    </svg>
  )
}

export function SearchIcon(props: SVGProps<SVGSVGElement>) {
  return (
    <svg width="15" height="15" viewBox="0 0 16 16" fill="none" stroke="currentColor" {...props}>
      <circle cx="6.5" cy="6.5" r="4.5" strokeWidth="1.6" />
      <line x1="10" y1="10" x2="14" y2="14" strokeWidth="1.6" strokeLinecap="round" />
    </svg>
  )
}

export function ChevronLeftIcon(props: SVGProps<SVGSVGElement>) {
  return (
    <svg width="16" height="16" viewBox="0 0 16 16" fill="none" stroke="currentColor" {...props}>
      <path d="M10 3.5L5.5 8L10 12.5" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round" />
    </svg>
  )
}

export function ChevronRightIcon(props: SVGProps<SVGSVGElement>) {
  return (
    <svg width="16" height="16" viewBox="0 0 16 16" fill="none" stroke="currentColor" {...props}>
      <path d="M6 3.5L10.5 8L6 12.5" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round" />
    </svg>
  )
}

