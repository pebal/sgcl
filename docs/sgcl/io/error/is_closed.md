[sgcl](../../README.md) › [io](../README.md) › [error](../error.md)

# sgcl::io::error::is_closed

```cpp
bool is_closed() const noexcept;
```

Checks whether the operation failed because the stream was closed: `errc::closed`, which a file reports for an
operation after its `close()` and to a read or a write that was waiting when the close came, or `EBADF`
(`std::errc::bad_file_descriptor`), a descriptor the system no longer knows. Go's `errors.Is(err, fs.ErrClosed)`.

## Parameters

None.

## Return value

`true` when the code is one of the two.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto ends = io::pipe();
    ends->read.close();
    vector<byte> block(16);
    auto r = ends->read.read(block);
    println("{}: closed? {}", r.error().message(), r.error().is_closed());
}
```

Output:

```text
read pipe: stream closed: closed? true
```

## See also

- [file::close](../file/close.md)
- [sgcl::io::error](../error.md)
