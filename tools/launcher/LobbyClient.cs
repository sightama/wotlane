// ABOUTME: Cliente do lobby do wotm_server para o launcher (C# 5, .NET Framework): conexao TCP de linhas, fila das linhas recebidas
// ABOUTME: e o ambiente do wotm.exe para uma linha MATCH (o mesmo do tools/net_lobby.py). Testado por LobbyTest.cs contra o servidor.
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Net.Sockets;
using System.Text;
using System.Threading;

class LobbyError : Exception
{
    public LobbyError(string m) : base(m) { }
}

// Uma conexao com o lobby (protocolo de linhas de native/net/wotm_net_proto.hpp). Uma thread le as linhas para uma fila; quem usa
// le com ReadLine (testes, pedidos com resposta) ou esvazia com Poll (o launcher, num Timer da interface).
class LobbyClient : IDisposable
{
    public const int Protocol = 1;
    public readonly string Host;
    public readonly int Port;
    public int Lobbies { get; private set; }
    readonly TcpClient tcp;
    readonly StreamReader reader;
    readonly StreamWriter writer;
    readonly BlockingCollection<string> lines = new BlockingCollection<string>();
    readonly Thread thread;
    volatile bool closed;

    public LobbyClient(string host, int port, string build, int timeoutMs)
    {
        Host = host;
        Port = port;
        tcp = new TcpClient();
        IAsyncResult ar = tcp.BeginConnect(host, port, null, null);
        if (!ar.AsyncWaitHandle.WaitOne(timeoutMs) || !tcp.Connected)
        {
            tcp.Close();
            throw new LobbyError("sem resposta de " + host + ":" + port + " (o wotm_server esta rodando? a porta TCP esta aberta?)");
        }
        tcp.EndConnect(ar);
        tcp.NoDelay = true;
        NetworkStream s = tcp.GetStream();
        reader = new StreamReader(s, new UTF8Encoding(false));
        writer = new StreamWriter(s, new UTF8Encoding(false)) { AutoFlush = true, NewLine = "\n" };
        thread = new Thread(ReadLoop) { IsBackground = true };
        thread.Start();
        Send("HELLO " + Protocol + " " + build);
        string w = Expect("WELCOME", timeoutMs);
        Lobbies = int.Parse(w.Split(' ')[1]);
    }

    void ReadLoop()
    {
        try
        {
            string l;
            while ((l = reader.ReadLine()) != null) lines.Add(l);
        }
        catch (Exception) { }
        closed = true;
        lines.Add(null);   // acorda quem espera
    }

    public bool Closed { get { return closed; } }

    public void Send(string line) { writer.WriteLine(line); }

    // proxima linha em ate timeoutMs; null se acabou o tempo ou a conexao caiu
    public string ReadLine(int timeoutMs)
    {
        string l;
        return lines.TryTake(out l, timeoutMs) ? l : null;
    }

    // linhas ja recebidas, sem esperar
    public List<string> Poll()
    {
        var r = new List<string>();
        string l;
        while (lines.TryTake(out l)) if (l != null) r.Add(l);
        return r;
    }

    // le ate uma linha que comeca com prefix; ERROR vira LobbyError
    public string Expect(string prefix, int timeoutMs)
    {
        DateTime until = DateTime.UtcNow.AddMilliseconds(timeoutMs);
        while (true)
        {
            int left = (int)(until - DateTime.UtcNow).TotalMilliseconds;
            string l = left > 0 ? ReadLine(left) : null;
            if (l == null) throw new LobbyError(closed ? "o servidor fechou a conexao" : "sem resposta do servidor");
            if (l.StartsWith("ERROR")) throw new LobbyError(l.Length > 6 ? l.Substring(6) : l);
            if (l.StartsWith(prefix)) return l;
        }
    }

    public void Dispose()
    {
        try { tcp.Close(); } catch (Exception) { }
    }

    // LOBBY <i> <waiting|playing> <n>/<max> <nomes,...|->
    public class LobbyInfo
    {
        public int Index, Count, Max;
        public string State;
        public string[] Names;
    }

    public static LobbyInfo ParseLobby(string line)
    {
        string[] w = line.Split(' ');
        if (w.Length < 5 || w[0] != "LOBBY") return null;
        string[] n = w[3].Split('/');
        return new LobbyInfo
        {
            Index = int.Parse(w[1]), State = w[2], Count = int.Parse(n[0]), Max = int.Parse(n[1]),
            Names = w[4] == "-" ? new string[0] : w[4].Split(',')
        };
    }

    public List<LobbyInfo> List(int timeoutMs)
    {
        Send("LIST");
        var r = new List<LobbyInfo>();
        while (true)
        {
            string l = Expect("", timeoutMs);
            if (l == "END") return r;
            LobbyInfo li = ParseLobby(l);
            if (li != null) r.Add(li);
        }
    }

    public static Dictionary<string, string> KeyValues(string line)
    {
        var kv = new Dictionary<string, string>();
        foreach (string w in line.Split(' '))
        {
            int eq = w.IndexOf('=');
            if (eq > 0) kv[w.Substring(0, eq)] = w.Substring(eq + 1);
        }
        return kv;
    }

    // variaveis de ambiente do wotm.exe para uma linha MATCH (o mesmo que match_env do tools/net_lobby.py)
    public static Dictionary<string, string> MatchEnv(Dictionary<string, string> m, string host)
    {
        int you = int.Parse(m["you"]);
        return new Dictionary<string, string>
        {
            { "WOTM_NET", host + ":" + m["port"] },
            { "WOTM_NETTOKEN", m["token"] },
            { "WOTM_NETNAME", m["names"].Split(',')[you] },
            { "WOTM_MATCH", m["monsters"] + "," + m["stage"] },
            { "WOTM_PLAYERS", m["players"] },
            { "WOTM_FIXEDSTEP", m["step"] },
            { "WOTM_SEED", m["seed"] },
            { "WOTM_LOCALPLAYER", (you + 1).ToString() },
        };
    }

    // nome de jogador como o servidor aceita: letras, digitos, '_' e '-', ate 16
    public static string CleanName(string s)
    {
        var b = new StringBuilder();
        foreach (char c in s)
            if ((char.IsLetterOrDigit(c) && c < 128 || c == '_' || c == '-') && b.Length < 16) b.Append(c);
        return b.ToString();
    }
}
