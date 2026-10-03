[sgcl](../../README.md) › [core](../README.md) › [expiry_queue](../expiry_queue.md) › [entry](../expiry_queue-entry.md)

# sgcl::expiry_queue\<T\>::entry::entry

```cpp
entry() noexcept = default;
```

Constructs an empty handle, with no entry: what [watch()](../expiry_queue/watch.md) returns for a null object. A
handle of an entry is made by `watch()` alone. The copy constructor and the copy assignment are the implicit ones:
copies share the entry.

## Parameters

None.

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
    expiry_queue<Texture>::entry empty;
    println("{} {}", bool(empty), empty.weak().expired());

    expiry_queue<Texture> gone;
    tracked_ptr texture = make_tracked<Texture>(1);
    expiry_queue<Texture>::entry entry = gone.watch(texture, [](tracked_ptr<Texture>) {});
    expiry_queue<Texture>::entry copy = entry;  // the same entry
    println("{}", copy.cancel());
    println("{}", entry.cancel());
}
```

Output:

```text
false true
true
false
```

## See also

- [watch](../expiry_queue/watch.md): makes an entry and returns its handle
- [operator bool](operator_bool.md): checks whether the handle has an entry
- [sgcl::expiry_queue\<T\>::entry](../expiry_queue-entry.md)
