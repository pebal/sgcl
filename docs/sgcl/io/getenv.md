[sgcl](../README.md) › [io](README.md)

# sgcl::io::getenv

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    optional<string> getenv(const string& name) noexcept;
}
```

Returns the value of the environment variable `name`, or `nullopt` when it is not set: Go's `os.LookupEnv`, which
tells an unset variable from an empty one. An empty value is a value, `""`. The name is qualified in a program,
`io::getenv`: under `using namespace sgcl;` a bare `getenv("HOME")` is the C library's `::getenv`, an exact match
for a literal.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the variable |

## Return value

The value as a [string](../core/string/README.md), `nullopt` when the variable is not set.

## Complexity

Linear in the size of the environment, as the C library's `getenv`, plus a copy of the value.

## Exceptions

None.

## Notes

The environment is the process's, shared by every thread: a [setenv](setenv.md) or an [unsetenv](unsetenv.md) on one
thread while another reads a variable is a race on every platform, as in C. A variable read as a typed value with a
fallback is [env](env.md).

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::setenv("APP_MODE", "");
    optional<string> mode = io::getenv("APP_MODE");
    println("set: {}, value: \"{}\"", mode.has_value(), mode.value_or("none"));

    optional<string> missing = io::getenv("APP_NO_SUCH_VARIABLE");
    println("set: {}, value: \"{}\"", missing.has_value(), missing.value_or("none"));
}
```

Output:

```text
set: true, value: ""
set: false, value: "none"
```

## See also

- [env](env.md): a variable as a value of a type, with a fallback
- [setenv](setenv.md), [unsetenv](unsetenv.md): set and remove a variable
- [environ](environ.md): every variable
- [expand_env](expand_env.md): the variables named in a text replaced by their values
