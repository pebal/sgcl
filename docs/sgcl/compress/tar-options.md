[sgcl](../README.md) › [compress](README.md) › [tar](tar.md)

# sgcl::compress::tar::options

```cpp
#include "sgcl/compress/tar.h"   // or "sgcl/compress.h"

namespace sgcl::compress::tar {
    struct options {
        compress::level level;
        uint64_t max_size = limits{}.max_size;
    };
}
```

`sgcl::compress::tar::options` is what [extract](tar-extract.md) and [create](tar-create.md) take besides the
paths: the level of the gzip or xz `create` writes around the archive, and the bound `extract` holds the archive's
files to.

## Member objects

| Member | Description |
|---|---|
| `level` | `create`: the level of gzip or xz around the archive; 6 by default ([level](level.md)) |
| `max_size` | `extract`: the files' bytes together past which nothing is written (`errc::too_large`); 1 GiB by default, 0: no bound |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("logs");
    (void)io::write_file("logs/big.log", string("a line of the log\n").repeat(1000));
    (void)compress::tar::create("logs", "logs.tar.xz", {.level = 9});

    auto done = compress::tar::extract("logs.tar.xz", "out", {.max_size = 1000});
    println("{}", done.error().message());
    println("{}", io::exists("out"));
    (void)io::remove_all("logs");
    (void)io::remove("logs.tar.xz");
}
```

Output:

```text
tar: the files are larger than max_size
false
```

## See also

- [extract](tar-extract.md), [create](tar-create.md)
- [sgcl::compress::tar](tar.md)
