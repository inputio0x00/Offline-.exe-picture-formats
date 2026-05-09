using System.Windows;
using ImgConvert.ViewModels;
using Wpf.Ui.Controls;

namespace ImgConvert;

public partial class MainWindow : FluentWindow
{
    public MainWindow()
    {
        InitializeComponent();
    }

    private MainViewModel? Vm => DataContext as MainViewModel;

    private void OnDragOver(object sender, DragEventArgs e)
    {
        e.Effects = e.Data.GetDataPresent(DataFormats.FileDrop)
            ? DragDropEffects.Copy
            : DragDropEffects.None;
        e.Handled = true;
    }

    private void OnDrop(object sender, DragEventArgs e)
    {
        if (Vm is null) return;
        if (Vm.IsConverting) return;
        if (e.Data.GetData(DataFormats.FileDrop) is string[] paths)
        {
            Vm.AddPaths(paths);
        }
        e.Handled = true;
    }
}
