[sgcl](../../README.md) › [async](../README.md) › [channel](README.md)

# sgcl::async::channel\<T\>::begin

```cpp
iterator begin() const noexcept;
```

Returns an iterator at the start of a range-for over the channel, Go's `for v := range ch`. The iterator receives
lazily: an element is received, waiting for it as `receive().wait()` does, at the first look at it — `*it`, `it->`
or the comparison with [end](end.md) — and `++` only marks it used, so that the next look receives the next one. An
iterator left after its `++` has taken nothing more from the channel: a `break`, `std::views::take` and
`std::views::take_while` leave every element they did not look at to the other receivers. Once the channel is closed
and drained, the iterator compares equal to [end](end.md). The iterator is a `std::input_iterator`, move-only when
`T` is: `*it` is the element it holds, a `T&` that may be moved from, and the views of `std::ranges`
(`std::views::filter`, `std::views::transform`, `std::views::take`) take the channel as they take any input range.

`channel<void>` has no `begin`.

## Parameters

None.

## Return value

An iterator at the first element, which it has not received yet.

## Complexity

Constant. A look at an element costs a receive: constant when the element is there, and a wait for it when not.

## Exceptions

None. A look at an element (`*it`, `it->`, the comparison with the end) throws what a receive throws:

- `std::system_error` when the receive wakes a waiting sender's task and the wake starts the scheduler's workers,
  one of which cannot be started.
- What the move constructor of `T` throws.

A move of `T` that throws loses the element being moved and leaves the channel otherwise as it was: its slot
of the buffer is free for the sends after it. A receive that waits throws what the move of its element into it
threw, made by the send that served it.

## Notes

The range-for blocks the calling thread at every element, as `.wait()` does: it is a thread's loop, never a task's,
where it would hold a worker. A task writes `while (auto v = co_await ch.receive())` ([receive](receive.md)).

`std::views::filter` looks for the next element its predicate takes in its own `++`: a filter followed by a
`std::views::take` receives the elements up to the first match after the last one taken, and that match is lost
with the filter's iterator. A loop that stops early over a filter breaks inside the loop instead.

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

The views of the standard library over a channel, and what they leave in it:

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <ranges>

using namespace sgcl;

int main() {
    async::channel<int> numbers(8);
    for (int i : range(1, 7)) {
        numbers.send(i).wait();
    }
    numbers.close();

    auto square = [](int n) { return n * n; };
    for (int n : numbers | std::views::transform(square) | std::views::take(2)) {
        println("{}", n);
    }
    println("left: {}", numbers.size());  // 3 to 6, never looked at
}
```

Output:

```text
1
4
left: 4
```

## See also

- [end](end.md): the end of a range-for
- [receive](receive.md): one element, for a task or a thread
- [sgcl::async::channel\<T\>](README.md)
