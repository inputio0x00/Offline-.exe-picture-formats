using System.IO;
using System.Threading;
using System.Threading.Tasks;
using ImgConvert.Models;

namespace ImgConvert.Services;

internal static class ConversionService
{
    public static string ExtensionFor(NativeBridge.Format f) => f switch
    {
        NativeBridge.Format.Jpg  => ".jpg",
        NativeBridge.Format.Png  => ".png",
        NativeBridge.Format.Webp => ".webp",
        NativeBridge.Format.Tiff => ".tiff",
        NativeBridge.Format.Heif => ".heic",
        _ => ".bin",
    };

    // Convert one item, mutating its status fields. Returns true on success.
    public static async Task<bool> ConvertOneAsync(
        ConversionItem item,
        string outputDir,
        NativeBridge.Format target,
        NativeBridge.Options opts,
        bool overwrite,
        CancellationToken ct)
    {
        ct.ThrowIfCancellationRequested();
        item.Status = ItemStatus.Converting;
        item.StatusMessage = string.Empty;

        string baseName = Path.GetFileNameWithoutExtension(item.SourcePath);
        string outPath = Path.Combine(outputDir, baseName + ExtensionFor(target));

        if (!overwrite && File.Exists(outPath))
        {
            int n = 1;
            string candidate;
            do
            {
                candidate = Path.Combine(outputDir, $"{baseName} ({n}){ExtensionFor(target)}");
                n++;
            } while (File.Exists(candidate));
            outPath = candidate;
        }

        try
        {
            Directory.CreateDirectory(outputDir);
        }
        catch (System.Exception ex)
        {
            item.Status = ItemStatus.Failed;
            item.StatusMessage = $"output dir: {ex.Message}";
            return false;
        }

        item.OutputPath = outPath;

        // Native call is synchronous and CPU-bound. Run it on the thread pool
        // so the UI stays responsive and multiple files can run in parallel.
        bool ok = await Task.Run(() =>
        {
            string? err = NativeBridge.Convert(item.SourcePath, outPath, target, opts);
            if (err is null) return true;
            item.StatusMessage = err;
            return false;
        }, ct);

        item.Status = ok ? ItemStatus.Done : ItemStatus.Failed;
        if (ok && string.IsNullOrEmpty(item.StatusMessage))
            item.StatusMessage = "OK";
        return ok;
    }
}
