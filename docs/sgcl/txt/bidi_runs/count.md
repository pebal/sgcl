[sgcl](../../README.md) › [txt](../README.md) › [bidi_runs](README.md)

# sgcl::txt::bidi_runs::count

```cpp
size_type count() const noexcept;
```

Returns the number of pieces, held since the construction.

## Parameters

None.

## Return value

The number of pieces.

## Complexity

Constant.

## Exceptions

None.

## Notes

`count()` counts every piece; `count_of(pred)` of [mixin::enumerable](../../core/mixin/enumerable/README.md) counts those a
predicate accepts.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::bidi_runs pieces("Nazwa: שלום 123 OK");
    println("{} pieces, {} right to left", pieces.count(),
            pieces.count_of([](const txt::bidi_runs::run& r) { return r.right_to_left(); }));
}
```

Output:

```text
4 pieces, 1 right to left
```

## See also

- [empty](empty.md): whether there is no piece
- [sgcl::txt::bidi_runs](README.md)
