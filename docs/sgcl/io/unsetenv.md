[sgcl](../README.md) › [io](README.md)

# sgcl::io::unsetenv

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<void, error> unsetenv(const string& name) noexcept;
}
```

Removes the environment variable `name`: Go's `os.Unsetenv`, the C library's `unsetenv`. A variable that is not set
is no error.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the variable: not empty, without `=` |

## Return value

Nothing, or the [error](error/README.md) of the call: `std::errc::invalid_argument` for a name that is empty or holds
`=`. The operation is `unsetenv` and the path the name.

## Complexity

Linear in the size of the environment.

## Exceptions

None.

## Notes

The environment is the process's: an `unsetenv` on one thread while another reads or writes a variable is a race on
every platform, as in C.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::setenv("APP_TOKEN", "secret");
    println("{}", io::getenv("APP_TOKEN").has_value());
    (void)io::unsetenv("APP_TOKEN");
    println("{}", io::getenv("APP_TOKEN").has_value());
    println("{}", io::unsetenv("APP_TOKEN").has_value());
}
```

Output:

```text
true
false
true
```

## See also

- [setenv](setenv.md): sets a variable
- [getenv](getenv.md): reads a variable
