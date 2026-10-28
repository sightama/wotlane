// ABOUTME: Jogador como host para o launcher: roda o wotm_server escondido como processo filho, espera a porta, lista os enderecos que
// ABOUTME: os amigos podem usar (internet, Tailscale, LAN) e abre a porta no roteador por UPnP. Testado por LobbyTest.cs.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Threading;

// O mesmo wotm_server do servidor dedicado, filho deste launcher: fecha junto com ele. Se a porta ja tem um servidor (outro launcher,
// a janela do wotm_server), usa esse e nao o fecha.
class ServerHost : IDisposable
{
    public readonly int Port;
    readonly Process proc;
    readonly StreamWriter log;
    readonly List<string> mapped = new List<string>();

    public bool Owned { get { return proc != null; } }
    public bool Running { get { return proc != null ? !proc.HasExited : PortOpen(Port, 500); } }

    ServerHost(int port, Process p, StreamWriter w)
    {
        Port = port;
        proc = p;
        log = w;
    }

    public static ServerHost Start(string exe, int port, string logPath, int timeoutMs)
    {
        if (PortOpen(port, 300)) return new ServerHost(port, null, null);
        if (!File.Exists(exe)) throw new LobbyError("nao achei " + exe + " (tools\\build_net.bat)");
        StreamWriter w = null;
        if (logPath != null) w = new StreamWriter(logPath, false) { AutoFlush = true };
        var psi = new ProcessStartInfo(exe, "--port " + port)
        {
            UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true,
            WorkingDirectory = Path.GetDirectoryName(exe)
        };
        Process p = Process.Start(psi);
        // le as duas saidas sempre: com o cano cheio o servidor travaria no meio da partida
        DataReceivedEventHandler write = delegate(object s, DataReceivedEventArgs e)
        {
            if (e.Data == null || w == null) return;
            try { lock (w) w.WriteLine(e.Data); } catch (ObjectDisposedException) { }   // ultimas linhas depois do Dispose
        };
        p.OutputDataReceived += write;
        p.ErrorDataReceived += write;
        p.BeginOutputReadLine();
        p.BeginErrorReadLine();
        var h = new ServerHost(port, p, w);
        for (int waited = 0; waited < timeoutMs; waited += 100)
        {
            if (p.HasExited) { h.Dispose(); throw new LobbyError("o wotm_server fechou ao abrir a porta " + port + " (outro programa nela?)"); }
            if (PortOpen(port, 100)) return h;
            Thread.Sleep(100);
        }
        h.Dispose();
        throw new LobbyError("o wotm_server nao abriu a porta " + port);
    }

    public static bool PortOpen(int port, int timeoutMs)
    {
        using (var c = new TcpClient())
        {
            try
            {
                IAsyncResult ar = c.BeginConnect(IPAddress.Loopback, port, null, null);
                if (!ar.AsyncWaitHandle.WaitOne(timeoutMs)) return false;
                c.EndConnect(ar);
                return c.Connected;
            }
            catch (SocketException) { return false; }
        }
    }

    // "Tailscale" (100.64.0.0/10), "LAN" (enderecos privados), "public"; null para loopback e link-local (inuteis para um amigo)
    public static string Kind(IPAddress a)
    {
        if (a.AddressFamily != AddressFamily.InterNetwork) return null;
        byte[] b = a.GetAddressBytes();
        if (b[0] == 127 || (b[0] == 169 && b[1] == 254) || b[0] == 0) return null;
        if (b[0] == 100 && (b[1] & 0xC0) == 64) return "Tailscale";
        if (b[0] == 10 || (b[0] == 172 && (b[1] & 0xF0) == 16) || (b[0] == 192 && b[1] == 168)) return "LAN";
        return "public";
    }

    // linhas para mostrar ao host, na ordem de quem e mais provavel de servir a um amigo: internet (UPnP), Tailscale, LAN
    public static List<string> ShareLines(IEnumerable<IPAddress> local, string external, int port)
    {
        var lines = new List<string>();
        if (!string.IsNullOrEmpty(external)) lines.Add("Internet: " + external + ":" + port);
        foreach (string kind in new[] { "Tailscale", "public", "LAN" })
            foreach (IPAddress a in local)
                if (Kind(a) == kind) lines.Add((kind == "public" ? "Internet" : kind) + ": " + a + ":" + port);
        return lines.Distinct().ToList();
    }

