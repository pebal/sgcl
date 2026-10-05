// The multicast case of the net benchmark (benchmarks/net/net.cpp has the
// SGCL side, the same work).
//
//	udp_multicast [n]  64 B sent to a group of 239.255/16 on the loopback interface and
//	                   received by a socket that joined it (net.ListenMulticastUDP), one
//	                   after the other: per datagram
//
// Go's net has no call for the outgoing multicast interface; the sender's
// IP_MULTICAST_IF is set through syscall, as the SGCL side's
// set_multicast_interface sets it.
package main

import (
	"net"
	"syscall"
	"time"
)

func benchMulticast(n int64) bool {
	if n == 0 {
		n = 200000
	}
	var lo *net.Interface
	ifs, _ := net.Interfaces()
	for i := range ifs {
		if ifs[i].Flags&net.FlagLoopback != 0 && ifs[i].Flags&net.FlagMulticast != 0 {
			lo = &ifs[i]
			break
		}
	}
	in, err := net.ListenMulticastUDP("udp4", lo, &net.UDPAddr{IP: net.IPv4(239, 255, 77, 1), Port: 0})
	if err != nil {
		panic(err)
	}
	out, err := net.ListenUDP("udp4", &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1)})
	if err != nil {
		panic(err)
	}
	raw, _ := out.SyscallConn()
	raw.Control(func(fd uintptr) {
		syscall.SetsockoptInet4Addr(int(fd), syscall.IPPROTO_IP, syscall.IP_MULTICAST_IF, [4]byte{127, 0, 0, 1})
	})
	to := &net.UDPAddr{IP: net.IPv4(239, 255, 77, 1), Port: in.LocalAddr().(*net.UDPAddr).Port}
	in.SetReadDeadline(time.Now().Add(60 * time.Second))
	block := make([]byte, 64)
	room := make([]byte, 2048)
	rounds := func(count int64) int64 {
		ok := int64(0)
		for i := int64(0); i < count; i++ {
			if _, err := out.WriteToUDP(block, to); err != nil {
				break
			}
			k, _, err := in.ReadFromUDP(room)
			if err != nil {
				break
			}
			if k == 64 {
				ok++
			}
		}
		return ok
	}
	rounds(1000)
	t0 := time.Now()
	got := rounds(n)
	report("udp_multicast", time.Since(t0).Seconds(), float64(n), "")
	in.Close()
	out.Close()
	return got == n
}
