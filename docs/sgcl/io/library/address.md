[sgcl](../../README.md) › [io](../README.md) › [library](README.md)

# sgcl::io::library::address

```cpp
expected<void*, error> address(const string& name) const noexcept;
```

The address of a symbol of the library (`dlsym`): a variable's, which the caller casts to a pointer of its type, or a
function's, untyped. A symbol whose value is null is no error.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the symbol's name |

## Return value

The address, or the [error](../error/README.md), its operation `symbol`: `errc::library` with the dynamic loader's text
for a name the library does not have; `errc::closed` after [close](close.md).

## Complexity

The loader's lookup.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include <cstdlib>

using namespace sgcl;

int main() {
    std::system("printf 'int twice(int x) { return 2 * x; }\\nint counter = 41;\\n' > plugin.c");
    std::system("cc -shared -fPIC -o libplugin.dylib plugin.c");
    io::library plugin = io::open_library("./libplugin.dylib").value();
    int* counter = static_cast<int*>(plugin.address("counter").value());
    ++*counter;
    println("{}", *counter);
}
```

Output:

```text
42
```

## See also

- [symbol](symbol.md): a function, typed
- [sgcl::io::library](README.md)
