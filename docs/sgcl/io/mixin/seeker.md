[sgcl](../../README.md) › [io](../README.md) › [mixin](README.md)

# sgcl::io::mixin::seeker\<Derived\>

```cpp
#include "sgcl/io/mixin/seeker.h"   // or "sgcl/io.h"

namespace sgcl::io::mixin {
    template<class Derived>
    class seeker;
}
```

`mixin::seeker<Derived>` gives a class with `seek(offset, from)`, which returns the position after the seek, the
questions asked of a position, as members over that `seek`: where the stream is (`tell`), how long it is (`size`),
and back to the start (`rewind`). Each is one or three seeks: `tell()` is `seek(0, seek_from::current)`, `size()`
seeks to the end and back, `rewind()` is `seek(0, seek_from::begin)`.

Go has `Seek` alone, and `Seek(0, io.SeekCurrent)` is how a Go program asks for the position; `std::fstream` has
`tellg` and `seekg`. Here the three are members of every stream that seeks: a `file`, a `buffer` (whose own `size()`
hides the mixin's), and a class of your own that derives from the mixin.

## Rules

- `Derived` names itself as the argument (`class file : public mixin::seeker<file>`) and has
  `seek(int64_t, seek_from)` of the shape of [req::seeker](../req/seeker.md).
- A member is noexcept when `Derived`'s `seek` is.
- The mixin has no state and nothing virtual; its constructor and destructor are protected, so that it exists
  only as a base.

## Template parameters

| Parameter | Description |
|---|---|
| `Derived` | The class that carries the mixin and names itself as the argument. It has `seek(int64_t, seek_from)`, returning the position after the seek. |

## Member functions

| Function | Description |
|---|---|
| `(constructor)`, `(destructor)` | protected: the mixin exists only as a base |

#### Position

| Function | Description |
|---|---|
| [tell](seeker/tell.md) | the position |
| [size](seeker/size.md) | the size of the stream, the position kept |
| [rewind](seeker/rewind.md) | back to the first byte |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

// A seeker of its own: a position over a device of 1000 bytes, seek alone
class device : public io::mixin::seeker<device> {
public:
    expected<uint64_t, io::error> seek(int64_t offset, io::seek_from from) {
        int64_t base = 0;
        if (from == io::seek_from::current) {
            base = _at;
        } else if (from == io::seek_from::end) {
            base = 1000;
        }
        _at = base + offset;
        return uint64_t(_at);
    }

private:
    int64_t _at = 0;
};

int main() {
    device d;
    d.seek(300, io::seek_from::begin);
    println("{} {}", *d.tell(), *d.size());
    d.rewind();
    println("{}", *d.tell());
}
```

Output:

```text
300 1000
0
```

## See also

- [req::seeker](../req/seeker.md): what `Derived` has
- [seek_from](../seek_from.md): where an offset counts from
- [the mixins of io](README.md)
