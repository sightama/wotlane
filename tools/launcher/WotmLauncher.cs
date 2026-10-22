// Launcher do jogo ao vivo: janela com caixas de marcar para as flags de ambiente (WOTM_*, PS2X_*) e um botao que roda o
// run_live.ps1 do runtime com elas. Para adicionar uma flag, acrescente uma linha em Flags. O estado fica em WotM_Launcher.ini,
// ao lado do executavel. Compilar: tools\launcher\build.bat (csc do .NET Framework, C# 5).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;
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

// GroupBox com borda e titulo desenhados aqui no tema escuro: com estilo visual o titulo sai na cor do tema e ignora ForeColor.
class DarkGroupBox : GroupBox
{
    protected override void OnPaint(PaintEventArgs e)
    {
        if (!Launcher.Dark) { base.OnPaint(e); return; }
        Graphics g = e.Graphics;
        Size sz = TextRenderer.MeasureText(g, Text, Font);
        int y = sz.Height / 2;
        g.Clear(BackColor);
        using (var pen = new Pen(Color.FromArgb(85, 85, 85))) g.DrawRectangle(pen, 0, y, Width - 1, Height - y - 1);
        var r = new Rectangle(7, 0, sz.Width, sz.Height);
        using (var b = new SolidBrush(BackColor)) g.FillRectangle(b, r);
        TextRenderer.DrawText(g, Text, Font, r, ForeColor);
    }
}

// CheckBox desenhada aqui no tema escuro (caixa escura; marcada, azul com o visto branco): o desenho do Windows nao tem versao escura
// que respeite a cor do texto.
class DarkCheckBox : CheckBox
{
    bool hover;
    protected override void OnMouseEnter(EventArgs e) { hover = true; Invalidate(); base.OnMouseEnter(e); }
    protected override void OnMouseLeave(EventArgs e) { hover = false; Invalidate(); base.OnMouseLeave(e); }

    protected override void OnPaint(PaintEventArgs e)
    {
        if (!Launcher.Dark) { base.OnPaint(e); return; }
        Graphics g = e.Graphics;
        g.Clear(BackColor);
        const int box = 14;
        var r = new Rectangle(0, (Height - box) / 2, box, box);
        Color fill, border;
        if (Checked)
        {
            fill = Enabled ? (hover ? Color.FromArgb(120, 185, 255) : Color.FromArgb(96, 165, 250)) : Color.FromArgb(70, 90, 115);
            border = fill;
        }
        else
        {
            fill = Color.FromArgb(45, 45, 45);
            border = !Enabled ? Color.FromArgb(75, 75, 75) : (hover ? Color.FromArgb(200, 200, 200) : Color.FromArgb(150, 150, 150));
        }
        g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
        using (var b = new SolidBrush(fill)) g.FillRectangle(b, r);
        using (var p = new Pen(border)) g.DrawRectangle(p, r.X, r.Y, box - 1, box - 1);
        if (Checked)
            using (var p = new Pen(Enabled ? Color.FromArgb(20, 20, 20) : Color.FromArgb(150, 150, 150), 2f))
                g.DrawLines(p, new[] { new PointF(r.X + 3f, r.Y + 7f), new PointF(r.X + 6f, r.Y + 10f), new PointF(r.X + 11f, r.Y + 4f) });
        g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.Default;
        var tr = new Rectangle(box + 4, 0, Width - box - 4, Height);
        TextRenderer.DrawText(g, Text, Font, tr, Enabled ? ForeColor : Color.FromArgb(120, 120, 120),
            TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix);
    }
}

