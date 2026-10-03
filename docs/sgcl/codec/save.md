[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::save, async_save

```cpp
#include "sgcl/codec/files.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    expected<void, error> save(const image& im, const string& path,                        // (1)
                               const save_options& o = {});
    async::task<expected<void, error>> async_save(const image& im, const string& path,     // (2)
                                                  const save_options& o = {}) noexcept;
}
```

Writes the image into the file at `path`, in the format the path's extension names, its letters in either case:

| Extension | What is written |
|---|---|
| `.png` | PNG, at the level `o.level` ([png::encode](png/encode.md)) |
| `.jpg`, `.jpeg` | JPEG, at the quality `o.quality` and with the subsampling `o.subsampling` ([jpeg::encode](jpeg/encode.md)) |
| `.heic`, `.heif` | HEIC, at the quality `o.quality`, where the system writes it (macOS); `errc::unsupported` elsewhere ([heif::encode](heif/encode.md)) |
| `.gif`, `.webp`, `.avif` | nothing: formats the module reads and does not write, `errc::unsupported` |
| any other, or none | nothing: `errc::unsupported` |

When the extension is refused nothing is written. Otherwise the file is written as `path + ".part"` and renamed over
`path` when whole: a failure, of the file or of the encoder, an exception included, leaves no `.part` and `path` as it
was. [image::save](image/save.md) is the same as a method of the image, and [load](load.md) the other way.

1. On the calling thread.
2. For a task: `save` run on the [blocking pool](../async/spawn_blocking.md), so that the task holds no worker while
   the image is encoded and written. The path and the options are copied into the task, and the image is held by a
   copy of its handle, which shares the pixels: what is written is the pixels as they are when the task runs.

## Parameters

| Parameter | Description |
|---|---|
| `im` | the image |
| `path` | the file, its format told by its extension |
| `o` | the PNG level, the JPEG and HEIC quality, the JPEG subsampling; each field is for the formats it names ([save_options](save_options.md)) |

## Return value

Nothing; or the [error](error/README.md): `errc::unsupported` for an extension of no format the module writes (and for HEIC
where the system has no HEVC encoder), `errc::invalid_argument` for a HEIC quality outside 1 to 100 and for an image
the format cannot hold (a JPEG side past 65 535 pixels, a PNG side past 2^31 − 1), and `errc::io` when the file
cannot be created, written, closed or renamed, with io's error inside ([io_error](error/io_error.md)). (2) gives it
through the task.

## Complexity

Linear in the size of the image.

## Exceptions

- (1) `invalid_argument` when `path` names JPEG and `o.quality` is outside 1 to 100 or `o.subsampling` outside the
  list, the contract of [jpeg::encode](jpeg/encode.md); no `.part` is left and `path` is as it was.
- (2) None from the call. `co_await` and `wait()` of the task rethrow what (1) threw, and `std::system_error` when
  the blocking pool has to start a thread and cannot.

## Notes

A save whose result is not looked at fails quietly: `codec::save(picture, "out.png");` as a statement throws nothing
when the file cannot be written.

The encoder writes into the open `.part` file as into any [io::writer](../io/writer/README.md), as the image is encoded:
every format's `encode` into a stream returns `expected<void, codec::error>`, with `errc::io` when the stream fails.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(320, 200, codec::pixel_format::rgb8);
    codec::save(picture, "picture.png");
    codec::save(picture, "picture.JPG", {.quality = 90});
    codec::async_save(picture, "copy.png").wait();
    println("{} {} {}", io::exists("picture.png"), io::exists("picture.JPG"),
            io::exists("copy.png"));

    expected<void, codec::error> gif = codec::save(picture, "picture.gif");
    println("{}, {}", gif.error().message(), io::exists("picture.gif"));

    try {
        codec::save(picture, "bad.jpg", {.quality = 0});
    } catch (const invalid_argument& e) {
        println("{}, {} {}", e.what(), io::exists("bad.jpg"), io::exists("bad.jpg.part"));
    }

    for (const char* path : {"picture.png", "picture.JPG", "copy.png"}) {
        io::remove(path);
    }
}
```

Output:

```text
true true true
offset 0: codec: .gif is read, not written (no encoder), false
sgcl::codec::jpeg::encode: quality outside 1..100, false false
```

## See also

- [save_options](save_options.md): the level, the quality, the subsampling
- [image::save](image/save.md): the same as a method
- [load](load.md): the image of a file
- [codec](README.md)
