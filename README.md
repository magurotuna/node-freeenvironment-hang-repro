# node-freeenvironment-hang-repro

`node::FreeEnvironment()` can hang when you call it from a callback of the
event loop that it runs.

```sh
npm install   # builds the addon with node-gyp
npm test      # runs repro.js in both modes, with a 10-second timeout
```

- `threadpool` mode frees a second Environment from an `fs.stat()` callback.
  It hangs.
- `immediate` mode frees it from a `setImmediate()` callback. It returns.
- `deferred` mode is the same as `threadpool` mode, but it frees the
  Environment from a platform task after `uv_run()` returns (`freeLater()` in
  `repro.cc`). It returns.

The results on Linux, macOS, and Windows are in the
[Actions tab](../../actions).

The code and this README were written with an AI coding agent (Claude Code).
