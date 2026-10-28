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
                    Check(watcher.Lobbies == 0, "nenhum lobby aberto");
                    Check(watcher.List(2000).Count == 0, "lista inicial vazia");
                    string code = a.Create("Sala", "Ana", 3, null, 2000);
                    Check(code.Length == 4, "codigo de 4 letras");
                    Check(a.Settings["stage"] == "1" && a.Settings["wins"] == "0", "fase e vitorias iniciais");
                    b.Join(code.ToLowerInvariant(), "Bia", 1, null, 2000);
                    List<LobbyClient.LobbyInfo> l = watcher.List(2000);
                    Check(l.Count == 1 && l[0].Code == code && l[0].State == "waiting" && !l[0].Locked, "lobby na lista");
                    Check(l[0].Name == "Sala" && l[0].Host == "Ana" && l[0].Names.Length == 2 && l[0].Names[1] == "Bia", "nome, dono e membros");
                    Dictionary<string, string> s = a.Set(4, 2, 2000);
                    Check(s["stage"] == "4" && s["wins"] == "2", "SET do dono");
                    a.Send("START");
                    Dictionary<string, string> ea = LobbyClient.MatchEnv(LobbyClient.KeyValues(a.Expect("MATCH", 2000)), "127.0.0.1");
                    Dictionary<string, string> eb = LobbyClient.MatchEnv(LobbyClient.KeyValues(b.Expect("MATCH", 2000)), "127.0.0.1");
                    Check(ea["WOTM_MATCH"] == "3,1,4" && eb["WOTM_MATCH"] == "3,1,4", "WOTM_MATCH");
                    Check(ea["WOTM_KILLTARGET"] == "2" && eb["WOTM_KILLTARGET"] == "2", "vitorias");
                    Check(ea["WOTM_PLAYERS"] == "2" && ea["WOTM_FIXEDSTEP"] == "1", "jogadores e passo");
                    Check(ea["WOTM_SEED"] == eb["WOTM_SEED"], "mesma semente");
                    Check(ea["WOTM_LOCALPLAYER"] == "1" && eb["WOTM_LOCALPLAYER"] == "2", "jogador local");
                    Check(ea["WOTM_NETNAME"] == "Ana" && eb["WOTM_NETNAME"] == "Bia", "nomes");
                    Check(ea["WOTM_NET"] == "127.0.0.1:" + port && ea["WOTM_NETTOKEN"] != eb["WOTM_NETTOKEN"], "endereco e fichas");
                }
            });
            Run("senha_e_dono", delegate
            {
                using (var a = new LobbyClient("127.0.0.1", port, "b1", 2000))
                using (var b = new LobbyClient("127.0.0.1", port, "b1", 2000))
                {
                    string code = a.Create("Amigos", "Ana", 3, "pw", 2000);
                    bool wrong = false;
                    try { b.Join(code, "Bia", 1, null, 2000); }
                    catch (LobbyError e) { wrong = e.Message.Contains("senha"); }
                    Check(wrong, "sem senha nao entra");
                    b.Join(code, "Bia", 1, "pw", 2000);
                    bool notHost = false;
                    try { b.Set(2, 1, 2000); }
                    catch (LobbyError e) { notHost = e.Message.Contains("dono"); }
                    Check(notHost, "so o dono muda as opcoes");
                    LobbyClient.MemberInfo m = LobbyClient.ParseMembers("MEMBERS " + code + " Ana Ana:3,Bia:1");
                    Check(m != null && m.Code == code && m.Host == "Ana" && m.Names.Length == 2 && m.Names[1] == "Bia:1", "linha MEMBERS");
                }
            });
            Run("erro_vira_excecao", delegate
            {
                using (var a = new LobbyClient("127.0.0.1", port, "b1", 2000))
                {
                    bool threw = false;
                    try { a.Join("QQQQ", "Ana", 3, null, 2000); }
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
