using Microsoft.Win32;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using IOPath = System.IO.Path;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;

namespace PlutoLiveSpectrumGui;

public partial class MainWindow : Window
{
    private const int WaterfallHeight = 320;

    private Process? _process;
    private double[] _lastPowerDb = Array.Empty<double>();
    private double _centerHz = 146_520_000;
    private double _sampleRateHz = 1_000_000;
    private double _binWidthHz = 0;
    private double _minOffsetHz = 0;
    private int _frame;

    private WriteableBitmap? _waterfallBitmap;
    private int[] _waterfallPixels = Array.Empty<int>();
    private int _waterfallWidth = 0;

    private readonly ObservableCollection<PeakItem> _peaks = new();

    public MainWindow()
    {
        InitializeComponent();
        ProjectFolderTextBox.Text = GuessDefaultProjectFolder();
        PeaksDataGrid.ItemsSource = _peaks;
    }

    private static string GuessDefaultProjectFolder()
    {
        string candidate = IOPath.Combine(@"C:\msys64\home", Environment.UserName, "sdrdev", "pluto_native_test");
        return Directory.Exists(candidate) ? candidate : candidate;
    }

    private string ProjectFolder => ProjectFolderTextBox.Text.Trim();
    private string StreamerPath => IOPath.Combine(ProjectFolder, "build", "native", "pluto_spectrum_stream.exe");

