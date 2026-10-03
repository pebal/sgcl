[sgcl](../README.md) › [io](README.md)

# sgcl::io::setenv

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<void, error> setenv(const string& name, const string& value) noexcept;
}
```

Sets the environment variable `name` to `value`, replacing what it held: Go's `os.Setenv`, the C library's
`setenv` with the replacement on. A child process started after the call inherits the variable unless its
[command](command.md) is given an environment of its own.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the variable: not empty, without `=` |
| `value` | its new value; an empty one sets the variable to `""` |

## Return value

Nothing, or the [error](error.md) of the call: `std::errc::invalid_argument` for a name that is empty or holds
`=`. The operation is `setenv` and the path the name.

## Complexity

Linear in the size of the environment.

## Exceptions

None.

## Notes

The environment is the process's: a `setenv` on one thread while another reads or writes a variable is a race on
every platform, as in C. A program sets its variables before it starts its threads.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    if (auto r = io::setenv("APP_LEVEL", "debug"); r) {
        println("{}", *io::getenv("APP_LEVEL"));
    }
    if (auto r = io::setenv("A=B", "x"); !r) {
        println("{}", r.error().message());
    }
}
```

Output:

```text
debug
setenv A=B: Invalid argument
```

## See also

- [unsetenv](unsetenv.md): removes a variable
- [getenv](getenv.md), [env](env.md): read a variable
