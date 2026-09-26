// The net module's stage 1a counterparts in Go: net and net/netip, one case
// per run (benchmarks/net/net.cpp has the SGCL side, the same cases).
// Prints one line, ns per operation.
//
//	pingpong [n]   64 B there and back over one TCP connection, both ends goroutines: per round trip
//	stream [mb]    mb megabytes (1024 by default) one way over one connection, 32 KB writes: per byte, and GB/s
//	connect [n]    net.Dial and Accept on the loopback, then both closed: per connection
//	parse [n]      netip.ParseAddr of a mix of IPv4 and IPv6 text: per address
//	format [n]     netip.Addr.String of the same mix: per address
package main

import (
	"bufio"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/netip"
	"net/url"
	"os"
	"strconv"
	"strings"
	"syscall"
	"time"
)

var urls = []string{"http://example.com/", "https://user:pass@example.com:8443/a/b/c?x=1&y=2#top", "http://10.0.0.1/index.html", "https://[2001:db8::1]:443/", "http://example.com/a/../b/./c/d?q=hello%20world", "ftp://ftp.example.org/pub/file.tar.gz", "https://api.example.com/v1/users/12345/orders?limit=50&offset=100", "ws://chat.example.com/socket"}

var addresses = []string{"127.0.0.1", "10.1.2.3", "192.168.100.200", "8.8.8.8", "::1", "2001:db8::1", "fe80::1:2:3:4", "2001:db8:85a3::8a2e:370:7334", "::ffff:192.0.2.128", "2606:4700:4700::1111"}

func cpuSeconds() float64 {
	var ru syscall.Rusage
	syscall.Getrusage(syscall.RUSAGE_SELF, &ru)
	return float64(ru.Utime.Sec) + float64(ru.Utime.Usec)*1e-6 + float64(ru.Stime.Sec) + float64(ru.Stime.Usec)*1e-6
}

func report(what string, wall float64, ops float64, extra string) {
	fmt.Printf("net %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs%s\n", what, wall*1e9/ops, ops/wall, wall, cpuSeconds(), extra)
}

