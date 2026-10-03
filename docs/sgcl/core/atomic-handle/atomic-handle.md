[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [handle](README.md)

# sgcl::atomic\<H\>::atomic

```cpp
atomic() noexcept(std::is_nothrow_default_constructible_v<H>) = default;    // (1)
atomic(const H& h) noexcept;                                                // (2)
atomic(const atomic&) = delete;                                             // (3)
```

Constructs the atomic.

1. The handle's own default, what `H()` makes: an empty string, a file that is none, an unbuffered channel.
2. The object `h` holds: the atomic and `h` share it.
3. An atomic is neither copyable nor movable.

The initialization is not an atomic operation, as with `std::atomic`.

## Parameters

| Parameter | Description |
|---|---|
| `h` | the handle the atomic starts with |

## Complexity

- (1) What the default constructor of `H` costs.
- (2) Constant.

## Exceptions

- (1) What the default constructor of `H` throws; none when it is noexcept.
- (2) None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    atomic<string> empty;
    string name("sgcl");
    atomic<string> shared(name);  // the same object as name
    atomic deduced = string("db");  // atomic<string>

    string loaded = shared.load();
    println("\"{}\" {} {}", empty.load(), deduced.load(),
            shared.compare_exchange_strong(name, loaded));  // name holds the object there
}
```

Output:

```text
"" db true
```

## See also

- [operator=](operator_assign.md), [store](store.md): replace the handle
- [sgcl::atomic\<H\>](README.md)
