[sgcl](../../README.md) › [core](../README.md) › [expiry_queue](../expiry_queue/README.md) › [entry](README.md)

# sgcl::expiry_queue\<T\>::entry::expired

```cpp
bool expired() const noexcept;
```

Checks whether a cycle has found the object unreachable and kept it for the queue: its function waits for
`drain()`, or ran, or the entry was cancelled after. It stays `true` from then on.

## Parameters

None.

## Return value

`true` once a cycle has found the object unreachable while the entry was pending; `false` before, for an entry
cancelled before that cycle, and for an empty handle.

## Complexity

Constant.

## Exceptions

None.

## Notes

`expired()` is not `weak().expired()`: the object of an expired entry is alive, kept for the queue, and its
`weak_ptr` locks it until the drain lets it go.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Texture {
    int id;
};

// Run on a thread of its own: no word of the object stays on main's stack
static expiry_queue<Texture>::entry watch_dropped(expiry_queue<Texture>& gone) {
    tracked_ptr texture = make_tracked<Texture>(1);
    return gone.watch(texture, [](tracked_ptr<Texture> t) { println("texture {} gone", t->id); });
}

int main() {
    expiry_queue<Texture> gone;
    expiry_queue<Texture>::entry entry;
    thread([&] { entry = watch_dropped(gone); }).join();
    println("{}", entry.expired());

    collector::force_collect(true);  // optional, for the demonstration
    println("{} {}", entry.expired(), entry.weak().expired());
    gone.drain();
    println("{}", entry.expired());
}
```

Output:

```text
false
true false
texture 1 gone
true
```

## See also

- [weak](weak.md): a weak pointer to the object
- [cancel](cancel.md): withdraws the entry
- [sgcl::expiry_queue\<T\>::entry](README.md)
