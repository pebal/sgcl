[sgcl](../../README.md) › [concurrent](../README.md) › [intern](../intern.md)

# sgcl::concurrent::intern\<T, Hash, KeyEqual\>::clear

```cpp
void clear() noexcept;
```

Forgets every entry there is at the time of the walk, the live ones included. The objects live on where they are
held, and the next [of](of.md) of their values makes new ones.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of entries.

## Exceptions

None.

## Notes

Under concurrent insertions `clear` erases what it reaches: an entry inserted while it runs may be erased or left.
Every erasure of it is lock-free. A handle taken before the `clear` and one taken after it for the same value are
different objects: the identity of a value holds only within what the pool remembers.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::intern<string> tags;
    string before = tags.of("red");

    tags.clear();
    string after = tags.of("red");
    println("{} {}", tags.size(), before.object() == after.object());
    println("{} {}", before, after);
}
```

Output:

```text
1 false
red red
```

## See also

- [sweep](sweep.md): drops only the dead entries
- [sgcl::concurrent::intern\<T, Hash, KeyEqual\>](../intern.md)
