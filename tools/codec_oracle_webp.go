// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The Go oracle of the WebP tests: a file decoded by golang.org/x/image/webp
// (a still image, VP8L or VP8 with ALPH; no animation), written to stdout in
// the form of tools/codec_oracle.c, so that a test holds the module against a
// second decoder beside libwebp.
//
//	codec_oracle_webp webp <file>     "W H 8", then the pixels RGBA, not
//	                                  premultiplied
//	codec_oracle_webp webpyuv <file>  a lossy image's planes as Go decodes
//	                                  them (image.YCbCr, 4:2:0, before any
//	                                  color conversion): "W H 8", then the
//	                                  rows of Y (W bytes), of Cb and of Cr
//	                                  ((W + 1) / 2 bytes, (H + 1) / 2 rows)
//
// Not in the Go standard library: built by the tests (tests/codec/oracle.h)
// in a module of its own requiring golang.org/x/image, from the module cache
// in ~/Programming/oracles/gomod, offline. A file Go refuses: "ERROR <what>"
// on stderr, exit 1.
package main

import (
	"bufio"
	"fmt"
	"image"
	"image/color"
	"os"

	"golang.org/x/image/webp"
)

func main() {
	if len(os.Args) != 3 || (os.Args[1] != "webp" && os.Args[1] != "webpyuv") {
		fmt.Fprintln(os.Stderr, "usage: codec_oracle_webp webp|webpyuv <file>")
		os.Exit(2)
	}
	f, err := os.Open(os.Args[2])
	if err != nil {
		fmt.Fprintln(os.Stderr, "ERROR", err)
		os.Exit(1)
	}
	defer f.Close()
	img, err := webp.Decode(bufio.NewReader(f))
	if err != nil {
		fmt.Fprintln(os.Stderr, "ERROR", err)
		os.Exit(1)
	}
	b := img.Bounds()
	out := bufio.NewWriter(os.Stdout)
	defer out.Flush()
	if os.Args[1] == "webpyuv" {
		var y *image.YCbCr
		switch v := img.(type) {
		case *image.YCbCr:
			y = v
		case *image.NYCbCrA:
			y = &v.YCbCr
		default:
			fmt.Fprintln(os.Stderr, "ERROR not a lossy image")
			os.Exit(1)
		}
		w, h := b.Dx(), b.Dy()
		cw, ch := (w+1)/2, (h+1)/2
		fmt.Fprintf(out, "%d %d 8\n", w, h)
		for r := 0; r < h; r++ {
			out.Write(y.Y[r*y.YStride : r*y.YStride+w])
		}
		for r := 0; r < ch; r++ {
			out.Write(y.Cb[r*y.CStride : r*y.CStride+cw])
		}
		for r := 0; r < ch; r++ {
			out.Write(y.Cr[r*y.CStride : r*y.CStride+cw])
		}
		return
	}
	fmt.Fprintf(out, "%d %d 8\n", b.Dx(), b.Dy())
	row := make([]byte, 0, b.Dx()*4)
	for y := b.Min.Y; y < b.Max.Y; y++ {
		row = row[:0]
		for x := b.Min.X; x < b.Max.X; x++ {
			c := color.NRGBAModel.Convert(img.At(x, y)).(color.NRGBA)
			row = append(row, c.R, c.G, c.B, c.A)
		}
		out.Write(row)
	}
}
