[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::percent_set

```cpp
#include "sgcl/txt/percent.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class percent_set;
}
```

`sgcl::txt::percent_set` is which ASCII characters a [percent encoding](percent/encode.md) is to leave alone: a mask of
128 bits. It is a value rather than a tag because there is no one answer: RFC 3986 gives a different set for the
path, for the query and for the user information, and a program often has a set of its own. It is built from the
characters themselves, the way the RFC writes them — `percent_set{"!$&'()*+,;="}` is its sub-delims — and the sets
compose: `|` puts two together, `-` takes one from another. The sets of both families are constants of
[percent](percent.md).

## Rules

- A literal type of 16 bytes, `constexpr` throughout: a set of one's own is a constant.
- A byte above ASCII is never in a set: the escaping is over bytes, and a reader has no way to know what encoding
  they were, so such a byte is always encoded.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](percent_set/percent_set.md) | constructs an empty set, or the set of some characters |

#### Lookup

| Function | Description |
|---|---|
| [holds](percent_set/holds.md) | checks whether a character is in the set |

#### Operators

| Function | Description |
|---|---|
| [operator\|, operator-](percent_set/operator_arith.md) | the union and the difference of two sets |
| [operator==](percent_set/operator_cmp.md) | checks whether two sets hold the same characters |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

inline constexpr auto mine = txt::percent::unreserved | txt::percent_set{"/:"};

int main() {
    println("{}", txt::percent::encode("a b/c:d?", mine));
    println("{} {}", mine.holds('/'), mine.holds('?'));
}
```

Output:

```text
a%20b/c:d%3F
true false
```

## See also

- [percent](percent.md): the sets of RFC 3986 and of the WHATWG, `encode`, `decode`
- [sgcl::txt](README.md)
