// ABOUTME: Teste do LobbyClient.cs contra o wotm_server de verdade (native\net\build\wotm_server.exe): lista, entra, comeca e confere
// ABOUTME: o ambiente de cada jogador. Compilar e rodar: tools\launcher\test.bat (sai com 0 se passar).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading;

static class LobbyTest
{
    static int failures, checks;

    static void Check(bool ok, string what)
    {
        ++checks;
        if (!ok) { ++failures; Console.WriteLine("  FALHOU " + what); }
    }

    static int Main(string[] args)
    {
        string server = args.Length > 0 ? args[0] : Path.Combine(AppDomain.CurrentDomain.BaseDirectory, @"..\..\native\net\build\wotm_server.exe");
        if (!File.Exists(server)) { Console.WriteLine("nao achei " + server + " (tools\\build_net.bat)"); return 2; }
        const int port = 7897;
        var p = Process.Start(new ProcessStartInfo(server, "--port " + port) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true });
        try
        {
            Thread.Sleep(300);
            Run("lista_e_partida", delegate
            {
                using (var watcher = new LobbyClient("127.0.0.1", port, "b1", 2000))
                using (var a = new LobbyClient("127.0.0.1", port, "b1", 2000))
                using (var b = new LobbyClient("127.0.0.1", port, "b1", 2000))
                {
                    Check(watcher.Lobbies == 2, "dois lobbies");
                    List<LobbyClient.LobbyInfo> l = watcher.List(2000);
                    Check(l.Count == 2 && l[0].State == "waiting" && l[0].Names.Length == 0, "lista inicial");
                    a.Send("JOIN 0 Ana 3");
                    a.Expect("JOINED", 2000);
                    b.Send("JOIN 0 Bia 1");
                    b.Expect("JOINED", 2000);
                    l = watcher.List(2000);
                    Check(l[0].Count == 2 && l[0].Names.Length == 2 && l[0].Names[1] == "Bia", "lista com os dois");
                    a.Send("START 4");
                    Dictionary<string, string> ea = LobbyClient.MatchEnv(LobbyClient.KeyValues(a.Expect("MATCH", 2000)), "127.0.0.1");
                    Dictionary<string, string> eb = LobbyClient.MatchEnv(LobbyClient.KeyValues(b.Expect("MATCH", 2000)), "127.0.0.1");
                    Check(ea["WOTM_MATCH"] == "3,1,4" && eb["WOTM_MATCH"] == "3,1,4", "WOTM_MATCH");
                    Check(ea["WOTM_PLAYERS"] == "2" && ea["WOTM_FIXEDSTEP"] == "1", "jogadores e passo");
                    Check(ea["WOTM_SEED"] == eb["WOTM_SEED"], "mesma semente");
                    Check(ea["WOTM_LOCALPLAYER"] == "1" && eb["WOTM_LOCALPLAYER"] == "2", "jogador local");
                    Check(ea["WOTM_NETNAME"] == "Ana" && eb["WOTM_NETNAME"] == "Bia", "nomes");
                    Check(ea["WOTM_NET"] == "127.0.0.1:" + port && ea["WOTM_NETTOKEN"] != eb["WOTM_NETTOKEN"], "endereco e fichas");
                }
            });
            Run("erro_vira_excecao", delegate
            {
                using (var a = new LobbyClient("127.0.0.1", port, "b1", 2000))
                {
                    a.Send("JOIN 7 Ana 3");
                    bool threw = false;
                    try { a.Expect("JOINED", 2000); }
                    catch (LobbyError e) { threw = e.Message.Contains("inexistente"); }
                    Check(threw, "lobby inexistente vira LobbyError");
                }
            });
            Run("sem_servidor", delegate
            {
                bool threw = false;
                try { using (new LobbyClient("127.0.0.1", port + 1, "b1", 500)) { } }
                catch (LobbyError) { threw = true; }
                Check(threw, "porta sem servidor vira LobbyError");
            });
            Run("nome_limpo", delegate
            {
                Check(LobbyClient.CleanName("La ne<>!_9-") == "Lane_9-", "caracteres fora");
                Check(LobbyClient.CleanName("abcdefghijklmnopqrst").Length == 16, "16 no maximo");
            });
        }
        finally
        {
            try { p.Kill(); } catch (Exception) { }
        }
        Console.WriteLine(checks + " checagens, " + failures + " falha(s)");
        return failures == 0 ? 0 : 1;
    }

    static void Run(string name, Action t)
    {
        int before = failures;
        try { t(); }
        catch (Exception e) { ++failures; Console.WriteLine("  EXCECAO " + e.Message); }
        Console.WriteLine((failures == before ? "ok      " : "FALHOU  ") + name);
    }
}
