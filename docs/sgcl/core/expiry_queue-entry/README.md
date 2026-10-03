[sgcl](../../README.md) › [core](../README.md) › [expiry_queue](../expiry_queue/README.md)

# sgcl::expiry_queue\<T\>::entry

```cpp
#include "sgcl/core/expiry_queue.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class expiry_queue {
    public:
        class entry;
    };
}
```

`sgcl::expiry_queue<T>::entry` is the handle of one entry of an [expiry_queue](../expiry_queue/README.md), what
[watch()](../expiry_queue/watch.md) returns. It shares the entry's cell, the weak cell `watch()` made for the object;
copies share the entry. The caller may keep it or discard it: dropping it cancels nothing.

[cancel()](cancel.md) withdraws the entry: the object is no longer kept for the queue, its
function will not be called, and the entry leaves the queue with the next `drain()`. What `drain()` does to an entry
after calling its function, and `clear()` to every entry, `cancel()` does to one without the call: for a resource
the program released by hand, or an object another owner took over. [expired()](expired.md)
tells whether a cycle has found the object unreachable, and [weak()](weak.md) is an ordinary
`weak_ptr` to the object, sharing the cell and holding nothing.

## Rules

- The handle holds its cell by a `tracked_ptr`, so it lives where the queue's pointers live: on a stack or inside a
  managed object.
- `cancel()` alone is one atomic flag and may come from any thread, against a `drain()` running on another: the
  answer is exact. The rest is shared between threads with the program's own synchronization, as the queue is.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](expiry_queue-entry.md) | constructs an empty handle |
| [cancel](cancel.md) | withdraws the entry |
| [expired](expired.md) | checks whether a cycle has found the object unreachable |
| [weak](weak.md) | a weak pointer to the object, sharing the entry's cell |
| [operator bool](operator_bool.md) | checks whether the handle has an entry |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Texture {
    int id;
};

static void release(int id) {
    println("texture {} released", id);
}

// Run on a thread of its own: no word of the objects stays on main's stack
static expiry_queue<Texture>::entry watch_two(expiry_queue<Texture>& gone) {
    tracked_ptr first = make_tracked<Texture>(1);
    tracked_ptr second = make_tracked<Texture>(2);
    gone.watch(first, [](tracked_ptr<Texture> t) { release(t->id); });
    auto entry = gone.watch(second, [](tracked_ptr<Texture> t) { release(t->id); });
    release(second->id);  // the program releases texture 2 itself
    return entry;
}

int main() {
    expiry_queue<Texture> gone;
    expiry_queue<Texture>::entry entry;
    thread([&] { entry = watch_two(gone); }).join();
    entry.cancel();  // no second release when nothing reaches texture 2

    collector::force_collect(true);  // optional, for the demonstration
    println("{} drained", gone.drain());
    println("{}", entry.weak().expired());  // texture 2 died with the cycle
}
```

Output:

```text
texture 2 released
texture 1 released
1 drained
true
```

## See also

- [expiry_queue](../expiry_queue/README.md): the queue, and [watch](../expiry_queue/watch.md), which makes an entry
- [weak_ptr](../weak_ptr/README.md): what `weak()` returns
- [sgcl::expiry_queue\<T\>](../expiry_queue/README.md)
