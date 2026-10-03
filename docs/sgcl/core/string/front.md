[sgcl](../../README.md) › [core](../README.md) › [string](../string.md)

# sgcl::string::front

```cpp
const CharT& front() const noexcept;
```

Returns the first character, read only. The string must not be empty: `front()` of the empty string is undefined
behaviour, as in `std`; a debug build asserts it.

## Parameters

None.

## Return value

A reference to the first character, valid while some string holds the object.

## Complexity

Constant.

## Exceptions

None.

## Notes

In a `string` the first character is the first byte: the first byte of the first code point, which outside ASCII is
not the letter itself. `starts_with` takes the letter as a `char32_t` ([mixin::text](../mixin/text.md)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string path = "/usr/lib";
    println("{} {}", path.front(), path.front() == '/');

    string city = "Łódź";
    println("{} {}", city.front() == 'L', city.starts_with(U'Ł'));
}
```

Output:

```text
/ true
false true
```

## See also

- [back](back.md): the last character
- [operator[]](operator_at.md): the character at a position
- [sgcl::string](../string.md)