    // Tailscale de qualquer placa; os outros so de placas com gateway (a do WSL/Hyper-V nao tem e nao serve a um amigo)
    public static List<IPAddress> LocalAddresses()
    {
        var list = new List<IPAddress>();
        foreach (NetworkInterface ni in NetworkInterface.GetAllNetworkInterfaces())
        {
            if (ni.OperationalStatus != OperationalStatus.Up) continue;
            IPInterfaceProperties ip = ni.GetIPProperties();
            foreach (UnicastIPAddressInformation u in ip.UnicastAddresses)
                if (Kind(u.Address) == "Tailscale" || (Kind(u.Address) != null && HasGateway(ip))) list.Add(u.Address);
        }
        return list;
    }

    static bool HasGateway(IPInterfaceProperties ip)
    {
        return ip.GatewayAddresses.Any(g => g.Address.AddressFamily == AddressFamily.InterNetwork && !g.Address.Equals(IPAddress.Any));
    }

    // o endereco LAN da placa que tem gateway (o roteador encaminha a porta para ele); null sem nenhuma
    public static IPAddress RouterFacingAddress()
    {
        foreach (NetworkInterface ni in NetworkInterface.GetAllNetworkInterfaces())
        {
            if (ni.OperationalStatus != OperationalStatus.Up) continue;
            IPInterfaceProperties ip = ni.GetIPProperties();
            if (!HasGateway(ip)) continue;
            foreach (UnicastIPAddressInformation u in ip.UnicastAddresses)
                if (Kind(u.Address) == "LAN") return u.Address;
        }
        return null;
    }

    // Abre a porta (TCP e UDP) no roteador pelo UPnP do Windows (HNetCfg.NATUPnP). Devolve o IP externo, ou null com o motivo em why.
    // Demora alguns segundos: chame fora da thread da interface.
    public string MapUpnp(out string why)
    {
        why = null;
        IPAddress lan = RouterFacingAddress();
        if (lan == null) { why = "no LAN address with a router"; return null; }
        try
        {
            dynamic nat = Activator.CreateInstance(Type.GetTypeFromProgID("HNetCfg.NATUPnP"));
            dynamic maps = nat.StaticPortMappingCollection;
            if (maps == null) { why = "the router does not offer UPnP (or it is off)"; return null; }
            string external = null;
            foreach (string proto in new[] { "TCP", "UDP" })
            {
                dynamic m = maps.Add(Port, proto, Port, lan.ToString(), true, "WotM wotm_server");
                lock (mapped) mapped.Add(proto);
                if (m != null && external == null) external = m.ExternalIPAddress;
            }
            if (string.IsNullOrEmpty(external) || Kind(IPAddress.Parse(external)) != "public")
            {
                why = "the router has no public address (" + (external ?? "none") + "): likely CGNAT, use Tailscale";
                return null;
            }
            return external;
        }
        catch (Exception e) { why = "UPnP failed: " + e.Message; return null; }
    }

    void UnmapUpnp()
    {
        string[] protos;
        lock (mapped) { protos = mapped.ToArray(); mapped.Clear(); }
        if (protos.Length == 0) return;
        try
        {
            dynamic nat = Activator.CreateInstance(Type.GetTypeFromProgID("HNetCfg.NATUPnP"));
            dynamic maps = nat.StaticPortMappingCollection;
            if (maps != null) foreach (string proto in protos) maps.Remove(Port, proto);
        }
        catch (Exception) { }
    }

    public void Dispose()
    {
        UnmapUpnp();
        if (proc != null)
        {
            try { if (!proc.HasExited) { proc.Kill(); proc.WaitForExit(2000); } } catch (Exception) { }
            proc.Dispose();
        }
        if (log != null) lock (log) log.Dispose();
    }
}
