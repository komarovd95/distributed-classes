# Distributing computing

## Inter Process Communication

Implemented by Komarov Dmitry

### Requirements

You need [CLang 3.5](https://pkgs.org/download/clang-3.5) installed on your OS 
(only Linux Mint was tested).

### How to build and run

Just
```
./run.sh X
```
`X` is a number of child processes (see below).

### Implementation details and requirements

This project implements simple IPC protocol:
1. Main (parent) process spawns `X` child processes (child).
2. Each child broadcasts `STARTED` event.
3. Each child and parent wait for all `STARTED` events.
4. Each child broadcasts `DONE` event (no useful work for simplicity).
5. Each child and parent wait for all `DONE` events.
6. Parent waits until children are shutting down.

Implementation requirements:
* for child spawning `fork()` should be used.
* for message (event) transferring `pipe()` and blocking `read()` and `write` 
  should be used.
* processes are fully connected: every process can read/write from/to every 
  other process.
* all unused pipes should be closed
* all processes must log events and pipes operations (creating and closing)
* all processes must be single-threaded

### Message format

Every message is represented by byte-buffer while transferred and has the 
following format:

| Byte position   | Content                                                |
|-----------------|--------------------------------------------------------|
| 0-1             | `MESSAGE_MAGIC` constant that used to validate message |
| 2-3             | Type of message: only `STARTED` and `DONE` are used    |
| 4-5             | Length of message payload (may be 0)                   |
| 6-7             | Always 0                                               |
| 8-PayloadLength | Message payload                                        |

### Project structure

#### Header files 

`common.h` contains log files names (provided by teacher)

`core.h` contains definitions of core functions and structures such as logging
and message constructing (provided by me).

`ipc.h` contains definitions of basic IPC functions and structures (provided by 
teacher).

`pa1.h` contains event payload formats (provided by teacher).

`phases.h` contains definitions of processes' routine sub-functions aka phases 
(provided by me)

`pipes.h` contains definitions of functions that crate and close pipes (provided
by me)

#### C files

All C files are written by me.

`ipc.c` contains implementation of IPC functions.

`lab.c` contains implementation of child and parent phases.

`main.c` contains spawning and waiting processes. Program entry point.

`pipes.c` contains implementation of pipes operations (create and close).

#### Bash Scripts

`dist.sh` is used to assemble tar archive with sources.

`run.sh` is used to [run program](#how-to-build-and-run).
