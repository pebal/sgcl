// The Go side of benchmarks/compress/compress.cpp: the same text (words
// drawn by splitmix64), the same cases, one a run.
//
//	go run ./compress <deflate-1|deflate-6|deflate-9|inflate|gunzip-stream|zip-open>
package main

import (
	"archive/zip"
	"bytes"
	"compress/flate"
	"compress/gzip"
	"fmt"
	"io"
	"os"
	"strings"
	"time"
)

func splitmix(s *uint64) uint64 {
	*s += 0x9E3779B97F4A7C15
	z := *s
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9
	z = (z ^ (z >> 27)) * 0x94D049BB133111EB
	return z ^ (z >> 31)
}

var words = strings.Fields("the of and a to in is you that it he was for on are as with his they I at be this have from or one had by word but not what all were we when your can said there use an each which she do how collector pointer compress archive stream window symbol length distance block table entry header checksum data value")

func text(n int) []byte {
	var b bytes.Buffer
	state := uint64(42)
	for b.Len() < n {
		r := splitmix(&state)
		b.WriteString(words[r%64])
		if (r>>8)%13 == 0 {
			b.WriteString(".\n")
		} else {
			b.WriteString(" ")
		}
	}
	return b.Bytes()[:n]
}

func rate(f func(), n int, seconds float64) float64 {
	start := time.Now()
	calls := 0
	for time.Since(start).Seconds() < seconds {
		f()
		calls++
	}
	return float64(n) * float64(calls) / time.Since(start).Seconds() / 1e6
}

var sink int

func main() {
	what := os.Args[1]
	data := text(8 << 20)
	report := func(mbs float64, out int) {
		fmt.Printf("compress %s go MB/s=%.1f ratio=%.4f\n", what, mbs, float64(out)/float64(len(data)))
	}
	switch what {
	case "deflate-1", "deflate-6", "deflate-9":
		level := int(what[len(what)-1] - '0')
		out := 0
		f := func() {
			var b bytes.Buffer
			w, _ := flate.NewWriter(&b, level)
			w.Write(data)
			w.Close()
			out = b.Len()
		}
		rate(f, len(data), 0.25)
		report(rate(f, len(data), 2), out)
	case "inflate":
		var b bytes.Buffer
		w, _ := flate.NewWriter(&b, 6)
		w.Write(data)
		w.Close()
		c := b.Bytes()
		// the output in a buffer made by the call, as the C++ variants have it
		f := func() {
			r := flate.NewReader(bytes.NewReader(c))
			out := bytes.NewBuffer(make([]byte, 0, len(data)))
			n, _ := io.Copy(out, r)
			sink = int(n)
		}
		rate(f, len(data), 0.25)
		report(rate(f, len(data), 2), len(c))
	case "gunzip-stream":
		var b bytes.Buffer
		w := gzip.NewWriter(&b)
		w.Write(data)
		w.Close()
		c := b.Bytes()
		buf := make([]byte, 65536)
		f := func() {
			r, _ := gzip.NewReader(bytes.NewReader(c))
			total := 0
			for {
				n, err := r.Read(buf)
				total += n
				if err != nil {
					break
				}
			}
			sink = total
		}
		rate(f, len(data), 0.25)
		report(rate(f, len(data), 2), len(c))
	case "zip-open":
		var b bytes.Buffer
		zw := zip.NewWriter(&b)
		for i := 0; i < 10000; i++ {
			w, _ := zw.Create(fmt.Sprintf("dir/file-%d.txt", i))
			w.Write(data[i*7 : i*7+64])
		}
		zw.Close()
		z := b.Bytes()
		start := time.Now()
		opens := 0
		for time.Since(start).Seconds() < 2 {
			r, _ := zip.NewReader(bytes.NewReader(z), int64(len(z)))
			sink = len(r.File)
			opens++
		}
		fmt.Printf("compress zip-open go ms/open=%.3f entries=10000\n", time.Since(start).Seconds()*1000/float64(opens))
	}
}
