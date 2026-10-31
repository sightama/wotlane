// ABOUTME: Test of LobbyClient.cs against the real wotm_server (native\net\build\wotm_server.exe): lists, joins, starts and checks
// ABOUTME: each player's environment. Build and run: tools\launcher\test.bat (exits with 0 on success).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Threading;

static class LobbyTest
{
    static int failures, checks;

    static void Check(bool ok, string what)
    {
        ++checks;
        if (!ok) { ++failures; Console.WriteLine("  FAILED " + what); }
    }

    static int Main(string[] args)
    {
        string server = args.Length > 0 ? args[0] : Path.Combine(AppDomain.CurrentDomain.BaseDirectory, @"..\..\native\net\build\wotm_server.exe");
        if (!File.Exists(server)) { Console.WriteLine("could not find " + server + " (tools\\build_net.bat)"); return 2; }
        const int port = 7897;
        var p = Process.Start(new ProcessStartInfo(server, "--port " + port) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true });
        try
        {
            Thread.Sleep(300);
            Run("list_and_match", delegate
            {
                using (var watcher = new LobbyClient("127.0.0.1", port, "b1", 2000))
                using (var a = new LobbyClient("127.0.0.1", port, "b1", 2000))
                using (var b = new LobbyClient("127.0.0.1", port, "b1", 2000))
                {
                    Check(watcher.Lobbies == 0, "no open lobby");
                    Check(watcher.List(2000).Count == 0, "initial list empty");
                    string code = a.Create("Sala", "Ana", 3, null, 2000);
                    Check(code.Length == 4, "4-letter code");
                    Check(a.Settings["stage"] == "1" && a.Settings["wins"] == "0", "initial stage and wins");
                    b.Join(code.ToLowerInvariant(), "Bia", 1, null, 2000);
                    List<LobbyClient.LobbyInfo> l = watcher.List(2000);
                    Check(l.Count == 1 && l[0].Code == code && l[0].State == "waiting" && !l[0].Locked, "lobby in the list");
                    Check(l[0].Name == "Sala" && l[0].Host == "Ana" && l[0].Names.Length == 2 && l[0].Names[1] == "Bia", "name, owner and members");
                    Dictionary<string, string> s = a.Set(4, 2, 2000);
                    Check(s["stage"] == "4" && s["wins"] == "2", "owner's SET");
                    a.Send("START");
                    Dictionary<string, string> ea = LobbyClient.MatchEnv(LobbyClient.KeyValues(a.Expect("MATCH", 2000)), "127.0.0.1");
                    Dictionary<string, string> eb = LobbyClient.MatchEnv(LobbyClient.KeyValues(b.Expect("MATCH", 2000)), "127.0.0.1");
                    Check(ea["WOTM_MATCH"] == "3,1,4" && eb["WOTM_MATCH"] == "3,1,4", "WOTM_MATCH");
                    Check(ea["WOTM_KILLTARGET"] == "2" && eb["WOTM_KILLTARGET"] == "2", "wins");
                    Check(ea["WOTM_PLAYERS"] == "2" && ea["WOTM_FIXEDSTEP"] == "1", "players and step");
                    Check(ea["WOTM_SEED"] == eb["WOTM_SEED"], "same seed");
                    Check(ea["WOTM_LOCALPLAYER"] == "1" && eb["WOTM_LOCALPLAYER"] == "2", "local player");
                    Check(ea["WOTM_NETNAME"] == "Ana" && eb["WOTM_NETNAME"] == "Bia", "names");
                    Check(ea["WOTM_NET"] == "127.0.0.1:" + port && ea["WOTM_NETTOKEN"] != eb["WOTM_NETTOKEN"], "address and tokens");
                }
            });
            Run("password_and_owner", delegate
            {
                using (var a = new LobbyClient("127.0.0.1", port, "b1", 2000))
                using (var b = new LobbyClient("127.0.0.1", port, "b1", 2000))
                {
                    string code = a.Create("Amigos", "Ana", 3, "pw", 2000);
                    bool wrong = false;
                    try { b.Join(code, "Bia", 1, null, 2000); }
                    catch (LobbyError e) { wrong = e.Message.Contains("password"); }
                    Check(wrong, "no password, no entry");
                    b.Join(code, "Bia", 1, "pw", 2000);
                    bool notHost = false;
                    try { b.Set(2, 1, 2000); }
                    catch (LobbyError e) { notHost = e.Message.Contains("owner"); }
                    Check(notHost, "only the owner changes the settings");
                    LobbyClient.MemberInfo m = LobbyClient.ParseMembers("MEMBERS " + code + " Ana Ana:3,Bia:1");
                    Check(m != null && m.Code == code && m.Host == "Ana" && m.Names.Length == 2 && m.Names[1] == "Bia:1", "MEMBERS line");
                }
            });
            Run("error_becomes_exception", delegate
            {
                using (var a = new LobbyClient("127.0.0.1", port, "b1", 2000))
                {
                    bool threw = false;
                    try { a.Join("QQQQ", "Ana", 3, null, 2000); }
                    catch (LobbyError e) { threw = e.Message.Contains("no such lobby"); }
                    Check(threw, "missing lobby becomes LobbyError");
                }
            });
            Run("no_server", delegate
            {
                bool threw = false;
                try { using (new LobbyClient("127.0.0.1", port + 1, "b1", 500)) { } }
                catch (LobbyError) { threw = true; }
                Check(threw, "port with no server becomes LobbyError");
            });
            Run("clean_name", delegate
            {
                Check(LobbyClient.CleanName("La ne<>!_9-") == "Lane_9-", "characters dropped");
                Check(LobbyClient.CleanName("abcdefghijklmnopqrst").Length == 16, "16 at most");
            });
            Run("player_server", delegate
            {
                const int own = 7898;
                string log = Path.Combine(Path.GetTempPath(), "wotm_server_teste.log");
                using (ServerHost h = ServerHost.Start(server, own, log, 3000))
                {
                    Check(h.Owned, "this launcher runs the server");
                    Check(ServerHost.PortOpen(own, 500), "port open");
                    using (var a = new LobbyClient("127.0.0.1", own, "b1", 2000))
                        Check(a.Create("Casa", "Ana", 3, null, 2000).Length == 4, "lobby on the player's server");
                    Check(h.Running, "server running");
                }
                Check(!ServerHost.PortOpen(own, 500), "server closed with the launcher");
                Check(File.Exists(log), "server log");
            });
            Run("server_already_running", delegate
            {
                using (ServerHost h = ServerHost.Start(server, port, null, 3000))
                {
                    Check(!h.Owned, "uses the server already on the port");
                    Check(h.Running, "existing server running");
                }
                Check(ServerHost.PortOpen(port, 500), "does not close a server it does not own");
            });
            Run("server_that_fails_to_start", delegate
            {
                bool threw = false;
                try { using (ServerHost.Start(server + ".nao_existe", 7899, null, 1000)) { } }
                catch (LobbyError) { threw = true; }
                Check(threw, "missing wotm_server.exe becomes LobbyError");
            });
            Run("addresses_for_friends", delegate
            {
                Check(ServerHost.Kind(IPAddress.Parse("100.101.2.3")) == "Tailscale", "100.64.0.0/10 is Tailscale");
                Check(ServerHost.Kind(IPAddress.Parse("100.63.0.1")) == "public", "100.63 is not Tailscale");
                Check(ServerHost.Kind(IPAddress.Parse("192.168.1.5")) == "LAN", "192.168/16");
                Check(ServerHost.Kind(IPAddress.Parse("10.0.0.2")) == "LAN", "10/8");
                Check(ServerHost.Kind(IPAddress.Parse("172.20.1.1")) == "LAN", "172.16/12");
                Check(ServerHost.Kind(IPAddress.Parse("172.32.0.1")) == "public", "172.32 is not LAN");
                Check(ServerHost.Kind(IPAddress.Parse("127.0.0.1")) == null, "loopback excluded");
                Check(ServerHost.Kind(IPAddress.Parse("169.254.3.4")) == null, "link-local excluded");
                List<string> s = ServerHost.ShareLines(new[] { IPAddress.Parse("192.168.1.5"), IPAddress.Parse("127.0.0.1"),
                                                               IPAddress.Parse("100.101.2.3") }, "203.0.113.7", 7777);
                Check(s.Count == 3 && s[0] == "Internet: 203.0.113.7:7777" && s[1] == "Tailscale: 100.101.2.3:7777" &&
                      s[2] == "LAN: 192.168.1.5:7777", "order: internet, Tailscale, LAN (" + string.Join(" | ", s.ToArray()) + ")");
                Check(ServerHost.ShareLines(new IPAddress[0], null, 7777).Count == 0, "no addresses");
            });
        }
        finally
        {
            try { p.Kill(); } catch (Exception) { }
        }
        Console.WriteLine(checks + " checks, " + failures + " failure(s)");
        return failures == 0 ? 0 : 1;
    }

    static void Run(string name, Action t)
    {
        int before = failures;
        try { t(); }
        catch (Exception e) { ++failures; Console.WriteLine("  EXCEPTION " + e.Message); }
        Console.WriteLine((failures == before ? "ok      " : "FAILED  ") + name);
    }
}
