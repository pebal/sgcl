[sgcl](../README.md) › [io](README.md)

# sgcl::io::lock_options

```cpp
#include "sgcl/io/lock.h"   // or "sgcl/io.h"

namespace sgcl::io {
    struct lock_options {
        lock_mode mode = lock_mode::exclusive;
        uint64_t offset = 0;
        uint64_t length = 0;
        optional<duration> timeout;
    };
}
```

`sgcl::io::lock_options` is what a lock of [lock_file](lock_file.md) takes beyond the one-line form, by field name:
`{.mode = io::lock_mode::shared}`, `{.offset = 4096, .length = 512}`, `{.timeout = duration::zero()}`.

## Member objects

| Field | Description |
|---|---|
| `mode` | shared or exclusive ([lock_mode](lock_mode.md)); exclusive by default |
| `offset` | the first byte of a range; with `length` 0 as well, the whole file (`flock`) |
| `length` | the bytes of the range; 0 with an `offset`: to the end of the file and past it |
| `timeout` | none (the default): wait as long as it takes; zero: try once; else wait at most this long |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    io::file a = io::open("shared.db", io::open_flags::read | io::open_flags::write | io::open_flags::create).value();
    io::file b = io::open("shared.db", io::open_flags::read | io::open_flags::write).value();
    io::file_lock reading = io::lock_file(a, {.mode = io::lock_mode::shared}).value();
    auto also = io::lock_file(b, {.mode = io::lock_mode::shared, .timeout = duration::zero()});
    auto writing = io::lock_file(b, {.timeout = 50ms});
    println("{} {}", also.has_value(), writing.error().message());
}
```

Output:

```text
true lock shared.db: Operation timed out
```

## See also

- [lock_file](lock_file.md), [lock_mode](lock_mode.md)
