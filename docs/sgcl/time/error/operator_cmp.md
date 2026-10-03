[sgcl](../../README.md) › [time](../README.md) › [error](../error.md)

# sgcl::time::operator== (sgcl::time::error)

```cpp
friend bool operator==(const error& a, const error& b) noexcept;
```

Compares two errors by what they say: the same sentence at the same byte. `!=` is made from it by the compiler.
An error compared with a fixed one tells a refusal the program expects from the others.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the errors compared |

## Return value

`true` when the offsets are equal and the sentences have the same characters.

## Complexity

Linear in the length of the sentences.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    auto a = time::date::parse("2026-13-01");
    auto b = time::date::parse("2025-13-31");
    println("{}", a.error() == b.error());
    println("{}", a.error() == time::error("a month from 01 to 12 expected", 5));
    println("{}", a.error() == time::error("a month from 01 to 12 expected"));
}
```

Output:

```text
true
true
false
```

## See also

- [message](message.md), [offset](offset.md): what is compared
- [sgcl::time::error](../error.md)
