# waynav

## Architecture

The grid model and config parser MUST remain independent of Wayland state and
calls. `overlay.c` owns Wayland state; `input.c` is the boundary between parsed
commands, region mutations, and overlay actions.

### Config parser

- A binding whose first command is `start` MUST store the remaining chain in
  `config.start_commands`, not as a normal binding. `main.c` executes that chain
  once after overlay creation.
- Bindings use post-modifier xkb keysyms. Shifted bindings therefore require the
  shifted symbol, such as `shift+H` rather than `shift+h`.
- All command-keyword parsing MUST use `match_keyword()` and its returned
  argument pointer. Raw prefix checks and hard-coded argument offsets
  reintroduce prefix-collision bugs.

### Overlay

The layer output MUST remain unset, and the virtual pointer MUST bind to the
output reported by `wl_surface.enter`. Overlay and pointer coordinates MUST
remain logical. The normal input region MUST be empty; pointer capture MUST
restore it, and virtual warps MUST update the cached pointer position.

`xkb_mods_to_config()` MUST map depressed Shift, Ctrl, Alt, and Super state to
`MOD_*` before `config_find_binding()`; otherwise modified bindings silently
fail.

### Input dispatch

`execute_commands()` MUST release an active drag on `end`. After each command
chain, it MUST request a redraw and save history unless `history-back` ran.

## Linting and logging

`misc-include-cleaner` SHOULD remain disabled because it emits false positives
for Wayland-generated headers.

Runtime diagnostics MUST use `log_*`; direct stdio is reserved for CLI
help/version output and `log.c`. Info logs SHOULD be limited to lifecycle
events.

## Integration tests

`make int-test` SHOULD be run when Docker is available after overlay,
output-selection, or protocol changes.

Synthetic integration-test failure branches MUST call `tests:eval false`
immediately before `tests:assert-success`. The duplicated `:cleanup-process`
helper MUST retain one signature across both smoke scripts.
