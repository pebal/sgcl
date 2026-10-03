[sgcl](../../README.md) › [io](../README.md) › mixin

# sgcl::io::mixin

```cpp
#include "sgcl/io/mixin/reader.h"   // namespace sgcl::io::mixin; or "sgcl/io.h"
#include "sgcl/io/mixin/seeker.h"   // namespace sgcl::io::mixin; or "sgcl/io.h"
#include "sgcl/io/mixin/writer.h"   // namespace sgcl::io::mixin; or "sgcl/io.h"
```

A stream has one primitive, `read`, `write` or `seek` ([req](../req.md)); everything else a stream does is built on
it once, as the functions of io ([read_full](../read_full.md), [read_all](../read_all.md),
[read_all_text](../read_all_text.md), [write](../write.md), [copy](../copy.md)) over any stream. A **mixin** of io
gives the same as members of a class: a base class the stream names itself as the argument of
(`class file : public mixin::reader<file>, public mixin::writer<file>, public mixin::seeker<file>`), each member the
function of io over this stream, so that `f.read_all()` is `io::read_all(f)` and `out.write("text")` is
`io::write(out, "text")`. It is the pattern of the [mixins of core](../../core/mixin/README.md), where
`mixin::enumerable` is a mixin over `begin()` and `end()`: a static interface, no virtual method, no state, its
constructor and destructor protected so that it exists only as such a base.

Unlike core's, io's mixins declare nothing: the requirements of io are structural, `req::reader<T>` asks whether `T`
has `read`, not whether it carries `mixin::reader`, so a class of the program's is a stream without them and every
function of io takes it. A mixin is for the class that wants the members: every stream of the library carries the
ones of its primitives, and a class of your own gets them by deriving from the mixin, with no virtual anywhere.

## The rules

- A mixin is a base of the class it names, `Derived`, and calls `Derived`'s primitive: `read(slice<byte>)` for
  [reader](reader.md), `write(slice<const byte>)` for [writer](writer.md), `seek(int64_t, seek_from)` for
  [seeker](seeker.md). The async forms of a member exist where `Derived` has `async_read` (`async_write`), and are
  instantiated only where they are called.
- A class that defines `write` hides the mixin's overloads of the name, as C++ hides a base's name, and brings them
  back with using-declarations, as every writer of the library does:
  `using mixin::writer<Derived>::write;` and `using mixin::writer<Derived>::async_write;`. A class of your own that
  does not derive from the mixin writes text through [io::write(w, text)](../write.md).
- A class whose own answer is better hides the mixin's with a member of the same name: `io::buffer`'s `size()` is
  the number of bytes it holds, and hides `mixin::seeker`'s.
- A member is noexcept as far as the primitive it calls is: `buffer.read_all()` cannot throw, since the buffer's
  `read` cannot. `read_all_text` may throw whatever the stream, a string past 4 GiB being `length_error`. An
  `async_` form throws, when it is made, at most what the move of a temporary argument into its frame throws; its
  body's exceptions are the task's, rethrown by its `co_await`.

## Mixins

| Mixin | Header | Description |
|---|---|---|
| [reader](reader.md) | `sgcl/io/mixin/reader.h` | the rest of a reader over `read`: `read_full`, `read_all`, `read_all_text`, `copy_to`, each with its `async_` form |
| [seeker](seeker.md) | `sgcl/io/mixin/seeker.h` | the rest of a seeker over `seek`: `tell`, `size`, `rewind` |
| [writer](writer.md) | `sgcl/io/mixin/writer.h` | the rest of a writer over `write`: `write` of text or a byte, `copy_from`, each with its `async_` form |

## Requirements

The page of the requirements: [req](../req.md).

| Requirement | Header | Description |
|---|---|---|
| [closer, async_closer](../req/closer.md) | `sgcl/io/req.h` | `close()`; `async_close()`, a task |
| [reader, async_reader](../req/reader.md) | `sgcl/io/req.h` | `read(slice<byte>)`; `async_read`, a task; or a callable of that shape |
| [seeker](../req/seeker.md) | `sgcl/io/req.h` | `seek(int64_t, seek_from)` |
| [writer, async_writer](../req/writer.md) | `sgcl/io/req.h` | `write(slice<const byte>)`; `async_write`, a task; or a callable of that shape |

## Who carries what

| Class | Reader | Writer | Seeker |
|---|---|---|---|
| [buffer](../buffer.md) | ✓ | ✓ | ✓ (`size` its own) |
| [buffered_reader](../buffered_reader.md) | ✓ | | |
| [buffered_writer](../buffered_writer.md) | | ✓ | |
| [discard_writer](../discard_writer.md) | | ✓ | |
| [file](../file.md) | ✓ | ✓ | ✓ |
| [limit_reader](../limit_reader.md) | ✓ | | |
| [multi_reader](../multi_reader.md) | ✓ | | |
| [multi_writer](../multi_writer.md) | | ✓ | |
| [reader](../reader.md) | ✓ | | |
| [standard_stream](../standard_stream.md) | ✓ | ✓ | |
| [tee_reader](../tee_reader.md) | ✓ | | |
| [transform_reader](../transform_reader.md) | ✓ | | |
| [writer](../writer.md) | | ✓ | |

Outside io, the readers and writers of `compress` and the encoders and decoders of `encoding` carry them, and
[net::connection](../../net/connection.md) has the same members, forwarded to the object inside it.

## See also

- [req](../req.md): the requirements of io, what a stream is
- [the mixins of core](../../core/mixin/README.md): the same pattern for containers
- [io](../README.md)
- `tests/io/stream.cpp`: the requirements, the mixins and the functions over the streams, checked
