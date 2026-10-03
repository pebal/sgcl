[sgcl](../../README.md) › [core](../README.md) › [expiry_queue](../expiry_queue.md)

# sgcl::expiry_queue\<T\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the queue holds no entry: `size() == 0`. An entry not drained yet counts, whether its object has been
found unreachable or not, and a cancelled one until the next drain.

## Parameters

None.

## Return value

`true` when the queue holds no entry, `false` otherwise.

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
    gone.watch(nullptr, [](tracked_ptr<Texture>) {});  // a null object gets no entry
    println("{}", gone.empty());

    tracked_ptr texture = make_tracked<Texture>(1);
    gone.watch(texture, [](tracked_ptr<Texture>) {});
    println("{}", gone.empty());

    gone.clear();
    println("{}", gone.empty());
}
```

Output:

```text
true
false
true
```

## See also

- [size](size.md): the number of entries
- [sgcl::expiry_queue\<T\>](../expiry_queue.md)
