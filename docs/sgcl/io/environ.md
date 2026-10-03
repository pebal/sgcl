[sgcl](../README.md) › [io](README.md)

# sgcl::io::environ

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    vector<pair<string, string>> environ() noexcept;
}
```

Returns every variable of the environment as a name and a value, in the order the environment holds them: Go's
`os.Environ`, split at the first `=`. An entry without `=` is a name with an empty value. The list is what a
[command](command.md)'s `env` takes: a copy of the program's environment, changed, for a child.

## Parameters

None.

## Return value

The variables, each a `pair` of the name and the value.

## Complexity

Linear in the size of the environment.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::setenv("APP_COLOR", "blue");
    for (const auto& [name, value] : io::environ()) {
        if (name == "APP_COLOR") {
            println("{}={}", name, value);
        }
    }
}
```

Output:

```text
APP_COLOR=blue
```

## See also

- [getenv](getenv.md): one variable
- [command](command.md): `env`, the environment of a child
