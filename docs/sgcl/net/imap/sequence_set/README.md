[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md)

# sgcl::net::imap::sequence_set

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    class sequence_set;
    inline constexpr uint32_t last = 0;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

A set of message numbers or UIDs, IMAP's sequence-set (RFC 9051 §9): ranges written `1:4,7,10:*`, kept in the order
given, `*` (`net::imap::last`) the largest number in use — the last message, the highest UID. The client's methods on
messages take one as UIDs, made from a number (`session.fetch(7)`), a range, the list of UIDs a
[search](../client/search.md) gave, or the text. A value: a copy is the same set.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](sequence_set.md) | an empty set, a number, a range, a list of numbers, or the text |
| [parse](parse.md) | the text read, or nothing (static) |
| [all](all.md) | `1:*` (static) |
| [saved](saved.md) | `$`, the result a search saved (static) |
| [add](add.md) | a number or a range added |
| [contains](contains.md) | whether a number is in the set |
| [empty](empty.md) | whether the set is empty |
| [is_saved](is_saved.md) | whether it is `$` |
| [expand](expand.md) | the numbers, `*` given its value |
| [ranges](ranges.md) | the ranges as pairs |
| [to_string](to_string.md) | the text |
| [operator==](operator_cmp.md) | the same ranges |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    net::imap::sequence_set s(sgcl::vector<uint32_t>{9, 1, 2, 3, 7});
    println("{}", s.to_string());
    s.add(20, net::imap::last);
    println("{} {}", s.to_string(), s.contains(25, 30));
}
```

Output:

```text
1:3,7,9
1:3,7,9,20:* true
```

## See also

- [fetch](../client/fetch.md), [store](../client/store.md), [copy](../client/copy.md): what take one
- [criteria::uid](../criteria/uid.md): a set in a search
