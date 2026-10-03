[sgcl](../../README.md) › [io](../README.md) › [error](README.md)

# sgcl::io::error::is_permission

```cpp
bool is_permission() const noexcept;
```

Checks whether the operation failed because it was not permitted: `EACCES` (`std::errc::permission_denied`) or
`EPERM` (`std::errc::operation_not_permitted`), whatever the category they are reported in. A file the user may
not read or write, a directory the user may not list, a signal to another user's process. Go's
`errors.Is(err, fs.ErrPermission)`.

## Parameters

None.

## Return value

`true` when the code is one of the two.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int exit_code(const io::error& e) {
    if (e.is_permission()) {
        return 77;  // EX_NOPERM of sysexits.h
    }
    return e.is_not_found() ? 66 : 1;
}

int main() {
    io::error denied(std::make_error_code(std::errc::permission_denied), "open", "/root/notes");
    io::error not_permitted(std::make_error_code(std::errc::operation_not_permitted), "kill", "1");
    println("{} {}", denied.is_permission(), not_permitted.is_permission());
    println("{} {}", exit_code(denied), exit_code(io::open("missing").error()));
}
```

Output:

```text
true true
77 66
```

## See also

- [chmod](../chmod.md): the permissions of a file
- [sgcl::io::error](README.md)
