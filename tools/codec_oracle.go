// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The Go oracle of the codec tests: a file decoded by Go's image packages,
// its pixels written to stdout in the form of tools/codec_oracle.c, so that
// a test compares both oracles' output with the module's byte for byte.
//
//	codec_oracle png <file>    image/png, each channel at the file's depth
//	codec_oracle jpeg <file>   image/jpeg (its own IDCT, no fancy upsampling:
//	                           a second opinion, not bit for bit), 8 bits
//	codec_oracle gif <file>    image/gif (DecodeAll): every frame composed on
//	                           a canvas of the logical screen as the module
//	                           composes them (transparent start; disposal 2
//	                           clears the rectangle, 3 puts back what was under
//	                           it); "W H 8", then the frames RGBA one after
//	                           another
//	codec_oracle gifmeta <file>  "plays P" (0 forever: LoopCount 0; none: 1;
//	                           n: n + 1) and "delay D" of each frame
//	codec_oracle png8 <file>   the same, 16-bit channels scaled to 8 by the
//	                           module's rule (the nearest value, round(v /
//	                           257)), not by Go's own (v >> 8)
//
// The output: a line "W H D" (D the bits of a channel, 8 or 16), then H rows
// of W pixels RGBA, not premultiplied, a 16-bit channel big-endian. A file Go
// refuses: "ERROR <what>" on stderr, exit 1.
package main

import (
	"bufio"
	"fmt"
	"image"
	"image/color"
	"image/gif"
	"image/jpeg"
	"image/png"
	"os"
)

// The bits of a channel of the decoded image, as the file had them
func depth(m image.Image) int {
	switch m.(type) {
	case *image.Paletted, *image.Gray, *image.NRGBA, *image.RGBA, *image.YCbCr, *image.CMYK:
		return 8
	}
	return 16
}

func wide8(v uint8) uint16 {
	return uint16(v) * 0x101
}

// A pixel's channels at 16 bits, straight (not premultiplied): read from
// the image's own type where it has one, so that a transparent pixel keeps
// its color (NRGBA64Model goes through premultiplied values, which lose it)
func pixel(m image.Image, x, y int) [4]uint16 {
	straight := func(c color.Color) [4]uint16 {
		switch c := c.(type) {
		case color.NRGBA:
			return [4]uint16{wide8(c.R), wide8(c.G), wide8(c.B), wide8(c.A)}
		case color.RGBA:
			if c.A == 0xFF {
				return [4]uint16{wide8(c.R), wide8(c.G), wide8(c.B), 0xFFFF}
			}
		}
		n := color.NRGBA64Model.Convert(c).(color.NRGBA64)
		return [4]uint16{n.R, n.G, n.B, n.A}
	}
	switch m := m.(type) {
	case *image.NRGBA:
		return straight(m.NRGBAAt(x, y))
	case *image.NRGBA64:
		c := m.NRGBA64At(x, y)
		return [4]uint16{c.R, c.G, c.B, c.A}
	case *image.RGBA64:
		c := m.RGBA64At(x, y)
		if c.A == 0xFFFF {
			return [4]uint16{c.R, c.G, c.B, c.A}
		}
	case *image.Gray:
		g := wide8(m.GrayAt(x, y).Y)
		return [4]uint16{g, g, g, 0xFFFF}
	case *image.Gray16:
		g := m.Gray16At(x, y).Y
		return [4]uint16{g, g, g, 0xFFFF}
	case *image.Paletted:
		return straight(m.Palette[m.ColorIndexAt(x, y)])
	case *image.YCbCr:
		c := m.YCbCrAt(x, y)
		r, g, b := color.YCbCrToRGB(c.Y, c.Cb, c.Cr)
		return [4]uint16{wide8(r), wide8(g), wide8(b), 0xFFFF}
	case *image.CMYK:
		c := m.CMYKAt(x, y)
		r, g, b := color.CMYKToRGB(c.C, c.M, c.Y, c.K)
		return [4]uint16{wide8(r), wide8(g), wide8(b), 0xFFFF}
	}
	return straight(m.At(x, y))
}

