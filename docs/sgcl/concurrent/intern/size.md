[sgcl](../../README.md) › [concurrent](../README.md) › [intern](../intern.md)

# sgcl::concurrent::intern\<T, Hash, KeyEqual\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of entries: those of live objects, and the dead ones not yet swept.

## Parameters

None.

## Return value

The number of entries.

## Complexity

Constant: the sum of the table's stripes.

## Exceptions

None.

## Notes

The count is striped over cache lines, as the count of [concurrent::set](../set.md) is: under concurrent insertions
and sweeps it is a snapshot of no particular moment, exact once the other threads are quiet. The number of live
objects is the size right after a [sweep](sweep.md) with the other threads quiet.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::intern<string> tags;
    string a = tags.of("red");
    string b = tags.of("green");
    string c = tags.of("red");
    println("{}", tags.size());
}
```

Output:

```text
2
```

## See also

- [empty](empty.md): checks whether the pool holds an entry
- [sweep](sweep.md): drops the dead entries
- [sgcl::concurrent::intern\<T, Hash, KeyEqual\>](../intern.md)
