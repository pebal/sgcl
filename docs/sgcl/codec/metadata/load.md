[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::load

```cpp
static expected<metadata, error> load(const string& path, const limits& l = {});
```

Reads the metadata of the file at `path`: the file read whole, then [read](read.md) of its bytes.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |
| `l` | the size an EXIF block or an XMP packet may have; the default limits unless told |

## Return value

The metadata, or the [error](../error/README.md): `errc::io` when the file does not read, io's error inside
([io_error](../error/io_error.md)), and what [read](read.md) gives.

## Complexity

Linear in the size of the file, which is read whole.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    expected<codec::metadata,
            codec::error> m = codec::metadata::load("tests/codec/fuzz/seeds/metadata/camera.jpg");
    println("{}", *m->date_time_original());
    expected<codec::metadata, codec::error> missing = codec::metadata::load("no-such-photo.jpg");
    println("{}", missing.error().code() == codec::errc::io);
}
```

Output:

```text
2024-05-06T18:29:41.25+02:00
true
```

## See also

- [read](read.md): of a file in memory or a stream
- [codec::load](../load.md): the image of a file
- [sgcl::codec::metadata](README.md)
