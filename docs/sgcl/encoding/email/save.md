[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::save, async_save

```cpp
expected<void, error> save(const string& path) const;                             // (1)
expected<void, error> save(const string& path, const write_options& o) const;     // (2)
async::task<expected<void, error>> async_save(string path) const noexcept;        // (3)
async::task<expected<void, error>> async_save(string path,                        // (4)
                                              write_options o) const noexcept;
```

The message into the file at `path` (an .eml), made or written over.

- (1, 3) As [to_string](to_string.md) writes it for 7 bits.
- (2, 4) As `o` says.
- (3–4) The same for a task, the file written on the blocking pool.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |
| `o` | how the message is written |

## Return value

Nothing, or the [error](../error/README.md) `errc::io` with the file's `io_error()`, without a place.

## Complexity

Linear in the size of the message.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email m("a@example.com", "b@example.com", "Saved", "body");
    println("{}", m.save("saved.eml").has_value());
    println("{}", encoding::email::load("saved.eml")->subject());
    println("{}", m.save("no/such/dir/x.eml").error().code() == encoding::errc::io);
}
```

Output:

```text
true
Saved
true
```

## See also

- [load, async_load](load.md)
- [email](README.md)
