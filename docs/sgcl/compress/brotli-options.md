[sgcl](../README.md) › [compress](README.md) › [brotli](brotli/README.md)

# sgcl::compress::brotli::options

```cpp
#include "sgcl/compress/brotli.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class brotli {
    public:
        struct options {
            brotli::level level;
            uint8_t window_log = 22;
        };
    };
}
```

`sgcl::compress::brotli::options` is how a brotli stream is made: the [level](brotli-level/README.md) (brotli's
quality) and the window. [compress](brotli/compress.md) and the [writer](brotli-writer/README.md) take it; a reader
takes none, the window coming from the stream's header.

## Rules

- An aggregate: `{.level = 5, .window_log = 24}`, the other field at its default, which is the `brotli` command's.
- **The window** is how far back a copy may reach, and what a reader must hold: 2^`window_log` - 16 bytes, 10 to 24
  (brotli's `lgwin`). [compress](brotli/compress.md) never makes it larger than the data. A larger window finds
  matches farther back in a long stream; a smaller one bounds a reader's memory. A value outside 10 to 24 is the
  program's mistake: `compress` throws `std::invalid_argument`, and a writer's first write reports
  `errc::invalid_argument`.

## Member objects

| Member | Description |
|---|---|
| `level` | the [level](brotli-level/README.md): brotli's quality; 11 by default |
| `window_log` | a window of 2^`window_log` - 16 bytes, 10 to 24; 22 by default |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<string> lines;
    for (int i : range(3000)) {
        lines.push_back(string::concat("record ", to_string(i), " of the log: ", i % 7 ? "served" : "failed"));
    }
    string text = string::join(lines, '\n');
    for (int q : {1, 6, 11}) {
        println("quality {}: {} bytes", q, compress::brotli::compress(text, {.level = q, .window_log = 16}).size());
    }
}
```

Output:

```text
quality 1: 4872 bytes
quality 6: 4304 bytes
quality 11: 4188 bytes
```

## See also

- [level](brotli-level/README.md)
- [sgcl::compress::brotli](brotli/README.md)
