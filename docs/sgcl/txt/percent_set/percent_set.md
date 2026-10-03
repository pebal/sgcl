[sgcl](../../README.md) › [txt](../README.md) › [percent_set](README.md)

# sgcl::txt::percent_set::percent_set

```cpp
constexpr percent_set() noexcept = default;                    // (1)
constexpr explicit percent_set(const char* chars) noexcept;    // (2)
```

Constructs a set.

1. The empty set: every character is escaped.
2. The set of the characters of `chars`, to its NUL. A byte above ASCII in it is ignored; a null pointer is the empty
   set.

## Parameters

| Parameter | Description |
|---|---|
| `chars` | the characters, as the RFC writes them |

## Complexity

Linear in the length of `chars`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    constexpr txt::percent_set none;
    constexpr txt::percent_set digits{"0123456789"};
    println("{}", txt::percent::encode("a1", none));
    println("{}", txt::percent::encode("a1", digits));
}
```

Output:

```text
%61%31
%611
```

## See also

- [holds](holds.md)
- [sgcl::txt::percent_set](README.md)
