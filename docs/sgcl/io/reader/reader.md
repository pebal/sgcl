[sgcl](../../README.md) › [io](../README.md) › [reader](README.md)

# sgcl::io::reader::reader

```cpp
reader() noexcept = default;                                   // (1)
template<class R>
requires (!std::same_as<std::remove_cvref_t<R>, reader>) &&
         (req::reader<R> || req::async_reader<R>)
reader(R&& r) noexcept(/* see below */);                       // (2)
```

Constructs a reader.

1. An empty reader: it holds no stream, `bool(r)` is `false`, and only [close](close.md), the observers and a
   comparison may be called on it.
2. A reader of `r`, any type that meets [req::reader](../req/reader.md) or
   [req::async_reader](../req/reader.md). Not explicit: a stream converts to a reader wherever one is taken.
   What is kept depends on `r`:
   - a handle of the library (a `file`, a `buffer`, a `buffered_reader`), as it is or through a pointer to it: the
     object its copies share, so the handle may go first;
   - a `tracked_ptr` to a stream: held by it; a `unique_ptr` from `make_tracked` given as a temporary: taken over;
   - a stream of the program's given by reference: referenced, with the managed object it lies in kept, if it lies
     in one; one on a stack, a global or one a `unique_ptr` owns is the caller's to keep alive;
   - a callable or a temporary of the program's: moved or copied into a managed object of its own.

   An empty handle (a default-constructed `buffered_reader`), or a null pointer (a `tracked_ptr`, a `root_ptr`, a
   `unique_ptr` from `make_tracked`) to a handle or to a stream of the program's, makes an empty reader. The copy and
   the move of a reader are implicit: the three words are copied.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the stream to read from |

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
    io::reader empty;
    io::reader handle = io::buffer("from a buffer\n");  // the buffer's object is kept
    io::reader standard = io::stdin;                   // a global, by reference

    tracked_ptr held = make_tracked<io::buffer>("from a tracked_ptr\n");
    io::reader pointer = held;

    println("{} {} {} {}", bool(empty), bool(handle), bool(standard), bool(pointer));
    io::copy(io::stdout, handle);
    io::copy(io::stdout, pointer);
}
```

Output:

```text
false true true true
from a buffer
from a tracked_ptr
```

## See also

- [operator bool](operator_bool.md): whether a reader holds a stream
- [writer](../writer/writer.md): the writer's constructors
- [sgcl::io::reader](README.md)
