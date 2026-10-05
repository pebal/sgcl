[sgcl](../README.md) › async

# sgcl::async

```cpp
#include "sgcl/async.h"   // namespace sgcl::async
```

What Go has around goroutines: coroutine frames on the managed heap and the tasks over them, the pool of workers
that runs them, channels and `select`, time and cancellation, the composition and the synchronization of tasks,
the signals of the process and the readiness of a file descriptor. The module depends on
[core](../core/README.md) and [concurrent](../concurrent/README.md) (the channel's buffer is a ring of the one
and its lists of waiters are queues of the other); [io](../io/README.md) and [net](../net/README.md) are built on
it, the blocking pool under a file and the reactor under a pipe or a socket. The index of the whole interface is
[docs/sgcl/](../README.md).

The idea the module rests on is that everything that waits is a channel under another name, and so is waited for
the same three ways: a thread blocks, a task `co_await`s, a [select](select.md) takes it as a case. A
[task](task/README.md) is a C++20 coroutine whose frame is on the managed heap; [spawn](spawn.md) puts it on the queue of
the ready, a worker of the [scheduler](scheduler/README.md) runs it to its next `co_await`, and whatever it waited for
makes it ready again. A task that waits is nowhere: a frame on the managed heap and a word on a list, no thread
held, so a hundred thousand tasks waiting on a channel cost their frames and nothing else. This is the model of
Go with the two differences of C++: tasks are stackless, only the body of a coroutine can suspend and not a
function it calls, and cooperative, a task that computes for a second holds its worker for a second.

Around the tasks stand the time ([sleep](sleep.md), [after](after.md), [tick](tick.md), [every](every.md),
[timeout](timeout.md): one timer thread under them), the signals of the process as a channel
([signals](signals.md)), cancellation ([stop_source](stop_source/README.md) and [stop_token](stop_token/README.md), a token being
a channel closed by the stop), the
composition of tasks ([when_all](when_all.md), [when_any](when_any.md), [task_group](task_group/README.md),
[with_timeout](with_timeout.md)), their synchronization with each other and with threads ([mutex](mutex/README.md) and
its family, none of which parks a worker), and the world outside them: a completion set by a callback
([promise](promise/README.md)), a call that blocks run on threads apart from the workers
([spawn_blocking](spawn_blocking.md)), a thread of the program's choosing ([executor](executor/README.md),
[strand](strand/README.md)), a value a task passes to everything under it ([task_local](task_local/README.md)), and the
readiness of a descriptor ([readable](readable.md), [writable](writable.md), one thread on the kernel's queue
under them). A loop over a range is spread over the workers and the caller by [parallel_for](parallel_for.md),
[parallel_for_each](parallel_for_each.md) and [parallel_reduce](parallel_reduce.md): no waiting operations but
computations, they run the loop inside the call and return when it is done, on a thread and in a task alike, the
caller computing as one of the lanes. The module reads the time in one place, the core's
[clock::now()](../core/clock/README.md), so a test owns it with a [manual_clock](manual_clock/README.md) and a thirty-second
timeout takes microseconds.

## The rules

1. A task waits with `co_await`, never with a blocking call: a blocking call on a worker takes the worker from
   every other task. A call that has to block goes through [spawn_blocking](spawn_blocking.md).
2. The handles of the module (below) are tracked words and live where a `tracked_ptr` may; a task takes them by
   value. A `task` and a `generator` live anywhere.
3. Cancellation is only ever through a [stop_token](stop_token/README.md), which the task looks at itself: a started task
   whose object is dropped runs to its end ([task](task/README.md)).
4. What a task nobody waits for throws goes to the handler of [on_unhandled](on_unhandled.md), which by default
   prints it and ends the program, as a goroutine's panic ends a Go program.
