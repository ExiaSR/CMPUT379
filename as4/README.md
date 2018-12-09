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

I first tested my implementation on Ubuntu 16.04 with `g++-5.5.0` running in VMWare, because school server is really laggy and `sftp` does not work well. Everything works fine. However, when I tested the program on school server, there are a lot of concurrency issue randomly showing up.

e.g,
```
monitor:
    [WAIT]
    [RUN] t1
    [IDLE] t2 t3 t4 t5
task: t2 (tid= 140171524425472, iter= 19, time= 4558 msec)
task: t3 (tid= 140171516032768, iter= 18, time= 4608 msec)
monitor:
    [WAIT] t2
    [RUN] t3
    [IDLE] t1 t4 t5
task: t1 (tid= 140171532818176, iter= 19, time= 4659 msec)
```

When `t3` should be able to run along with `t5`, my best guess is the lock preventing from printing and changing task state together is blocking the task execution. Properly compiler bug? or maybe different `libpthread.so` version? or even hardware issue.

## Testing and Results

We use the example from the assignment instruction for testing.

- Run `./a4tasks t1.dat 100 20`
- Observed in each monitor message, at least one of the following tasks should appear in `RUN` state at once to verify program concurrency. Unless it is the last iteratoin.
  - `t1` and `t4`
  - `t2` and `t5`
  - `t1` and `t3`
  - `t2` and `t4`
  - `t3` and `t5`

## Acknowledgments

- <https://en.cppreference.com/w/cpp/thread/lock_guard>
