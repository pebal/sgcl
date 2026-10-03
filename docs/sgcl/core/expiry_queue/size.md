[sgcl](../../README.md) › [core](../README.md) › [expiry_queue](README.md)

# sgcl::expiry_queue\<T\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of entries not drained yet, whether their objects have been found unreachable or not. An entry
leaves the queue only through `drain()` (when its object was found unreachable, or the entry was cancelled) or
`clear()`: a cancelled entry counts until the next drain.

## Parameters

None.

## Return value

The number of entries.

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
    tracked_ptr a = make_tracked<Texture>(1);
    tracked_ptr b = make_tracked<Texture>(2);
    gone.watch(a, [](tracked_ptr<Texture>) {});
    auto entry = gone.watch(b, [](tracked_ptr<Texture>) {});
    println("{}", gone.size());  // both alive: both still in the queue

    entry.cancel();
    println("{}", gone.size());
    gone.drain();
    println("{}", gone.size());
}
```

Output:

```text
2
2
1
```

## See also

- [empty](empty.md): checks whether the queue holds no entry
- [drain](drain.md): drops the entries whose objects were found unreachable, and the cancelled ones
- [sgcl::expiry_queue\<T\>](README.md)
