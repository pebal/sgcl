[sgcl](../../README.md) › [txt](../README.md) › [sentences](../sentences.md)

# sgcl::txt::sentences::count

```cpp
size_type count() const noexcept;
```

Returns the number of sentences of the text. The number is walked and counted at each call, not stored.

## Parameters

None.

## Return value

The number of sentences.

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
    println("{}", txt::sentences("Pan J. Kowalski przyszedł. Usiadł.").count());
}
```

Output:

```text
3
```

## See also

- [empty](empty.md): whether there is none
- [sgcl::txt::sentences](../sentences.md)
