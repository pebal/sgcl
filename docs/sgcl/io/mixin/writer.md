[sgcl](../../README.md) › [io](../README.md) › [mixin](README.md)

# sgcl::io::mixin::writer\<Derived\>

```cpp
#include "sgcl/io/mixin/writer.h"   // or "sgcl/io.h"

namespace sgcl::io::mixin {
    template<class Derived>
    class writer;
}
```

`mixin::writer<Derived>` gives a class with `write(slice<const byte>)` (all of it written, or an error that says how
far it got, as Go's `Write`) the rest of what a writer does, as members over that `write`: `write` of any data, the
type telling what it is, and `copy_from` a reader. Each member is the function of io of the same name over this
stream: `w.write("text")` is [io::write(w, "text")](../write.md), `w.copy_from(r)` is
[io::copy(w, r)](../copy.md). The `async_` members are the same for a task, over `Derived`'s `async_write`.

`write` takes text — a [string](../../core/string.md), a text slice (a line of a
[buffered_reader](../buffered_reader.md), a piece of a string), a literal or a character array (to its first NUL),
a C string, a `std::string_view` — or one byte, each written from where it lies, with no string made: Go's
`io.WriteString`, a member of every writer of the library (a `file`, a `buffer`, a `buffered_writer`, an
`io::writer`, the standard streams, a connection, a gzip writer) and of a class of your own that derives from the
mixin.

## Rules

- `Derived` names itself as the argument (`class buffer : public mixin::writer<buffer>`) and has
  `write(slice<const byte>)` of the shape of [req::writer](../req/writer.md); the bytes are written by that
  `write`, the mixin adds the text and the byte.
- A class that defines `write` hides the mixin's overloads of the name, as C++ hides a base's name, and brings them
  back by using-declarations, as every writer of the library does:
  `using mixin::writer<file>::write;` and `using mixin::writer<file>::async_write;`.
- The `async_write` members carry no constraint of their own: they are instantiated only where they are called,
  over a `Derived` that has `async_write`. `async_copy_from` takes part only when `Derived` is an
  async writer ([req::async_writer](../req/writer.md)).
- The mixin has no state and nothing virtual; its constructor and destructor are protected, so that it exists
  only as a base.

## Template parameters

| Parameter | Description |
|---|---|
| `Derived` | The class that carries the mixin and names itself as the argument. It has `write(slice<const byte>)`, and `async_write(slice<const byte>)` for the `async_` members. |

## Member functions

| Function | Description |
|---|---|
| `(constructor)`, `(destructor)` | protected: the mixin exists only as a base |

#### Writing

| Function | Description |
|---|---|
| [write, async_write](writer/write.md) | writes text or one byte |

#### Copying

| Function | Description |
|---|---|
| [copy_from, async_copy_from](writer/copy_from.md) | writes everything a reader gives, to its end |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

// A writer of its own: write of bytes alone, the rest from the mixin
class shouting : public io::mixin::writer<shouting> {
public:
    using io::mixin::writer<shouting>::write;  // the mixin's overloads, hidden by the one below

    size_t write(slice<const byte> b) {
        for (byte c : b) {
            char ch = char(c);
            print("{}", ch >= 'a' && ch <= 'z' ? char(ch - 'a' + 'A') : ch);
        }
        return b.size();
    }
};

int main() {
    shouting out;
    out.write("hello, ");
    out.write(string("mixin"));
    out.write(byte('\n'));
    out.copy_from(io::buffer("and a copy\n"));
}
```

Output:

```text
HELLO, MIXIN
AND A COPY
```

## See also

- [req::writer](../req/writer.md): what `Derived` has, and what the functions of io take
- [mixin::reader](reader.md): the same for a reader
- [the mixins of io](README.md)