class Launcher : Form
{
    // Chaves com ':' sao parametros do run_live.ps1; as outras viram variaveis de ambiente (=1 quando marcadas, removidas quando nao).
    // Diagnosticos sem caixa continuam valendo pela variavel (WOTM_RATEMIN=0, WOTM_VU0_INTERP, PS2X_PAD_DEADZONE; tabela no NATIVO.md).
    // Caixas de investigacoes fechadas sairam (buracos nos monstros: NOSKIN/NOVCOL/OPAQUE/ONEPASS; giro sozinho: TURNLOG/PS2X_PADLOG).
    static readonly Flag[] Flags = {
        new Flag("QoL features", "WOTM_LOCKTOGGLE", true, "Toggle lock-on on R1 (D-pad switches target)",
            "Lock-on is no longer held: one tap on R1 locks onto the nearest monster and it stays locked until the next tap. " +
            "While locked, the stick moves relative to the camera, getting hit does not change the target, and left/right on the D-pad switch targets. Block on L1, strafe on L2/R2 (default " +
            "controller setup). Applies to human players; the AI is unchanged. Unchecked: the original game's buttons."),
        new Flag("QoL features", "WOTM_ENEMYBARS", true, "Health and energy bars over enemies",
            "Replaces the numbered dot over each enemy, in the style of the player HUD: a health bar (green, recent damage in red) and an energy bar below it " +
            "(blue; gray while depleted). Also shows every enemy, not only the first 4. Unchecked: the original game's dot."),
        new Flag("QoL features", "WOTM_DAMAGEPOP", true, "Damage numbers (hits, throws, specials)",
            "Each hit shows a number at the point of impact that rises and fades: white for hits, orange for thrown objects or " +
            "monsters, cyan for specials. Applies to every monster, including yours."),

        new Flag("Controls", "WOTM_KBSEPARATE", false, "Keyboard is a separate player",
            "Connected controllers are players 1 and 2, in connection order. Unchecked: the keyboard is the same player as the first " +
            "controller (player 1). Checked: the keyboard is its own player after the controllers: player 1 with no controller, " +
            "player 2 with one controller, unused with two."),

        new Flag("Run", ":Auto", false, "Scripted input up to a stage", "-Auto 1: a scripted pad plays up to a stage (~50 s). Unchecked: you play with the keyboard."),
        new Flag("Run", "WOTM_FOTOS", false, "Debug photos (F7 pauses, F6 steps, F8 photo with a note)", "F8 saves the frame (a VU1 vs native check snapshot, ~40 MB), a PNG of the window and your note in <runtime>\\fotos\\<date>. Delete the photos once the issue is fixed."),
        new Flag("Run", "WOTM_AIVSAI", false, "Player controlled by the game's AI", "Player 1's monster becomes an AI (playerInit -> aiInit): AI vs AI fight. For batches without the menu use tools\\lote.ps1."),
        new Flag("Run", "WOTM_WATCHDOG=0", false, "Turn off the freeze watchdog", "By default, g_frame stuck for 8 s (outside movies) writes [vigia] to live.err.log and ram_/vram_vigia_<g_frame>.bin to discroot; game frames below 25/s log a [vigia] LENTO warning."),
        new Flag("Run", "WOTM_FMV_MUTE", false, "Mute movies", ""),
        new Flag("Run", "WOTM_SKIPINTRO", false, "Skip intro movies", "Skips the two movies at boot (the intro and the main menu intro) and goes straight to the title screen."),

        new Flag("Rendering diagnostics", "WOTM_NOFX", false, "No particles/effects", "Turns off the fx[] particles and the wotm_fx effects."),
        new Flag("Rendering diagnostics", "WOTM_NOSHADOW", false, "No shadows", "Turns off blocker shadows (monsters and the like)."),

        new Flag("Logs (live.err.log)", ":Stats", false, "Statistics", "-Stats 1: [native]/[ui]/[time] lines."),
        new Flag("Logs (live.err.log)", "WOTM_FIELDLOG", false, "Fields per frame", "One [field] line per game frame: fields, updateRate, EE cycles, host time and the most expensive loops."),
        new Flag("Logs (live.err.log)", "WOTM_CAMLOG", false, "Camera on every draw", "One [cam] line per draw: t, g_frame, fov, position and direction."),
        new Flag("Logs (live.err.log)", "PS2X_SND_LOG", false, "Native sound", "989snd calls."),
    };

    // Video: altura do quadro nativo e proporcao (WOTM_ASPECT; "largura dinamica" = sem ela, segue a janela/monitor)
    static readonly string[] Resolutions = { "448", "720", "1080", "1440", "2160" };
    static readonly string[] ResolutionLabels = { "448 (original PS2)", "720", "1080", "1440", "2160" };
    static readonly string[] Aspects = { "4:3", "16:9", "16:10", "21:9", "32:9" };
    const string DefaultRes = "720", DefaultAspect = "16:9";
    // Taxa de apresentacao: "0" = a do monitor (o runtime detecta sozinho; a variavel nao e passada). A logica do jogo
    // continua a 60; acima disso a tela so reapresenta o ultimo quadro, com ritmo regular e vsync.
    static readonly string[] Rates = { "0", "60", "120", "144", "165", "180", "240" };

