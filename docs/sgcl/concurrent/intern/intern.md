[sgcl](../../README.md) › [concurrent](../README.md) › [intern](../intern.md)

# sgcl::concurrent::intern\<T, Hash, KeyEqual\>::intern

```cpp
intern();                          // (1)
intern(const intern&) = delete;    // (2)
```

1. An empty pool of its own: for the values of one subsystem, or when the lifetime of the default pool,
   [pool](pool.md), the program's, is too long. `Hash` and `KeyEqual` are default-constructed.
2. The pool is not copyable, and not movable: a structure shared by threads has one place.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

## Notes

The pool lives where a `tracked_ptr` may: on a thread's stack, or inside a managed object, where the threads that
share it reach it. Its entries hold the objects weakly, so a pool that dies takes no object with it: the objects
live on where they are held.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

struct Symbols {
    concurrent::intern<string> names;  // a member of a managed object
};

int main() {
    concurrent::intern<int> numbers;  // on the stack
    tracked_ptr symbols = make_tracked<Symbols>();

    println("{} {}", numbers.empty(), symbols->names.empty());
    println("{}", std::is_copy_constructible_v<concurrent::intern<int>>);
}
```

Output:

```text
true true
false
```

## See also

- [of](of.md): the canonical object of a value
- [pool](pool.md): the default pool of the type
- [sgcl::concurrent::intern\<T, Hash, KeyEqual\>](../intern.md)
