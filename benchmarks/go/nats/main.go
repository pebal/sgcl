// NATS in Go's standard library alone: Go has no NATS client (nats.go is not
// taken), so this side is a minimal client by hand from the protocol's
// documentation — lines written to the connection per operation, read
// through a bufio.Reader — against bench_nats's server
// (benchmarks/net/nats.cpp has the SGCL side, the same cases). Prints one
// line, ns per operation.
//
//	nats_publish ADDR [n]   publications of 64 B, then PING and its PONG: per message
//	nats_pubsub ADDR [n]    64 B from one connection to a subscriber on another: per message received
//	nats_request ADDR [n]   a request and its reply through a responder on another connection: per request
package main

import (
	"bufio"
	"fmt"
	"io"
	"net"
	"os"
	"strconv"
	"strings"
	"time"
)

func report(what string, d time.Duration, n int) {
	fmt.Printf("nats %s ns/op=%.1f ops/s=%.0f wall=%.2fs\n", what, float64(d.Nanoseconds())/float64(n), float64(n)/d.Seconds(), d.Seconds())
}

func count(i, def int) int {
	if len(os.Args) > i {
		if n, err := strconv.Atoi(os.Args[i]); err == nil {
			return n
		}
	}
	return def
}

type conn struct {
	c net.Conn
	r *bufio.Reader
}

func dial(addr string) *conn {
	c, err := net.Dial("tcp", addr)
	if err != nil {
		panic(err)
	}
	k := &conn{c, bufio.NewReaderSize(c, 65536)}
	k.line() // INFO
	k.c.Write([]byte("CONNECT {\"verbose\":false,\"pedantic\":false,\"headers\":true}\r\nPING\r\n"))
	for k.line() != "PONG" {
	}
	return k
}

func (k *conn) line() string {
	l, err := k.r.ReadString('\n')
	if err != nil {
		panic(err)
	}
	return strings.TrimRight(l, "\r\n")
}

// The next MSG: its reply subject and payload
func (k *conn) msg() (string, []byte) {
	for {
		l := k.line()
		if l == "PING" {
			k.c.Write([]byte("PONG\r\n"))
			continue
		}
		if !strings.HasPrefix(l, "MSG ") {
			continue
		}
		f := strings.Fields(l)
		n, _ := strconv.Atoi(f[len(f)-1])
		reply := ""
		if len(f) == 5 {
			reply = f[3]
		}
		b := make([]byte, n+2)
		if _, err := io.ReadFull(k.r, b); err != nil {
			panic(err)
		}
		return reply, b[:n]
	}
}

func main() {
	what, addr := os.Args[1], os.Args[2]
	data := strings.Repeat("d", 64)
	nc := dial(addr)
	switch what {
	case "nats_publish":
		n := count(3, 1000000)
		t0 := time.Now()
		for i := 0; i < n; i++ {
			nc.c.Write([]byte("PUB bench 64\r\n" + data + "\r\n"))
		}
		nc.c.Write([]byte("PING\r\n"))
		for nc.line() != "PONG" {
		}
		report(what, time.Since(t0), n)
	case "nats_pubsub":
		n := count(3, 500000)
		sc := dial(addr)
		sc.c.Write([]byte("SUB bench.sub 1\r\nPING\r\n"))
		for sc.line() != "PONG" {
		}
		t0 := time.Now()
		go func() {
			for i := 0; i < n; i++ {
				nc.c.Write([]byte("PUB bench.sub 64\r\n" + data + "\r\n"))
			}
		}()
		for i := 0; i < n; i++ {
			if _, b := sc.msg(); len(b) != 64 {
				panic("size")
			}
		}
		report(what, time.Since(t0), n)
	case "nats_request":
		n := count(3, 50000)
		rc := dial(addr)
		rc.c.Write([]byte("SUB bench.svc 1\r\nPING\r\n"))
		for rc.line() != "PONG" {
		}
		go func() {
			for {
				reply, b := rc.msg()
				rc.c.Write([]byte("PUB " + reply + " " + strconv.Itoa(len(b)) + "\r\n" + string(b) + "\r\n"))
			}
		}()
		nc.c.Write([]byte("SUB _INBOX.go.* 9\r\nPING\r\n"))
		for nc.line() != "PONG" {
		}
		t0 := time.Now()
		for i := 0; i < n; i++ {
			nc.c.Write([]byte("PUB bench.svc _INBOX.go." + strconv.Itoa(i) + " 64\r\n" + data + "\r\n"))
			if _, b := nc.msg(); len(b) != 64 {
				panic("size")
			}
		}
		report(what, time.Since(t0), n)
	}
}
