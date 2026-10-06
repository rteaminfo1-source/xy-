namespace Hronograf;

static class Program
{
    [STAThread]
    static void Main(string[] args)
    {
        Application.SetHighDpiMode(HighDpiMode.SystemAware);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        try
        {
            int v = Native.hg_version();
            if (v < 100) throw new InvalidOperationException("Устаревшая версия движка: " + v);
        }
        catch (Exception ex) when (ex is DllNotFoundException or EntryPointNotFoundException or BadImageFormatException or InvalidOperationException)
        {
            MessageBox.Show(
                "Не найден C++ движок hronograf_engine.dll.\n\n" +
                "Как исправить:\n" +
                " • откройте Hronograf.sln в Visual Studio и нажмите «Собрать решение»,\n" +
                " • или запустите build.bat в папке проекта.\n\n" +
                "Подробности: " + ex.Message,
                "Хронограф малой родины", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }

        // Параметры для показа на защите: --year 1945 --scenario 2 --night --pause --name "Сосновка"
        var opt = new StartOptions();
        for (int i = 0; i < args.Length; i++)
        {
            string a = args[i].ToLowerInvariant();
            string? next = i + 1 < args.Length ? args[i + 1] : null;
            if (a == "--year" && int.TryParse(next, out var y)) { opt.Year = y; i++; }
            else if (a == "--scenario" && int.TryParse(next, out var s)) { opt.Scenario = Math.Clamp(s - 1, 0, 2); i++; }
            else if (a == "--name" && next != null) { opt.Name = next; i++; }
            else if (a == "--night") opt.Night = true;
            else if (a == "--pause") opt.Paused = true;
            else if (a == "--today") opt.Today = true;
        }
        Application.Run(new MainForm(opt));
    }
}

public sealed class StartOptions
{
    public int? Year { get; set; }
    public int Scenario { get; set; } = 2;
    public string Name { get; set; } = "Берёзовка";
    public bool Night { get; set; }
    public bool Paused { get; set; }
    public bool Today { get; set; }
}
