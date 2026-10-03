[sgcl](../../README.md) › [core](../README.md) › [slice](README.md)

# sgcl::slice\<T\>::owned

```cpp
bool owned() const noexcept;
```

Checks whether the slice has an owner: `owner() != nullptr`. An owned slice keeps its elements alive; one without
an owner promises what a `std::span` does, the memory valid for the call.

## Parameters

None.

## Return value

`true` when the slice holds a managed object, `false` for unmanaged memory.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <vector>

using namespace sgcl;

int main() {
    vector v = {1, 2};
    std::vector<int> sv = {1, 2};
    println("{} {}", slice<int>(v).owned(), slice<int>(sv).owned());
}
```

Output:

```text
true false
```

## See also

- [owner](owner.md): the managed object the elements lie in
- [sgcl::slice\<T\>](README.md)
