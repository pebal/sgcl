[sgcl](../../README.md) › [encoding](../README.md) › [uuid](README.md)

# sgcl::encoding::uuid::hash

```cpp
size_t hash() const noexcept;
```

A hash of the sixteen bytes, what `std::hash<encoding::uuid>` gives: a `uuid` is a key of a
[map](../../core/map/README.md) or a [set](../../core/set/README.md).

## Parameters

None.

## Return value

The hash.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<encoding::uuid, string> users;
    encoding::uuid id = encoding::uuid::v7();
    users.insert_or_assign(id, "ala");
    println(users.at(id));
}
```

Output:

```text
ala
```

## See also

- [operator==, operator\\<=\\>](operator_cmp.md)
- [sgcl::encoding::uuid](README.md)
