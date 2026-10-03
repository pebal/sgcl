[sgcl](../../README.md) › [io](../README.md) › [mixin](README.md)

# sgcl::io::mixin::reader\<Derived\>

```cpp
#include "sgcl/io/mixin/reader.h"   // or "sgcl/io.h"

namespace sgcl::io::mixin {
    template<class Derived>
    class reader;
}
```

`mixin::reader<Derived>` gives a class with `read(slice<byte>)` the rest of what a reader does, as members over that
`read`: fill a buffer whole, read everything to the end as bytes or as text, copy the stream into a writer. Each
member is the function of io of the same name over this stream: `r.read_full(b)` is
[io::read_full(r, b)](../read_full.md), `r.read_all()` is [io::read_all(r)](../read_all.md), `r.copy_to(w)` is
[io::copy(w, r)](../copy.md). Where `Derived` also has `async_read`, each member has its form for a task.

What Go gives as functions over an `io.Reader` (`io.ReadFull`, `io.ReadAll`, `io.Copy`) and `std` as nothing at all
is here a member of every reader of the library: a `file`, a `buffer`, a `buffered_reader`, an `io::reader`, a
connection, a gzip reader, and a class of your own that derives from the mixin.

## Rules

- `Derived` names itself as the argument (`class file : public mixin::reader<file>`) and has `read(slice<byte>)`
  of the shape of [req::reader](../req/reader.md). The `async_` members take part only when `Derived` is an
  async reader as well ([req::async_reader](../req/reader.md)).
- The mixin has no state and nothing virtual; its constructor and destructor are protected, so that it exists
  only as a base.
- A member is noexcept when the function of io it calls is, which is when `Derived`'s `read` is; `read_all_text`
  is never declared noexcept, a string past 4 GiB being `length_error`.

## Template parameters

| Parameter | Description |
|---|---|
| `Derived` | The class that carries the mixin and names itself as the argument. It has `read(slice<byte>)`, and `async_read(slice<byte>)` for the `async_` members. |

## Member functions

| Function | Description |
|---|---|
| `(constructor)`, `(destructor)` | protected: the mixin exists only as a base |

#### Reading

| Function | Description |
|---|---|
| [read_full, async_read_full](reader/read_full.md) | fills the whole buffer, or says why not |
| [read_all, async_read_all](reader/read_all.md) | everything to the end of the stream, as bytes |
| [read_all_text, async_read_all_text](reader/read_all_text.md) | everything to the end of the stream, as a string |

#### Copying

| Function | Description |
|---|---|
| [copy_to, async_copy_to](reader/copy_to.md) | the stream to its end, written to a writer |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

// A reader of its own: read alone, the rest from the mixin
class letters : public io::mixin::reader<letters> {
public:
    size_t read(slice<byte> b) {
        size_t n = 0;
        while (n < b.size() && _next <= 'z') {
            b[n++] = byte(_next++);
        }
        return n;
    }

private:
    char _next = 'a';
};

int main() {
    letters abc;
    vector<byte> first(3);
    abc.read_full(first);
    println("{}", string(first));
    println("{}", *abc.read_all_text());

    println("{}", *letters().copy_to(io::discard));
}
```

Output:

```text
abc
defghijklmnopqrstuvwxyz
26
```

## See also

- [req::reader](../req/reader.md): what `Derived` has, and what the functions of io take
- [mixin::writer](writer.md): the same for a writer
- [the mixins of io](README.md)
