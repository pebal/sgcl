[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](README.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::runes

```cpp
sgcl::runes runes() const noexcept requires (sizeof(CharT) == 1);
```

Returns the code points of a UTF-8 text, decoded as they are walked: Go's `for i, r := range s`. A
[runes](../../utf8/README.md) is a forward range of `char32_t` over a slice of the text, taken from the class's
`as_slice()`, which holds the text's object for as long as the range lives: `for (char32_t c :
string("żółw").runes())` is safe. An invalid byte is one code point, the replacement character U+FFFD.

The iterator knows the byte position of its code point, `pos()`, and its width in bytes, `width()`, for the code
that goes back to the bytes. The range is a range of the library, [mixin::enumerable](../enumerable/README.md):
`s.runes().contains(U'ż')`, `s.runes().count_of(unicode::is_upper)`, `s.runes().find_if(...)`.

Takes part only when `CharT` is one byte: in a wide text a unit is a character already, and the text is walked as
it is.

## Parameters

None.

## Return value

The code points of the text, as a range that holds the text.

## Complexity

Constant; a walk of the range is linear in the size of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string s = "żółw 😀";
    for (char32_t c : s.runes()) {
        print("{:x} ", uint32_t(c));
    }
    println("");

    auto runes = s.runes();
    for (auto it = runes.begin(); it != runes.end(); ++it) {
        print("{}:{} ", it.pos(), it.width());
    }
    println("");

    println("{} {}", s.runes().count_of(unicode::is_lower), s.runes().contains(U'ł'));
}
```

Output:

```text
17c f3 142 77 20 1f600 
0:2 2:2 4:2 6:1 7:1 8:4 
4 true
```

## See also

- [rune_count](rune_count.md): the number of code points
- [decode](decode.md): the code point at a byte position
- [utf8, unicode, runes](../../utf8/README.md): the encoding and the range
- [sgcl::mixin::text\<Derived, CharT, Traits\>](README.md)
