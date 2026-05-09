using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using ImgConvert.Models;
using ImgConvert.Services;
using Microsoft.Win32;

namespace ImgConvert.ViewModels;

public partial class MainViewModel : ObservableObject
{
    private static readonly string[] SupportedExtensions =
        { ".jpg", ".jpeg", ".jpe", ".jfif", ".png", ".webp", ".tif", ".tiff", ".heic", ".heif", ".hif", ".avif" };

    public ObservableCollection<ConversionItem> Items { get; } = new();

    public string[] TargetFormats { get; } = { "JPG", "PNG", "WEBP", "TIFF", "HEIF" };

    [ObservableProperty] private string _selectedTargetFormat = "PNG";

    [ObservableProperty] private string _outputDirectory = string.Empty;
    [ObservableProperty] private bool _overwriteExisting;

    [ObservableProperty] private int _quality = 90;             // 1..100
    [ObservableProperty] private int _pngCompression = 6;       // 0..9
    [ObservableProperty] private bool _lossless;
    [ObservableProperty] private bool _jpegProgressive;

    public string[] TiffCompressionChoices { get; } = { "None", "LZW", "ZIP (Deflate)", "PackBits" };
    [ObservableProperty] private string _selectedTiffCompression = "LZW";

    [ObservableProperty] private bool _isConverting;
    [ObservableProperty] private double _progress;              // 0..1
    [ObservableProperty] private string _progressText = string.Empty;

    [ObservableProperty] private int _maxParallelism = System.Environment.ProcessorCount;

    private CancellationTokenSource? _cts;

    public MainViewModel()
    {
        OutputDirectory = Path.Combine(
            System.Environment.GetFolderPath(System.Environment.SpecialFolder.MyPictures),
            "ImgConvert");
    }

    public NativeBridge.Format CurrentTarget => SelectedTargetFormat switch
    {
        "JPG"  => NativeBridge.Format.Jpg,
        "PNG"  => NativeBridge.Format.Png,
        "WEBP" => NativeBridge.Format.Webp,
        "TIFF" => NativeBridge.Format.Tiff,
        "HEIF" => NativeBridge.Format.Heif,
        _      => NativeBridge.Format.Png,
    };

    public bool ShowQualitySlider     => CurrentTarget is NativeBridge.Format.Jpg or NativeBridge.Format.Webp or NativeBridge.Format.Heif;
    public bool ShowLosslessToggle    => CurrentTarget is NativeBridge.Format.Webp or NativeBridge.Format.Heif;
    public bool ShowProgressiveToggle => CurrentTarget is NativeBridge.Format.Jpg;
    public bool ShowPngCompression    => CurrentTarget is NativeBridge.Format.Png;
    public bool ShowTiffCompression   => CurrentTarget is NativeBridge.Format.Tiff;

    partial void OnSelectedTargetFormatChanged(string value)
    {
        OnPropertyChanged(nameof(CurrentTarget));
        OnPropertyChanged(nameof(ShowQualitySlider));
        OnPropertyChanged(nameof(ShowLosslessToggle));
        OnPropertyChanged(nameof(ShowProgressiveToggle));
        OnPropertyChanged(nameof(ShowPngCompression));
        OnPropertyChanged(nameof(ShowTiffCompression));
    }

    [RelayCommand]
    private void BrowseFiles()
    {
        var dlg = new OpenFileDialog
        {
            Multiselect = true,
            Title = "Select images to convert",
            Filter = "Images|*.jpg;*.jpeg;*.jpe;*.jfif;*.png;*.webp;*.tif;*.tiff;*.heic;*.heif;*.hif;*.avif|All files|*.*",
        };
        if (dlg.ShowDialog() == true)
        {
            AddPaths(dlg.FileNames);
        }
    }

    [RelayCommand]
    private void BrowseOutputDir()
    {
        var dlg = new OpenFolderDialog
        {
            Title = "Select output folder",
            InitialDirectory = Directory.Exists(OutputDirectory)
                ? OutputDirectory
                : System.Environment.GetFolderPath(System.Environment.SpecialFolder.MyPictures),
        };
        if (dlg.ShowDialog() == true)
        {
            OutputDirectory = dlg.FolderName;
        }
    }

