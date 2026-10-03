[sgcl](../../README.md) › [io](../README.md) › [error](README.md)

# sgcl::io::error::is_not_found

```cpp
bool is_not_found() const noexcept;
```

Checks whether the operation failed because nothing is at the path: `ENOENT` (`std::errc::no_such_file_or_directory`),
whatever the category it is reported in, or `errc::not_found`, which [look_path](../look_path.md) reports when no
executable of the name is in `PATH`. Go's `errors.Is(err, fs.ErrNotExist)`.

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

int main() {
    auto config = io::read_text("settings.ini");
    if (!config && config.error().is_not_found()) {
        println("no settings: the defaults");
    }
    println("{}", io::look_path("no-such-program-anywhere").error().is_not_found());
    println("{}", io::mkdir(".").error().is_not_found());
}
```

Output:

```text
no settings: the defaults
true
false
```

## See also

- [is_exists](is_exists.md)
- [sgcl::io::error](README.md)
