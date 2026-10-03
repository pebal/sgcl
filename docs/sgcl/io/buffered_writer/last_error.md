[sgcl](../../README.md) › [io](../README.md) › [buffered_writer](../buffered_writer.md)

# sgcl::io::buffered_writer::last_error

```cpp
const optional<error>& last_error() const noexcept;
```

Returns the first error the writer gave, kept: a failure of the stream underneath (in a write, a flush or the close),
or a write after the close. Every write and flush after it gives that error at once and writes nothing, and every
close gives it, as Go's `bufio.Writer` keeps its error; the compressing writers of `compress` keep theirs the same
way. Whoever wants to react before the close asks here, or checks the result of one write.

## Parameters

None.

## Return value

The kept error; an empty `optional` while the writer has given none. The reference is to the writer's state, valid
while a handle holds it.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include <system_error>

using namespace sgcl;

int main() {
    // A stream that takes 10 000 bytes, then fails as a full disk does
    size_t room = 10000;
    auto disk = [&](slice<const byte> b) -> expected<size_t, io::error> {
        if (b.size() > room) {
            auto full = std::make_error_code(std::errc::no_space_on_device);
            return unexpected(io::error(full, "write"));
        }
        room -= b.size();
        return b.size();
    };

    io::buffered_writer w(disk);
    vector<byte> chunk(3000);
    for (int i : {1, 2, 3, 4, 5}) {
        w.write(chunk);  // written freely: not checked here
    }
    println("{}", w.last_error() ? w.last_error()->message() : "none yet");
    println("{}", w.close().error().message());
}
```

Output:

```text
none yet
write: No space left on device
```

## See also

- [close](close.md): gives the kept error
- [error](../error.md): what it holds
- [sgcl::io::buffered_writer](../buffered_writer.md)
