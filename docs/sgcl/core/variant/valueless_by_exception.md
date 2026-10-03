[sgcl](../../README.md) › [core](../README.md) › [variant](../variant.md)

# sgcl::variant\<Ts...\>::valueless_by_exception

```cpp
bool valueless_by_exception() const noexcept;
```

Checks whether the variant holds no alternative. A variant becomes valueless when the construction of a new
alternative throws after the old one was destroyed: in [emplace](emplace.md), and in an
[assignment](operator_assign.md) that constructs the new alternative in place; a copy or a move of a valueless
variant is valueless too. Its [index](index.md) is then `variant_npos`, [get](get.md) and [visit](visit.md) throw
`bad_variant_access`, and the next assignment or `emplace` gives it an alternative again.

## Parameters

None.

## Return value

`true` when the variant holds no alternative, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Faulty {
    Faulty(int) {
        throw runtime_error("no");
    }
};

int main() {
    variant<int, Faulty> v = 1;
    try {
        v.emplace<Faulty>(2);
    } catch (const runtime_error&) {
        println("{} {}", v.valueless_by_exception(), v.index() == variant_npos);
    }
    v = 3;
    println("{} {}", v.valueless_by_exception(), v.index());
}
```

Output:

```text
true true
false 0
```

## See also

- [index](index.md): `variant_npos` when valueless
- [sgcl::variant\<Ts...\>](../variant.md)
