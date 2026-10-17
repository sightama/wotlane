// Launcher do jogo ao vivo: janela com caixas de marcar para as flags de ambiente (WOTM_*, PS2X_*) e um botao que roda o
// run_live.ps1 do runtime com elas. Para adicionar uma flag, acrescente uma linha em Flags. O estado fica em WotM_Launcher.ini,
// ao lado do executavel. Compilar: tools\launcher\build.bat (csc do .NET Framework, C# 5).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

class Flag
{
    public string Group, Key, Label, Tip;
    public bool Default;
    public Flag(string group, string key, bool def, string label, string tip)
    {
        Group = group; Key = key; Default = def; Label = label; Tip = tip;
    }
}

class Launcher : Form
{
    // Chaves com ':' sao parametros do run_live.ps1; as outras viram variaveis de ambiente (=1 quando marcadas, removidas quando nao).
    static readonly Flag[] Flags = {
        new Flag("Execucao", ":Auto", true, "Roteiro automatico ate a fase", "-Auto 1: o pad do roteiro joga ate uma fase (~50 s). Desmarcado: voce joga com o teclado."),
        new Flag("Execucao", ":Stats", false, "Estatisticas no log (PS2X_STATS)", "-Stats 1: [native]/[ui]/[time] em live.err.log."),
        new Flag("Execucao", ":LTCG", false, "Usar o build com LTCG (pasta build)", "-Build build em vez de build_dev. So vale se voce compilou com build_runtime.bat build."),
        new Flag("Execucao", "WOTM_RATEMIN=0", false, "Diagnostico: tirar o limite do updateRate (dt livre)", "Por padrao o runtime devolve updateRate >= 30 ao jogo (dt <= 1/30): sem isso, nos quadros lentos a fisica integra com dt grande e diverge (pelo do Congar esticado, monstro girando sozinho)."),
        new Flag("Execucao", "WOTM_FIELDLOG", false, "Log de fields por quadro (live.err.log)", "Uma linha [field] por quadro do jogo: fields, updateRate, ciclos do EE, tempo de host e os lacos que mais gastam."),
        new Flag("Execucao", "WOTM_OLDCLOCK", false, "Diagnostico: relogio antigo do EE", "Volta a espera pelo VU1 no particleDraw contando ciclos e o relogio virtual solto do de parede (ritmo irregular: rate 15/20/60 alternando)."),
        new Flag("Execucao", "WOTM_AIVSAI", false, "Jogador controlado pela IA do jogo", "O monstro do jogador 1 vira IA (playerInit -> aiInit): luta IA contra IA. Para lotes sem menu use tools\\lote.ps1."),
        new Flag("Execucao", "WOTM_FOTOS", true, "Fotos para depurar (F7 pausa, F6 avanca, F8 foto com nota)", "F8 grava o quadro (instantaneo da checagem VU1 x nativo, ~40 MB), um PNG da janela e a sua nota em <runtime>\\fotos\\<data>. Apague as fotos depois de resolvidas."),
        new Flag("Execucao", "WOTM_WATCHDOG=0", false, "Desligar o vigia de congelamento", "Por padrao, g_frame parado por 8 s (fora de filme) grava [vigia] no live.err.log e ram_/vram_vigia_<g_frame>.bin no discroot; quadros do jogo abaixo de 25/s viram aviso [vigia] LENTO."),
        new Flag("Execucao", "WOTM_VU0_INTERP", false, "Diagnostico: VU0 so no interpretador", "Desliga os microprogramas do VU0 em C++ (gerados do ELF por tools/vu0native.py) e roda todos no interpretador, como antes (logica bem mais lenta)."),

        new Flag("Controle", "PS2X_PADLOG", false, "Log do gamepad (nome e eixos no live.err.log)", "Linhas [pad]: nome do gamepad 0 e cada mudanca dos analogicos, com o horario."),
        new Flag("Controle", "PS2X_PAD_DEADZONE=0.25", false, "Zona morta de 0,25 nos analogicos", "Sem ela o runtime repassa qualquer desvio do eixo ao jogo."),

        new Flag("Camera", "WOTM_NOINTERP", false, "Comecar com a interpolacao 30->60 desligada", "O jogo anda a 30 quadros/s; a interpolacao mistura dois quadros para desenhar a 60. F4 alterna durante o jogo."),
        new Flag("Camera", "WOTM_TURNLOG", false, "Log da rotacao dos monstros (live.err.log)", "Uma linha [turn] por monstro por quadro do jogo: rate, aceleracao e velocidade de giro, angulos."),
        new Flag("Camera", "WOTM_CAMLOG", false, "Gravar a camera a cada desenho (live.err.log)", "Uma linha [cam] por desenho: t, g_frame, fov, posicao e direcao."),

        new Flag("Desenho", "WOTM_NOFX", false, "Sem particulas/efeitos", "Desliga as particulas de fx[] e os efeitos de wotm_fx."),
        new Flag("Desenho", "WOTM_NOSHADOW", false, "Sem sombras", "Desliga as sombras dos bloqueadores (monstros e afins)."),
        new Flag("Desenho", "WOTM_NODESTR", false, "Destrutiveis sempre intactos", "Ignora o estado de destruicao (sempre o estado 0)."),
        new Flag("Desenho", "WOTM_NOCTRL", false, "Sem predios fantasma (sempre child1)", "Comportamento antigo: nao troca para a versao transparente quando a camera esta dentro/atras."),
        new Flag("Desenho", "WOTM_NOSKIN", false, "Monstros na pose de referencia (sem skinning)", "Diagnostico de malha."),
        new Flag("Desenho", "WOTM_NOVCOL", false, "Sem cor de vertice", "Ignora o sombreamento por vertice."),
        new Flag("Desenho", "WOTM_OPAQUE", false, "Ignorar o alfa de vertice nos skinned", "Diagnostico de transparencia."),
        new Flag("Desenho", "WOTM_ONEPASS", false, "Tudo num passe so", "Nao separa os translucidos num segundo passe."),
        new Flag("Desenho", "WOTM_FUDGEOLD", false, "zBufferFudge na direcao antiga", "Diagnostico: > 1 empurra para longe (antes da correcao dos rotulos)."),

        new Flag("Som", "PS2X_SND=0", false, "Desligar o som nativo (fica mudo)", "PS2X_SND=0 volta ao 989snd no IOP emulado, que nao tem SPU2."),
        new Flag("Som", "PS2X_SND_LOG", false, "Log do som nativo", "Chamadas do 989snd no live.err.log."),
        new Flag("Som", "WOTM_FMV_MUTE", false, "Filmes sem audio", ""),
    };

