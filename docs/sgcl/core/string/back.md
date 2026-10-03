[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::string::back

```cpp
const CharT& back() const noexcept;
```

Returns the last character, read only: the one before the terminator. The string must not be empty: `back()` of the
empty string is undefined behaviour, as in `std`; a debug build asserts it.

## Parameters

None.

## Return value

A reference to the last character, valid while some string holds the object.

## Complexity

Constant.

## Exceptions

None.

## Notes

In a `string` the last character is the last byte: the last byte of the last code point, which outside ASCII is not
the letter itself. `ends_with` takes the letter as a `char32_t` ([mixin::text](../mixin/text/README.md)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "/usr/lib/";
    println("{} {}", dir.back(), dir.back() == '/');

    string city = "Łódź";
    println("{} {}", city.back() == 'z', city.ends_with(U'ź'));
}
```

Output:

```text
/ true
false true
```

## See also

- [front](front.md): the first character
- [operator[]](operator_at.md): the character at a position
- [sgcl::string](README.md)
