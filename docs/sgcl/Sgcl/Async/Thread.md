# Thread

```cpp
#include "sgcl/Sgcl/Async/Thread.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    class Thread;
    struct ThisThread;   // the calling thread: GetId, Yield, SleepFor, SleepUntil
}
```

The same class in the `sgcl` interface: [thread](../../async/thread.md).

`Thread` is `std::thread` under the interface's names, and `ThisThread` is `std::this_thread` (`GetId`, `Yield`, `SleepFor`, `SleepUntil`), so that a program written against the interface names one style for what it starts and what it shares: the containers, the atomics and the threads that use them. Nothing is added: the constructors, `Join`, `Detach`, `IsJoinable`, `GetId`, `HardwareConcurrency`, `Swap` and the destructor (a thread still joinable when destroyed ends the program) are the standard's; `Inner()` is the `std::thread` itself.

## Rules

- A thread is a thread to the collector: its stack is scanned, the tracked pointers on it are roots, from the first line of the function it runs ([README: The rules](../../core/README.md#the-rules), 1).
- The closure of the function a thread runs is copied to unmanaged memory, so a `Ptr` is not captured by value: it is captured by reference, to a frame that outlives the thread (a `main` that joins it), or handed over through a [`RootPtr`](../Core/RootPtr.md), which lives anywhere.
- The threads of a program, kept until joined, live in a container that holds them where it may: `List<Thread>` on a stack ([List](../Containers/List.md)).

## Members

```cpp
using InnerType = std::thread;
using Id = std::thread::id;

Thread() noexcept;                                        // no thread
template<class F, class... A> explicit Thread(F&& f, A&&... a);   // starts f(a...)
Thread(Thread&&) noexcept;  Thread& operator=(Thread&&) noexcept;  // not copyable
~Thread();                                                // joinable: std::terminate

void Join();                                              // waits until the thread ends
void Detach();                                            // lets it run on its own: no longer joinable
bool IsJoinable() const noexcept;
Id GetId() const noexcept;
void Swap(Thread&) noexcept;
static unsigned HardwareConcurrency() noexcept;
InnerType& Inner() noexcept;  const InnerType& Inner() const noexcept;
```

The free function, in `Sgcl`: `void swap(Thread&, Thread&) noexcept`.

### ThisThread

```cpp
static Thread::Id GetId() noexcept;
static void Yield() noexcept;
template<class Rep, class Period> static void SleepFor(const std::chrono::duration<Rep, Period>& d);
template<class Clock, class Duration> static void SleepUntil(const std::chrono::time_point<Clock, Duration>& t);
```

The calling thread, as `std::this_thread` has it: `ThisThread::SleepFor(10ms)` parks the thread (a task sleeps with [`Sleep`](Time.md), holding no thread).

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// Four threads count the elements of a shared set; the set stays in
// main's frame, referenced from each closure, and is joined before it
// is gone.
int main() {
    ConcurrentHashSet<int> seen;
    List<Thread> workers;
    for (int t : Range(4)) {
        workers.Emplace([&seen, t] {                   // by reference: main's frame outlives the threads it joins
            for (int i : Range(1000)) {
                seen.Add(t * 1000 + i);
            }
        });
    }
    for (auto& w : workers) {
        w.Join();
    }
    std::cout << seen.Count() << "\n";                 // 4000
    return seen.Count() == 4000 ? 0 : 1;
}
```

The output:

```
4000
```

## See also

- [RootPtr](../Core/RootPtr.md): a pointer handed to a thread that outlives the frame; [Atomic](../Concurrent/Atomic.md): a `Ptr` shared between threads; [List](../Containers/List.md): where the threads are kept
- [README: The rules](../../core/README.md#the-rules)
