// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The Go side of the multicast interoperability test (tests/net/multicast.cpp),
// an oracle only, the standard library alone:
//
//	multicast_oracle -listen 239.1.2.3:5000 -if lo0 -n 3
//	    net.ListenMulticastUDP on the group and the interface; prints
//	    "LISTEN", then "GOT <text>" for each of n datagrams, "TIMEOUT" when
//	    five seconds pass without one
//	multicast_oracle -send 239.1.2.3:5000 -if lo0 -n 3 -msg hello
//	    a UDP socket of the group's family on the loopback address, its
//	    outgoing multicast interface set to the interface (Go's net has no
//	    call for it: IP_MULTICAST_IF / IPV6_MULTICAST_IF through
//	    syscall), sends "<msg> <i>" n times, 20 ms apart; prints "SENT"
//
// Built by the test with `go build`.
package main

import (
	"flag"
	"fmt"
	"net"
	"os"
	"syscall"
	"time"
)

func fail(err error) {
	fmt.Println("ERROR", err)
	os.Exit(1)
}

func main() {
	listen := flag.String("listen", "", "group:port to receive")
	send := flag.String("send", "", "group:port to send to")
	ifname := flag.String("if", "", "the interface")
	n := flag.Int("n", 1, "datagrams")
	msg := flag.String("msg", "hello", "the text sent")
	flag.Parse()
	ifi, err := net.InterfaceByName(*ifname)
	if err != nil {
		fail(err)
	}
	if *listen != "" {
		g, err := net.ResolveUDPAddr("udp", *listen)
		if err != nil {
			fail(err)
		}
		network := "udp4"
		if g.IP.To4() == nil {
			network = "udp6"
		}
		c, err := net.ListenMulticastUDP(network, ifi, g)
		if err != nil {
			fail(err)
		}
		fmt.Println("LISTEN")
		buf := make([]byte, 2048)
		for i := 0; i < *n; i++ {
			c.SetReadDeadline(time.Now().Add(5 * time.Second))
			k, _, err := c.ReadFromUDP(buf)
			if err != nil {
				fmt.Println("TIMEOUT")
				os.Exit(1)
			}
			fmt.Printf("GOT %s\n", buf[:k])
		}
		return
	}
	g, err := net.ResolveUDPAddr("udp", *send)
	if err != nil {
		fail(err)
	}
	v4 := g.IP.To4() != nil
	var c *net.UDPConn
	if v4 {
		c, err = net.ListenUDP("udp4", &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1)})
	} else {
		c, err = net.ListenUDP("udp6", &net.UDPAddr{IP: net.IPv6loopback})
	}
	if err != nil {
		fail(err)
	}
	raw, err := c.SyscallConn()
	if err != nil {
		fail(err)
	}
	var serr error
	raw.Control(func(fd uintptr) {
		if v4 {
			serr = syscall.SetsockoptInet4Addr(int(fd), syscall.IPPROTO_IP, syscall.IP_MULTICAST_IF, [4]byte{127, 0, 0, 1})
		} else {
			serr = syscall.SetsockoptInt(int(fd), syscall.IPPROTO_IPV6, syscall.IPV6_MULTICAST_IF, ifi.Index)
		}
	})
	if serr != nil {
		fail(serr)
	}
	if !v4 {
		g.Zone = ifi.Name
	}
	for i := 0; i < *n; i++ {
		if _, err := c.WriteToUDP([]byte(fmt.Sprintf("%s %d", *msg, i)), g); err != nil {
			fail(err)
		}
		time.Sleep(20 * time.Millisecond)
	}
	fmt.Println("SENT")
}
