[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::yaml

```cpp
yaml() noexcept;                         // (1)
yaml(std::nullptr_t) noexcept;           // (2)
yaml(bool b) noexcept;                   // (3)
template<class I> yaml(I v) noexcept;    // (4)
yaml(double d) noexcept;                 // (5)
yaml(const string& s) noexcept;          // (6)
yaml(const char* s) noexcept;            // (7)
```

Constructs a scalar; a collection is made by [sequence](sequence.md) and [mapping](mapping.md), or read by
[parse](parse.md). None is `explicit`, so `mapping({{"port", 8080}})` takes the values as they are.

1. null; its [text](text.md) is empty.
2. null, of `nullptr`.
3. A boolean, `true` or `false`.
4. An integer of any integral type but `bool` and the characters (takes part only for those), in decimal.
5. A float in its shortest digits, `.inf`, `-.inf`, `.nan`; an integral one with `.0`, so it reads back as a float.
6. A string; its characters shared, not copied.
7. A string of the characters up to the null.

The copy and the move are the implicit ones and copy the handle.

## Parameters

| Parameter | Description |
|---|---|
| `b` | the boolean |
| `v` | the integer |
| `d` | the float |
| `s` | the string, UTF-8 |

## Complexity

Constant; (4), (5), (7) linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (encoding::yaml v : {encoding::yaml(), encoding::yaml(true), encoding::yaml(42), encoding::yaml(1.0),
                             encoding::yaml(0.1), encoding::yaml("text"), encoding::yaml("12")}) {
        print(v.to_string());
    }
}
```

Output:

```text
null
true
42
1.0
0.1
text
"12"
```

## See also

- [sequence](sequence.md), [mapping](mapping.md)
- [sgcl::encoding::yaml](README.md)
