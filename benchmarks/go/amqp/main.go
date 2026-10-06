// AMQP 0-9-1 in Go's standard library alone: Go has no AMQP (amqp091-go is
// not taken), so this side is a minimal client by hand from the
// specification — frames written into a buffer and written to the
// connection per operation, read through a bufio.Reader — against
// bench_amqp's broker (benchmarks/net/amqp.cpp has the SGCL side, the same
// cases). Prints one line, ns per operation.
//
//	amqp_publish ADDR [n]   publications of 64 B, then a declaration as the point they all arrived: per message
//	amqp_confirm ADDR [n]   publications with publisher confirms, each waited for: per message
//	amqp_consume ADDR [n]   deliveries the broker streams, each acked: per delivery
//	amqp_get ADDR [n]       basic.get and its message, in turn: per get
package main

import (
	"bufio"
	"encoding/binary"
	"fmt"
	"io"
	"net"
	"os"
	"strconv"
	"time"
)

func report(what string, d time.Duration, n int) {
	fmt.Printf("amqp %s ns/op=%.1f ops/s=%.0f wall=%.2fs\n", what, float64(d.Nanoseconds())/float64(n), float64(n)/d.Seconds(), d.Seconds())
}

func count(i, def int) int {
	if len(os.Args) > i {
		if n, err := strconv.Atoi(os.Args[i]); err == nil {
			return n
		}
	}
	return def
}

type writer struct{ b []byte }

func (w *writer) u8(v byte)    { w.b = append(w.b, v) }
func (w *writer) u16(v uint16) { w.b = binary.BigEndian.AppendUint16(w.b, v) }
func (w *writer) u32(v uint32) { w.b = binary.BigEndian.AppendUint32(w.b, v) }
func (w *writer) u64(v uint64) { w.b = binary.BigEndian.AppendUint64(w.b, v) }
func (w *writer) short(s string) {
	w.u8(byte(len(s)))
	w.b = append(w.b, s...)
}
func (w *writer) long(s string) {
	w.u32(uint32(len(s)))
	w.b = append(w.b, s...)
}

// A method frame: its head, class and method, the arguments by f
func (w *writer) method(ch uint16, class, id uint16, f func(w *writer)) {
	start := len(w.b)
	w.u8(1)
	w.u16(ch)
	w.u32(0)
	w.u16(class)
	w.u16(id)
	if f != nil {
		f(w)
	}
	binary.BigEndian.PutUint32(w.b[start+3:], uint32(len(w.b)-start-7))
	w.u8(0xCE)
}

func (w *writer) content(ch uint16, body []byte) {
	start := len(w.b)
	w.u8(2)
	w.u16(ch)
	w.u32(0)
	w.u16(60)
	w.u16(0)
	w.u64(uint64(len(body)))
	w.u16(0)
	binary.BigEndian.PutUint32(w.b[start+3:], uint32(len(w.b)-start-7))
	w.u8(0xCE)
	start = len(w.b)
	w.u8(3)
	w.u16(ch)
	w.u32(uint32(len(body)))
	w.b = append(w.b, body...)
	w.u8(0xCE)
}

type frame struct {
	typ     byte
	ch      uint16
	payload []byte
}

func readFrame(r *bufio.Reader) (frame, error) {
	var h [7]byte
	if _, err := io.ReadFull(r, h[:]); err != nil {
		return frame{}, err
	}
	size := binary.BigEndian.Uint32(h[3:])
	p := make([]byte, size+1)
	if _, err := io.ReadFull(r, p); err != nil {
		return frame{}, err
	}
	return frame{h[0], binary.BigEndian.Uint16(h[1:]), p[:size]}, nil
}

func method(f frame) uint32 {
	if f.typ != 1 || len(f.payload) < 4 {
		return 0
	}
	return binary.BigEndian.Uint32(f.payload)
}

func expect(r *bufio.Reader, m uint32) frame {
	for {
		f, err := readFrame(r)
		if err != nil {
			panic(err)
		}
		if f.typ == 8 {
			continue
		}
		if method(f) == m {
			return f
		}
		panic(fmt.Sprintf("unexpected %x", method(f)))
	}
}

