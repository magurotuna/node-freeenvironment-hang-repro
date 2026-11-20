// Runs repro.js in each mode in a child process, and reports whether the
// process ends within 10 seconds.
'use strict'
const { spawnSync } = require('node:child_process')
const fs = require('node:fs')
const path = require('node:path')

const TIMEOUT_MS = 10_000
const rows = []
let failed = false

console.log(`Node.js ${process.version}, ${process.platform}-${process.arch}`)
for (const mode of ['threadpool', 'immediate']) {
  const result = spawnSync(
    process.execPath,
    [path.join(__dirname, 'repro.js'), mode],
    {
      env: { ...process.env, UV_THREADPOOL_SIZE: '1' },
      encoding: 'utf8',
      timeout: TIMEOUT_MS,
    },
  )
  let outcome
  if (result.error?.code === 'ETIMEDOUT') {
    outcome = `hung (stopped after ${TIMEOUT_MS / 1000} s)`
  } else if (result.status === 0) {
    outcome = 'returned'
  } else {
    outcome = `failed (exit code ${result.status}, signal ${result.signal})`
    failed = true
  }
  console.log(`\n${mode}: ${outcome}`)
  if (result.stdout) console.log(result.stdout.trimEnd())
  if (result.stderr) console.error(result.stderr.trimEnd())
  rows.push(`| ${mode} | ${outcome} | ${result.stdout.trim().split('\n').join('<br>')} |`)
}

if (process.env.GITHUB_STEP_SUMMARY) {
  fs.appendFileSync(
    process.env.GITHUB_STEP_SUMMARY,
    [
      `### Node.js ${process.version}, ${process.platform}-${process.arch}`,
      '',
      '| mode | result | output |',
      '| --- | --- | --- |',
      ...rows,
      '',
    ].join('\n'),
  )
}
process.exitCode = failed ? 1 : 0
