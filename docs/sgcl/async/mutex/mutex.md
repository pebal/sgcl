[sgcl](../../README.md) › [async](../README.md) › [mutex](../mutex.md)

# sgcl::async::mutex::mutex

```cpp
mutex() noexcept;                          // (1)
mutex(const mutex&) noexcept = default;    // (2)
mutex(mutex&&) noexcept = default;         // (3)
```

1. A new mutex, unlocked: its state, a channel of one signal with the signal in it, made on the managed heap.
2. A handle of the same mutex: the copy shares the state, and a lock through either is a lock of both.
3. The same, taken from the other handle.

There is no empty mutex: every handle stands for one.

## Parameters

| Parameter | Description |
|---|---|
| `const mutex&`, `mutex&&` | the handle of the mutex to share |

## Complexity

- (1) Constant: one allocation.
- (2–3) Constant: one tracked word copied.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::mutex m;
    async::mutex same = m;  // the same mutex
    async::mutex other;
    same.lock();
    println("{} {}", m.try_lock(), other.try_lock());
    same.unlock();
    other.unlock();
    println("{}", m.try_lock());
    m.unlock();
}
```

Output:

```text
false true
true
```

## See also

- [operator=](operator_assign.md): makes the handle one of another mutex
- [operator==](operator_cmp.md): whether two handles are the same mutex
- [sgcl::async::mutex](../mutex.md)
