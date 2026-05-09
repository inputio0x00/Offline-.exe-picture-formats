using System.IO;
using CommunityToolkit.Mvvm.ComponentModel;

namespace ImgConvert.Models;

public enum ItemStatus
{
    Pending,
    Converting,
    Done,
    Failed,
    Skipped,
}

public partial class ConversionItem : ObservableObject
{
    [ObservableProperty] private string _sourcePath = string.Empty;
    [ObservableProperty] private string _sourceFormat = string.Empty;
    [ObservableProperty] private long _sourceSizeBytes;
    [ObservableProperty] private ItemStatus _status = ItemStatus.Pending;
    [ObservableProperty] private string _statusMessage = string.Empty;
    [ObservableProperty] private string _outputPath = string.Empty;

    public string FileName => Path.GetFileName(SourcePath);

    public string SizeDisplay
    {
        get
        {
            double bytes = SourceSizeBytes;
            string[] suffix = { "B", "KB", "MB", "GB" };
            int i = 0;
            while (bytes >= 1024 && i < suffix.Length - 1)
            {
                bytes /= 1024;
                i++;
            }
            return $"{bytes:0.##} {suffix[i]}";
        }
    }

    partial void OnSourceSizeBytesChanged(long value) => OnPropertyChanged(nameof(SizeDisplay));
    partial void OnSourcePathChanged(string value) => OnPropertyChanged(nameof(FileName));
}
