[sgcl](../../README.md) › [core](../README.md) › [expiry_queue](../expiry_queue.md) › [entry](../expiry_queue-entry.md)

# sgcl::expiry_queue\<T\>::entry::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle has an entry: `true` for a handle `watch()` returned for an object, `false` for one it
returned for a null object and for a default-constructed one. A drained or cancelled entry is still the handle's.

## Parameters

None.

## Return value

`true` when the handle has an entry, `false` otherwise.

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

int main() {
    expiry_queue<Texture> gone;
    tracked_ptr texture = make_tracked<Texture>(1);
    auto entry = gone.watch(texture, [](tracked_ptr<Texture>) {});
    auto none = gone.watch(nullptr, [](tracked_ptr<Texture>) {});
    println("{} {}", bool(entry), bool(none));

    entry.cancel();
    println("{}", bool(entry));
}
```

Output:

```text
true false
true
```

## See also

- [(constructor)](expiry_queue-entry.md): constructs an empty handle
- [sgcl::expiry_queue\<T\>::entry](../expiry_queue-entry.md)
