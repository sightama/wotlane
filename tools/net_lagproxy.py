#!/usr/bin/env python3
# ABOUTME: Latency proxy between game clients and wotm_server that adds a round trip, jitter and periodic spikes, one listening port per
# ABOUTME: player, so the server's own ping sees them: UDP for ENet (tools/net_check.py --rtt), --tcp for the browsers' WebSocket.
"""Usage: python3 tools/net_lagproxy.py --to <host>:<port> --link <listen port>:<rtt ms>[:<jitter ms>] [--link ...] [--tcp]
                                    [--spike <listen port>:<every s>:<extra rtt ms>:<length ms>] [--seed N]
  Each link is one client: its packets (UDP) or bytes (--tcp: every connection to the listen port gets its own to the server, which is
  how a browser's /lobby and /match WebSockets go through) go to the server and the replies come back, each way delayed by half the
  round trip plus a random part spread evenly over the jitter (rtt/2 - jitter/2 to rtt/2 + jitter/2), never earlier than the data
  before it in the same direction. A spike adds its extra round trip to that link for `length` ms every `every` s (a ping that jumps
  from 60 to 230 now and then, as a home connection does). Prints one 'listening' line per link once ready."""
import argparse
import asyncio
import random
import sys


def parse_link(spec):
    parts = spec.split(':')
    if len(parts) not in (2, 3):
        raise ValueError(f'bad link {spec!r}: expected <listen port>:<rtt ms>[:<jitter ms>]')
    return int(parts[0]), float(parts[1]), float(parts[2]) if len(parts) == 3 else 0.0


def parse_spike(spec):
    parts = spec.split(':')
    if len(parts) != 4:
        raise ValueError(f'bad spike {spec!r}: expected <listen port>:<every s>:<extra rtt ms>:<length ms>')
    return int(parts[0]), float(parts[1]), float(parts[2]), float(parts[3])


def spike_extra_ms(spike, t):
    """The extra round trip at `t` s since the proxy started: (every s, extra ms, length ms), or None for no spikes."""
    if not spike:
        return 0.0
    every, extra, length = spike
    return extra if t >= every and (t % every) * 1000 < length else 0.0


def one_way_ms(rtt, jitter, x):
    """One direction's delay for a uniform draw x in [0, 1)."""
    return max(0.0, rtt / 2 + (x - 0.5) * jitter)


class Direction:
    """Delays packets by the link's one-way time, keeping their order."""

    def __init__(self, loop, rtt, jitter, rng, spike=None, t0=0.0):
        self.loop, self.rtt, self.jitter, self.rng, self.spike, self.t0 = loop, rtt, jitter, rng, spike, t0
        self.last = 0.0

    def send(self, deliver):
        now = self.loop.time()
        rtt = self.rtt + spike_extra_ms(self.spike, now - self.t0)
        due = max(self.last + 1e-6, now + one_way_ms(rtt, self.jitter, self.rng.random()) / 1000)
        self.last = due
        self.loop.call_at(due, deliver)


class Upstream(asyncio.DatagramProtocol):
    """The proxy's socket toward the server for one link: replies go back to the client."""

    def __init__(self, link):
        self.link = link

    def datagram_received(self, data, addr):
        self.link.down.send(lambda: self.link.listener.sendto(data, self.link.client))


class Link(asyncio.DatagramProtocol):
    def __init__(self, loop, server, rtt, jitter, rng, spike=None, t0=0.0):
        self.loop, self.server = loop, server
        self.up = Direction(loop, rtt, jitter, rng, spike, t0)
        self.down = Direction(loop, rtt, jitter, rng, spike, t0)
        self.listener = self.upstream = self.client = None

    def connection_made(self, transport):
        self.listener = transport

    def datagram_received(self, data, addr):
        self.client = addr
        self.up.send(lambda: self.upstream.sendto(data, self.server))


async def pump(reader, writer, direction):
    """Copies a byte stream, each chunk after the direction's delay; closes the far side when the near one ends."""
    try:
        while data := await reader.read(65536):
            direction.send(lambda data=data: writer.write(data))
    except ConnectionError:
        pass
    direction.send(writer.close)


async def tcp_link(loop, server, listen, rtt, jitter, rng, spike, t0):
    async def accepted(client_reader, client_writer):
        try:
            server_reader, server_writer = await asyncio.open_connection(*server)
        except OSError:
            client_writer.close()
            return
        up, down = Direction(loop, rtt, jitter, rng, spike, t0), Direction(loop, rtt, jitter, rng, spike, t0)
        await asyncio.gather(pump(client_reader, server_writer, up), pump(server_reader, client_writer, down))

    return await asyncio.start_server(accepted, '127.0.0.1', listen)


async def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--to', required=True)
    ap.add_argument('--link', action='append', required=True)
    ap.add_argument('--spike', action='append', default=[])
    ap.add_argument('--tcp', action='store_true')
    ap.add_argument('--seed', type=int, default=1)
    a = ap.parse_args()
    host, port = a.to.rsplit(':', 1)
    server = (host, int(port))
    loop = asyncio.get_running_loop()
    t0 = loop.time()
    rng = random.Random(a.seed)
    spikes = {p: (every, extra, length) for p, every, extra, length in map(parse_spike, a.spike)}
    servers = []
    for spec in a.link:
        listen, rtt, jitter = parse_link(spec)
        spike = spikes.get(listen)
        if a.tcp:
            servers.append(await tcp_link(loop, server, listen, rtt, jitter, rng, spike, t0))
        else:
            link = Link(loop, server, rtt, jitter, rng, spike, t0)
            await loop.create_datagram_endpoint(lambda link=link: link, local_addr=('127.0.0.1', listen))
            link.upstream, _ = await loop.create_datagram_endpoint(lambda link=link: Upstream(link), remote_addr=server)
        extra = f', spikes of +{spike[1]:g} ms for {spike[2]:g} ms every {spike[0]:g} s' if spike else ''
        print(f'listening on {listen} ({"tcp" if a.tcp else "udp"}): rtt {rtt:g} ms, jitter {jitter:g} ms{extra}', flush=True)
    await asyncio.Event().wait()


if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        sys.exit(0)
