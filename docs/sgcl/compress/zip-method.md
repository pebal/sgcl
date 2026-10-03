[sgcl](../README.md) › [compress](README.md) › [zip](zip.md)

# sgcl::compress::zip::method

```cpp
#include "sgcl/compress/zip.h"   // or "sgcl/compress.h"

namespace sgcl::compress::zip {
    enum class method : uint16_t {
        store = 0,
        deflate = 8,
        deflate64 = 9
    };
}
```

How an entry's data is kept (APPNOTE 4.4.5): the values are the format's own, of which the library reads and writes
the first two and reads Deflate64. An entry of another method is listed by the [archive](zip-archive/README.md) and is
`errc::unsupported` when it is read.

| Value | Description |
|---|---|
| `store` | the data as it is: for what does not compress (JPEG, PNG, archives) and for directories |
| `deflate` | [DEFLATE](flate/README.md), the default of the writer and of `create` |
| `deflate64` | PKWARE's "enhanced deflate", as 7-Zip and Windows write large archives: DEFLATE with a window of 64 KB, lengths up to 65 538 and two more distance codes; read as deflated entries are, the reader's window twice 64 KB; not written (`errc::unsupported`, "only store and deflate are written") |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::zip::writer w(archive);
    string text = string("the same words again and again. ").repeat(100);
    for (auto m : {compress::zip::method::store, compress::zip::method::deflate}) {
        string name = m == compress::zip::method::store ? "stored.txt" : "deflated.txt";
        auto entry = w.create({.name = name, .method = m});
        (void)entry->write(text);
    }
    (void)w.close();

    auto a = compress::zip::archive::from(archive.data());
    for (auto& e : a->entries()) {
        println("{}: {} into {}", e.name, e.size, e.compressed_size);
    }
}
```

Output:

```text
stored.txt: 3200 into 3200
deflated.txt: 3200 into 54
```

## See also

- [entry](zip-entry/README.md), [options](zip-options.md)
- [sgcl::compress::zip](zip.md)
