// Images in Go, the codec benchmark's third side (benchmarks/codec/codec.cpp
// has the module's and the C libraries'): image/png, image/jpeg, image/gif
// and golang.org/x/image/webp, the same cases on the same files, one case a
// run, 0.4 s timed after 0.1 s thrown away; prints "case|ms/op|MP/s". Go has
// no optimized Huffman tables for JPEG: jpeg-enc-opt is not a case here.
// gif-enc and gif-enc-nodither quantize to gif.Encode's default palette
// (Plan9), where the module makes a palette for the image (median cut).
// tiff-dec, tiff-enc-deflate, bmp-dec and bmp-enc through x/image's tiff
// and bmp (no LZW encoder in x/image/tiff).
// Also the inputs Go makes for run.sh.
//
//	codec <case> <data dir>
//	codec prep-detail <photo> <out.png>          2400x2400, the photo (PNG or JPEG) tiled with mirroring
//	codec prep-gif <in.png> <out.gif>            256 colors, Go's quantizer with Floyd-Steinberg
//
// A module of its own (golang.org/x/image, which the benchmarks' module does
// not require); run.sh builds it.
package main

import (
	"bytes"
	"fmt"
	"image"
	"image/draw"
	"image/gif"
	"image/jpeg"
	"image/png"
	"io"
	"os"
	"strings"
	"time"

	"golang.org/x/image/bmp"
	"golang.org/x/image/tiff"
	"golang.org/x/image/webp"
)

func must(err error) {
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func run(name string, pixels float64, f func()) {
	t0 := time.Now()
	for time.Since(t0) < 100*time.Millisecond {
		f()
	}
	calls := 0
	t0 = time.Now()
	var wall time.Duration
	for {
		f()
		calls++
		wall = time.Since(t0)
		if wall >= 400*time.Millisecond {
			break
		}
	}
	ms := wall.Seconds() * 1e3 / float64(calls)
	fmt.Printf("%s|%.3f|%.1f\n", name, ms, pixels/(ms*1e3))
}

func decodeCase(name string, file []byte, dec func(io.Reader) (image.Image, error)) {
	im, err := dec(bytes.NewReader(file))
	must(err)
	b := im.Bounds()
	run(name, float64(b.Dx()*b.Dy()), func() {
		_, err := dec(bytes.NewReader(file))
		must(err)
	})
}

func prepDetail(in, out string) {
	f, err := os.Open(in)
	must(err)
	src, _, err := image.Decode(f)
	must(err)
	sb := src.Bounds()
	tiled := image.NewRGBA(image.Rect(0, 0, 2400, 2400))
	for y := 0; y < 2400; y++ {
		ty, ry := y/sb.Dy(), y%sb.Dy()
		if ty%2 == 1 {
			ry = sb.Dy() - 1 - ry
		}
		for x := 0; x < 2400; x++ {
			tx, rx := x/sb.Dx(), x%sb.Dx()
			if tx%2 == 1 {
				rx = sb.Dx() - 1 - rx
			}
			tiled.Set(x, y, src.At(sb.Min.X+rx, sb.Min.Y+ry))
		}
	}
	w, err := os.Create(out)
	must(err)
	must(png.Encode(w, tiled))
	must(w.Close())
}

func prepGif(in, out string) {
	f, err := os.Open(in)
	must(err)
	src, err := png.Decode(f)
	must(err)
	w, err := os.Create(out)
	must(err)
	must(gif.Encode(w, src, &gif.Options{NumColors: 256, Drawer: draw.FloydSteinberg}))
	must(w.Close())
}

func main() {
	if len(os.Args) < 3 {
		fmt.Fprintln(os.Stderr, "codec <case> <data dir> | prep-detail <in> <out> | prep-gif <in> <out>")
		os.Exit(2)
	}
	c, dir := os.Args[1], os.Args[2]
	switch c {
	case "prep-detail":
		prepDetail(dir, os.Args[3])
		return
	case "prep-gif":
		prepGif(dir, os.Args[3])
		return
	}
	read := func(name string) []byte {
		b, err := os.ReadFile(dir + "/" + name)
		must(err)
		return b
	}
	switch {
	case c == "gif-enc" || c == "gif-enc-nodither":
		// gif.Encode's default palette (Plan9), with Floyd-Steinberg or
		// drawn as it is
		im, err := png.Decode(bytes.NewReader(read("rgb8.png")))
		must(err)
		b := im.Bounds()
		o := &gif.Options{NumColors: 256, Drawer: draw.FloydSteinberg}
		if c == "gif-enc-nodither" {
			o.Drawer = draw.Src
		}
		var buf bytes.Buffer
		run(c, float64(b.Dx()*b.Dy()), func() {
			buf.Reset()
			must(gif.Encode(&buf, im, o))
		})
	case c == "tiff-dec":
		decodeCase(c, read("lzw.tif"), tiff.Decode)
	case c == "bmp-dec":
		decodeCase(c, read("rgb8.bmp"), bmp.Decode)
	case c == "tiff-enc-deflate" || c == "bmp-enc":
		im, err := png.Decode(bytes.NewReader(read("rgb8.png")))
		must(err)
		b := im.Bounds()
		var buf bytes.Buffer
		run(c, float64(b.Dx()*b.Dy()), func() {
			buf.Reset()
			if c == "bmp-enc" {
				must(bmp.Encode(&buf, im))
			} else {
				must(tiff.Encode(&buf, im, &tiff.Options{Compression: tiff.Deflate, Predictor: true}))
			}
		})
	case c == "gif-enc-exact":
		// the indices of image.gif again: an image.Paletted, no quantizing
		im, err := gif.Decode(bytes.NewReader(read("image.gif")))
		must(err)
		b := im.Bounds()
		var buf bytes.Buffer
		run(c, float64(b.Dx()*b.Dy()), func() {
			buf.Reset()
			must(gif.Encode(&buf, im, nil))
		})
	case c == "png-enc" || c == "jpeg-enc":
		im, err := png.Decode(bytes.NewReader(read("rgb8.png")))
		must(err)
		b := im.Bounds()
		var buf bytes.Buffer
		run(c, float64(b.Dx()*b.Dy()), func() {
			buf.Reset()
			if c == "jpeg-enc" {
				must(jpeg.Encode(&buf, im, &jpeg.Options{Quality: 90}))
			} else {
				must(png.Encode(&buf, im))
			}
		})
	case strings.HasPrefix(c, "png-"):
		decodeCase(c, read(strings.TrimPrefix(c, "png-")+".png"), png.Decode)
	case c == "jpeg-base":
		decodeCase(c, read("base.jpg"), jpeg.Decode)
	case c == "jpeg-prog":
		decodeCase(c, read("prog.jpg"), jpeg.Decode)
	case c == "webp-lossy":
		decodeCase(c, read("lossy.webp"), webp.Decode)
	case c == "webp-lossless":
		decodeCase(c, read("lossless.webp"), webp.Decode)
	case c == "gif":
		decodeCase(c, read("image.gif"), gif.Decode)
	default:
		os.Exit(2)
	}
}
