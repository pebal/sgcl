[sgcl](../README.md) › [compress](README.md) › [zip](zip.md)

# sgcl::compress::zip::options

```cpp
#include "sgcl/compress/zip.h"   // or "sgcl/compress.h"

namespace sgcl::compress::zip {
    struct options {
        zip::method method = zip::method::deflate;
        uint64_t max_size = limits{}.max_size;
    };
}
```

`sgcl::compress::zip::options` is what [extract](zip-extract.md) and [create](zip-create.md) take besides the
paths: how `create` keeps the files, and the bound `extract` holds the archive's files to.

## Member objects

| Member | Description |
|---|---|
| `method` | `create`: how the files are kept, `method::deflate` (the default) or `method::store` |
| `max_size` | `extract`: the files' bytes together past which nothing is written (`errc::too_large`); 1 GiB by default, 0: no bound |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("photos");
    (void)io::write_file("photos/beach.jpg", string("\xff\xd8 not really a JPEG").repeat(100));
    // JPEGs do not deflate: store them
    (void)compress::zip::create("photos", "photos.zip", {.method = compress::zip::method::store});
    auto a = compress::zip::archive::open("photos.zip");
    println("{}", a->entries()[0].method == compress::zip::method::store);
    (void)a->close();

    auto done = compress::zip::extract("photos.zip", "restored", {.max_size = 100});
    println("{}", done.error().message());
    (void)io::remove_all("photos");
    (void)io::remove("photos.zip");
}
```

Output:

```text
true
zip: the files are larger than max_size
```

## See also

- [extract](zip-extract.md), [create](zip-create.md)
- [sgcl::compress::zip](zip.md)
