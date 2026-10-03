[sgcl](../../README.md) › [txt](../README.md) › [word_breaks](../word_breaks.md)

# sgcl::txt::word_breaks::count

```cpp
size_type count() const noexcept;
```

Returns the number of segments of the text. The number is walked and counted at each call, not stored.

## Parameters

None.

## Return value

The number of segments.

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
    string s = "Ala, ma kota.";
    println("{} segments", txt::word_breaks(s).count());
}
```

Output:

```text
7 segments
```

## See also

- [empty](empty.md): whether there is none
- [sgcl::txt::word_breaks](../word_breaks.md)