    [RelayCommand]
    private void Clear()
    {
        if (IsConverting) return;
        Items.Clear();
        Progress = 0;
        ProgressText = string.Empty;
    }

    [RelayCommand]
    private void RemoveItem(ConversionItem? item)
    {
        if (item is null) return;
        Items.Remove(item);
    }

    public void AddPaths(System.Collections.Generic.IEnumerable<string> paths)
    {
        foreach (string raw in paths)
        {
            if (Directory.Exists(raw))
            {
                foreach (string nested in Directory.EnumerateFiles(raw, "*", SearchOption.AllDirectories))
                {
                    TryAddFile(nested);
                }
            }
            else if (File.Exists(raw))
            {
                TryAddFile(raw);
            }
        }
    }

    private void TryAddFile(string path)
    {
        string ext = Path.GetExtension(path).ToLowerInvariant();
        if (!SupportedExtensions.Contains(ext)) return;
        if (Items.Any(i => string.Equals(i.SourcePath, path, System.StringComparison.OrdinalIgnoreCase))) return;

        var info = new FileInfo(path);
        var item = new ConversionItem
        {
            SourcePath = path,
            SourceSizeBytes = info.Exists ? info.Length : 0,
            SourceFormat = NativeBridge.DetectFormat(path).ToString().ToUpperInvariant(),
        };
        Items.Add(item);
    }

    [RelayCommand(CanExecute = nameof(CanStart))]
    private async Task StartAsync()
    {
        if (Items.Count == 0) return;

        IsConverting = true;
        Progress = 0;
        ProgressText = $"Converting 0 / {Items.Count}";
        StartCommand.NotifyCanExecuteChanged();
        CancelCommand.NotifyCanExecuteChanged();

        _cts = new CancellationTokenSource();
        var ct = _cts.Token;

        foreach (var i in Items) i.Status = ItemStatus.Pending;

        var opts = BuildOptions();
        var target = CurrentTarget;
        string outDir = OutputDirectory;
        bool overwrite = OverwriteExisting;

        int done = 0;
        int total = Items.Count;
        int parallelism = System.Math.Max(1, MaxParallelism);

        try
        {
            using var throttle = new SemaphoreSlim(parallelism);
            var tasks = Items.Select(async item =>
            {
                await throttle.WaitAsync(ct);
                try
                {
                    await ConversionService.ConvertOneAsync(item, outDir, target, opts, overwrite, ct);
                }
                finally
                {
                    int now = Interlocked.Increment(ref done);
                    Progress = (double)now / total;
                    ProgressText = $"Converting {now} / {total}";
                    throttle.Release();
                }
            }).ToArray();

            await Task.WhenAll(tasks);
        }
        catch (System.OperationCanceledException)
        {
            ProgressText = "Cancelled";
        }
        finally
        {
            int ok = Items.Count(i => i.Status == ItemStatus.Done);
            int failed = Items.Count(i => i.Status == ItemStatus.Failed);
            ProgressText = $"Finished — {ok} succeeded, {failed} failed";
            IsConverting = false;
            _cts?.Dispose();
            _cts = null;
            StartCommand.NotifyCanExecuteChanged();
            CancelCommand.NotifyCanExecuteChanged();
        }
    }

    private bool CanStart() => !IsConverting && Items.Count > 0;

    [RelayCommand(CanExecute = nameof(CanCancel))]
    private void Cancel() => _cts?.Cancel();
    private bool CanCancel() => IsConverting;

    private NativeBridge.Options BuildOptions() => new()
    {
        Quality          = Quality,
        PngCompression   = PngCompression,
        Lossless         = Lossless ? 1 : 0,
        JpegProgressive  = JpegProgressive ? 1 : 0,
        TiffCompression  = SelectedTiffCompression switch
        {
            "None"           => (int)NativeBridge.TiffCompression.None,
            "LZW"            => (int)NativeBridge.TiffCompression.Lzw,
            "ZIP (Deflate)"  => (int)NativeBridge.TiffCompression.Zip,
            "PackBits"       => (int)NativeBridge.TiffCompression.PackBits,
            _                => (int)NativeBridge.TiffCompression.Lzw,
        },
    };

    partial void OnIsConvertingChanged(bool value)
    {
        StartCommand.NotifyCanExecuteChanged();
        CancelCommand.NotifyCanExecuteChanged();
    }
}
