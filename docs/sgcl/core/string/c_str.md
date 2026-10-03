[sgcl](../../README.md) › [core](../README.md) › [string](../string.md)

# sgcl::string::c_str

```cpp
const CharT* c_str() const noexcept;
```

Returns the characters as a C string: the same pointer as [data](data.md), to characters followed by a terminator.
There is no copy to make, as the string's object always holds the terminator; the empty string returns a pointer to
an empty C string, never null.

The pointer is valid while some string holds the object, as `data()`'s is.

## Parameters

None.

## Return value

A pointer to the characters followed by a terminator, `data()`.

## Complexity

Constant.

## Exceptions

None.

## Notes

A C function reads the text up to its first NUL: a string that holds a NUL of its own is cut there.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <cstdlib>
#include <cstring>

using namespace sgcl;

int main() {
    string port = "8080";
    int number = std::atoi(port.c_str());
    println("{} {}", number + 1, port.c_str() == port.data());

    string empty;
    println("{}", std::strcmp(empty.c_str(), ""));
}
```

Output:

```text
8081 true
0
```

## See also

- [data](data.md): the characters, terminated
- [parse](../parse.md): a number from its text, without a C string
- [sgcl::string](../string.md)
