using System.Globalization;
using System.Windows;
using System.Windows.Data;
using System.Windows.Media;
using ImgConvert.Models;

namespace ImgConvert.Converters;

public sealed class BoolToVisibility : IValueConverter
{
    public object Convert(object value, System.Type t, object p, CultureInfo c)
        => value is bool b && b ? Visibility.Visible : Visibility.Collapsed;
    public object ConvertBack(object value, System.Type t, object p, CultureInfo c)
        => value is Visibility v && v == Visibility.Visible;
}

public sealed class StatusToBrush : IValueConverter
{
    public object Convert(object value, System.Type t, object p, CultureInfo c)
    {
        if (value is not ItemStatus s) return Brushes.Gray;
        return s switch
        {
            ItemStatus.Done       => new SolidColorBrush(Color.FromRgb(0x2E, 0xA0, 0x43)),
            ItemStatus.Failed     => new SolidColorBrush(Color.FromRgb(0xCF, 0x40, 0x40)),
            ItemStatus.Converting => new SolidColorBrush(Color.FromRgb(0x3B, 0x82, 0xF6)),
            ItemStatus.Skipped    => new SolidColorBrush(Color.FromRgb(0x9C, 0xA3, 0xAF)),
            _                     => new SolidColorBrush(Color.FromRgb(0x6B, 0x72, 0x80)),
        };
    }
    public object ConvertBack(object value, System.Type t, object p, CultureInfo c)
        => Binding.DoNothing;
}

public sealed class StatusToText : IValueConverter
{
    public object Convert(object value, System.Type t, object p, CultureInfo c)
    {
        if (value is not ItemStatus s) return string.Empty;
        return s switch
        {
            ItemStatus.Pending    => "Pending",
            ItemStatus.Converting => "Converting…",
            ItemStatus.Done       => "Done",
            ItemStatus.Failed     => "Failed",
            ItemStatus.Skipped    => "Skipped",
            _ => s.ToString(),
        };
    }
    public object ConvertBack(object value, System.Type t, object p, CultureInfo c)
        => Binding.DoNothing;
}
