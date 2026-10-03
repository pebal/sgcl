[sgcl](../../README.md) › [core](../README.md) › [expiry_queue](README.md)

# sgcl::expiry_queue\<T\>::expiry_queue

```cpp
expiry_queue() = default;                       // (1)
expiry_queue(expiry_queue&& other) noexcept;    // (2)
expiry_queue(const expiry_queue&) = delete;     // (3)
```

Constructs a queue.

1. An empty queue. It costs nothing beyond an empty `sgcl::vector`: nothing is allocated.
2. Takes the entries of `other` over, with the count of the automatic drain; `other` is left empty, as after
   `clear()`.
3. A queue is not copied: two queues would call one entry's function.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the queue whose entries are taken over |

## Complexity

Constant.

## Exceptions

None.

## Notes

The queue holds its entries by tracked pointers, so it lives where a `tracked_ptr` may: on a stack or inside a
managed object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

struct Texture {
    int id;
};

struct Cache {
    expiry_queue<Texture> evicted;  // inside a managed object
};

int main() {
    expiry_queue<Texture> local;  // on a stack
    tracked_ptr cache = make_tracked<Cache>();
    tracked_ptr texture = make_tracked<Texture>(1);
    local.watch(texture, [](tracked_ptr<Texture>) {});

    expiry_queue<Texture> moved = std::move(local);
    println("{} {} {}", moved.size(), local.size(), cache->evicted.empty());
    println("{}", std::is_copy_constructible_v<expiry_queue<Texture>>);
}
```

Output:

```text
1 0 true
false
```

## See also

- [operator=](operator_assign.md): takes the entries of another queue over
- [watch](watch.md): adds an entry
- [sgcl::expiry_queue\<T\>](README.md)
