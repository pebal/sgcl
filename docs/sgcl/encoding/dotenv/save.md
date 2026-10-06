[sgcl](../../README.md) › [encoding](../README.md) › [dotenv](README.md)

# sgcl::encoding::dotenv::save, async_save

```cpp
expected<void, error> save(const string& path) const;                         // (1)
async::task<expected<void, error>> async_save(string path) const noexcept;    // (2)
```

The entries' [to_string](to_string.md) into a file, made or written over.

1. Written now.
2. (1) for a task: the entries copied into it, the file written on a thread of the blocking pool.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |

## Return value

Nothing, or the [error](../error/README.md) `io`, without a place, for a file that does not open or write.

## Complexity

Linear in the size of the entries.

## Exceptions

- (1) What [to_string](to_string.md) throws, and what a write of the file throws.\n- (2) None from the call; awaiting the task throws what (1) throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::dotenv env = encoding::dotenv::from({{"HOST", "example.com"}, {"GREETING", "hello world"}});
    env.save(".env");
    print("{}", io::read_text(".env").value_or(string("?")));
    println(env.save("no/such/dir/.env").error().message());
}
```

Output:

```text
HOST=example.com
GREETING=hello world
input/output error: open no/such/dir/.env: No such file or directory
```

## See also

- [load](load.md)
- [sgcl::encoding::dotenv](README.md)
