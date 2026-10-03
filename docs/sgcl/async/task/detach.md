[sgcl](../../README.md) › [async](../README.md) › [task](README.md)

# sgcl::async::task\<T\>::detach

```cpp
void detach() noexcept;
```

Lets go of the task, for a task whose result nobody needs: it runs on, or stays wherever it waits, and destroys its
frame when it is done, the locals and parameters with it (a task it awaited, a `root_ptr` it held, released); the
memory is the collector's from then on. A task detached after it is done is destroyed at once. The task object is
empty after. [go](../go.md) is a spawn and a detach in one, and the destructor and [operator=](operator_assign.md)
detach a task that started.

A task detached before anyone started it never runs: its frame is left to the collector as it is, its locals never
destroyed. The task may not be empty.

## Parameters

None.

## Return value

None.

## Complexity

Constant; for a task that is done, the destructors of its locals and promise.

## Exceptions

None. An exception the task threw that nobody read goes to [on_unhandled](../on_unhandled.md)'s handler: here, on
the calling thread, for a task that is done, and on the thread that ends it otherwise.

## Notes

Letting go is not cancelling; a task stops early only through its [stop_token](../stop_token/README.md), which it looks at
itself. What follows from that:

- A task that nothing will wake again lives until what it waits for is closed or let go of itself, and is collected
  with it: a task left waiting on a channel that nobody sends to or closes, or on an event never set, holds its
  frame as long as the channel or the event lives, and goes with them.
- The task's side effects happen after it was let go of: what it sends, writes or changes after its wait, it still
  does.
- What the frame holds (its locals and parameters, a `root_ptr`, a connection or a file in it) lives until the task's
  end, not until the object's.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> logger(async::channel<string> lines, async::channel<int> count) {
    int n = 0;
    while (auto line = co_await lines.receive()) {
        println("log: {}", *line);
        ++n;
    }
    co_await count.send(n);
}

int main() {
    async::channel<string> lines;
    async::channel<int> count;
    auto t = async::spawn(logger(lines, count));
    t.detach();  // nobody waits for it; it runs on
    println("{}", t.done());

    (void)lines.send("one").wait();
    (void)lines.send("two").wait();
    lines.close();
    println("{} lines", *count.receive().wait());
}
```

Output:

```text
true
log: one
log: two
2 lines
```

## See also

- [go](../go.md): a spawn and a detach in one
- [destroy](destroy.md): destroys a task that never ran or is done
- [on_unhandled](../on_unhandled.md): what becomes of what a task let go of throws
- [sgcl::async::task\<T\>](README.md)