5. What may wake a task (a send, a close, a set, a release, an unlock, a notify, a spawn) may start the
   scheduler's workers, and so stays potentially throwing: a thread that cannot be started is
   `std::system_error`. The operation is done all the same, and the task it woke or started is queued before the
   start, so it runs when the workers next start; a start that fails part-way leaves no worker running. A
   destructor that wakes (a guard's, a [task_group](task_group/README.md)'s) and the end of a task throw nothing and lose
   no task the same way. Out of memory is never thrown ([collector](../core/collector/README.md#the-memory-limit)).

### Waiting operations

Every operation of the module that may wait (a channel's `send` and `receive`, a semaphore's `acquire`, a condition
variable's `wait(guard)`, a mutex's `scoped_lock`) has one name and returns an [operation](operation/README.md): a
description of the operation, marked nodiscard, which does nothing until it is carried out in one of two ways. In
a task, `co_await` carries it out and gives the worker back while it waits; on a thread that is no coroutine
(`main`, a `thread`), `.wait()` carries it out and blocks the thread, going straight to the operation's own
blocking code, never through the scheduler. A `.wait()` inside a task would hold a worker, and so is never written
there. A debug build asserts on an operation made and never carried out. [select](select.md) returns an object of
its own, nodiscard too, carried out the same two ways.

An event, a wait group and a task group are waited for as a task is: `wait()`, which returns nothing, blocks a
thread, and a task writes `co_await` on the object itself. A task, a promise and a job of the blocking pool are
waited for themselves too, and the wait gives the result, as an operation's does (`co_await t` in a task,
`t.wait()` on a thread, the way `std::this_thread::sync_wait` gives a sender's); `t.result()` reads the result of
one that is done. The Lockable members the standard names (`lock`, `try_lock`, `unlock`, `lock_shared`...) stay
blocking, for `std::lock_guard` and `std::shared_lock`.

The modules built on this one (io, net, encoding, hash) do not return an operation: each of their operations has
two names, `read(...)`, which does the work on the calling thread, and `async_read(...)`, which returns a `task`
for a task to `co_await`.

### Handles

A [channel](channel/README.md), an [event](event/README.md), a [mutex](mutex/README.md), a [wait_group](wait_group/README.md) and a
[promise](promise/README.md) are handles: one word, a tracked word to a state on the managed heap, made by the
constructor and shared by the copies, `==` when they are the same. A task takes them by value, and its copy keeps
the state for as long as it runs, so a function may start a task with its channels and return; a reference
parameter would leave the task a reference into a frame that is gone. What the reactor and the timers give is one
of them too: [readable](readable.md), [after](after.md) and [at](at.md) an event, [tick](tick.md) and
[signals](signals.md) a channel, and [stop_token::channel](stop_token/channel.md) a
[receive_channel](receive_channel/README.md), a channel's receiving end alone. A move of a handle copies its word: the
handle moved from stands for the same object still. A [stop_token](stop_token/README.md) and a
[stop_source](stop_source/README.md) are tracked words too, under the same rule of where they live; a default-constructed
`stop_token` is empty.

A handle is a tracked word: it lives on a stack, in a task, in a managed object; in a global or a `std` container
it is held by a [rooted](../core/rooted/README.md), the same object reached with `->`. A root is never in a managed
object or a task's frame, since a root is never part of a cycle. The objects of a scope stay objects:
[task_group](task_group/README.md) (its end stops its children), [semaphore](semaphore/README.md), [once](once/README.md),
[shared_mutex](shared_mutex/README.md), [condition_variable](condition_variable/README.md), [broadcast](broadcast/README.md), and the
guards of the mutexes.

### Threads

Any thread may create and copy managed pointers; the library registers a thread's stack the first time the thread
copies one, and forgets it when the thread exits, with a handshake so that a scan in progress finishes reading the
stack first. The collector runs on a thread of its own, started with the first managed object, and on a pool of
helpers that park between cycles; destructors of collected objects run on those threads, so a destructor must be
prepared to run on a thread other than the one that created the object, and must not touch the dying peers of its
object ([The rules](../core/README.md#the-rules), 5). Nothing a mutator does waits for a cycle, and nothing a cycle
does waits for a mutator: a thread blocked in a system call, spinning, or descheduled holds up no one; a thread that
exits mid-scan waits for the scan of its own stack only.

A buffer that another thread will read or write after the call returns (the
[blocking pool](spawn_blocking.md), a queue of the platform's completions) is always owned by a managed object; a
buffer used only inside the task's own frame may be unowned. The library keeps this itself: io copies a slice
without an owner through a managed block on the way to the pool ([buffers](../io/README.md#buffers)). A job of
`spawn_blocking` of your own captures what it touches by value, or through a handle or a `tracked_ptr`, and never
as a view into the frame of the task that waits for it.

A `fork()` gives the child a copy-on-write snapshot of the managed heap and none of the threads: the child may read
managed objects and `exec` or exit (the page headers live outside the pages, so the parent's marking does not copy
the snapshot page by page), but the collector does not run in it, and a managed allocation that needs a page or a
call into the collector terminates the child with a message rather than hang on a copied lock. A child that needs
the collector is a `fork` before the first managed object, or an `exec`.

### Coroutines

The frame of a C++20 coroutine is allocated with `operator new`: memory the collector does not see, where a
`tracked_ptr` breaks rule 1 of the core ([The rules](../core/README.md#the-rules)). A promise type that derives
from the core's [managed_frame](../core/managed_frame.md) gets its frames from the managed heap instead, traced
conservatively, so whatever the coroutine holds is a root while its frame is held; the frame is held through a
[frame_ptr](../core/frame_ptr/README.md), which the promise's `get_return_object` makes from the handle. The core's
[generator](../core/generator/README.md) runs where it is called; this module's [task](task/README.md) runs on the scheduler,
and keeps its executor, its task-locals and the link of an executor's queue in the four words the core leaves in
front of every managed frame; its [generator](generator/README.md) may wait between the values it yields.

- A `task` or a `generator` (a `frame_ptr`) lives anywhere, a `std::vector<async::task<>>` included, at the cost
  of a cell per handle. The coroutine is destroyed when its `frame_ptr` is, which runs the destructors of its
  locals and promise on the calling thread, and the frame's memory goes to the collector once nothing holds it, so
  no `coroutine_handle` may outlive the `frame_ptr`. A waiting coroutine is held by what it waits on.
- A task is lazy: nothing of its body runs until the first of `spawn`, `wait()`, `co_await`, `result()` or
  `resume()`.
- A promise that does not derive from `managed_frame` lives in `operator new` memory with the rest of the frame, so
  neither it nor the coroutine's parameters and locals may hold a `tracked_ptr`. A `std::coroutine_handle` keeps
  nothing alive: a coroutine is destroyed through its `frame_ptr`, never through the handle.
- A thread waits for a task with `wait()`, a task with `co_await`. A task nobody waits for is started with
  [go](go.md), a spawn whose handle nobody keeps (`spawn` is nodiscard, so that a handle is kept or `go` says it is
  not), and destroys its frame itself when it is done.
- A send to a waiting task is a push on the scheduler's queue; a worker is woken through the kernel only when the
  whole pool sleeps.
- The cost of a task is the allocation of its frame as a managed buffer (a few tens of nanoseconds instead of
  `malloc`) and, per cycle, a conservative pass over the frame's words; a frame that holds no managed pointers
  costs the collector the same pass and nothing else.
- The scheduler is one per process, started by the first `spawn`, stopped when the program ends or by
  [scheduler::stop](scheduler/stop.md); the number of workers is
  [scheduler::set_workers](scheduler/set_workers.md), else `SGCL_WORKERS` in the environment, else
  `config::workers` (`-DSGCL_WORKERS`), one per core by default. Every setting of the module (the workers, their
  spin, the threads of the blocking pool) is the program's call first, then the environment (`SGCL_WORKERS`,
  `SGCL_WORKER_SPIN_US`, `SGCL_BLOCKING_THREADS`, read once), then the build's `config`; a value of the
  environment that does not read is ignored with one line on stderr. A stop of the scheduler stops the timer
  thread, the reactor and the blocking pool before it joins the workers.
- The end of the program stops the runtime once, at the first of its objects destroyed (the scheduler, the
  reactor, the timers and the blocking pool are statics): the pool runs the jobs queued and leaves, the workers run
  what is ready until it suspends and leave, then the timer thread and the reactor stop, the reactor waking none
  of its waits. A task suspended then stays suspended, as a goroutine does when `main` returns, and nothing starts
  the runtime again. In a static destructor after that, a task spawned never runs and a thread that waits for it
  waits forever; a task that waits for I/O, a timer or a blocking job stays suspended; on a thread, a wait for I/O
  ends cancelled, a wait on a timer ends at once (`sleep(d).wait()` returns without sleeping) and a blocking job runs
  on the thread itself.

### Time

The module reads the time through the core's [clock::now()](../core/clock/README.md), which a test owns with a
[manual_clock](manual_clock/README.md): installed, it serves every timer of the module, the deadline of
[with_timeout](with_timeout.md) and a thread's `sleep(d).wait()` included, but not `this_thread::sleep_for`, which
is the operating system's. The timers ([sleep](sleep.md), [sleep_until](sleep_until.md), [after](after.md),
[at](at.md), [tick](tick.md), [timeout](timeout.md), a [stop_source](stop_source/README.md)'s deadline) are served by one
thread, started with the first timer and stopped with the scheduler; a timer fires at its time or a little after,
never before, and holds the frame it will resume or the channel it will signal ([sleep](sleep.md#notes)).

## Functions

| Function | Header | Description |
|---|---|---|
| [after](after.md) | `timer.h` | an event set after a while |
| [at](at.md) | `timer.h` | an event set at a point of the clock |
| [cancel_waits](cancel_waits.md) | `reactor.h` | ends the waits on a descriptor with nothing, before it is closed |
| [every](every.md) | `every.h` | a function called every period until a stop |
| [exited](exited.md) | `reactor.h` | an event set when a child process ends |
| [go](go.md) | `coroutine.h`, `executor.h` | starts a task nobody waits for |
| [go_blocking](go_blocking.md) | `blocking.h` | runs a blocking call nobody waits for on the blocking pool |
| [ignore_signals](ignore_signals.md) | `signal.h` | the signals ignored by the process |
| [on](on.md) | `executor.h` | moves a task to an executor or a strand (`co_await on(ex)`) |
| [on_unhandled](on_unhandled.md) | `coroutine.h` | the handler of what a task nobody waits for throws |
| [on_workers](on_workers.md) | `executor.h` | moves a task back to the pool of workers |
| [otherwise](otherwise.md) | `select.h` | the case of a select taken when no other is ready: a poll |
| [parallel_for](parallel_for.md) | `parallel.h` | calls a function for every index of a range on the workers and the caller, and returns when it is done |
| [parallel_for_each](parallel_for_each.md) | `parallel.h` | calls a function for every element of a random-access range on the workers and the caller, and returns when it is done |
| [parallel_reduce](parallel_reduce.md) | `parallel.h` | combines a value of every index of a range in the order of the indices, on the workers and the caller, and returns the result |
| [readable](readable.md) | `reactor.h` | an event set when a descriptor can be read |
| [reset_signals](reset_signals.md) | `signal.h` | gives signals back the disposition they had before `signals` |
| [run](run.md) | `run.h` | the entry of a program: a task waited for on this thread, its token stopped by SIGINT or SIGTERM |
| [select](select.md) | `select.h` | waits on several cases at once and runs the body of the first ready |
| [signals](signals.md) | `signal.h` | the signals of the process as a channel |
| [sleep](sleep.md) | `timer.h` | a task suspended, or a thread blocked, for a while |
| [sleep_until](sleep_until.md) | `timer.h` | a task suspended, or a thread blocked, until a point of the clock |
| [spawn](spawn.md) | `coroutine.h`, `executor.h` | starts a task on the scheduler or an executor and returns its handle |
| [spawn_blocking](spawn_blocking.md) | `blocking.h` | runs a blocking call on the blocking pool |
| [tick](tick.md) | `timer.h` | a channel signalled every period |
| [timeout](timeout.md) | `timer.h` | the case of a select taken after a while |
| [when_all](when_all.md) | `when.h` | waits for every task and gives every result |
| [when_any](when_any.md) | `when.h` | waits for the first task to finish and gives its index |
| [with_deadline](with_deadline.md) | `timeout.h` | the result of a task, or `timed_out` past a point, or `stopped` |
| [with_timeout](with_timeout.md) | `timeout.h` | the result of a task, or `timed_out` after a while |
| [writable](writable.md) | `reactor.h` | an event set when a descriptor can be written |
| [yield](yield.md) | `scheduler.h` | the task to the back of the queue of the ready |

## Classes

| Class | Header | Description |
|---|---|---|
| [blocking_pool](blocking_pool/README.md) | `blocking.h` | the pool of threads apart from the workers: its statistics, its size and its stop |
| [blocking_task\<T\>](blocking_task/README.md) | `blocking.h` | a job of the blocking pool, waited for as a task is |
| [broadcast\<T\>](broadcast/README.md) | `broadcast.h` | a channel every subscription receives every value from |
| [channel\<T\>](channel/README.md) | `channel.h` | the channel of Go: buffered or rendezvous, closed to end the stream |
| [condition_variable](condition_variable/README.md) | `condition_variable.h` | Go's `sync.Cond` over the module's mutex |
| [event](event/README.md) | `event.h` | set once, waited for by any number |
| [executor](executor/README.md) | `executor.h` | a queue of tasks one thread of the program's choosing runs |
| [generator\<T\>](generator/README.md) | `generator.h` | a generator that may wait between the values it yields |
| [manual_clock](manual_clock/README.md) | `timer.h` | the clock of a test: the time moves only by `advance` |
| [mutex](mutex/README.md) | `mutex.h` | one holder at a time; a task waiting holds no thread |
| [once](once/README.md) | `once.h` | the first caller runs it, the others wait for it |
| [operation\<F\>](operation/README.md) | `operation.h` | a waiting operation, carried out by `co_await` or `wait()` |
| [parallel_options](parallel_options.md) | `parallel.h` | how a parallel loop is spread: its lanes and its grain |
| [promise\<T\>](promise/README.md) | `promise.h` | a completion set once by any thread or callback, awaited by a task |
| [receive_channel\<T\>](receive_channel/README.md) | `channel.h` | a channel seen from its receiving end: receives, no send, no close |
| [scheduler](scheduler/README.md) | `scheduler.h` | the pool of workers that runs the tasks: its size, its statistics, its stop |
| [semaphore](semaphore/README.md) | `semaphore.h` | a number of permits |
| [shared_mutex](shared_mutex/README.md) | `shared_mutex.h` | any number of readers or one writer |
| [stop_source](stop_source/README.md) | `stop_token.h` | requests the stop: at once, after a while, at a point, with a parent |
| [stop_token](stop_token/README.md) | `stop_token.h` | the stop seen by a task: a channel closed by the stop |
| [stopped](stopped/README.md) | `timeout.h` | the error of a task stopped through its token before it ended |
| [strand](strand/README.md) | `executor.h` | tasks on the workers one at a time, in order |
| [task\<T\>](task/README.md) | `coroutine.h` | a coroutine run on the scheduler, waited for by a task or a thread |
| [task_group](task_group/README.md) | `task_group.h` | a scope that owns the tasks it spawns, waited for and stopped as one |
| [task_local\<T\>](task_local/README.md) | `task_local.h` | a value a task sets and every function and task under it reads |
| [timed_out](timed_out/README.md) | `timeout.h` | the error of a task that did not end in time |
| [wait_group](wait_group/README.md) | `wait_group.h` | counts work down to zero, Go's `WaitGroup` |

## See also

- [Benchmarks](benchmarks.md): a hop, a wait and a race on the scheduler against Go and Java
- [managed_frame](../core/managed_frame.md), [frame_ptr](../core/frame_ptr/README.md), [generator](../core/generator/README.md):
  the coroutine frames of the core
- [clock](../core/clock/README.md): the time the module reads
- [concurrent](../concurrent/README.md): the structures under the channel
- [io](../io/README.md), [net](../net/README.md): the modules built on this one
- [The modules](../README.md)
