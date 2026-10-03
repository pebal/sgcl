[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::load, async_load

```cpp
#include "sgcl/codec/files.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    /*(1)*/ expected<image, error> load(const string& path, const decode_options& o = {});
    /*(2)*/ async::task<expected<image, error>> async_load(const string& path,
                                                           const decode_options& o = {}) noexcept;
}
```

Reads the image of the file at `path`, in any of the module's formats: the whole file is read, and decoded as
[decode](decode.md) decodes it, the format told by the file's first bytes ([sniff](sniff.md)) whatever its name says.
A file that does not read is `errc::io`, with io's error inside ([io_error](error/io_error.md)); a file of no format
the module reads is `errc::unsupported`. The other way is [save](save.md).

1. On the calling thread.
2. For a task: `load` run on the [blocking pool](../async/spawn_blocking.md), so that the task holds no worker while
   the file is read and decoded. The path and the options are copied into the task, which may run after the caller's
   are gone.

An error is a value: `codec::image photo = codec::load(path);` takes the image as it is, and throws a
[bad_expected_access](../core/bad_expected_access.md) in place of one when the file does not load.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |
| `o` | the pixel format wanted, the limits, and whether the metadata is read ([decode_options](decode_options.md)) |

## Return value

The image, in the pixel format `o.want` names or in the file's own; or the [error](error.md): `errc::io` when the file
does not read, `errc::unsupported` for a file of no format the module reads, and the decoder's error otherwise. (2)
gives it through the task.

## Complexity

Linear in the size of the file and of the image.

## Exceptions

- (1) What [io::read_file](../io/read_file.md) throws: `std::system_error` when the path names a FIFO or a device,
  which the file reads on the reactor, the read has to wait, and the reactor's thread cannot be made.
- (2) None from the call. `co_await` and `wait()` of the task rethrow what (1) threw, and `std::system_error` when
  the blocking pool has to start a thread and cannot.

## Notes

The whole file is in memory while it is decoded: the memory is the file and the image. [decode](decode.md) of a
stream over the open file reads it as it comes instead.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> show(string path) {
    codec::image picture = co_await codec::async_load(path);
    println("in a task: {}x{}", picture.width(), picture.height());
}

int main() {
    codec::image picture(320, 200, codec::pixel_format::rgb8);
    io::write_file("photo.jpg", codec::png::encode(picture));  // a PNG under the name of a JPEG

    codec::image loaded = codec::load("photo.jpg", {.want = codec::pixel_format::rgba8});
    println("{}x{}, rgba8: {}", loaded.width(), loaded.height(),
            loaded.format() == codec::pixel_format::rgba8);
    show("photo.jpg").wait();

    expected<codec::image, codec::error> missing = codec::load("missing.png");
    println("{}", missing.error().io_error().has_value());
    println(missing.error().message());
    io::remove("photo.jpg");
}
```

Output:

```text
320x200, rgba8: true
in a task: 320x200
true
offset 0: input/output error: open missing.png: No such file or directory
```

## See also

- [save](save.md): an image into a file
- [decode](decode.md): the same of bytes in memory, or of a stream
- [decode_options](decode_options.md), [image](image.md), [error](error.md)
- [codec](README.md)
