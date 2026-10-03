[sgcl](../../README.md) › [txt](../README.md) › [graphemes](../graphemes.md)

# sgcl::txt::graphemes::count

```cpp
size_type count() const noexcept;
```

Returns the number of grapheme clusters of the text. The number is walked and counted at each call, not stored.

## Parameters

None.

## Return value

The number of grapheme clusters.

## Complexity

Linear in the bytes of the text.

## Exceptions

None.

## Notes

`count()` counts every element; `count_of(pred)` of [mixin::enumerable](../../core/mixin/enumerable.md) counts those a
predicate accepts.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string s = "e\u0301\U0001F1F5\U0001F1F1";
    txt::graphemes all(s);
    println("{} bytes, {} code points, {} characters", s.size(), s.rune_count(), all.count());
    auto wide = [](const slice<const char>& g) { return g.size() > 1; };
    println("{} of more than a byte", all.count_of(wide));
}
```

Output:

```text
11 bytes, 4 code points, 2 characters
2 of more than a byte
```

## See also

- [empty](empty.md): whether there is none
- [sgcl::txt::graphemes](../graphemes.md)
