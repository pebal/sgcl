[sgcl](../../README.md) › [codec](../README.md) › [image](../image.md)

# sgcl::codec::image::save, async_save

```cpp
expected<void, error> save(const string& path) const;                                   // (1)
expected<void, error> save(const string& path, const save_options& o) const;            // (2)
async::task<expected<void, error>> async_save(const string& path) const noexcept;       // (3)
async::task<expected<void, error>> async_save(const string& path,                       // (4)
                                              const save_options& o) const noexcept;
```

Writes the image into the file at `path`, in the format the path's extension names, in letters of either case:
`.png`, `.jpg` or `.jpeg`, and `.heic` or `.heif` where the system writes HEIC (macOS). `.gif`, `.webp` and `.avif`
are formats the module reads and does not write. The file is written as `path + ".part"` and renamed over `path`
when it is whole: a failure, of the encoder or of the file, an exception too, leaves no part and `path` as it was.
It is [codec::save](../save.md) of this image.

1. Writes with the default [save_options](../save_options.md).
2. Writes with the options `o`: each field is for the formats it names, the others leave it alone.
3. (1) as a [task](../../async/task.md), run on the blocking pool ([spawn_blocking](../../async/spawn_blocking.md)).
4. (2) as a task, run on the blocking pool.

- (3–4) The task holds a copy of the image's handle, of the path and of the options, so the arguments may be gone
  before it runs. It is lazy: it starts when it is awaited, waited for or spawned.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file to write; its extension names the format |
| `o` | the PNG level, the JPEG and HEIC quality, the JPEG subsampling |

## Return value

- (1–2) An empty `expected` when the file is written, or the [error](../error.md):
  [errc](../errc.md)`::unsupported` for an extension the module does not write, or none, and for HEIC on a system
  without the encoder; `errc::io`, with the [io_error](../error/io_error.md) inside, when the file cannot be
  created, written or renamed; `errc::invalid_argument` for a HEIC quality outside 1 to 100 and for an image the
  format cannot hold (a JPEG side past 65 535 pixels, a PNG side past 2^31 − 1).
- (3–4) The task, which gives the same.

## Complexity

Linear in the number of pixels: the encoding and the write.

## Exceptions

- (1–2) `invalid_argument` when the path names a JPEG and `o.quality` is outside 1 to 100 or `o.subsampling`
  outside the list, as [jpeg::encode](../jpeg/encode.md) throws it. The part is removed and `path` is as it was.
- (3–4) None. `co_await` and `wait()` of the task rethrow what (1–2) throw.

## Notes

The functions are declared in `sgcl/codec/image.h` and defined in `sgcl/codec/files.h`, with
[codec::save](../save.md), which `sgcl/codec.h` brings: a program that includes `image.h` alone and calls
them does not link.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(64, 48, codec::pixel_format::rgb8);
    auto saved = picture.save("picture.png");
    println("png: {}", saved.has_value());
    auto written = picture.async_save("picture.jpg", {.quality = 90}).wait();
    println("jpg: {}", written.has_value());
    codec::image back = codec::load("picture.jpg");
    println("{}x{}", back.width(), back.height());
    auto refused = picture.save("picture.gif");
    println(refused.error().message());
    io::remove("picture.png");
    io::remove("picture.jpg");
}
```

Output:

```text
png: true
jpg: true
64x48
offset 0: codec: .gif is read, not written (no encoder)
```

## See also

- [save_options](../save_options.md): the options
- [codec::save](../save.md), [codec::load](../load.md): images on files
- [png::encode](../png/encode.md), [jpeg::encode](../jpeg/encode.md), [heif::encode](../heif/encode.md): the
  encoders
- [sgcl::codec::image](../image.md)
