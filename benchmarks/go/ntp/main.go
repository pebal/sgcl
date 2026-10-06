// SNTP in Go's standard library alone: Go has no NTP client, so this side is
// a minimal one by hand from RFC 4330 over net.UDPConn — a socket a query,
// as the module's query opens one — against bench_ntp's server
// (benchmarks/net/ntp.cpp has the SGCL side). Prints one line, ns per
// operation.
//
//	ntp_query ADDR [n]       a query and its answer, the offset computed: per query
//	udp_roundtrip ADDR [n]   a datagram of 48 bytes and its answer over one socket: per round trip
package main

import (
	"encoding/binary"
	"fmt"
	"net"
	"os"
	"strconv"
	"time"
)

func report(what string, d time.Duration, n int) {
	fmt.Printf("ntp %s ns/op=%.1f ops/s=%.0f wall=%.2fs\n", what, float64(d.Nanoseconds())/float64(n), float64(n)/d.Seconds(), d.Seconds())
}

func count(i, def int) int {
	if len(os.Args) > i {
		if n, err := strconv.Atoi(os.Args[i]); err == nil {
			return n
		}
	}
	return def
}

func toNtp(t time.Time) uint64 {
	secs := uint64(t.Unix() + 2208988800)
	frac := uint64(t.Nanosecond()) << 32 / 1000000000
	return secs<<32 | frac
}

func fromNtp(v uint64) time.Time {
	secs := int64(v>>32) - 2208988800
	nanos := int64((v & 0xFFFFFFFF) * 1000000000 >> 32)
	return time.Unix(secs, nanos)
}

func query(addr string) (time.Duration, error) {
	c, err := net.Dial("udp", addr)
	if err != nil {
		return 0, err
	}
	defer c.Close()
	c.SetReadDeadline(time.Now().Add(5 * time.Second))
	var req [48]byte
	req[0] = 4<<3 | 3
	t1 := time.Now()
	binary.BigEndian.PutUint64(req[40:], toNtp(t1))
	if _, err := c.Write(req[:]); err != nil {
		return 0, err
	}
	var resp [512]byte
	n, err := c.Read(resp[:])
	t4 := time.Now()
	if err != nil || n < 48 || resp[0]&7 != 4 || binary.BigEndian.Uint64(resp[24:]) != binary.BigEndian.Uint64(req[40:]) {
		return 0, fmt.Errorf("bad answer")
	}
	t2 := fromNtp(binary.BigEndian.Uint64(resp[32:]))
	t3 := fromNtp(binary.BigEndian.Uint64(resp[40:]))
	return (t2.Sub(t1) + t3.Sub(t4)) / 2, nil
}

func main() {
	what, addr := os.Args[1], os.Args[2]
	n := count(3, 30000)
	switch what {
	case "ntp_query":
		t0 := time.Now()
		for i := 0; i < n; i++ {
			if _, err := query(addr); err != nil {
				panic(err)
			}
		}
		report(what, time.Since(t0), n)
	case "udp_roundtrip":
		c, err := net.Dial("udp", addr)
		if err != nil {
			panic(err)
		}
		var req [48]byte
		req[0] = 4<<3 | 3
		var resp [512]byte
		t0 := time.Now()
		for i := 0; i < n; i++ {
			c.Write(req[:])
			if _, err := c.Read(resp[:]); err != nil {
				panic(err)
			}
		}
		report(what, time.Since(t0), n)
	}
}
