[sgcl](../../README.md) › [txt](../README.md) › [percent_set](../percent_set.md)

# sgcl::txt::percent_set::operator|, operator-

```cpp
constexpr percent_set operator|(const percent_set& other) const noexcept;    // (1)
constexpr percent_set operator-(const percent_set& other) const noexcept;    // (2)
```

1. The union: the characters of either set. RFC 3986 builds its sets up this way: `path` is
   `segment | percent_set{"/"}`.
2. The difference: the characters of this set that are not in `other`. The WHATWG URL Standard builds its sets down
   this way — "the query set and also the question mark" — so `whatwg::path` is
   `whatwg::query - percent_set{"?^`{}"}`.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the other set |

## Return value

The new set.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto set = txt::percent::unreserved | txt::percent_set{"/"};
    println("{}", txt::percent::encode("a/b c", set));
    println("{}", txt::percent::encode("a/b c", set - txt::percent_set{"a"}));
}
```

Output:

```text
a/b%20c
%61/b%20c
```

## See also

- [sgcl::txt::percent_set](../percent_set.md)
