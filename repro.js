// Usage: UV_THREADPOOL_SIZE=1 node repro.js [threadpool|immediate|deferred]
//
// threadpool: calls FreeEnvironment() from an fs callback. The request of
//   the other Environment completed in the same thread pool batch, so
//   FreeEnvironment() never returns.
// immediate: calls FreeEnvironment() from a setImmediate() callback. This
//   returns.
// deferred: the same as threadpool, but the fs callback calls freeLater(),
//   which calls FreeEnvironment() after uv_run() returns. This returns.
'use strict'
const addon = require('./build/Release/repro.node')
const fs = require('node:fs')

const GUEST = "require('node:fs').stat('/', () => {})"
const mode = process.argv[2] ?? 'threadpool'

function busyWait(ms) {
  const end = Date.now() + ms
  while (Date.now() < end);
}

if (mode === 'threadpool' || mode === 'deferred') {
  // With one thread pool thread, this request completes first.
  fs.stat('/', () => {
    console.log('freeing from an fs callback')
    if (mode === 'deferred') {
      addon.freeLater(id, () => console.log('freed'))
      return
    }
    addon.free(id)
    console.log('freed')
  })
  // The new Environment starts a request that completes second.
  const id = addon.create(GUEST)
  // Wait until both requests are done, so that the event loop takes both
  // completions in one batch.
  busyWait(100)
} else {
  setImmediate(() => {
    const id = addon.create(GUEST)
    busyWait(100)
    console.log('freeing from a setImmediate() callback')
    addon.free(id)
    console.log('freed')
  })
}
