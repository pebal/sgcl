[sgcl](../../README.md) › [core](../README.md) › [expiry_queue](README.md)

# sgcl::expiry_queue\<T\>::operator=

```cpp
expiry_queue& operator=(expiry_queue&& other) noexcept;    // (1)
expiry_queue& operator=(const expiry_queue&) = delete;     // (2)
```

1. Drops the entries this queue held, as `clear()` does: without a call, and the objects they kept are no longer
   kept, whatever handles or weak pointers of theirs live on; then takes the entries of `other` over, with the count
   of the automatic drain. `other` is left empty, as after `clear()`. An assignment to itself does nothing.
2. A queue is not copied: two queues would call one entry's function.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the queue whose entries are taken over |

## Return value

`*this`.

## Complexity

Linear in the number of entries this queue held.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Texture {
    int id;
};

// Run on a thread of its own: no word of the objects stays on main's stack
static weak_ptr<Texture> watch_two(expiry_queue<Texture>& a, expiry_queue<Texture>& b) {
    tracked_ptr first = make_tracked<Texture>(1);
    tracked_ptr second = make_tracked<Texture>(2);
    auto entry = a.watch(first, [](tracked_ptr<Texture> t) { println("texture {}", t->id); });
    b.watch(second, [](tracked_ptr<Texture> t) { println("texture {}", t->id); });
    return entry.weak();
}

int main() {
    expiry_queue<Texture> a;
    expiry_queue<Texture> b;
    weak_ptr<Texture> dropped;
    thread([&] { dropped = watch_two(a, b); }).join();
    a = std::move(b);  // texture 1's entry dropped, texture 2's taken over
    println("{} {}", a.size(), b.size());

    collector::force_collect(true);  // optional, for the demonstration
    println("{}", dropped.expired());
    println("{} drained", a.drain());
}
```

Output:

```text
1 0
true
texture 2
1 drained
```

## See also

- [clear](clear.md): drops every entry without calling its function
- [(constructor)](expiry_queue.md): takes the entries of another queue over
- [sgcl::expiry_queue\<T\>](README.md)
