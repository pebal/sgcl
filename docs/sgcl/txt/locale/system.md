[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::system

```cpp
static locale system() noexcept;
```

Returns the user's locale as the environment of the process names it: `LC_ALL`, else `LC_MESSAGES`, else `LANG`, the
first of them set and not empty, read as the constructor reads a tag (`"pl_PL.UTF-8"` is `pl-PL`, `"C"` and
`"POSIX"` the root locale); the root locale when none is set.

## Parameters

None.

## Return value

The locale of the environment.

## Complexity

Linear in the length of the variable.

## Exceptions

None.

## Notes

The environment is read on every call, with `std::getenv`, which another thread's `setenv` races with as everywhere
in C and C++.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::locale here = txt::locale::system();
    println("{}", here == txt::locale() ? "no locale" : "a locale");
}
```

Output:

```text
no locale
```

## See also

- [(constructor)](locale.md)
- [sgcl::txt::locale](README.md)
