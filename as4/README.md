# CMPUT 379 - Assignment 4

## Objectives

The objective of this assignment can provide hands-on experience in using Linux system API, such as, multithreaded programming, thread synchronization and deadlock prevention.

## Design Overview

- Each task has its own thread, the `worker` in my implementation is refering to each task.
- Two global mutexs to lock resources pool and task state.
- Two global objects to track resources and task state;
- Resources have two states, `BUSY` and `FREE`.
- Tasks have three states, `WAIT`, `RUN`, and `IDLE`.
- Lock resource pool before acquiring and releasing the resource in task.
- Lock task state before updating task state.
- Lock task state before printing task state from monitor thread.

## Project Status

All required features are implemented with error handling for most important functions, however, some edge cases may not be handled properly.

## Testing and Results

- Run `./a4tasks t1.dat 100 20`
- Observed in each monitor message, at least one of the following tasks should appear in `RUN` state at once to verify program concurrency. Unless it is the last iteratoin.
  - `t1` and `t4`
  - `t2` and `t5`
  - `t1` and `t3`
  - `t2` and `t4`
  - `t3` and `t5`

## Acknowledgments

- <https://en.cppreference.com/w/cpp/thread/lock_guard>
