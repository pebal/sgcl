[sgcl](../README.md) › [io](README.md)

# sgcl::io::look_path

```cpp
#include "sgcl/io/exec.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<string, error> look_path(const string& file) noexcept;
}
```

Returns the executable a name stands for, Go's `exec.LookPath`: the name itself when it holds a `/` and names an
executable regular file, else the first executable regular file of that name in the directories of `PATH`, in
their order, an empty entry of `PATH` being the current directory. [command::start](command/start.md) looks its
program up so.

## Parameters

| Parameter | Description |
|---|---|
| `file` | the name of the program, or a path to it |

## Return value

The path of the executable, or the [error](error/README.md) `errc::not_found` (`executable file not found in PATH`) when
there is none, the name empty included; the operation is `look_path` and the path the name.

## Complexity

Linear in the number of directories of `PATH`: a `stat` and an `access` in each until one has the file.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", io::look_path("/bin/sh").value_or("?"));
    if (auto found = io::look_path("no-such-program"); !found) {
        println("{}", found.error().message());
    }
    if (auto sh = io::look_path("sh")) {
        println("{}", io::path::base(*sh));
    }
}
```

Output:

```text
/bin/sh
look_path no-such-program: executable file not found in PATH
sh
```

## See also

- [command](command/README.md): the program started under the path found
- [executable](executable.md): the path of the running program
