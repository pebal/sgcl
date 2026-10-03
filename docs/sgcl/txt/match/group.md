[sgcl](../../README.md) › [txt](../README.md) › [match](../match.md)

# sgcl::txt::match::group

```cpp
/*(1)*/ optional<slice<const char>> group(size_t n) const noexcept;
/*(2)*/ optional<slice<const char>> group(const string& name) const noexcept;
/*(3)*/ template<size_t N> optional<slice<const char>> group(const char (&name)[N]) const noexcept;
/*(4)*/ template<class P> requires std::same_as<P, const char*> || std::same_as<P, char*>
        optional<slice<const char>> group(P name) const noexcept;
```

Returns what a group matched, as a slice of the text. A group that took no part in the match is nothing, which an
empty group is not: in `(a)|(b)` over `"b"` group one took no part, and in `(a?)b` over `"b"` group one matched and
is empty.

1. The group of number `n`, counted from 1 by the opening parenthesis; `0` is the whole match, [text](text.md).
2. The group of that name, `(?<name> )` or `(?P<name> )`.
3. The name as an array of `char`, up to its first NUL or its end.
4. The name at a pointer, up to its NUL.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of the group; `0` for the whole match |
| `name` | the name of the group |

## Return value

The bytes the group matched, or an empty `optional` when the group took no part in the match, or the pattern has
no group of that number or name.

## Complexity

- (1) Constant.
- (2–4) Linear in the number of named groups.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::regex either("(a)|(b)");
    auto m = either.find("b");
    println("{} {}", m->group(1).has_value(), *m->group(2));

    auto e = txt::regex("(?<opt>a?)b").find("b");
    println("{} [{}]", e->group("opt").has_value(), *e->group("opt"));
    println("{}", e->group("other").has_value());
}
```

Output:

```text
false b
true []
false
```

## See also

- [operator[]](operator_at.md): a group that took no part reads as empty
- [group_count](group_count.md): the number of groups
- [regex::group_index](../regex/group_index.md): the number of a name
- [sgcl::txt::match](../match.md)
