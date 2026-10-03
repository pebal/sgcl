[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](../text.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::rune_count

```cpp
size_type rune_count() const noexcept;
```

Returns the number of code points of the text: in a UTF-8 text `utf8::count` of its bytes, each invalid byte one
code point, Go's `utf8.RuneCountInString`; in a UTF-16 text its units less one for every surrogate pair, a surrogate
without its pair one code point; in a 32-bit text the number of units, `size()`.

## Parameters

None.

## Return value

The number of code points.

## Complexity

Linear in the size of a UTF-8 or UTF-16 text, a run of ASCII in UTF-8 counted eight bytes at a time; constant in a
32-bit one.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string s = "żółw 😀";
    println("{} {}", s.size(), s.rune_count());

    string bad = "a\xff" "b";  // an invalid byte between two letters
    println("{} {}", bad.size(), bad.rune_count());

    wstring w = L"żółw";
    println("{} {}", w.size(), w.rune_count());

    u16string u = u"żółw 😀";  // the emoji a surrogate pair
    println("{} {}", u.size(), u.rune_count());
}
```

Output:

```text
12 6
3 3
4 4
7 6
```

## See also

- [length](length.md): the number of units
- [runes](runes.md): the code points, walked
- [sgcl::mixin::text\<Derived, CharT, Traits\>](../text.md)