func id(c, m uint16) uint32 { return uint32(c)<<16 | uint32(m) }

func main() {
	what := os.Args[1]
	conn, err := net.Dial("tcp", os.Args[2])
	if err != nil {
		panic(err)
	}
	r := bufio.NewReaderSize(conn, 65536)
	w := &writer{}
	send := func() {
		if _, err := conn.Write(w.b); err != nil {
			panic(err)
		}
		w.b = w.b[:0]
	}
	conn.Write([]byte("AMQP\x00\x00\x09\x01"))
	expect(r, id(10, 10))
	w.method(0, 10, 11, func(w *writer) {
		w.u32(0)
		w.short("PLAIN")
		w.long("\x00guest\x00guest")
		w.short("en_US")
	})
	send()
	tune := expect(r, id(10, 30))
	_ = tune
	w.method(0, 10, 31, func(w *writer) { w.u16(2047); w.u32(131072); w.u16(0) })
	w.method(0, 10, 40, func(w *writer) { w.short("/"); w.short(""); w.u8(0) })
	send()
	expect(r, id(10, 41))
	w.method(1, 20, 10, func(w *writer) { w.short("") })
	send()
	expect(r, id(20, 11))
	body := make([]byte, 64)
	for i := range body {
		body[i] = 'm'
	}
	publish := func() {
		w.method(1, 60, 40, func(w *writer) { w.u16(0); w.short(""); w.short("bench"); w.u8(0) })
		w.content(1, body)
		send()
	}
	switch what {
	case "amqp_publish":
		n := count(3, 1000000)
		t0 := time.Now()
		for i := 0; i < n; i++ {
			publish()
		}
		w.method(1, 50, 10, func(w *writer) { w.u16(0); w.short("bench"); w.u8(0); w.u32(0) })
		send()
		ok := expect(r, id(50, 11))
		if got := binary.BigEndian.Uint32(ok.payload[4+1+5:]); got != uint32(n) {
			panic(fmt.Sprintf("broker counted %d", got))
		}
		report(what, time.Since(t0), n)
	case "amqp_confirm":
		n := count(3, 50000)
		w.method(1, 85, 10, func(w *writer) { w.u8(0) })
		send()
		expect(r, id(85, 11))
		t0 := time.Now()
		for i := 0; i < n; i++ {
			publish()
			expect(r, id(60, 80))
		}
		report(what, time.Since(t0), n)
	case "amqp_consume":
		n := count(3, 1000000)
		t0 := time.Now()
		w.method(1, 60, 20, func(w *writer) { w.u16(0); w.short("stream:" + strconv.Itoa(n)); w.short(""); w.u8(0); w.u32(0) })
		send()
		expect(r, id(60, 21))
		for i := 0; i < n; i++ {
			d := expect(r, id(60, 60))
			p := d.payload[4:]
			tag := binary.BigEndian.Uint64(p[1+int(p[0]):])
			h, _ := readFrame(r)
			size := binary.BigEndian.Uint64(h.payload[4:])
			var got uint64
			for got < size {
				b, _ := readFrame(r)
				got += uint64(len(b.payload))
			}
			w.method(1, 60, 80, func(w *writer) { w.u64(tag); w.u8(0) })
			send()
		}
		report(what, time.Since(t0), n)
	case "amqp_get":
		n := count(3, 50000)
		t0 := time.Now()
		for i := 0; i < n; i++ {
			w.method(1, 60, 70, func(w *writer) { w.u16(0); w.short("bench"); w.u8(0) })
			send()
			expect(r, id(60, 71))
			h, _ := readFrame(r)
			size := binary.BigEndian.Uint64(h.payload[4:])
			var got uint64
			for got < size {
				b, _ := readFrame(r)
				got += uint64(len(b.payload))
			}
		}
		report(what, time.Since(t0), n)
	}
	w.method(0, 10, 50, func(w *writer) { w.u16(200); w.short(""); w.u16(0); w.u16(0) })
	send()
	conn.Close()
}