// Every frame of a GIF composed on its canvas, RGBA 8 bits
func gifFrames(g *gif.GIF) (int, int, []byte) {
	w, h := g.Config.Width, g.Config.Height
	canvas := make([]byte, w*h*4)
	saved := make([]byte, w*h*4)
	var out []byte
	lastDisposal := 0
	var last image.Rectangle
	for i, m := range g.Image {
		if lastDisposal == 2 || lastDisposal == 3 {
			for y := last.Min.Y; y < last.Max.Y; y++ {
				a, b := (y*w+last.Min.X)*4, (y*w+last.Max.X)*4
				if lastDisposal == 2 {
					for k := a; k < b; k++ {
						canvas[k] = 0
					}
				} else {
					copy(canvas[a:b], saved[a:b])
				}
			}
		}
		disposal := int(g.Disposal[i])
		if disposal > 3 {
			disposal = 1
		}
		r := m.Bounds().Intersect(image.Rect(0, 0, w, h))
		if disposal == 3 {
			for y := r.Min.Y; y < r.Max.Y; y++ {
				a, b := (y*w+r.Min.X)*4, (y*w+r.Max.X)*4
				copy(saved[a:b], canvas[a:b])
			}
		}
		for y := r.Min.Y; y < r.Max.Y; y++ {
			for x := r.Min.X; x < r.Max.X; x++ {
				c := m.Palette[m.ColorIndexAt(x, y)]
				_, _, _, alpha := c.RGBA()
				if alpha == 0 {
					continue // the transparent index: left as it was
				}
				n := color.NRGBAModel.Convert(c).(color.NRGBA)
				k := (y*w + x) * 4
				canvas[k], canvas[k+1], canvas[k+2], canvas[k+3] = n.R, n.G, n.B, 255
			}
		}
		out = append(out, canvas...)
		lastDisposal = disposal
		last = r
	}
	return w, h, out
}

func main() {
	if len(os.Args) != 3 {
		fmt.Fprintln(os.Stderr, "usage: codec_oracle png|png8|jpeg|gif|gifmeta <file>")
		os.Exit(2)
	}
	mode, path := os.Args[1], os.Args[2]
	f, err := os.Open(path)
	if err != nil {
		fmt.Fprintln(os.Stderr, "ERROR", err)
		os.Exit(1)
	}
	defer f.Close()
	var m image.Image
	switch mode {
	case "png", "png8":
		m, err = png.Decode(bufio.NewReader(f))
	case "jpeg":
		m, err = jpeg.Decode(bufio.NewReader(f))
	case "gif", "gifmeta":
		g, err := gif.DecodeAll(bufio.NewReader(f))
		if err != nil {
			fmt.Fprintln(os.Stderr, "ERROR", err)
			os.Exit(1)
		}
		out := bufio.NewWriter(os.Stdout)
		defer out.Flush()
		if mode == "gifmeta" {
			plays := 1
			if g.LoopCount == 0 {
				plays = 0
			} else if g.LoopCount > 0 {
				plays = g.LoopCount + 1
			}
			fmt.Fprintf(out, "plays %d\n", plays)
			for _, d := range g.Delay {
				fmt.Fprintf(out, "delay %d\n", d)
			}
			return
		}
		w, h, frames := gifFrames(g)
		fmt.Fprintf(out, "%d %d 8\n", w, h)
		out.Write(frames)
		return
	default:
		fmt.Fprintln(os.Stderr, "ERROR unknown mode", mode)
		os.Exit(2)
	}
	if err != nil {
		fmt.Fprintln(os.Stderr, "ERROR", err)
		os.Exit(1)
	}
	d := depth(m)
	if mode == "png8" {
		d = 8
	}
	b := m.Bounds()
	out := bufio.NewWriter(os.Stdout)
	defer out.Flush()
	fmt.Fprintf(out, "%d %d %d\n", b.Dx(), b.Dy(), d)
	for y := b.Min.Y; y < b.Max.Y; y++ {
		for x := b.Min.X; x < b.Max.X; x++ {
			for _, v := range pixel(m, x, y) {
				if d == 16 {
					out.WriteByte(byte(v >> 8))
					out.WriteByte(byte(v))
				} else {
					out.WriteByte(byte((uint32(v)*255 + 32895) >> 16))
				}
			}
		}
	}
}
