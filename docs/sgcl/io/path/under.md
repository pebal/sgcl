[sgcl](../../README.md) › [io](../README.md) › [path](../path.md)

# sgcl::io::path::under

```cpp
expected<string, error> under(const string& directory, const string& name) noexcept;
```

Joins a name from outside the program to a directory, when the name stays inside it: the name is checked by
[is_local](is_local.md) and, when it passes, [joined](join.md) to the directory and cleaned; when it does not, the
result is the error and nothing is joined. The one guard for a name that becomes a file's path: an entry of an
archive, the path of a request (which may have been `..%2f` before it was decoded), a name a user typed. The error
is Go's `ErrInsecurePath` of `archive/tar` and `archive/zip`.

## Parameters

| Parameter | Description |
|---|---|
| `directory` | the directory the name must stay inside |
| `name` | the name from outside |

## Return value

The directory and the name joined and cleaned, or an [error](../error.md) with `errc::insecure_path`, the operation
`under` and the name as its path.

## Complexity

Linear in the lengths of `directory` and `name`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (const char* name : {"docs/a.txt", "a/../b.txt", "../secret.txt", "/etc/passwd"}) {
        auto file = io::path::under("public", name);
        println("{} -> {}", name, file ? *file : file.error().message());
    }
}
```

Output:

```text
docs/a.txt -> public/docs/a.txt
a/../b.txt -> public/b.txt
../secret.txt -> under ../secret.txt: insecure path
/etc/passwd -> under /etc/passwd: insecure path
```

## See also

- [is_local](is_local.md): the check alone
- [join](join.md): the join alone
- [errc](../errc.md): `insecure_path`
- [sgcl::io::path](../path.md)
