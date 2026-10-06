using System.Diagnostics;
using System.Net;
using System.Text;

namespace Hranitel;

/// <summary>HTML-отчёты: летопись игры и «Цифровой паспорт малой родины».</summary>
public static class Report
{
    const string Css = @"
:root{--bg:#090d1a;--panel:#10172a;--panel2:#18223d;--border:#2a3a60;--text:#eaeef7;--muted:#8895b4;--gold:#f2b84b;--cyan:#4cc9f0;--green:#76e294;--coral:#ff7a70;--violet:#a78bfa}
*{box-sizing:border-box}html{background:#090d1a}body{margin:0;min-height:100vh;background:radial-gradient(1200px 600px at 20% -10%,#1b2a55 0,transparent 60%),var(--bg);color:var(--text);
font:16px/1.55 'Segoe UI',system-ui,-apple-system,Roboto,sans-serif;padding:40px 16px}
.wrap{max-width:920px;margin:0 auto}.kicker{color:var(--muted);letter-spacing:.14em;text-transform:uppercase;font-size:12px;font-weight:600}
h1{font-size:44px;line-height:1.1;margin:.2em 0 .1em;background:linear-gradient(90deg,var(--gold),var(--cyan));-webkit-background-clip:text;background-clip:text;color:transparent}
h2{font-size:20px;margin:0 0 12px}.card{background:var(--panel);border:1px solid var(--border);border-radius:18px;padding:24px;margin:18px 0}
.fate{border-width:2px}.fate h2{font-size:30px;margin:4px 0 8px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:14px}
.stat{display:grid;grid-template-columns:110px 1fr 40px;gap:10px;align-items:center;margin:8px 0}.bar{height:10px;background:var(--panel2);border-radius:6px;overflow:hidden}
.bar i{display:block;height:100%;border-radius:6px}.muted{color:var(--muted)}.big{font-size:56px;font-weight:300;line-height:1}
.tl{position:relative;margin-left:8px;border-left:2px solid var(--border);padding-left:22px}.tl .item{position:relative;margin:0 0 18px}
.tl .item:before{content:'';position:absolute;left:-30px;top:6px;width:12px;height:12px;border-radius:50%;background:var(--gold);box-shadow:0 0 0 4px #f2b84b33}
.year{color:var(--gold);font-weight:700}.chip{display:inline-block;padding:2px 10px;border-radius:999px;font-size:13px;margin:4px 6px 0 0;font-weight:600}
.plus{background:#76e29422;color:var(--green)}.minus{background:#ff7a7022;color:var(--coral)}.ok{color:var(--green)}.no{color:var(--coral)}
.rec{border-left:3px solid var(--cyan);padding:6px 0 6px 14px;margin:10px 0}footer{color:var(--muted);font-size:13px;text-align:center;margin-top:30px}
@media print{body{background:#fff;color:#000}.card{border-color:#ccc;background:#fff}}";

    static string E(string s) => WebUtility.HtmlEncode(s);

    static string Page(string title, string body) =>
        $"<!doctype html><html lang=\"ru\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">" +
        $"<title>{E(title)}</title><style>{Css}</style></head><body><div class=\"wrap\">{body}" +
        $"<footer>Создано программой «Хранитель малой родины» · {DateTime.Now:dd.MM.yyyy HH:mm}</footer></div></body></html>";

    static string Bar(string label, int value, Rgb color) =>
        $"<div class=\"stat\"><span>{E(label)}</span><div class=\"bar\"><i style=\"width:{value}%;background:{color.Css}\"></i></div><b style=\"color:{color.Css}\">{value}</b></div>";

    static string SafeName(string s)
    {
        var bad = Path.GetInvalidFileNameChars();
        return new string(s.Select(c => bad.Contains(c) || c == ' ' ? '_' : c).ToArray());
    }

    static string Save(string file, string html)
    {
        var dir = Path.Combine(AppContext.BaseDirectory, "Летописи");
        try { Directory.CreateDirectory(dir); }
        catch { dir = Path.GetTempPath(); }
        var path = Path.Combine(dir, file);
        File.WriteAllText(path, html, new UTF8Encoding(true));
        try { Process.Start(new ProcessStartInfo(path) { UseShellExecute = true }); } catch { /* браузер не открылся — файл всё равно сохранён */ }
        return path;
    }

    public static string SaveChronicle(string name, Ending end, int[] stats, List<ChronicleEntry> log)
    {
        var sb = new StringBuilder();
        sb.Append($"<div class=\"kicker\">Хранитель малой родины · летопись</div><h1>{E(name)}: 1925 — 2050</h1>");
        sb.Append("<p class=\"muted\">Судьба малой родины в объективе технологий: девять эпох — девять решений.</p>");
        sb.Append($"<div class=\"card fate\" style=\"border-color:{end.Color.Css}\"><div class=\"kicker\">Судьба села к 2050 году</div>" +
                  $"<h2 style=\"color:{end.Color.Css}\">{E(end.Title)}</h2><p>{E(end.Text)}</p></div>");
        sb.Append("<div class=\"grid\"><div class=\"card\"><h2>Итоговые показатели</h2>");
        sb.Append($"<p class=\"muted\">Население: <b style=\"color:var(--gold)\">{Story.Population(stats[Stat.People])}</b> жителей</p>");
        for (int i = 0; i < Stat.Count; i++) sb.Append(Bar(Stat.Names[i], stats[i], Stat.Colors[i]));
        sb.Append("</div><div class=\"card\"><h2>Что важно</h2><p>Технологии сами по себе не спасают и не губят село. " +
                  "Решает то, работают ли они на людей и сохраняют ли память о прошлом.</p>" +
                  "<p class=\"muted\">Сыграйте ещё раз и сделайте другие выборы — судьба сложится иначе.</p></div></div>");
        sb.Append("<div class=\"card\"><h2>Летопись решений</h2><div class=\"tl\">");
        foreach (var e in log)
        {
            sb.Append($"<div class=\"item\"><span class=\"year\">{e.Year}</span> · <span class=\"muted\">{E(e.Era)}</span><br><b>{E(e.Choice)}</b><br>{E(e.Result)}<br>");
            for (int i = 0; i < Stat.Count; i++)
                if (e.Delta[i] != 0)
                    sb.Append($"<span class=\"chip {(e.Delta[i] > 0 ? "plus" : "minus")}\">{E(Stat.Names[i])} {(e.Delta[i] > 0 ? "+" : "")}{e.Delta[i]}</span>");
            sb.Append("</div>");
        }
        sb.Append("</div></div>");
        return Save($"Летопись_{SafeName(name)}.html", Page($"Летопись: {name}", sb.ToString()));
    }

    public static string SavePassport(Passport.Result r)
    {
        var sb = new StringBuilder();
        sb.Append($"<div class=\"kicker\">Цифровой паспорт малой родины</div><h1>{E(r.Name)}</h1>");
        if (!string.IsNullOrWhiteSpace(r.Region)) sb.Append($"<p class=\"muted\">{E(r.Region)}</p>");
        sb.Append($"<div class=\"grid\"><div class=\"card\"><div class=\"kicker\">Индекс цифровой устойчивости</div>" +
                  $"<div class=\"big\" style=\"color:{r.Color.Css}\">{r.Index}</div><p><b style=\"color:{r.Color.Css}\">{E(r.Verdict)}</b></p>" +
                  $"<p class=\"muted\">Население: {r.PopThen} → {r.PopNow} ({r.Trend:+0;-0;0}%)</p></div><div class=\"card\"><h2>Составляющие</h2>");
        foreach (var (label, value, color) in r.Parts) sb.Append(Bar(label, value, color));
        sb.Append("</div></div><div class=\"card\"><h2>Что уже есть в селе</h2><div class=\"grid\">");
        foreach (var (item, has) in r.Items)
            sb.Append($"<div><span class=\"{(has ? "ok" : "no")}\">{(has ? "✔" : "✘")}</span> {E(item)}</div>");
        sb.Append("</div></div><div class=\"card\"><h2>Рекомендации: какие технологии помогут</h2>");
        foreach (var rec in r.Recommendations) sb.Append($"<div class=\"rec\">{E(rec)}</div>");
        if (r.Recommendations.Count == 0) sb.Append("<p class=\"ok\">Отлично! Все ключевые направления развития уже работают.</p>");
        sb.Append("</div>");
        return Save($"Паспорт_{SafeName(r.Name)}.html", Page($"Паспорт: {r.Name}", sb.ToString()));
    }
}
