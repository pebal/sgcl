[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [writer](../zip-writer.md)

# sgcl::compress::zip::writer::create, async_create

```cpp
expected<io::writer, error> create(const string& name);                     // (1)
expected<io::writer, error> create(const entry& e);                         // (2)
async::task<expected<io::writer, error>> async_create(entry e) noexcept;    // (3)
```

Ends the current entry and starts a new one: writes its local header and gives an [io writer](../../io/writer.md) of
its data, compressed by the entry's method, its sizes and CRC-32 written after it in a data descriptor. The next
`create`, `add` or `close` ends it; the entry writer's own `close()` does too, and may be left out. A write to an
entry that was ended is `io::errc::closed`, an error of that write alone: the writer keeps nothing of it.

1. A deflated entry of the name, modified now, mode 0644; a name ending in `/` is a directory, stored, mode 0755.
2. With the method, the time, the mode and the comment of `e` (its sizes and CRC-32 are the writer's to fill).
3. Returns a task that does (2); its local header waits for the entry's first bytes, its data or its descriptor, and
   goes out in one write with them.

An error is the entry's own (a name or a comment too long, a method not written) or a create after the close, and is
kept as the writer's first error; once an error is kept, `create` gives an entry writer whose writes give it, so a
program that writes freely and checks at the close finds it there.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the entry's name, `/` between the parts |
| `e` | the entry: its name, method, time, mode, comment |

## Return value

The writer of the entry's data, or the [error](../error.md): `method::deflate64` (`errc::unsupported`, "only store
and deflate are written"), a name or a comment past 65 535 bytes (`errc::invalid_argument`), a create after
the close (`errc::invalid_argument`), a failure of `out`.

## Complexity

Constant, and the end of the current entry.

## Exceptions

- (1–2) What the `write` of `out` throws.
- (3) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::zip::writer w(archive);
    io::writer log = *w.create("app.log");
    (void)log.write("started\n");
    (void)log.write("stopped\n");
    io::writer photo = *w.create({.name = "photo.jpg", .method = compress::zip::method::store});
    (void)photo.write(vector<byte>(100, byte(0xff)));
    (void)w.close();

    for (auto& e : compress::zip::archive::from(archive.data())->entries()) {
        println("{}: {} bytes, method {}", e.name, e.size, int(e.method));
    }
}
```

Output:

```text
app.log: 16 bytes, method 8
photo.jpg: 100 bytes, method 0
```

## See also

- [add](add.md): a whole entry at once
- [tar::entry](../tar-entry.md), [zip::entry](../zip-entry.md)
- [sgcl::compress::zip::writer](../zip-writer.md)
