# TPOOL
A C++ thread pool library.

Use GitWiki to know more.

## Major features of this library

### FixedThreadPool (ftpool)
Stack-based, non-resizable, non-movable thread pool. Tasks are evaluated based on their priority.

threads are allocated on the stack.

### Worker (worker)
A single reusable thread. Tasks are evaluated based on their priority.

It is implemented using ftpool of 1 size. Thus inherits all the capabilities if ftpool.

### DynamicThreadPool (dtpool)
Heap-based, resizable, non-movable thread pool. Tasks are evaluated based on their priority.

Threads are allocated on the heap. Thread creation and destruction operations are asynchronous to avoid blocking the main the thread, but thread pool can be used with immediate effect.