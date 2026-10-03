[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [writer](README.md)

# sgcl::compress::sevenzip::writer::close, async_close

```cpp
expected<void, error> close();                                // (1)
async::task<expected<void, error>> async_close() noexcept;    // (2)
```

Ends the archive: ends the last entry and folder, writes the header, packed with LZMA (encrypted with a password and
`encrypt_header`), then the signature header at the archive's start, and closes a file the writer made from a path.
The encoder's memory is given back and a key's memory zeroed. The result is the writer's first error, if any: a
stream written freely is checked once, here. A second close does nothing more.

1. Blocks the calling thread.
2. Returns a task that does the last folder and the header in portions of 64 KB, letting the worker go between them,
   and writes the output through a managed block the file's write holds.

## Parameters

None.

## Return value

Nothing, or the writer's first [error](../error/README.md), of any step before.

## Complexity

Linear in what the last folder holds and in the number of entries.

## Exceptions

- (1) What the writes of the output throw.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> pack(io::buffer archive) {
    compress::sevenzip::writer w(archive);
    co_await w.create("notes.txt").async_write("remember the milk\n");
    auto done = co_await w.async_close();
    println("{}", done.has_value());
}

int main() {
    io::buffer archive;
    async::spawn(pack(archive)).wait();
    auto a = compress::sevenzip::archive::from(archive.data());
    print("{}", string(slice<const byte>(*a->read("notes.txt"))));
}
```

Output:

```text
true
remember the milk
```

## See also

- [last_error](last_error.md)
- [sgcl::compress::sevenzip::writer](README.md)
