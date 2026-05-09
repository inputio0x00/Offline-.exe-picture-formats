using System.Runtime.InteropServices;
using System.Text;

namespace ImgConvert.Services;

// P/Invoke surface for the native img_convert.dll. The DLL is copied into
// the application output by the .csproj's <Content> item; on first run of
// the single-file .exe, .NET extracts native files to a temp dir and the
// loader picks them up automatically.
internal static class NativeBridge
{
    private const string Lib = "img_convert";

    public enum Format
    {
        Unknown = 0,
        Jpg     = 1,
        Png     = 2,
        Webp    = 3,
        Tiff    = 4,
        Heif    = 5,
    }

    public enum TiffCompression
    {
        None     = 0,
        Lzw      = 1,
        Zip      = 2,
        PackBits = 3,
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Options
    {
        public int Quality;
        public int PngCompression;
        public int Lossless;
        public int JpegProgressive;
        public int TiffCompression;
        // matches `int32_t reserved[6]` on the C side
        public int Reserved0;
        public int Reserved1;
        public int Reserved2;
        public int Reserved3;
        public int Reserved4;
        public int Reserved5;
    }

    [DllImport(Lib, EntryPoint = "ic_detect_format", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ic_detect_format(IntPtr utf8Path);

    [DllImport(Lib, EntryPoint = "ic_convert_file", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ic_convert_file(
        IntPtr utf8In,
        IntPtr utf8Out,
        int target,
        ref Options opts,
        IntPtr errBuf,
        int errBufSize);

    [DllImport(Lib, EntryPoint = "ic_version", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr ic_version();

    public static string Version()
    {
        IntPtr p = ic_version();
        return Marshal.PtrToStringUTF8(p) ?? "?";
    }

    public static Format DetectFormat(string path)
    {
        IntPtr p = MarshalUtf8(path);
        try
        {
            int code = ic_detect_format(p);
            return (Format)code;
        }
        finally
        {
            Marshal.FreeHGlobal(p);
        }
    }

    // Returns null on success, or an error message on failure.
    public static string? Convert(string inPath, string outPath, Format target, Options opts)
    {
        IntPtr inP = MarshalUtf8(inPath);
        IntPtr outP = MarshalUtf8(outPath);
        IntPtr errP = Marshal.AllocHGlobal(512);
        try
        {
            // Zero the error buffer so a successful call doesn't return stale bytes.
            for (int i = 0; i < 512; i++) Marshal.WriteByte(errP, i, 0);
            int code = ic_convert_file(inP, outP, (int)target, ref opts, errP, 512);
            if (code == 0) return null;
            string msg = Marshal.PtrToStringUTF8(errP) ?? string.Empty;
            return string.IsNullOrEmpty(msg) ? $"native error {code}" : msg;
        }
        finally
        {
            Marshal.FreeHGlobal(errP);
            Marshal.FreeHGlobal(outP);
            Marshal.FreeHGlobal(inP);
        }
    }

    private static IntPtr MarshalUtf8(string s)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(s);
        IntPtr p = Marshal.AllocHGlobal(bytes.Length + 1);
        Marshal.Copy(bytes, 0, p, bytes.Length);
        Marshal.WriteByte(p, bytes.Length, 0);
        return p;
    }
}
