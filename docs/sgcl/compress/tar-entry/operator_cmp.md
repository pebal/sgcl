[sgcl](../../README.md) › [compress](../README.md) › [tar](../tar.md) › [entry](README.md)

# sgcl::compress::tar::operator== (sgcl::compress::tar::entry)

```cpp
friend bool operator==(const entry& a, const entry& b) = default;
```

Compares two entries field by field, the pax records in their order. `!=` is made from it by the compiler. An entry
written and read back is equal to the one written, every field the archive holds being kept.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the entries compared |

## Return value

`true` when every field is equal.

## Complexity

Linear in the length of the names and the records.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    compress::tar::entry a{.name = "a.txt", .size = 5};
    compress::tar::entry b = a;
    println("{}", a == b);
    b.pax.push_back({string("comment"), string("checked")});
    println("{}", a == b);
}
```

Output:

```text
true
false
```

## See also

- [sgcl::compress::tar::entry](README.md)
