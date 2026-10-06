using System.Reflection;
using System.Runtime.InteropServices;

namespace Hronograf;

// ---------------------------------------------------------------------
//  Мост C# → C++. Структуры повторяют engine.h байт в байт.
// ---------------------------------------------------------------------

[StructLayout(LayoutKind.Sequential)]
public struct HgStats
{
    public int Year, Population, Houses, Abandoned, Young, Middle, Old, Sites;
    public double Quality, Connectivity, Economy, Memory, Ecology, Services;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = Native.TechCount)]
    public double[] Tech;
}

[StructLayout(LayoutKind.Sequential)]
public struct HgBuilding
{
    public int X, Y, W, H, Type, Flags, Id, Variant;
}

[StructLayout(LayoutKind.Sequential)]
public struct HgEvent
{
    public int Year, Code, Value, Scenario;
}

[StructLayout(LayoutKind.Sequential)]
public struct HgLight
{
    public float X, Y, Radius, R, G, B, Intensity;
    public int Blink;

    public HgLight(float x, float y, float radius, float r, float g, float b, float intensity, bool blink = false)
    {
        X = x; Y = y; Radius = radius; R = r; G = g; B = b; Intensity = intensity; Blink = blink ? 1 : 0;
    }
}

[StructLayout(LayoutKind.Sequential)]
public struct HgLens
{
    public float Year, Night, Time;
    public int Frame;
}

public static class Native
{
    public const string Lib = "hronograf_engine";
    public const int TechCount = 13;

    static Native()
    {
        // Ищем DLL движка: сначала свежая сборка C++ проекта, потом копия рядом с exe.
        NativeLibrary.SetDllImportResolver(typeof(Native).Assembly, Resolve);
    }

    public static string? LoadedFrom { get; private set; }

    static IntPtr Resolve(string name, Assembly asm, DllImportSearchPath? path)
    {
        if (name != Lib) return IntPtr.Zero;
        foreach (var file in Candidates())
        {
            if (File.Exists(file) && NativeLibrary.TryLoad(file, out var handle))
            {
                LoadedFrom = file;
                return handle;
            }
        }
        return IntPtr.Zero;
    }

    static IEnumerable<string> Candidates()
    {
        string file = OperatingSystem.IsWindows() ? "hronograf_engine.dll"
                    : OperatingSystem.IsMacOS() ? "libhronograf_engine.dylib" : "libhronograf_engine.so";
        var fresh = new List<string>();
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        for (int i = 0; i < 7 && dir != null; i++, dir = dir.Parent)
        {
            foreach (var sub in new[] { "Engine/bin/x64/Release", "Engine/bin/x64/Debug", "Engine/build/Release",
                                        "Engine/build/Debug", "Engine/build", "Engine" })
            {
                var p = Path.Combine(dir.FullName, sub, file);
                if (File.Exists(p)) fresh.Add(p);
            }
        }
        // Самая свежая собственная сборка — в приоритете.
        foreach (var p in fresh.OrderByDescending(File.GetLastWriteTimeUtc)) yield return p;
        yield return Path.Combine(AppContext.BaseDirectory, file);
        dir = new DirectoryInfo(AppContext.BaseDirectory);
        for (int i = 0; i < 7 && dir != null; i++, dir = dir.Parent)
            yield return Path.Combine(dir.FullName, "Engine", "prebuilt", "win-x64", file);
    }

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int hg_version();

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int hg_info(int what);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr hg_create([MarshalAs(UnmanagedType.LPUTF8Str)] string name, int scenario);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern void hg_destroy(IntPtr engine);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int hg_shade(IntPtr engine, [Out] byte[] output);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int hg_terrain(IntPtr engine, int year, [Out] byte[] output);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int hg_buildings(IntPtr engine, int year, [Out] HgBuilding[]? output, int max);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int hg_stats(IntPtr engine, int year, out HgStats output);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int hg_events(IntPtr engine, [Out] HgEvent[]? output, int max);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern double hg_lens(IntPtr bgra, int w, int h, int stride,
                                        [In] HgLight[] lights, int count, ref HgLens lens);
}
