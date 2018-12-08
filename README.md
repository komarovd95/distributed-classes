# Distributing computing

## Inter Process Communication. Distributed banking system.

Implemented by Komarov Dmitry

### Requirements

You need [CLang 3.5](https://pkgs.org/download/clang-3.5) installed on your OS 
(only Linux Mint was tested).

### How to build and run

Just
```
./run.sh X "Balance1 Balance2 ... BalanceX"
```
`X` is a number of child processes (see below).

`BalanceX` is an initial balance of Xth child process.

### Implementation details and requirements

This project implements simple distributed banking system using IPC library
implemented [previously](https://github.com/komarovd95/distributed-classes/tree/feature/lab0-pa1):
1. Main (parent, bank broker) process spawns `X` child processes (child, bank branch).
2. Each child broadcasts `STARTED` event.
3. Each child and process wait for all `STARTED` events.
4. Bank broker does series of transfers between bank branches. Transfer
   processing is:
   * send `TRANSFER` event to transfer source
   * wait for `ACK` event from transfer target
5. Transfer source (child) receives `TRANSFER` event and processes it:
   * decrement own balance by transfer amount
   * re-send `TRANSFER` event to transfer target
6. Transfer target (child) receives `TRANSFER` event and processes it:
   * increment own balance by transfer amount
   * send `ACK` event to bank broker
7. Sometimes between transfers bank broker calculates balance between
   bank branches:
   * bank broker broadcasts `SNAPSHOT_VTIME` with future time (local clock doesn't change)
   * every bank branch saves incoming time for balance snapshot
   * every bank branch responds with `SNAPSHOT_ACK` message (local clock doesn't change)
   * bank broker sends `EMPTY` messages to branches to gain up their local clocks
   * when balance snapshot is coming every bank branch sends `BALANCE_STATE` message to
     bank broker
8. After all processed transfers bank broker broadcasts `STOP` event.
9. Each child broadcasts `DONE` event (no useful work for simplicity).
10. Each child and parent process wait for all `DONE` events.
11. Parent waits until children are shutting down.

Implementation requirements:
* for child spawning `fork()` should be used.
* for message (event) transferring `pipe()` and non-blocking `read()` and `write`
  should be used.
* processes are fully connected: every process can read/write from/to every 
  other process.
* all unused pipes should be closed
* all processes must log events and pipes operations (creating and closing)
* all processes must be single-threaded
* [Vector Clock](https://en.wikipedia.org/wiki/Vector_clock)
  should be used for sending and receiving messages (no internal events)
* Transfer can be initiated only by bank broker
* Transfers can not be processed by bank branch after receiving `STOP` event

### Message format

Every message is represented by byte-buffer while transferred and has the 
following format:

| Byte position                          | Content                                                |
|----------------------------------------|--------------------------------------------------------|
| 0-1                                    | `MESSAGE_MAGIC` constant that used to validate message |
| 2-3                                    | One of the following types of message: `STARTED`,      |
|                                        | `TRANSFER`, `ACK`, `STOP`, `DONE`, `BALANCE_STATE`,    |
|                                        | `SNAPSHOT_VTIME`, `SNAPSHOT_ACK`, `EMPTY`              |
| 4-5                                    | Length of message payload (may be 0)                   |
| 5-(5 + 2 * ProcessesCount)             | Vector clock                                           |
| (5 + 2 * ProcessesCount)-PayloadLength | Message payload                                        |

### Project structure

#### Header files

`banking.h` contains definitions of functions and structures thar are needed to
implement banking system and scalar clocks (provided by teacher).

`common.h` contains log files names (provided by teacher)

`core.h` contains definitions of core functions and structures such as logging
and message constructing (provided by me).

`ipc.h` contains definitions of basic IPC functions and structures (provided by 
teacher).

`pa2345.h` contains event payload formats (provided by teacher).

`phases.h` contains definitions of processes' routine sub-functions aka phases 
(provided by me)

`pipes.h` contains definitions of functions that crate and close pipes (provided
by me)

`transfers.h` contains definitions of functions thar are needed to process
transfers (provided by me).

#### C files

All C files (except `bank_robbery.c`) are written by me.

`bank_robbery.c` contains toy implementation of transfers thar are initiated by
parent process (provided by teacher).

`ipc.c` contains implementation of IPC functions.

`lab.c` contains implementation of child and parent phases.

`main.c` contains spawning and waiting processes. Program entry point.

`pipes.c` contains implementation of pipes operations (create and close).

#### Bash Scripts

`dist.sh` is used to assemble tar archive with sources.

`run.sh` is used to [run program](#how-to-build-and-run).