func listen() net.Listener {
	l, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		panic(err)
	}
	return l
}

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: net <pingpong|stream|connect|parse|format|url|http_parse|http_hello> [n]")
		os.Exit(2)
	}
	what := os.Args[1]
	var n int64
	if len(os.Args) > 2 {
		n, _ = strconv.ParseInt(os.Args[2], 10, 64)
	}
	ok := true
	switch what {
	case "pingpong":
		if n == 0 {
			n = 50000
		}
		l := listen()
		go func() {
			c, err := l.Accept()
			if err != nil {
				return
			}
			io.Copy(c, c)
			c.Close()
		}()
		c, err := net.Dial("tcp", l.Addr().String())
		if err != nil {
			panic(err)
		}
		buf := make([]byte, 64)
		t0 := time.Now()
		done := make(chan int64)
		go func() {
			var k int64
			for ; k < n; k++ {
				if _, err := c.Write(buf); err != nil {
					break
				}
				if _, err := io.ReadFull(c, buf); err != nil {
					break
				}
			}
			c.Close()
			done <- k
		}()
		k := <-done
		report("pingpong", time.Since(t0).Seconds(), float64(n), "")
		l.Close()
		ok = k == n
	case "stream":
		mb := n
		if mb == 0 {
			mb = 1024
		}
		bytes := mb << 20
		l := listen()
		received := make(chan int64)
		go func() {
			c, err := l.Accept()
			if err != nil {
				received <- 0
				return
			}
			buf := make([]byte, 32768)
			var total int64
			for {
				r, err := c.Read(buf)
				total += int64(r)
				if err != nil {
					break
				}
			}
			c.Close()
			received <- total
		}()
		c, err := net.Dial("tcp", l.Addr().String())
		if err != nil {
			panic(err)
		}
		buf := make([]byte, 32768)
		t0 := time.Now()
		var sent int64
		for sent < bytes {
			w, err := c.Write(buf)
			if err != nil {
				break
			}
			sent += int64(w)
		}
		c.Close()
		got := <-received
		wall := time.Since(t0).Seconds()
		report("stream", wall, float64(got), fmt.Sprintf(" GB/s=%.2f", float64(got)/wall/1e9))
		l.Close()
		ok = got == sent
	case "connect":
		if n == 0 {
			n = 2000 // RUNS of both variants inside the 16384 ephemeral ports a TIME_WAIT of 30 s leaves
		}
		l := listen()
		taken := make(chan int64)
		go func() {
			var k int64
			for ; k < n; k++ {
				c, err := l.Accept()
				if err != nil {
					break
				}
				c.Close()
			}
			taken <- k
		}()
		t0 := time.Now()
		var made int64
		for ; made < n; made++ {
			c, err := net.Dial("tcp", l.Addr().String())
			if err != nil {
				break
			}
			c.Close()
		}
		if made < n {
			l.Close() // a dial failed (the ports ran out): the accepts end rather than wait for connections that will not come
		}
		k := <-taken
		report("connect", time.Since(t0).Seconds(), float64(n), "")
		l.Close()
		ok = made == n && k == n
	case "parse", "format":
		if n == 0 {
			n = 20000000
		}
		parsed := make([]netip.Addr, len(addresses))
		for i, s := range addresses {
			parsed[i] = netip.MustParseAddr(s)
		}
		check := 0
		t0 := time.Now()
		if what == "parse" {
			for i := int64(0); i < n; i++ {
				a, _ := netip.ParseAddr(addresses[i%int64(len(addresses))])
				if a.Is4() {
					check++
				}
			}
		} else {
			for i := int64(0); i < n; i++ {
				check += len(parsed[i%int64(len(parsed))].String())
			}
		}
		report(what, time.Since(t0).Seconds(), float64(n), "")
		ok = check > 0
	case "url":
		if n == 0 {
			n = 2000000
		}
		check := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			u, _ := url.Parse(urls[i%int64(len(urls))])
			check += len(u.EscapedPath())
		}
		report("url", time.Since(t0).Seconds(), float64(n), "")
		ok = check > 0
	case "http_parse":
		if n == 0 {
			n = 2000000
		}
		text := "GET /api/v1/users/12345?fields=name,email HTTP/1.1\r\nHost: api.example.com\r\nUser-Agent: bench/1.0\r\n" +
			"Accept: application/json\r\nAccept-Encoding: gzip, deflate\r\nAccept-Language: en-US,en;q=0.9\r\n" +
			"Connection: keep-alive\r\nCookie: session=abc123; theme=dark\r\nReferer: https://example.com/page\r\n" +
			"Cache-Control: no-cache\r\nX-Request-Id: 7f3c9a2e-1b4d-4e6f-8a9b-0c1d2e3f4a5b\r\n" +
			"Authorization: Bearer eyJhbGciOiJIUzI1NiJ9.e30.x\r\nX-Forwarded-For: 203.0.113.7\r\n\r\n"
		check := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			r, err := http.ReadRequest(bufio.NewReader(strings.NewReader(text)))
			if err == nil {
				check += len(r.Header) + 1
			}
		}
		report("http_parse", time.Since(t0).Seconds(), float64(n), "")
		ok = check == int(n)*12
	case "http_hello":
		if n == 0 {
			n = 50000
		}
		l := listen()
		mux := http.NewServeMux()
		mux.HandleFunc("GET /hello", func(w http.ResponseWriter, r *http.Request) { io.WriteString(w, "hello, world\n") })
		srv := &http.Server{Handler: mux}
		go srv.Serve(l)
		url := "http://" + l.Addr().String() + "/hello"
		c := &http.Client{}
		check := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			res, err := c.Get(url)
			if err == nil {
				b, _ := io.ReadAll(res.Body)
				res.Body.Close()
				check += len(b)
			}
		}
		report("http_hello", time.Since(t0).Seconds(), float64(n), "")
		srv.Close()
		ok = check == int(n)*13
	default:
		fmt.Fprintf(os.Stderr, "unknown case %s\n", what)
		os.Exit(2)
	}
	if !ok {
		os.Exit(1)
	}
}
