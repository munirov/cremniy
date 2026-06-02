# Cremniy — overview

## What it is

Cremniy is a development environment for low-level work: binaries, memory, system code.
Built as a single desktop application (Tauri + React), one process, no tool switching.

## Vision

One deep low-level workbench where everything is linked: byte ↔ instruction ↔ source line ↔
memory address ↔ live register. One model, a click in any view drills into all the others. Not
only static analysis of a dead file — also a live, running process in real time.

The web stack was picked so an AI can work in Cremniy the same way a human does — not through
buttons, but through commands that those same buttons call. See
[AGENT_CONTROL](../architecture/AGENT_CONTROL.md).

## Principles

- **All in one.** Everything needed for low-level work in one environment.
- **Minimal.** Nothing extra — UI and feature set both.
- **One model, many views.** Byte, instruction, source line, address, register — different views
  of the same thing. Navigation is bidirectional.
- **Static and live.** Inspect a file on disk; inspect a running process; same UI, same commands.
- **Capability first, button second.** Every UI capability is a named command in
  `window.cremniy`. Buttons call commands. Humans use buttons, scripts and AI use commands —
  same logic, same data.
