# Distributing computing

## Inter Process Communication. Distributed mutual exclusion.

Implemented by Komarov Dmitry

### Requirements

You need [CLang 3.5](https://pkgs.org/download/clang-3.5) installed on your OS 
(only Linux Mint was tested).

### How to build and run

Just
```
./run.sh X [--mutexl]
```
`X` is a number of child processes (see below).

`mutexl` is an optional flag which indicates that critical sections are used.

### Implementation details and requirements

This project implements simple distributed mutual exclusion algorithm using
IPC library implemented [previously](https://github.com/komarovd95/distributed-classes/tree/feature/lab0-pa3):
1. Main (parent) process spawns `X` child processes (child).
2. Each child broadcasts `STARTED` event.
3. Each child and parent wait for all `STARTED` events.
4. Each child prints some information `5 * LocalPID` times using
   (if `mutexl` flag is provided) critical sections.
5. Each child broadcasts `DONE` event.
6. Each child and parent process wait for all `DONE` events.
7. Parent waits until children are shutting down.

Implementation requirements:
* for child spawning `fork()` should be used.
* for message (event) transferring `pipe()` and non-blocking `read()` and `write`
  should be used.
* processes are fully connected: every process can read/write from/to every 
  other process.
* all unused pipes should be closed
* all processes must log events and pipes operations (creating and closing)
* all processes must be single-threaded
* [Lamport's Scalar Clock](https://en.wikipedia.org/wiki/Lamport_timestamps)
  should be used for sending and receiving messages (no internal events)
* [Lamport's Distributed Mutual Exclusion Algorithm](https://en.wikipedia.org/wiki/Lamport%27s_distributed_mutual_exclusion_algorithm)
  should be used

#### Mutual exclusion algorithm

##### Entering CS

To request CS each child process broadcasts `CS_REQUEST` event. When such event
received it is enqueued into CS requests queue. This queue is prioritized by
request timestamp and sender's local PID.

When not an own CS request is at top of local queue `CS_REPLY` event should be
sent to sender of current top CS request.

Process can enter CS when 2 conditions are satisfied:
1. Process own CS request is at top of local queue.
2. All `CS_REPLY` events are received.

When this conditions are satisfied process is allowed to enter the CS.

##### Leaving CS

To leave CS process broadcasts `CS_RELEASE` event.

### Message format

Every message is represented by byte-buffer while transferred and has the 
following format:

| Byte position   | Content                                                |
|-----------------|--------------------------------------------------------|
| 0-1             | `MESSAGE_MAGIC` constant that used to validate message |
| 2-3             | One of the following types of message: `STARTED`,      |
|                 | `CS_REQUEST`, `CS_RELEASE`, `CS_REPLY`, `DONE`         |
| 4-5             | Length of message payload (may be 0)                   |
| 6-7             | Message Lamport's Scalar Clock timestamp               |
| 8-PayloadLength | Message payload                                        |

### Project structure

#### Header files

`banking.h` contains definitions of functions and structures thar are needed to 
implement scalar clocks (provided by teacher).

`common.h` contains log files names (provided by teacher)

`core.h` contains definitions of core functions and structures such as logging
and message constructing (provided by me).

`ipc.h` contains definitions of basic IPC functions and structures (provided by 
teacher).

`pa2345.h` contains event payload formats (provided by teacher).

`phases.h` contains definitions of processes' routine sub-functions aka phases 
(provided by me)

`pipes.h` contains definitions of functions that create and close pipes (provided
by me)

`queue.h` contains definitions of functions and structures that are needed to
implement CS requests priority queue (provided by me)

#### C files

All C files are written by me.

`ipc.c` contains implementation of IPC functions.

`lab.c` contains implementation of child and parent phases.

`main.c` contains spawning and waiting processes. Program entry point.

`pipes.c` contains implementation of pipes operations (create and close).

`queue.c` contains implementation of CS requests priority queue.

#### Bash Scripts

`dist.sh` is used to assemble tar archive with sources.

`run.sh` is used to [run program](#how-to-build-and-run).

#### Libraries

`libruntime.so` is library that used to print iteration information by child
process (provided by teacher).
