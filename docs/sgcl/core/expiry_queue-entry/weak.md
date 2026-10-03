[sgcl](../../README.md) › [core](../README.md) › [expiry_queue](../expiry_queue/README.md) › [entry](README.md)

# sgcl::expiry_queue\<T\>::entry::weak

```cpp
weak_type weak() const noexcept;
```

Returns a weak pointer to the object, sharing the entry's cell: an ordinary [weak_ptr](../weak_ptr/README.md) (`lock()`,
`expired()`), holding nothing. Until the drain, an object found unreachable is kept for the queue, and the weak
pointer locks it; after the drain it expires with the next cycle that finds the object unreachable. Dropping the
weak pointer, or the handle, cancels nothing.

## Parameters

None.

## Return value

A `weak_ptr<T>` to the object; an expired one for an empty handle.

## Complexity

Constant.

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

// Run on a thread of its own: no word of the object stays on main's stack
static weak_ptr<Texture> watch_dropped(expiry_queue<Texture>& gone) {
    tracked_ptr texture = make_tracked<Texture>(7);
    return gone.watch(texture, [](tracked_ptr<Texture>) {}).weak();
}

int main() {
    expiry_queue<Texture> gone;
    weak_ptr<Texture> weak;
    thread([&] { weak = watch_dropped(gone); }).join();
    collector::force_collect(true);  // optional, for the demonstration
    println("{}", weak.expired());  // found unreachable, kept for the queue
    thread([&] { println("{}", weak.lock()->id); }).join();  // the lock off main's stack too

    gone.drain();
    collector::force_collect(true);  // optional, as above
    println("{}", weak.expired());

    println("{}", expiry_queue<Texture>::entry().weak().expired());
}
```

Output:

```text
false
7
true
true
```

## See also

- [expired](expired.md): checks whether a cycle has found the object unreachable
- [weak_ptr](../weak_ptr/README.md): the weak pointer
- [sgcl::expiry_queue\<T\>::entry](README.md)
