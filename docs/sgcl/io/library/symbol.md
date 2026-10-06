[sgcl](../../README.md) › [io](../README.md) › [library](README.md)

# sgcl::io::library::symbol

```cpp
template<class F>
expected<F*, error> symbol(const string& name) const noexcept;
```

A function of the library by its name (`dlsym`), as a pointer of the function type `F` the caller names:
`lib.symbol<int(const char*)>("puts")`. The type is the caller's word: nothing checks it against the library's,
and a call through a wrong one is undefined. Takes part only when `F` is a function type.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the symbol's name, as the library exports it (C's, unmangled) |

## Return value

The pointer, or the [error](../error/README.md), its operation `symbol`: `errc::library` with the dynamic loader's text
as the error's path for a name the library does not have; `errc::closed` after [close](close.md).

## Complexity

The loader's lookup, a hash of the name.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::library z = io::open_library(io::library_file_name("z")).value();
    auto bound = z.symbol<unsigned long(unsigned long)>("compressBound").value();
    println("{}", bound(1000));
}
```

Output:

```text
1013
```

## See also

- [address](address.md): untyped, or a variable's
- [sgcl::io::library](README.md)
