[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::save, async_save

```cpp
expected<void, error> save(const string& path) const;                         // (1)
async::task<expected<void, error>> async_save(string path) const noexcept;    // (2)
```

The table's [to_string](to_string.md) into a file, made or written over.

1. Written now.
2. (1) for a task: the value copied into it, the file written on a thread of the blocking pool.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |

## Return value

Nothing, or the [error](../error/README.md) `io`, without a place, for a file that does not open or write.

## Complexity

Linear in the size of the value as written.

## Exceptions

- (1) `invalid_argument` for a value that is no table, as [to_string](to_string.md) throws, and what a write of the file throws.
- (2) None from the call; awaiting the task throws what (1) throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::toml config = encoding::toml::table({{"name", "app"}, {"server", encoding::toml::table({{"port", 8080}})}});
    config.save("app.toml");
    print("{}", io::read_text("app.toml").value_or(string("?")));
    println(config.save("no/such/dir/app.toml").error().message());
}
```

Output:

```text
name = "app"

[server]
port = 8080
input/output error: open no/such/dir/app.toml: No such file or directory
```

## See also

- [load](load.md)
- [to_string](to_string.md)
- [sgcl::encoding::toml](README.md)