    private void BrowseProjectFolder_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFolderDialog { Title = "Select Pluto project folder", InitialDirectory = Directory.Exists(ProjectFolder) ? ProjectFolder : @"C:\msys64\home" };
        if (dialog.ShowDialog(this) == true) ProjectFolderTextBox.Text = dialog.FolderName;
    }

    private void OpenProjectFolder_Click(object sender, RoutedEventArgs e)
    {
        if (Directory.Exists(ProjectFolder)) Process.Start(new ProcessStartInfo { FileName = ProjectFolder, UseShellExecute = true });
    }

    private async void StartButton_Click(object sender, RoutedEventArgs e) => await StartSpectrumAsync();

    private async Task StartSpectrumAsync()
    {
        if (_process != null) return;

        if (!File.Exists(StreamerPath))
        {
            MessageBox.Show(this, $"Could not find:\n{StreamerPath}\n\nBuild the native tools after the repo layout change.", "Missing streamer", MessageBoxButton.OK, MessageBoxImage.Error);
            return;
        }

        string args = BuildArguments();
        LogTextBox.Clear();
        AppendLog($"Running: {StreamerPath} {args}");

        var psi = new ProcessStartInfo
        {
            FileName = StreamerPath,
            Arguments = args,
            WorkingDirectory = ProjectFolder,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true
        };

        AddMsys2RuntimePath(psi);

        var process = new Process { StartInfo = psi, EnableRaisingEvents = true };
        process.OutputDataReceived += (_, a) => { if (a.Data != null) Dispatcher.Invoke(() => HandleOutputLine(a.Data)); };
        process.ErrorDataReceived += (_, a) => { if (a.Data != null) Dispatcher.Invoke(() => AppendLog(a.Data)); };

        try
        {
            _process = process;
            StartButton.IsEnabled = false;
            StopButton.IsEnabled = true;

            process.Start();
            process.BeginOutputReadLine();
            process.BeginErrorReadLine();

            await process.WaitForExitAsync();
            AppendLog($"Streamer exited with code {process.ExitCode}");
        }
        finally
        {
            process.Dispose();
            _process = null;
            StartButton.IsEnabled = true;
            StopButton.IsEnabled = false;
        }
    }

    private string BuildArguments()
    {
        var parts = new List<string>
        {
            "--freq", FrequencyTextBox.Text.Trim(),
            "--rate", RateTextBox.Text.Trim(),
            "--fft", SelectedComboText(FftComboBox),
            "--avg", AverageTextBox.Text.Trim(),
            "--interval-ms", IntervalTextBox.Text.Trim(),
            "--gain-mode", Quote(SelectedComboText(GainModeComboBox))
        };

        if (!string.IsNullOrWhiteSpace(GainDbTextBox.Text))
        {
            parts.Add("--gain-db");
            parts.Add(GainDbTextBox.Text.Trim());
        }

        if (!string.IsNullOrWhiteSpace(ExtraArgsTextBox.Text))
        {
            parts.Add(ExtraArgsTextBox.Text.Trim());
        }

        return string.Join(" ", parts);
    }

    private void StopButton_Click(object sender, RoutedEventArgs e)
    {
        if (_process != null && !_process.HasExited) _process.Kill(entireProcessTree: true);
    }

    private void HandleOutputLine(string line)
    {
        if (line.StartsWith("SPECTRUM,", StringComparison.Ordinal)) ParseSpectrumLine(line);
        else if (line.StartsWith("STATUS,", StringComparison.Ordinal)) { AppendLog(line); StatusTextBlock.Text = line[7..]; }
        else AppendLog(line);
    }

    private void ParseSpectrumLine(string line)
    {
        string[] p = line.Split(',');
        if (p.Length < 8) return;

        try
        {
            _frame = int.Parse(p[1], CultureInfo.InvariantCulture);
            _centerHz = double.Parse(p[2], CultureInfo.InvariantCulture);
            _sampleRateHz = double.Parse(p[3], CultureInfo.InvariantCulture);
            int fftSize = int.Parse(p[4], CultureInfo.InvariantCulture);
            _binWidthHz = double.Parse(p[5], CultureInfo.InvariantCulture);
            _minOffsetHz = double.Parse(p[6], CultureInfo.InvariantCulture);
            if (p.Length < 7 + fftSize) return;

            double[] db = new double[fftSize];
            for (int i = 0; i < fftSize; i++) db[i] = double.Parse(p[7 + i], CultureInfo.InvariantCulture);

            _lastPowerDb = db;
            DetectPeaks();
            DrawSpectrum();

            if (WaterfallEnabledCheckBox.IsChecked == true) UpdateWaterfall(db);

            StatusTextBlock.Text = $"Frame {_frame}, center {_centerHz / 1e6:F6} MHz, span {_sampleRateHz / 1e6:F3} MHz, peaks {_peaks.Count}";
        }
        catch
        {
        }
    }

    private void DetectPeaks()
    {
        _peaks.Clear();
        if (_lastPowerDb.Length == 0) return;

        int top = Math.Clamp(ParseIntOrDefault(TopPeaksTextBox.Text, 10), 1, 50);
        double threshold = ParseDoubleOrDefault(PeakThresholdTextBox.Text, 10);
        double floor = Percentile(_lastPowerDb, 0.50);
        bool[] used = new bool[_lastPowerDb.Length];
        int exclude = Math.Max(8, _lastPowerDb.Length / 128);

        for (int rank = 1; rank <= top; rank++)
        {
            int best = -1;
            double bestDb = double.NegativeInfinity;

            for (int i = 1; i < _lastPowerDb.Length - 1; i++)
            {
                if (used[i]) continue;
                if (_lastPowerDb[i] < _lastPowerDb[i - 1] || _lastPowerDb[i] < _lastPowerDb[i + 1]) continue;
                if (_lastPowerDb[i] > bestDb) { bestDb = _lastPowerDb[i]; best = i; }
            }

            if (best < 0) break;

            double snr = bestDb - floor;
            if (snr < threshold) break;

            double offsetHz = _minOffsetHz + best * _binWidthHz;
            double freqHz = _centerHz + offsetHz;

            _peaks.Add(new PeakItem
            {
                Rank = rank.ToString(CultureInfo.InvariantCulture),
                Bin = best,
                FrequencyMHz = (freqHz / 1e6).ToString("F6", CultureInfo.InvariantCulture),
                OffsetKHz = (offsetHz / 1e3).ToString("F2", CultureInfo.InvariantCulture),
                PowerDb = bestDb.ToString("F1", CultureInfo.InvariantCulture),
                SnrDb = snr.ToString("F1", CultureInfo.InvariantCulture)
            });

            for (int i = Math.Max(0, best - exclude); i <= Math.Min(_lastPowerDb.Length - 1, best + exclude); i++) used[i] = true;
        }
    }

    private void DrawSpectrum()
    {
        SpectrumCanvas.Children.Clear();
        double w = SpectrumCanvas.ActualWidth;
        double h = SpectrumCanvas.ActualHeight;
        if (w <= 10 || h <= 10 || _lastPowerDb.Length == 0) return;

        for (int i = 1; i < 10; i++)
        {
            double x = i * w / 10.0;
            SpectrumCanvas.Children.Add(new Line { X1 = x, X2 = x, Y1 = 0, Y2 = h, Stroke = new SolidColorBrush(Color.FromRgb(45, 58, 70)), StrokeThickness = 1 });
        }

        for (int i = 1; i < 6; i++)
        {
            double y = i * h / 6.0;
            SpectrumCanvas.Children.Add(new Line { X1 = 0, X2 = w, Y1 = y, Y2 = y, Stroke = new SolidColorBrush(Color.FromRgb(45, 58, 70)), StrokeThickness = 1 });
        }

        double min = _lastPowerDb.Min();
        double max = _lastPowerDb.Max();
        double floor = Math.Floor(min / 10) * 10;
        double ceiling = Math.Max(floor + 20, Math.Ceiling(max / 10) * 10);

        var poly = new Polyline { Stroke = Brushes.LimeGreen, StrokeThickness = 1.5 };
        for (int i = 0; i < _lastPowerDb.Length; i++)
        {
            double x = (double)i / (_lastPowerDb.Length - 1) * w;
            double y = h - Math.Clamp((_lastPowerDb[i] - floor) / (ceiling - floor), 0, 1) * h;
            poly.Points.Add(new Point(x, y));
        }
        SpectrumCanvas.Children.Add(poly);

        if (ShowPeaksCheckBox.IsChecked == true)
        {
            foreach (PeakItem peak in _peaks.Take(12))
            {
                double x = (double)peak.Bin / (_lastPowerDb.Length - 1) * w;
                SpectrumCanvas.Children.Add(new Line { X1 = x, X2 = x, Y1 = 0, Y2 = h, Stroke = Brushes.Orange, StrokeThickness = 1, StrokeDashArray = new DoubleCollection { 4, 4 } });
                var label = new TextBlock { Text = peak.Rank, Foreground = Brushes.Orange, FontWeight = FontWeights.Bold };
                Canvas.SetLeft(label, Math.Clamp(x + 3, 0, Math.Max(0, w - 20)));
                Canvas.SetTop(label, 8);
                SpectrumCanvas.Children.Add(label);
            }
        }

        OverlayTextBlock.Text = $"Center: {_centerHz / 1e6:F6} MHz   Span: {_sampleRateHz / 1e6:F3} MHz   Peak: {max:F1} dB";
    }

    private void UpdateWaterfall(double[] db)
    {
        EnsureWaterfallBitmap(db.Length);
        double min = AutoScaleCheckBox.IsChecked == true ? Percentile(db, 0.08) : -80;
        double max = AutoScaleCheckBox.IsChecked == true ? Percentile(db, 0.995) : -25;
        if (max - min < 15) max = min + 15;

        Array.Copy(_waterfallPixels, 0, _waterfallPixels, _waterfallWidth, _waterfallWidth * (WaterfallHeight - 1));
        for (int x = 0; x < _waterfallWidth; x++)
        {
            double t = Math.Clamp((db[x] - min) / (max - min), 0, 1);
            _waterfallPixels[x] = ColorMap(t);
        }
        _waterfallBitmap!.WritePixels(new Int32Rect(0, 0, _waterfallWidth, WaterfallHeight), _waterfallPixels, _waterfallWidth * 4, 0);
        WaterfallOverlayTextBlock.Text = $"Waterfall   {min:F1} to {max:F1} dB   Newest at top";
    }

    private void EnsureWaterfallBitmap(int width)
    {
        if (_waterfallBitmap != null && _waterfallWidth == width) return;
        _waterfallWidth = width;
        _waterfallPixels = new int[_waterfallWidth * WaterfallHeight];
        _waterfallBitmap = new WriteableBitmap(_waterfallWidth, WaterfallHeight, 96, 96, PixelFormats.Bgr32, null);
        WaterfallImage.Source = _waterfallBitmap;
    }

    private static int ColorMap(double t)
    {
        t = Math.Clamp(t, 0, 1);
        byte r = (byte)(255 * Math.Clamp((t - 0.45) / 0.55, 0, 1));
        byte g = (byte)(255 * Math.Clamp(1 - Math.Abs(t - 0.60) / 0.45, 0, 1));
        byte b = (byte)(255 * Math.Clamp(1 - t / 0.65, 0, 1));
        return b | (g << 8) | (r << 16);
    }

    private static double Percentile(double[] v, double p)
    {
        double[] c = new double[v.Length];
        Array.Copy(v, c, v.Length);
        Array.Sort(c);
        int i = (int)Math.Clamp(Math.Round(p * (c.Length - 1)), 0, c.Length - 1);
        return c[i];
    }

    private void ClearWaterfall_Click(object sender, RoutedEventArgs e)
    {
        Array.Fill(_waterfallPixels, 0);
        _waterfallBitmap?.WritePixels(new Int32Rect(0, 0, _waterfallWidth, WaterfallHeight), _waterfallPixels, _waterfallWidth * 4, 0);
    }

    private void CopyPeaks_Click(object sender, RoutedEventArgs e)
    {
        var sb = new StringBuilder();
        sb.AppendLine("rank,frequency_mhz,offset_khz,power_db,snr_db");
        foreach (var p in _peaks) sb.AppendLine($"{p.Rank},{p.FrequencyMHz},{p.OffsetKHz},{p.PowerDb},{p.SnrDb}");
        Clipboard.SetText(sb.ToString());
    }

    private void PresetFm_Click(object sender, RoutedEventArgs e) { FrequencyTextBox.Text = "100000000"; RateTextBox.Text = "2000000"; FftComboBox.SelectedIndex = 2; }
    private void PresetNoaa_Click(object sender, RoutedEventArgs e) { FrequencyTextBox.Text = "162550000"; RateTextBox.Text = "1000000"; FftComboBox.SelectedIndex = 1; }
    private void Preset2m_Click(object sender, RoutedEventArgs e) { FrequencyTextBox.Text = "146520000"; RateTextBox.Text = "1000000"; FftComboBox.SelectedIndex = 1; }
    private void PresetAirband_Click(object sender, RoutedEventArgs e) { FrequencyTextBox.Text = "125000000"; RateTextBox.Text = "1000000"; FftComboBox.SelectedIndex = 1; }
    private void SpectrumCanvas_SizeChanged(object sender, SizeChangedEventArgs e) => DrawSpectrum();

    private void AppendLog(string text) { LogTextBox.AppendText(text + Environment.NewLine); LogTextBox.ScrollToEnd(); }
    private static int ParseIntOrDefault(string s, int fallback) => int.TryParse(s.Trim(), out int v) ? v : fallback;
    private static double ParseDoubleOrDefault(string s, double fallback) => double.TryParse(s.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out double v) ? v : fallback;
    private static string SelectedComboText(ComboBox c) => c.SelectedItem is ComboBoxItem item ? item.Content?.ToString() ?? "" : c.Text ?? "";
    private static void AddMsys2RuntimePath(ProcessStartInfo psi)
    {
        string oldPath = psi.EnvironmentVariables["PATH"] ?? "";
        psi.EnvironmentVariables["PATH"] = $@"C:\msys64\ucrt64\bin;C:\msys64\usr\bin;{oldPath}";
    }
    private static string Quote(string v) => "\"" + v.Replace("\"", "\\\"") + "\"";

    public sealed class PeakItem
    {
        public string Rank { get; set; } = "";
        public int Bin { get; set; }
        public string FrequencyMHz { get; set; } = "";
        public string OffsetKHz { get; set; } = "";
        public string PowerDb { get; set; } = "";
        public string SnrDb { get; set; } = "";
    }
}
