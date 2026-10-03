[sgcl](../../README.md) › [core](../README.md) › [runes](README.md)

# sgcl::runes::begin

```cpp
iterator begin() const noexcept;
```

Returns an [iterator](../runes-iterator/README.md) to the first code point of the text, decoded: at byte position 0. For an
empty text it equals [end](end.md).

## Parameters

None.

## Return value

An iterator to the first code point.

## Complexity

Constant: one code point decoded.

## Exceptions

None.

## Notes

The iterator holds a view of the text, not the range: it is valid while the text's object lives, so an iterator from
`s.runes().begin()` is valid while `s` is.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string s = "Ćma";
    auto first = s.runes().begin();
    println("U+{:04X} at {}, {} bytes", uint32_t(*first), first.pos(), first.width());
}
```

Output:

```text
U+0106 at 0, 2 bytes
```

## See also

- [end](end.md): the iterator past the last code point
- [sgcl::runes](README.md)
