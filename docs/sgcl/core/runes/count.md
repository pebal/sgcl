[sgcl](../../README.md) › [core](../README.md) › [runes](../runes.md)

# sgcl::runes::count

```cpp
size_type count() const noexcept;
```

Returns the number of code points of the text, an invalid byte counting as one: [utf8::count](../utf8/count.md) over
the text. The number is walked and counted at each call, not stored.

## Parameters

None.

## Return value

The number of code points.

## Complexity

Linear in the bytes of the text, a run of ASCII eight bytes at a time.

## Exceptions

None.

## Notes

`count()` counts every code point; `count_of(pred)` of [mixin::enumerable](../mixin/enumerable.md) counts those a
predicate accepts.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string s = "Żółw 🐢";
    runes r = s.runes();
    println("{} code points, {} upper, {} bytes", r.count(), r.count_of(unicode::is_upper), s.size());
}
```

Output:

```text
6 code points, 1 upper, 12 bytes
```

## See also

- [utf8::count](../utf8/count.md): the count over bytes
- [sgcl::runes](../runes.md)
