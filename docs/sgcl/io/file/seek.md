[sgcl](../../README.md) › [io](../README.md) › [file](README.md)

# sgcl::io::file::seek

```cpp
expected<uint64_t, error> seek(int64_t offset, seek_from from = seek_from::begin) const noexcept;
```

Moves the position of the file to `offset` bytes from the beginning, from the position or from the end, as `from`
says: the `lseek(2)` of the descriptor. A position past the end is allowed; a write there leaves a hole of zeros
before it. A pipe has no position: its seek is an error.

The position is shared by every handle of the file. [tell](../mixin/seeker/tell.md), [size](../mixin/seeker/size.md)
and [rewind](../mixin/seeker/rewind.md) are made of this seek ([mixin::seeker](../mixin/seeker/README.md)).

## Parameters

| Parameter | Description |
|---|---|
| `offset` | the distance from the place `from` names, negative to move back |
| `from` | where `offset` counts from: `seek_from::begin`, `seek_from::current`, `seek_from::end` ([seek_from](../seek_from.md)) |

## Return value

The new position, from the beginning of the file. Or the [error](../error/README.md), its operation `seek` and its path
the file's: `errc::closed` for a closed file, otherwise the `errno` of `lseek(2)` (`EINVAL` for a position before
the beginning, `ESPIPE` for a pipe).

## Complexity

Constant: one system call.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("letters.txt", "abcdefghij");
    io::file f = io::open("letters.txt");
    println("{}", *f.seek(3));
    println("{}", *f.seek(2, io::seek_from::current));
    println("{}", *f.seek(-1, io::seek_from::end));
    println("{}", *f.read_all_text());
    println("{}", f.seek(-20, io::seek_from::current).error().message());

    auto [in, out] = io::pipe().value();
    println("{}", in.seek(0).error().message());
}
```

Output:

```text
3
5
9
j
seek letters.txt: Invalid argument
seek pipe: Illegal seek
```

## See also

- [tell](../mixin/seeker/tell.md), [size](../mixin/seeker/size.md), [rewind](../mixin/seeker/rewind.md): the
  position, the size, the beginning
- [read_at](read_at.md), [write_at](write_at.md): an offset of their own, the position untouched
- [seek_from](../seek_from.md)
- [sgcl::io::file](README.md)
