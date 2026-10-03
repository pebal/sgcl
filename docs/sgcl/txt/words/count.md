[sgcl](../../README.md) › [txt](../README.md) › [words](README.md)

# sgcl::txt::words::count

```cpp
size_type count() const noexcept;
```

Returns the number of words of the text. The number is walked and counted at each call, not stored.

## Parameters

None.

## Return value

The number of words.

## Complexity

Linear in the bytes of the text.

## Exceptions

None.

## Notes

`count()` counts every element; `count_of(pred)` of [mixin::enumerable](../../core/mixin/enumerable/README.md) counts those a
predicate accepts.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string s = "Litwo! Ojczyzno moja! ty jesteś jak zdrowie.";
    println("{} words", txt::words(s).count());
    println("{} capitalised", txt::words(s).count_of([](const slice<const char>& w) {
        return unicode::is_upper(w.decode(0).first);
    }));
}
```

Output:

```text
7 words
2 capitalised
```

## See also

- [empty](empty.md): whether there is none
- [sgcl::txt::words](README.md)
