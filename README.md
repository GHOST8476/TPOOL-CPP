# TPOOL
A C++ thread pool library.

Use GitWiki to know more.

## Major features of this library

### FixedThreadPool (ftpool)
Stack-based, non-resizable, non-movable thread pool. Tasks are evaluated based on their priority.

threads are allocated on the stack.

### FixedThreadPoolMovable (ftpool_mv)
Movable version of FixedThreadPool.

Data is allocated on the heap to keep thread pool movable without blocking the thread.

To keep move operations thread-safe pointer to data is atomic (can be disabled using tweaks).

### Worker (worker)
A single reusable thread. Tasks are evaluated based on their priority.

It is implemented using ftpool of 1 size. Thus inherits all the capabilities if ftpool.

### WorkerMovable (worker_mv)
Movable version of worker.

It is implemented using ftpool_mv of 1 size. Thus inherits all the capabilities if ftpool_mv.

### DynamicThreadPool (dtpool)
Heap-based, resizable, non-movable thread pool. Tasks are evaluated based on their priority.

Threads are allocated on the heap. Thread creation and desrtuction operations are asynchronous to avoid blocking the main the thread, but thread pool can be used with imidiate effect.

### DynamicThreadPoolMovable (dtpool_mv)
Movable version of DynamicThreadPool.

Data is allocated on the heap to keep thread pool movable without blocking the thread.

To keep move operations thread-safe pointer to data is atomic (can be disabled using tweaks).
