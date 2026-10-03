[sgcl](../../README.md) › [io](../README.md) › [writer](../writer.md)

# sgcl::io::writer::writer

```cpp
writer() noexcept = default;                                   // (1)
template<class W>
requires (!std::same_as<std::remove_cvref_t<W>, writer>) &&
         (req::writer<W> || req::async_writer<W>)
writer(W&& w) noexcept(/* see below */);                       // (2)
```

Constructs a writer.

1. An empty writer: it holds no stream, `bool(w)` is `false`, and only [close](close.md), the observers and a
   comparison may be called on it.
2. A writer of `w`, any type that meets [req::writer](../req/writer.md) or
   [req::async_writer](../req/writer.md). Not explicit: a stream converts to a writer wherever one is taken.
   What is kept depends on `w`:
   - a handle of the library (a `file`, a `buffer`, a `buffered_writer`), as it is or through a pointer to it: the
     object its copies share, so the handle may go first;
   - a `tracked_ptr` to a stream: held by it; a `unique_ptr` from `make_tracked` given as a temporary: taken over;
   - a stream of the program's given by reference: referenced, with the managed object it lies in kept, if it lies
     in one; one on a stack, a global (`io::stdout`) or one a `unique_ptr` owns is the caller's to keep alive;
   - a callable or a temporary of the program's: moved or copied into a managed object of its own.

   An empty handle (a default-constructed `buffered_writer`), or a null pointer (a `tracked_ptr`, a `root_ptr`, a
   `unique_ptr` from `make_tracked`) to a handle or to a stream of the program's, makes an empty writer. The copy and
   the move of a writer are implicit: the three words are copied.

## Parameters

| Parameter | Description |
|---|---|
| `w` | the stream to write to |

## Complexity

Constant: one allocation for a callable or a temporary of the program's, none otherwise.

## Exceptions

- (1) None.
- (2) None, but for a callable or a temporary of the program's: what its copy or move into the managed object
  throws; none when that is noexcept, and so the constructor is.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::writer empty;
    io::writer terminal = io::stdout;  // a global, by reference

    io::buffer kept;
    io::writer memory = kept;  // the buffer's object, shared with kept

    size_t total = 0;
    io::writer counter = [&](slice<const byte> data) { total += data.size(); };  // cannot fail

    println("{} {} {}", bool(empty), bool(terminal), bool(memory));
    memory.write("into the buffer");
    counter.write("12345");
    println("{}, {} bytes counted", kept.text(), total);
}
```

Output:

```text
false true true
into the buffer, 5 bytes counted
```

## See also

- [operator bool](operator_bool.md): whether a writer holds a stream
- [reader](../reader/reader.md): the reader's constructors
- [sgcl::io::writer](../writer.md)