    readonly ComboBox rate = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 160 };
    readonly CheckBox vsync = new DarkCheckBox { Text = "Vsync", AutoSize = true };
    readonly int monitorHz, monitorWidth, monitorHeight;   // modo atual do monitor principal, em pixels reais (sem a escala do Windows)

    readonly Dictionary<Flag, CheckBox> boxes = new Dictionary<Flag, CheckBox>();
    readonly ComboBox res = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 160 };
    readonly ComboBox aspect = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 80 };
    readonly CheckBox dynWidth = new DarkCheckBox { Text = "Dynamic width (aspect follows the window)", AutoSize = true };
    // Modo da tela (WOTM_FULLSCREEN): 0 = janela, 1 = tela cheia sem borda, 2 = tela cheia exclusiva (o monitor troca de modo)
    static readonly string[] DisplayModes = { "0", "1", "2" };
    static readonly string[] DisplayModeLabels = { "Windowed", "Borderless fullscreen", "Exclusive fullscreen" };
    readonly ComboBox fullscreen = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 160 };
    readonly string exeDir = AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\');
    string IniPath { get { return Path.Combine(exeDir, "WotM_Launcher.ini"); } }
    readonly Label status = new Label();

    Launcher()
    {
        MonitorMode(out monitorWidth, out monitorHeight, out monitorHz);
        Text = "WotM - options";
        try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch (Exception) { }   // o do exe (native\wotm.ico)
        Font = new Font("Segoe UI", 9f);
        AutoSize = true;
        AutoSizeMode = AutoSizeMode.GrowAndShrink;
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;
        StartPosition = FormStartPosition.CenterScreen;

        var tips = new ToolTip { AutoPopDelay = 30000 };
        var root = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, AutoSize = true, Padding = new Padding(10), WrapContents = false };
        // duas colunas: video, partida e trapacas a esquerda; as caixas de Flags a direita
        var cols = new FlowLayoutPanel { FlowDirection = FlowDirection.LeftToRight, AutoSize = true, WrapContents = false, Margin = new Padding(0) };
        var left = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, AutoSize = true, WrapContents = false };
        var right = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, AutoSize = true, WrapContents = false };
        cols.Controls.Add(left);
        cols.Controls.Add(right);
        root.Controls.Add(cols);
        Dictionary<string, string> saved = LoadIni();
        MergeSettings(saved);   // o que o jogo mudou no painel "recompiled options" vale mais que o ini do launcher
        left.Controls.Add(VideoGroup(saved, tips));
        left.Controls.Add(MatchGroup(saved, tips));
        left.Controls.Add(CheatsGroup(saved, tips));
        GroupBox gb = null;
        FlowLayoutPanel inner = null;
        foreach (Flag f in Flags)
        {
            if (gb == null || gb.Text != f.Group)
            {
                gb = new DarkGroupBox { Text = f.Group, AutoSize = true, Width = 420, Padding = new Padding(8) };
                inner = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, AutoSize = true, Dock = DockStyle.Fill, WrapContents = false };
                gb.Controls.Add(inner);
                right.Controls.Add(gb);
            }
            var cb = new DarkCheckBox { Text = f.Label, AutoSize = true };
            string v;
            cb.Checked = saved.TryGetValue(f.Key, out v) ? v == "1" : f.Default;
            if (f.Tip.Length > 0) tips.SetToolTip(cb, f.Tip);
            inner.Controls.Add(cb);
            boxes[f] = cb;
        }
        // o aviso dos marcadores depende do mod de barras (caixas criadas depois do grupo da partida)
        foreach (var kv in boxes) if (kv.Key.Key == "WOTM_ENEMYBARS") kv.Value.CheckedChanged += delegate { UpdateMatch(); };
        UpdateMatch();
        int wide = 0;
        foreach (Control c in right.Controls) wide = Math.Max(wide, c.PreferredSize.Width);
        foreach (Control c in right.Controls) c.MinimumSize = new Size(wide, 0);

        var buttons = new FlowLayoutPanel { AutoSize = true };
        var play = new Button { Text = "Play", AutoSize = true };
        play.Click += delegate { Play(); };
        var log = new Button { Text = "Open live.err.log", AutoSize = true };
        log.Click += delegate { OpenLog(); };
        var reset = new Button { Text = "Defaults", AutoSize = true };
        reset.Click += delegate
        {
            foreach (var kv in boxes) kv.Value.Checked = kv.Key.Default;
            res.SelectedIndex = Array.IndexOf(Resolutions, MonitorRes());
            rate.SelectedIndex = 0;
            vsync.Checked = true;
            aspect.SelectedIndex = Array.IndexOf(Aspects, DefaultAspect);
            dynWidth.Checked = true;
            fullscreen.SelectedIndex = 1;
            matchOn.Checked = false;
            matchPlayer.SelectedIndex = 0;
            matchLevel.SelectedIndex = 0;
            for (int i = 0; i < matchAIs.Length; ++i) matchAIs[i].SelectedIndex = i == 0 ? 1 : 0;
            foreach (CheckBox c in cheatBoxes) c.Checked = false;
            cheatStamina.SelectedIndex = 0;
            cheatWin.Value = 0;
        };
        buttons.Controls.Add(play);
        buttons.Controls.Add(log);
        buttons.Controls.Add(reset);
        root.Controls.Add(buttons);
        status.AutoSize = true;
        status.MaximumSize = new Size(420, 0);
        root.Controls.Add(status);
        Controls.Add(root);
        if (Dark) ApplyDark(this);
        AcceptButton = play;
        FormClosing += delegate { SaveIni(); SaveSettings(false); };
    }

    GroupBox VideoGroup(Dictionary<string, string> saved, ToolTip tips)
    {
        var gb = new DarkGroupBox { Text = "Video", AutoSize = true, Width = 420, Padding = new Padding(8) };
        var inner = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, AutoSize = true, Dock = DockStyle.Fill, WrapContents = false };
        gb.Controls.Add(inner);
        string v;

        res.Items.AddRange(ResolutionLabels);
        // sem configuracao salva: a maior resolucao que cabe no monitor (primeira execucao)
        int ri = Array.IndexOf(Resolutions, saved.TryGetValue("WOTM_RES", out v) ? v : MonitorRes());
        res.SelectedIndex = ri >= 0 ? ri : Array.IndexOf(Resolutions, DefaultRes);
        tips.SetToolTip(res, "Height of the native frame in pixels; the width comes from the aspect ratio. Independent of the window size: " +
            "the window only scales the frame up or down.");
        inner.Controls.Add(Row("Resolution (lines)", res));

        dynWidth.Checked = !saved.TryGetValue(":DynWidth", out v) || v == "1";
        tips.SetToolTip(dynWidth, "Checked: the width follows the window (or the monitor, in fullscreen), from 4:3 to 32:9. " +
            "Unchecked: fixed aspect ratio, with black bars if the window does not match.");
        inner.Controls.Add(dynWidth);

        aspect.Items.AddRange(Aspects);
        int ai = Array.IndexOf(Aspects, saved.TryGetValue("WOTM_ASPECT", out v) ? v : DefaultAspect);
        aspect.SelectedIndex = ai >= 0 ? ai : Array.IndexOf(Aspects, DefaultAspect);
        tips.SetToolTip(aspect, "4:3 = the game's original screen.");
        inner.Controls.Add(Row("Fixed aspect", aspect));
        aspect.Enabled = !dynWidth.Checked;
        dynWidth.CheckedChanged += delegate { aspect.Enabled = !dynWidth.Checked; };

        fullscreen.Items.AddRange(DisplayModeLabels);
        int fi = Array.IndexOf(DisplayModes, saved.TryGetValue("WOTM_FULLSCREEN", out v) ? v : "1");
        fullscreen.SelectedIndex = fi >= 0 ? fi : 1;
        tips.SetToolTip(fullscreen, "Borderless: a window the size of the monitor; the desktop keeps its mode, Alt+Tab is instant. " +
            "Exclusive: the monitor switches to the game (native resolution); Alt+Tab is slower but the compositor stays out of the way. " +
            "Windowed: the window opens at the resolution's size (smaller if it does not fit). F11 toggles between the window and the chosen fullscreen.");
        inner.Controls.Add(Row("Display mode (F11)", fullscreen));

        rate.Items.Add("Monitor (" + (monitorHz > 0 ? monitorHz + " Hz" : "?") + ")");
        for (int i = 1; i < Rates.Length; ++i) rate.Items.Add(Rates[i] + " Hz");
        int hi = Array.IndexOf(Rates, saved.TryGetValue("WOTM_HZ", out v) ? v : "0");
        rate.SelectedIndex = hi >= 0 ? hi : 0;
        tips.SetToolTip(rate, "How many times per second the screen is presented. Game logic stays at 60: above that the frame " +
            "is repeated, with even pacing and no tearing. 'Monitor' uses the refresh rate of the window's monitor.");
        inner.Controls.Add(Row("Refresh rate", rate));
        vsync.Checked = !saved.TryGetValue("WOTM_VSYNC", out v) || v == "1";
        tips.SetToolTip(vsync, "Syncs with the monitor (no tearing). Unchecked: only limited by the clock.");
        inner.Controls.Add(vsync);
        inner.Controls.Add(new Label { Text = "Primary monitor: " + monitorWidth + "x" + monitorHeight +
            (monitorHz > 0 ? " at " + monitorHz + " Hz" : ""), AutoSize = true, ForeColor = DimColor });
        return gb;
    }

    // Partida direta (WOTM_MATCH=<jogador>[,<ia>...],<fase>; sem IA = sozinho na fase): indices do GLOSSARIO.md. O runtime aceita ate 7 IAs (8 monstros).
    // Fora da lista: Spider (6) e Ogre (12), cortados, e os chefes (13..15).
    static readonly int[] MonsterIds = { 1, 2, 3, 4, 5, 7, 8, 9, 10, 11 };
    static readonly string[] MonsterNames = { "Congar", "Robo-47", "Togera", "Ultra V", "Preytor", "Kineticlops", "Magmo", "Raptros", "Agamo", "Zorgulon" };
    static readonly int[] LevelIds = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16 };
    static readonly string[] LevelNames = { "Midtown Park", "Gamblers Gulch", "Rosdale Canyon", "Metro City", "Century Airfield", "Atomic Island",
        "Baytown", "Club Caldera", "Tsunopolis", "UFO", "Capitol", "Lake Powell", "Mini Baytown", "Volcano", "Tokyo Deathmatch" };
    const int MatchMaxAIs = 7;

    readonly CheckBox matchOn = new DarkCheckBox { Text = "Direct match, no menu", AutoSize = true };
    readonly ComboBox matchPlayer = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 160 };
    readonly ComboBox matchLevel = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 160 };
    readonly ComboBox[] matchAIs = new ComboBox[MatchMaxAIs];
    readonly Label matchInfo = new Label { AutoSize = true, MaximumSize = new Size(400, 0) };

    GroupBox MatchGroup(Dictionary<string, string> saved, ToolTip tips)
    {
        var gb = new DarkGroupBox { Text = "Match", AutoSize = true, Width = 420, Padding = new Padding(8) };
        var inner = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, AutoSize = true, Dock = DockStyle.Fill, WrapContents = false };
        gb.Controls.Add(inner);
        string v;

        matchOn.Checked = saved.TryGetValue(":MatchOn", out v) && v == "1";
        tips.SetToolTip(matchOn, "Skips the menu and goes straight to the fight (1 player against the CPU, 3 lives, no win target: " +
            "the fight goes on without the end-of-match dialog). With no AI you play alone on the stage. Forces -Auto 0, since the scripted pad would press buttons mid-fight. " +
            "With 'Player controlled by the game's AI' (Run) it becomes AI vs AI.");
        inner.Controls.Add(matchOn);

        matchPlayer.Items.AddRange(MonsterNames);
        matchPlayer.SelectedIndex = IndexOrZero(MonsterIds, saved, ":MatchPlayer");
        inner.Controls.Add(Row("Your monster", matchPlayer));

        foreach (int id in LevelIds) matchLevel.Items.Add(id + "  " + LevelNames[Array.IndexOf(LevelIds, id)]);
        matchLevel.SelectedIndex = IndexOrZero(LevelIds, saved, ":MatchLevel");
        tips.SetToolTip(matchLevel, "Number = stage index in GLOSSARIO.md. Every stage fits 8 monsters: the monsters load above 32 MB (64 MB runtime); " +
            "Metro City with 8 still has ~21 MB to spare.");
        inner.Controls.Add(Row("Stage", matchLevel));

        inner.Controls.Add(new Label { Text = "AI opponents (up to 7; none = play solo on the stage):", AutoSize = true, Margin = new Padding(3, 6, 3, 0) });
        var foes = new FlowLayoutPanel { FlowDirection = FlowDirection.LeftToRight, AutoSize = true, MaximumSize = new Size(400, 0), WrapContents = true };
        for (int i = 0; i < MatchMaxAIs; ++i)
        {
            var cb = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 122 };
            cb.Items.Add("-");
            cb.Items.AddRange(MonsterNames);
            int id = saved.TryGetValue(":MatchAI" + (i + 1), out v) ? ParseInt(v) : (i == 0 ? MonsterIds[0] : 0);
            int k = Array.IndexOf(MonsterIds, id);
            cb.SelectedIndex = k >= 0 ? k + 1 : 0;
            cb.SelectedIndexChanged += delegate { UpdateMatch(); };
            tips.SetToolTip(cb, "A repeated monster gets the next costume, as in the menu. '-' = empty slot.");
            matchAIs[i] = cb;
            foes.Controls.Add(cb);
        }
        inner.Controls.Add(foes);
        matchInfo.ForeColor = DimColor;
        inner.Controls.Add(matchInfo);

        matchOn.CheckedChanged += delegate { UpdateMatch(); };
        matchLevel.SelectedIndexChanged += delegate { UpdateMatch(); };
        UpdateMatch();
        return gb;
    }

    int MatchFoes()
    {
        int n = 0;
        foreach (ComboBox cb in matchAIs) if (cb.SelectedIndex > 0) ++n;
        return n;
    }

    void UpdateMatch()
    {
        bool on = matchOn.Checked;
        matchPlayer.Enabled = matchLevel.Enabled = on;
        foreach (ComboBox cb in matchAIs) cb.Enabled = on;
        int total = 1 + MatchFoes();
        string txt = total == 1 ? "Solo: just you on the stage." : total + " monsters in the fight.";
        if (total > 4 && !On("WOTM_ENEMYBARS")) txt += " Only 4 enemies get an off-screen health marker (the health bars mod shows all of them).";
        matchInfo.Text = txt;
    }

    string MatchValue()
    {
        var parts = new List<string> { MonsterIds[matchPlayer.SelectedIndex].ToString() };
        foreach (ComboBox cb in matchAIs) if (cb.SelectedIndex > 0) parts.Add(MonsterIds[cb.SelectedIndex - 1].ToString());
        parts.Add(LevelIds[matchLevel.SelectedIndex].ToString());
        return string.Join(",", parts.ToArray());
    }

    // Trapacas do menu F1 ligadas desde o inicio (WOTM_CHEATS=inv,stam|furia,esp,cloak,deus,vencer=N); valem para os jogadores.
    static readonly string[] CheatKeys = { "inv", "esp", "cloak", "deus" };
    static readonly string[] CheatLabels = { "Invincible", "Special always ready", "CLOAK", "God mode (no damage, no hit reaction)" };
    readonly CheckBox[] cheatBoxes = new CheckBox[4];
    readonly ComboBox cheatStamina = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 160 };
    readonly NumericUpDown cheatWin = new NumericUpDown { Minimum = 0, Maximum = 3600, Width = 70 };

    GroupBox CheatsGroup(Dictionary<string, string> saved, ToolTip tips)
    {
        var gb = new DarkGroupBox { Text = "Cheats from the start", AutoSize = true, Width = 420, Padding = new Padding(8) };
        var inner = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, AutoSize = true, Dock = DockStyle.Fill, WrapContents = false };
        gb.Controls.Add(inner);
        string v;
        inner.Controls.Add(new Label { Text = "Same as the in-fight F1 menu; apply to human players.", AutoSize = true, ForeColor = DimColor });
        for (int i = 0; i < CheatKeys.Length; ++i)
        {
            var cb = new DarkCheckBox { Text = CheatLabels[i], AutoSize = true };
            cb.Checked = saved.TryGetValue(":Cheat_" + CheatKeys[i], out v) && v == "1";
            cheatBoxes[i] = cb;
            inner.Controls.Add(cb);
        }
        tips.SetToolTip(cheatBoxes[0], "Health never drops; hits still push you.");
        tips.SetToolTip(cheatBoxes[3], "The game's m_godMode (full health, stamina and special) plus, from us, no hit reaction. A building falling on you still kills.");
        cheatStamina.Items.AddRange(new object[] { "Normal", "Always full", "Rage (130)" });
        cheatStamina.SelectedIndex = saved.TryGetValue(":CheatStamina", out v) ? Math.Max(0, Math.Min(2, ParseInt(v))) : 0;
        inner.Controls.Add(Row("Stamina", cheatStamina));
        cheatWin.Value = saved.TryGetValue(":CheatWin", out v) ? Math.Max(0, Math.Min(3600, ParseInt(v))) : 0;
        tips.SetToolTip(cheatWin, "Knocks out every living AI every N seconds of game time (F1's 'win the fight'). 0 = off.");
        inner.Controls.Add(Row("Win every (s, 0 = off)", cheatWin));
        return gb;
    }

    string CheatsValue()
    {
        var parts = new List<string>();
        for (int i = 0; i < CheatKeys.Length; ++i) if (cheatBoxes[i].Checked) parts.Add(CheatKeys[i]);
        if (cheatStamina.SelectedIndex == 1) parts.Add("stam");
        else if (cheatStamina.SelectedIndex == 2) parts.Add("furia");
        if (cheatWin.Value > 0) parts.Add("vencer=" + (int)cheatWin.Value);
        return string.Join(",", parts.ToArray());
    }

    static int ParseInt(string s)
    {
        int n;
        return int.TryParse(s, out n) ? n : 0;
    }

    static int IndexOrZero(int[] ids, Dictionary<string, string> saved, string key)
    {
        string v;
        int k = saved.TryGetValue(key, out v) ? Array.IndexOf(ids, ParseInt(v)) : -1;
        return k >= 0 ? k : 0;
    }

    // Maior resolucao da lista que cabe na altura do monitor principal.
    string MonitorRes()
    {
        string best = Resolutions[0];
        foreach (string r in Resolutions) if (int.Parse(r) <= monitorHeight) best = r;
        return best;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct DEVMODE
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmDeviceName;
        public short dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra;
        public int dmFields, dmPositionX, dmPositionY, dmDisplayOrientation, dmDisplayFixedOutput;
        public short dmColor, dmDuplex, dmYResolution, dmTTOption, dmCollate;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmFormName;
        public short dmLogPixels;
        public int dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags, dmDisplayFrequency;
        public int dmICMMethod, dmICMIntent, dmMediaType, dmDitherType, dmReserved1, dmReserved2, dmPanningWidth, dmPanningHeight;
    }
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    static extern bool EnumDisplaySettings(string deviceName, int modeNum, ref DEVMODE devMode);

    // Modo atual do monitor principal; se nao der para ler, o tamanho do Screen (pode vir reduzido pela escala) e taxa 0.
    static void MonitorMode(out int width, out int height, out int hz)
    {
        var dm = new DEVMODE();
        dm.dmSize = (short)Marshal.SizeOf(typeof(DEVMODE));
        const int ENUM_CURRENT_SETTINGS = -1;
        if (EnumDisplaySettings(Screen.PrimaryScreen.DeviceName, ENUM_CURRENT_SETTINGS, ref dm) && dm.dmPelsHeight > 0)
        {
            width = dm.dmPelsWidth;
            height = dm.dmPelsHeight;
            hz = dm.dmDisplayFrequency > 1 ? dm.dmDisplayFrequency : 0;
            return;
        }
        width = Screen.PrimaryScreen.Bounds.Width;
        height = Screen.PrimaryScreen.Bounds.Height;
        hz = 0;
    }

    static FlowLayoutPanel Row(string label, Control c)
    {
        var p = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = new Padding(0) };
        p.Controls.Add(new Label { Text = label, AutoSize = false, Size = new Size(210, 23), TextAlign = ContentAlignment.MiddleLeft });
        p.Controls.Add(c);
        return p;
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
        lines.Add("WOTM_RES|" + Resolutions[res.SelectedIndex]);
        lines.Add("WOTM_ASPECT|" + Aspects[aspect.SelectedIndex]);
        lines.Add(":DynWidth|" + (dynWidth.Checked ? "1" : "0"));
        lines.Add("WOTM_FULLSCREEN|" + DisplayModes[fullscreen.SelectedIndex]);
        lines.Add("WOTM_HZ|" + Rates[rate.SelectedIndex]);
        lines.Add("WOTM_VSYNC|" + (vsync.Checked ? "1" : "0"));
        lines.Add(":MatchOn|" + (matchOn.Checked ? "1" : "0"));
        lines.Add(":MatchPlayer|" + MonsterIds[matchPlayer.SelectedIndex]);
        lines.Add(":MatchLevel|" + LevelIds[matchLevel.SelectedIndex]);
        for (int i = 0; i < matchAIs.Length; ++i)
            lines.Add(":MatchAI" + (i + 1) + "|" + (matchAIs[i].SelectedIndex > 0 ? MonsterIds[matchAIs[i].SelectedIndex - 1] : 0));
        for (int i = 0; i < CheatKeys.Length; ++i) lines.Add(":Cheat_" + CheatKeys[i] + "|" + (cheatBoxes[i].Checked ? "1" : "0"));
        lines.Add(":CheatStamina|" + cheatStamina.SelectedIndex);
        lines.Add(":CheatWin|" + (int)cheatWin.Value);
        try { File.WriteAllLines(IniPath, lines.ToArray()); } catch (IOException) { } catch (UnauthorizedAccessException) { }
    }

    // Opcoes que o jogo tambem muda ao vivo (painel "recompiled options" do menu principal, native/wotm_settings.hpp): settings.ini
    // ao lado do save.json (<runtime>\discroot), CHAVE=valor com os nomes das variaveis. O jogo le na abertura; o launcher le ao abrir
    // (vale mais que o WotM_Launcher.ini) e grava ao jogar e ao fechar. Essas variaveis nao vao mais pelo ambiente.
    static readonly string[] SharedKeys = { "WOTM_RES", "WOTM_ASPECT", "WOTM_FULLSCREEN", "WOTM_HZ", "WOTM_VSYNC",
        "WOTM_LOCKTOGGLE", "WOTM_ENEMYBARS", "WOTM_DAMAGEPOP", "WOTM_KBSEPARATE" };

    string SettingsPath(bool ask)
    {
        string rt = RuntimeDir(ask);
        return rt == null ? null : Path.Combine(rt, "discroot", "settings.ini");
    }

    void MergeSettings(Dictionary<string, string> saved)
    {
        string p = SettingsPath(false);
        if (p == null || !File.Exists(p)) return;
        try
        {
            foreach (string raw in File.ReadAllLines(p))
            {
                string line = raw.Trim();
                int i = line.IndexOf('=');
                if (line.StartsWith("#") || i <= 0) continue;
                string key = line.Substring(0, i), v = line.Substring(i + 1).Trim();
                if (Array.IndexOf(SharedKeys, key) < 0) continue;
                if (key == "WOTM_ASPECT")
                {
                    bool dyn = v == "dynamic" || v == "0" || v.Length == 0;
                    saved[":DynWidth"] = dyn ? "1" : "0";
                    if (!dyn) saved[key] = v;
                }
                else saved[key] = v;
            }
        }
        catch (IOException) { }
    }

    void SaveSettings(bool ask)
    {
        string p = SettingsPath(ask);
        if (p == null || !Directory.Exists(Path.GetDirectoryName(p))) return;
        var lines = new List<string> { "# Recompiled options (menu principal) e launcher; mesma chave das variaveis de ambiente" };
        lines.Add("WOTM_RES=" + Resolutions[res.SelectedIndex]);
        lines.Add("WOTM_ASPECT=" + (dynWidth.Checked ? "dynamic" : Aspects[aspect.SelectedIndex]));
        lines.Add("WOTM_FULLSCREEN=" + DisplayModes[fullscreen.SelectedIndex]);
        lines.Add("WOTM_HZ=" + Rates[rate.SelectedIndex]);
        lines.Add("WOTM_VSYNC=" + (vsync.Checked ? "1" : "0"));
        foreach (string k in new[] { "WOTM_LOCKTOGGLE", "WOTM_ENEMYBARS", "WOTM_DAMAGEPOP", "WOTM_KBSEPARATE" })
            lines.Add(k + "=" + (On(k) ? "1" : "0"));
        try { File.WriteAllLines(p, lines.ToArray()); } catch (IOException) { } catch (UnauthorizedAccessException) { }
    }

    // A pasta do runtime (a que tem run_live.ps1): WOTM_RECOMP_DIR, depois o .wotm_recomp_dir que o run_live.bat salva na raiz do repo;
    // sem elas, pergunta (ask) ou devolve null.
    string RuntimeDir(bool ask = true)
    {
        string env = Environment.GetEnvironmentVariable("WOTM_RECOMP_DIR");
        if (!string.IsNullOrEmpty(env) && File.Exists(Path.Combine(env, "run_live.ps1"))) return env;
        string saved = Path.Combine(exeDir, ".wotm_recomp_dir");
        if (File.Exists(saved))
        {
            string dir = File.ReadAllText(saved).Trim();
            if (File.Exists(Path.Combine(dir, "run_live.ps1"))) return dir;
        }
        if (!ask) return null;
        using (var dlg = new FolderBrowserDialog { Description = "Runtime folder (the one containing run_live.ps1)" })
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
        if (rt == null) { status.Text = "Runtime folder not found (run_live.ps1)."; return; }
        // partida direta pula o menu: o roteiro do pad (-Auto 1) apertaria botoes no meio da luta
        string args = string.Format("-NoProfile -ExecutionPolicy Bypass -File \"{0}\" -Auto {1} -Stats {2} -Build {3}",
            Path.Combine(rt, "run_live.ps1"), On(":Auto") && !matchOn.Checked ? 1 : 0, On(":Stats") ? 1 : 0, "build_dev");
        var psi = new ProcessStartInfo("powershell.exe", args) { UseShellExecute = false, CreateNoWindow = true, WorkingDirectory = rt };
        psi.EnvironmentVariables["WOTM_REPO"] = exeDir;
        // video e QoL vao pelo settings.ini (o painel do jogo muda e grava o mesmo arquivo); variavel herdada passaria por cima dele
        SaveSettings(true);
        foreach (string k in SharedKeys) psi.EnvironmentVariables.Remove(k);
        foreach (var kv in boxes)
        {
            string key = kv.Key.Key;
            if (key.StartsWith(":") || Array.IndexOf(SharedKeys, key) >= 0) continue;
            string name = key, value = "1";
            int eq = key.IndexOf('=');
            if (eq > 0) { name = key.Substring(0, eq); value = key.Substring(eq + 1); }
            if (kv.Value.Checked) psi.EnvironmentVariables[name] = value;
            else psi.EnvironmentVariables.Remove(name);
        }
        if (matchOn.Checked) psi.EnvironmentVariables["WOTM_MATCH"] = MatchValue();
        else psi.EnvironmentVariables.Remove("WOTM_MATCH");
        string cheats = CheatsValue();
        if (cheats.Length > 0) psi.EnvironmentVariables["WOTM_CHEATS"] = cheats;
        else psi.EnvironmentVariables.Remove("WOTM_CHEATS");
        try
        {
            Process.Start(psi);
            Close();   // o jogo segue sozinho (processo independente); em erro a janela fica com a mensagem
        }
        catch (Exception e) { status.Text = "Failed: " + e.Message; }
    }

    void OpenLog()
    {
        string rt = RuntimeDir();
        string p = rt == null ? null : Path.Combine(rt, "logs", "live.err.log");
        if (p != null && !File.Exists(p)) p = Path.Combine(rt, "live.err.log");   // run_live.ps1 antigo grava na raiz
        if (p != null && File.Exists(p)) Process.Start("notepad.exe", "\"" + p + "\"");
        else status.Text = "No live.err.log yet.";
    }

    // Tema: segue o modo dos aplicativos do Windows (Personalizacao > Cores). Escuro = cores abaixo e barra de titulo escura (DWM).
    internal static readonly bool Dark = AppsUseDark();
    static readonly Color BgColor = Color.FromArgb(32, 32, 32), InputColor = Color.FromArgb(51, 51, 51), ButtonColor = Color.FromArgb(58, 58, 58);
    static readonly Color FgColor = Color.FromArgb(232, 232, 232);
    static readonly Color DimColor = Dark ? Color.FromArgb(150, 150, 150) : SystemColors.GrayText;

    static bool AppsUseDark()
    {
        try
        {
            object v = Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize", "AppsUseLightTheme", 1);
            return v is int && (int)v == 0;
        }
        catch (Exception) { return false; }
    }

    [DllImport("dwmapi.dll")]
    static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int value, int size);
    [DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]
    static extern int SetWindowTheme(IntPtr hwnd, string appName, string idList);
    // SetPreferredAppMode (sem nome exportado, ordinal 135, Windows 10 1903+): 1 = segue o sistema; sem ele a lista aberta sai clara
    [DllImport("uxtheme.dll", EntryPoint = "#135")]
    static extern int SetPreferredAppMode(int mode);

    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);
        if (!Dark) return;
        int on = 1;
        try
        {   // DWMWA_USE_IMMERSIVE_DARK_MODE: 20 no Windows 10 20H1+ e 11, 19 antes
            if (DwmSetWindowAttribute(Handle, 20, ref on, 4) != 0) DwmSetWindowAttribute(Handle, 19, ref on, 4);
        }
        catch (Exception) { }
    }

    // Texto com a cor padrao vira claro; cores escolhidas (cinza das notas, aviso vermelho) ficam.
    static void ApplyDark(Control c)
    {
        if (c.ForeColor == SystemColors.ControlText) c.ForeColor = FgColor;
        if (c is ComboBox)
        {   // tema escuro nativo do Windows (o das caixas de arquivo): legivel tambem desabilitado, ao contrario do FlatStyle com cores
            c.BackColor = InputColor;
            c.ForeColor = FgColor;   // o padrao da ComboBox e WindowText (nao ControlText): os itens da lista aberta saiam pretos
            Control cc = c;
            EventHandler theme = delegate { try { SetWindowTheme(cc.Handle, "DarkMode_CFD", null); } catch (Exception) { } };
            if (c.IsHandleCreated) theme(c, EventArgs.Empty);
            else c.HandleCreated += theme;
        }
        else if (c is NumericUpDown)
        {   // a cor tem que ir para a caixa de texto interna (UpDownEdit) tambem
            c.BackColor = InputColor;
            ((NumericUpDown)c).BorderStyle = BorderStyle.FixedSingle;
            foreach (Control k in c.Controls) if (k is TextBox) { k.BackColor = InputColor; k.ForeColor = FgColor; }
        }
        else if (c is Button)
        {
            var b = (Button)c;
            b.FlatStyle = FlatStyle.Flat;
            b.BackColor = ButtonColor;
            b.FlatAppearance.BorderColor = Color.FromArgb(100, 100, 100);
        }
        else c.BackColor = BgColor;
        if (!(c is NumericUpDown)) foreach (Control k in c.Controls) ApplyDark(k);
    }

    [STAThread]
    static void Main()
    {
        Application.EnableVisualStyles();
        if (Dark) try { SetPreferredAppMode(1); } catch (Exception) { }
        Application.Run(new Launcher());
    }
}
