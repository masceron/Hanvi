// Offers text, or a binary file, to the user as a downloaded file.
export function download(fileName: string, data: string | Uint8Array<ArrayBuffer>) {
  const type = typeof data === 'string' ? 'text/plain;charset=utf-8' : 'application/octet-stream'
  const url = URL.createObjectURL(new Blob([data], { type }))
  const a = document.createElement('a')
  a.href = url
  a.download = fileName
  a.click()
  // Some browsers (Safari) start reading a large blob only after click() returns.
  setTimeout(() => URL.revokeObjectURL(url), 60_000)
}
