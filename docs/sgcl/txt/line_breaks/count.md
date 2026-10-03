[sgcl](../../README.md) › [txt](../README.md) › [line_breaks](../line_breaks.md)

# sgcl::txt::line_breaks::count

```cpp
size_type count() const noexcept;
```

Returns the number of pieces of the text. The number is walked and counted at each call, not stored.

## Parameters

None.

## Return value

The number of pieces.

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
    println("{} pieces", txt::line_breaks("Zażółć gęślą jaźń — a potem 漢字").count());
}
```

Output:

```text
8 pieces
```

## See also

- [empty](empty.md): whether there is none
- [sgcl::txt::line_breaks](../line_breaks.md)
