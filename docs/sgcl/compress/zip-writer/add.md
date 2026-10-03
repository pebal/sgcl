[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [writer](../zip-writer.md)

# sgcl::compress::zip::writer::add, async_add

```cpp
/*(1)*/ expected<void, error> add(const string& name, const slice<const byte>& data);
/*(2)*/ async::task<expected<void, error>> async_add(string name, slice<const byte> data) noexcept;
```

Writes a whole entry: a [create](create.md) of the name (deflated, modified now; a name ending in `/` a directory),
its data, and its end. A text is its bytes, a `vector<byte>` likewise.

1. Blocks the calling thread for the writes of `out`.
2. Returns a task that does the same and gives the worker back while `out` writes. The bytes are read when the task
   runs: `data` lives until the task is done.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the entry's name, `/` between the parts |
| `data` | the entry's data |

## Return value

Nothing, or the writer's [error](../error.md), kept as its first: what `create` refuses, a failure of `out`, or the
error kept from before.

## Complexity

Linear in the size of `data`.

## Exceptions

- (1) What the `write` of `out` throws.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> pack(io::buffer archive) {
    compress::zip::writer w(archive);
    co_await w.async_add("a.txt", "first\n");
    co_await w.async_add("b.txt", "second\n");
    auto done = co_await w.async_close();
    println("{}", done.has_value());
}

int main() {
    io::buffer archive;
    async::spawn(pack(archive)).wait();
    auto a = compress::zip::archive::from(archive.data());
    for (auto& e : a->entries()) {
        print("{}: {}", e.name, string(slice<const byte>(*a->read(e))));
    }
}
```

Output:

```text
true
a.txt: first
b.txt: second
```

## See also

- [create](create.md): an entry written as a stream
- [add_file](add_file.md): a file as an entry
- [sgcl::compress::zip::writer](../zip-writer.md)
