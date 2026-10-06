[sgcl](../../README.md) › [encoding](../README.md) › [dotenv](README.md)

# sgcl::encoding::dotenv::apply

```cpp
void apply() const;                  // (1)
void apply(bool overwrite) const;    // (2)
```

Every entry into the process environment (`setenv`), where `std::getenv`, a child process and every library that
reads the environment see it. For the start of a program, before other threads read the environment: the C library
does not make `getenv` and `setenv` safe together.

1. A variable already set is kept: what the environment gives wins over the file, as the three tools do.
2. With `overwrite`, the file's value wins.

## Parameters

| Parameter | Description |
|---|---|
| `overwrite` | whether a variable already set takes the file's value |

## Return value

None.

## Complexity

Linear in the size of the entries.

## Exceptions

`invalid_argument` for a key the environment cannot hold (empty, with `=` or a null character; one made by [set](set.md) or [from](from.md)) or a value with a null character.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

#include <cstdlib>

using namespace sgcl;

int main() {
    encoding::dotenv::from({{"SGCL_EXAMPLE_HOST", "example.com"}, {"HOME", "/somewhere/else"}}).apply();
    println("{}", std::getenv("SGCL_EXAMPLE_HOST"));
    println(string(std::getenv("HOME")) != "/somewhere/else");
}
```

Output:

```text
example.com
true
```

## See also

- [load](load.md)
- [sgcl::encoding::dotenv](README.md)