    readonly Dictionary<Flag, CheckBox> boxes = new Dictionary<Flag, CheckBox>();
    readonly string exeDir = AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\');
    string IniPath { get { return Path.Combine(exeDir, "WotM_Launcher.ini"); } }
    readonly Label status = new Label();

    Launcher()
    {
        Text = "WotM - opcoes";
        Font = new Font("Segoe UI", 9f);
        AutoSize = true;
        AutoSizeMode = AutoSizeMode.GrowAndShrink;
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;
        StartPosition = FormStartPosition.CenterScreen;

        var tips = new ToolTip();
        var root = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, AutoSize = true, Padding = new Padding(10), WrapContents = false };
        Dictionary<string, string> saved = LoadIni();
        GroupBox gb = null;
        FlowLayoutPanel inner = null;
        foreach (Flag f in Flags)
        {
            if (gb == null || gb.Text != f.Group)
            {
                gb = new GroupBox { Text = f.Group, AutoSize = true, Width = 420, Padding = new Padding(8) };
                inner = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, AutoSize = true, Dock = DockStyle.Fill, WrapContents = false };
                gb.Controls.Add(inner);
                root.Controls.Add(gb);
            }
            var cb = new CheckBox { Text = f.Label + "   [" + f.Key.TrimStart(':') + "]", AutoSize = true };
            string v;
            cb.Checked = saved.TryGetValue(f.Key, out v) ? v == "1" : f.Default;
            if (f.Tip.Length > 0) tips.SetToolTip(cb, f.Tip);
            inner.Controls.Add(cb);
            boxes[f] = cb;
        }

        var buttons = new FlowLayoutPanel { AutoSize = true };
        var play = new Button { Text = "Jogar", AutoSize = true };
        play.Click += delegate { Play(); };
        var log = new Button { Text = "Abrir live.err.log", AutoSize = true };
        log.Click += delegate { OpenLog(); };
        var reset = new Button { Text = "Padrao", AutoSize = true };
        reset.Click += delegate { foreach (var kv in boxes) kv.Value.Checked = kv.Key.Default; };
        buttons.Controls.Add(play);
        buttons.Controls.Add(log);
        buttons.Controls.Add(reset);
        root.Controls.Add(buttons);
        status.AutoSize = true;
        status.MaximumSize = new Size(420, 0);
        root.Controls.Add(status);
        Controls.Add(root);
        AcceptButton = play;
        FormClosing += delegate { SaveIni(); };
    }

    Dictionary<string, string> LoadIni()
    {
        var d = new Dictionary<string, string>();
        try
        {
            foreach (string line in File.ReadAllLines(IniPath))
            {
                int i = line.LastIndexOf('|');
                if (i > 0) d[line.Substring(0, i)] = line.Substring(i + 1).Trim();
            }
        }
        catch (IOException) { }
        return d;
    }

    void SaveIni()
    {
        var lines = new List<string>();
        foreach (var kv in boxes) lines.Add(kv.Key.Key + "|" + (kv.Value.Checked ? "1" : "0"));
        try { File.WriteAllLines(IniPath, lines.ToArray()); } catch (IOException) { } catch (UnauthorizedAccessException) { }
    }

    // A pasta do runtime (a que tem run_live.ps1): WOTM_RECOMP_DIR, depois o .wotm_recomp_dir que o run_live.bat salva na raiz do repo.
    string RuntimeDir()
    {
        string env = Environment.GetEnvironmentVariable("WOTM_RECOMP_DIR");
        if (!string.IsNullOrEmpty(env) && File.Exists(Path.Combine(env, "run_live.ps1"))) return env;
        string saved = Path.Combine(exeDir, ".wotm_recomp_dir");
        if (File.Exists(saved))
        {
            string dir = File.ReadAllText(saved).Trim();
            if (File.Exists(Path.Combine(dir, "run_live.ps1"))) return dir;
        }
        using (var dlg = new FolderBrowserDialog { Description = "Pasta do runtime (a que contem run_live.ps1)" })
        {
            if (dlg.ShowDialog(this) == DialogResult.OK && File.Exists(Path.Combine(dlg.SelectedPath, "run_live.ps1")))
            {
                File.WriteAllText(saved, dlg.SelectedPath + Environment.NewLine);
                return dlg.SelectedPath;
            }
        }
        return null;
    }

    bool On(string key)
    {
        foreach (var kv in boxes) if (kv.Key.Key == key) return kv.Value.Checked;
        return false;
    }

    void Play()
    {
        SaveIni();
        string rt = RuntimeDir();
        if (rt == null) { status.Text = "Nao achei a pasta do runtime (run_live.ps1)."; return; }
        string args = string.Format("-NoProfile -ExecutionPolicy Bypass -File \"{0}\" -Auto {1} -Stats {2} -Build {3}",
            Path.Combine(rt, "run_live.ps1"), On(":Auto") ? 1 : 0, On(":Stats") ? 1 : 0, On(":LTCG") ? "build" : "build_dev");
        var psi = new ProcessStartInfo("powershell.exe", args) { UseShellExecute = false, CreateNoWindow = true, WorkingDirectory = rt };
        psi.EnvironmentVariables["WOTM_REPO"] = exeDir;
        foreach (var kv in boxes)
        {
            string key = kv.Key.Key;
            if (key.StartsWith(":")) continue;
            string name = key, value = "1";
            int eq = key.IndexOf('=');
            if (eq > 0) { name = key.Substring(0, eq); value = key.Substring(eq + 1); }
            if (kv.Value.Checked) psi.EnvironmentVariables[name] = value;
            else psi.EnvironmentVariables.Remove(name);
        }
        try
        {
            Process.Start(psi);
            status.Text = "Iniciado (" + rt + "). Saida em live.out.log / live.err.log.";
        }
        catch (Exception e) { status.Text = "Falhou: " + e.Message; }
    }

    void OpenLog()
    {
        string rt = RuntimeDir();
        string p = rt == null ? null : Path.Combine(rt, "live.err.log");
        if (p != null && File.Exists(p)) Process.Start("notepad.exe", "\"" + p + "\"");
        else status.Text = "Ainda nao ha live.err.log.";
    }

    [STAThread]
    static void Main()
    {
        Application.EnableVisualStyles();
        Application.Run(new Launcher());
    }
}
