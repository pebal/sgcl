[sgcl](../../README.md) › [encoding](../README.md) › [json](../json/README.md) › [builder](README.md)

# sgcl::encoding::json::builder::build

```cpp
json build() noexcept;
```

The value of what the builder holds: the array of its elements, or the object of its members, a key set twice
once with its last value. The builder is empty after, of no kind again, and may make the next value of either
kind. A builder to which nothing was added builds the empty array, `[]`.

## Parameters

None.

## Return value

The array or the object.

## Complexity

Linear in the number of elements or members.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json::builder b;
    println(b.build().to_string());

    b.push_back(1).push_back(2);
    encoding::json first = b.build();
    b.set("first", first);
    println(b.build().to_string());
    println(b.size());
}
```

Output:

```text
[]
{"first":[1,2]}
0
```

## See also

- [push_back](push_back.md), [set](set.md): what the value is made of
- [sgcl::encoding::json::builder](README.md)
