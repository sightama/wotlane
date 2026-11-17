#!/usr/bin/env python3
# ABOUTME: UDP proxy that adds a round trip and jitter between game clients and wotm_server (ENet), one listening port per player,
# ABOUTME: so the server's own ping sees the latency. Packets in each direction keep their order. Used by tools/net_check.py --rtt.
"""Usage: python3 tools/net_lagproxy.py --to <host>:<port> --link <listen port>:<rtt ms>[:<jitter ms>] [--link ...] [--seed N]
  Each link is one client: its packets go to the server and the server's replies come back, each way delayed by half the round
  trip plus a random part spread evenly over the jitter (rtt/2 - jitter/2 to rtt/2 + jitter/2), never earlier than the packet
  before it in the same direction. Prints one 'listening' line per link once ready."""
import argparse
import asyncio
import random
import sys


def parse_link(spec):
    parts = spec.split(':')
    if len(parts) not in (2, 3):
        raise ValueError(f'bad link {spec!r}: expected <listen port>:<rtt ms>[:<jitter ms>]')
    return int(parts[0]), float(parts[1]), float(parts[2]) if len(parts) == 3 else 0.0


def one_way_ms(rtt, jitter, x):
    """One direction's delay for a uniform draw x in [0, 1)."""
    return max(0.0, rtt / 2 + (x - 0.5) * jitter)


class Direction:
    """Delays packets by the link's one-way time, keeping their order."""

    def __init__(self, loop, rtt, jitter, rng):
        self.loop, self.rtt, self.jitter, self.rng = loop, rtt, jitter, rng
        self.last = 0.0

    def send(self, deliver):
        due = max(self.last + 1e-6, self.loop.time() + one_way_ms(self.rtt, self.jitter, self.rng.random()) / 1000)
        self.last = due
        self.loop.call_at(due, deliver)


class Upstream(asyncio.DatagramProtocol):
    """The proxy's socket toward the server for one link: replies go back to the client."""

    def __init__(self, link):
        self.link = link

    def datagram_received(self, data, addr):
        self.link.down.send(lambda: self.link.listener.sendto(data, self.link.client))


class Link(asyncio.DatagramProtocol):
    def __init__(self, loop, server, rtt, jitter, rng):
        self.loop, self.server = loop, server
        self.up = Direction(loop, rtt, jitter, rng)
        self.down = Direction(loop, rtt, jitter, rng)
        self.listener = self.upstream = self.client = None

    def connection_made(self, transport):
        self.listener = transport

    def datagram_received(self, data, addr):
        self.client = addr
        self.up.send(lambda: self.upstream.sendto(data, self.server))


async def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--to', required=True)
    ap.add_argument('--link', action='append', required=True)
    ap.add_argument('--seed', type=int, default=1)
    a = ap.parse_args()
    host, port = a.to.rsplit(':', 1)
    server = (host, int(port))
    loop = asyncio.get_running_loop()
    rng = random.Random(a.seed)
    for spec in a.link:
        listen, rtt, jitter = parse_link(spec)
        link = Link(loop, server, rtt, jitter, rng)
        await loop.create_datagram_endpoint(lambda link=link: link, local_addr=('127.0.0.1', listen))
        link.upstream, _ = await loop.create_datagram_endpoint(lambda link=link: Upstream(link), remote_addr=server)
        print(f'listening on {listen}: rtt {rtt:g} ms, jitter {jitter:g} ms', flush=True)
    await asyncio.Event().wait()


if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        sys.exit(0)
