[sgcl](../../README.md) › [core](../README.md) › [expiry_queue](../expiry_queue.md) › [entry](../expiry_queue-entry.md)

# sgcl::expiry_queue\<T\>::entry::cancel

```cpp
bool cancel() noexcept;
```

Withdraws the entry: the object is no longer kept for the queue, its function will not be called, and the entry
leaves the queue with the next `drain()` (until then `size()` counts it). What `drain()` does to an entry after
calling its function, and `clear()` to every entry, `cancel()` does to one without the call: for a resource the
program released by hand, or an object another owner took over.

The answer is exact even against a `drain()` running on another thread: one atomic flag decides, and a `cancel()`
that sets it first, even between the drain's look at the entry and its call, is the one that returns `true`, and the
function is not called. It may be called from any thread.

## Parameters

None.

## Return value

`true` when the entry was still pending; `false` for an entry drained or cancelled before, or an empty handle.

## Complexity

Constant.

## Exceptions

None.

## Notes

The object of a cancelled entry is an ordinary weak target from then on: once a cycle finds it unreachable it dies,
and the `weak_ptr`s to it expire. An entry cancelled after its object was found unreachable lets the object go at
the next cycle that finds it so.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Texture {
    int id;
};

// Run on a thread of its own: no word of the objects stays on main's stack
static void watch_two(expiry_queue<Texture>& gone, expiry_queue<Texture>::entry& first,
                      expiry_queue<Texture>::entry& second) {
    tracked_ptr a = make_tracked<Texture>(1);
    tracked_ptr b = make_tracked<Texture>(2);
    first = gone.watch(a, [](tracked_ptr<Texture> t) { println("texture {} released", t->id); });
    second = gone.watch(b, [](tracked_ptr<Texture> t) { println("texture {} released", t->id); });
}

int main() {
    expiry_queue<Texture> gone;
    expiry_queue<Texture>::entry first;
    expiry_queue<Texture>::entry second;
    thread([&] { watch_two(gone, first, second); }).join();
    println("{}", second.cancel());
    println("{}", second.cancel());

    collector::force_collect(true);  // optional, for the demonstration
    println("{} {}", gone.size(), second.weak().expired());
    println("{} drained", gone.drain());
    println("{} {}", gone.size(), first.cancel());
}
```

Output:

```text
true
false
2 true
texture 1 released
1 drained
0 false
```

## See also

- [expired](expired.md): checks whether a cycle has found the object unreachable
- [clear](../expiry_queue/clear.md): drops every entry without calling its function
- [sgcl::expiry_queue\<T\>::entry](../expiry_queue-entry.md)
