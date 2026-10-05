[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::operator&&, operator||, operator! (sgcl::net::imap::criteria)

```cpp
friend criteria operator&&(const criteria& a, const criteria& b) noexcept;    // (1)
friend criteria operator||(const criteria& a, const criteria& b) noexcept;    // (2)
friend criteria operator!(const criteria& a) noexcept;                        // (3)
```

1. The messages both take: the keys of `a` and of `b` one after the other (every message is no key: `a && all()`
   is `a`).
2. The messages either takes: `OR a b`.
3. The messages `a` does not take: `NOT a`.
- (2, 3) A combination given is parenthesized: `!(a && b)` is `NOT (a b)`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the criteria |

## Return value

The criteria.

## Complexity

Linear in the size of the keys.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    using net::imap::criteria;
    println("{}", (criteria::seen() && criteria::flagged()).to_string());
    println("{}", (criteria::seen() || criteria::flagged()).to_string());
    println("{}", (!(criteria::seen() && criteria::flagged())).to_string());
}
```

Output:

```text
SEEN FLAGGED
OR SEEN FLAGGED
NOT (SEEN FLAGGED)
```

## See also

- [sgcl::net::imap::criteria](README.md)
