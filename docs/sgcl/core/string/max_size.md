[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::string::max_size

```cpp
static constexpr size_type max_size() noexcept;
```

Returns the largest number of characters a string holds: 4 294 967 295, `UINT32_MAX`. The length is kept in 32 bits
of the string's object, beside the 32 bits of the hash, so the header of every string is eight bytes.

A text longer than that is refused with `length_error` before a character of it is read, as `std::string` refuses
what passes its own maximum: by the constructors and the assignments, `+`, `concat`, `join`, `repeat`, `replace`, and
whatever else makes a string.

## Parameters

None.

## Return value

`4294967295`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", string::max_size());
    try {
        string too_long(string::max_size() + 1, 'x');
    } catch (const length_error&) {
        println("length_error");
    }
}
```

Output:

```text
4294967295
length_error
```

## See also

- [size](size.md): the number of characters
- [(constructor)](string.md): constructs a string
- [sgcl::string](README.md)
