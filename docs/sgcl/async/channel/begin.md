[sgcl](../../README.md) › [async](../README.md) › [channel](README.md)

# sgcl::async::channel\<T\>::begin

```cpp
iterator begin() const;
```

Receives the first element, waiting for it as `receive().wait()` does, and returns an iterator that holds it: the
start of a range-for over the channel, Go's `for v := range ch`. Each `++` of the iterator receives the next element
the same way, and the iterator becomes the [end](end.md) iterator once the channel is closed and drained. The
iterator is an input iterator: `*it` is the element it holds, a `T&` that may be moved from.

`channel<void>` has no `begin`.

## Parameters

None.

## Return value

An iterator holding the first element, or the end iterator when the channel is closed and drained.

## Complexity

That of a receive: constant when the element is there, and a wait for it when not.

## Exceptions

- `std::system_error` when the receive wakes a waiting sender's task and the wake starts the scheduler's workers,
  one of which cannot be started.
- What the move constructor of `T` throws.

A move of `T` that throws loses the element being moved and leaves the channel otherwise as it was: its slot
of the buffer is free for the sends after it. A receive that waits throws what the move of its element into it
threw, made by the send that served it.

The same for every `++`.

## Notes

The range-for blocks the calling thread at every element, as `.wait()` does: it is a thread's loop, never a task's,
where it would hold a worker. A task writes `while (auto v = co_await ch.receive())` ([receive](receive.md)).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::channel<string> lines(4);
    thread producer([lines] {  // a copy of the handle: the same channel
        for (int i : range(1, 4)) {
            lines.send("line " + to_string(i)).wait();
        }
        lines.close();
    });

    for (string& line : lines) {  // until lines is closed and drained
        println("{}", line);
    }
    producer.join();
}
```

Output:

```text
line 1
line 2
line 3
```

## See also

- [end](end.md): the end iterator
- [receive](receive.md): one element, for a task or a thread
- [sgcl::async::channel\<T\>](README.md)
