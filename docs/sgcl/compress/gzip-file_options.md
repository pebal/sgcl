[sgcl](../README.md) › [compress](README.md) › [gzip](gzip/README.md)

# sgcl::compress::gzip::file_options

```cpp
#include "sgcl/compress/gzip.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class gzip {
    public:
        struct file_options {
            compress::level level;
            bool keep = true;
        };
    };
}
```

`sgcl::compress::gzip::file_options` is what [compress_file](gzip/compress_file.md) and
[decompress_file](gzip/decompress_file.md) take besides the path: the level of the compression, and whether the
original stays. The functions have an overload without it, which stands for a default argument that a nested struct
with member initializers cannot be inside its class.

## Member objects

| Member | Description |
|---|---|
| `level` | the level of `compress_file`; 6 by default ([level](level/README.md)); `decompress_file` does not read it |
| `keep` | whether the original stays; `true` by default, as gzip(1) with `-k`; `false` removes it once the other file is whole |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("notes.txt", "one line\n");
    (void)compress::gzip::compress_file("notes.txt", {.level = 9, .keep = false});
    println("{} {}", io::exists("notes.txt"), io::exists("notes.txt.gz"));
    (void)io::remove("notes.txt.gz");
}
```

Output:

```text
false true
```

## See also

- [compress_file](gzip/compress_file.md), [decompress_file](gzip/decompress_file.md)
- [sgcl::compress::gzip](gzip/README.md)
