# fsv

A process supervisor (work in progress).

`fsv` is a standalone process supervisor that is able to:

- start a process, restarting it if it crashes
- optionally attach another process to reliably log messages even if the main process crashes
- maintain state information for all current `fsv`-managed processes

# inspiration

`fsv` is inspired by `runit`,
aiming to provide similar benefits without the need for service directories.

This will allow `fsv` to function as a drop-in process supervisor for
practically any init system.
