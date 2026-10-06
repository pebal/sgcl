[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::library

```cpp
#include "sgcl/io/library.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class library final;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::io::library` is a dynamic library loaded at run time — a plug-in, an optional codec, a library of the system the
program may do without — as [open_library](../open_library.md) loads it (`dlopen`; `LoadLibraryExW` on Windows), with
its functions found by name, typed by the caller ([symbol](symbol.md)), and its variables ([address](address.md)).
Go's `plugin` package loads Go's own plug-ins only and never unloads them; this is the C ABI's, unloaded by
[close](close.md).

## Rules

- A handle of one word, its copies the same library. One made by its default constructor holds none (`!lib`); an
  operation on it is a contract violation.
- [close](close.md) unloads, explicitly; the collector does not: a function pointer taken from a library is a plain
  pointer, nothing ties it to the handle, and a library unloaded by a sweep under a call through one would leave it in
  unmapped code. A library never closed stays loaded to the end of the process, as every plug-in of Go's does.
- The type given to [symbol](symbol.md) is the caller's word: nothing checks it against the library.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](library.md) | constructs the handle: none, or a copy that is the same library |

#### Symbols

| Function | Description |
|---|---|
| [symbol](symbol.md) | a function of the library, typed |
| [address](address.md) | the address of a symbol: a variable's, or a function's untyped |

#### Loading

| Function | Description |
|---|---|
| [close](close.md) | unloads the library |
| [is_closed](is_closed.md) | checks whether it was closed |

#### Observers

| Function | Description |
|---|---|
| [path](path.md) | the path or the name it was opened with |
| [operator bool](operator_bool.md) | checks whether the handle holds a library |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | checks whether two handles are the same library |

## Example

```cpp
#include "sgcl/io.h"
#include <cstdlib>

using namespace sgcl;

int main() {
    std::system("printf 'int twice(int x) { return 2 * x; }\\nint counter = 41;\\n' > plugin.c");
    std::system("cc -shared -fPIC -o libplugin.dylib plugin.c");
    io::library plugin = io::open_library("./libplugin.dylib").value();
    auto twice = plugin.symbol<int(int)>("twice").value();
    println("{}", twice(21));
    println("{}", plugin.symbol<int(int)>("thrice").error().code() == io::errc::library);
    plugin.close();
}
```

Output:

```text
42
true
```

## See also

- [open_library](../open_library.md), [library_options](../library_options.md),
  [library_file_name](../library_file_name.md)
