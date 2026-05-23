using Microsoft.Win32;
using System.Data;
using System.Diagnostics;
using System.IO;
using IOPath = System.IO.Path;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace PlutoGuiStarter;

public partial class MainWindow : Window
{
    private Process? _currentProcess;
    private string _lastPrefix = "2m";

    public MainWindow()
    {
        InitializeComponent();
        ProjectFolderTextBox.Text = GuessDefaultProjectFolder();
        RefreshConfigs();
        RefreshReports();
    }

    private static string GuessDefaultProjectFolder()
    {
        string candidate = IOPath.Combine(@"C:\msys64\home", Environment.UserName, "sdrdev", "pluto_native_test");
        return Directory.Exists(candidate) ? candidate : candidate;
    }

    private string ProjectFolder => ProjectFolderTextBox.Text.Trim();

    private string ScanSessionExe => IOPath.Combine(ProjectFolder, "build", "native", "pluto_scan_session.exe");

    private void BrowseProjectFolder_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFolderDialog
        {
            Title = "Select Pluto+ SDR project folder",
            InitialDirectory = Directory.Exists(ProjectFolder) ? ProjectFolder : @"C:\msys64\home"
        };

        if (dialog.ShowDialog(this) == true)
        {
            ProjectFolderTextBox.Text = dialog.FolderName;
            RefreshConfigs();
            RefreshReports();
        }
    }

    private void OpenProjectFolder_Click(object sender, RoutedEventArgs e)
    {
        if (Directory.Exists(ProjectFolder))
        {
            Process.Start(new ProcessStartInfo { FileName = ProjectFolder, UseShellExecute = true });
        }
    }

    private void RefreshConfigs_Click(object sender, RoutedEventArgs e) => RefreshConfigs();

    private void RefreshConfigs()
    {
        ConfigComboBox.Items.Clear();

        string configDir = IOPath.Combine(ProjectFolder, "configs");

        if (!Directory.Exists(configDir))
        {
            StatusTextBlock.Text = $"Config folder not found: {configDir}";
            return;
        }

        foreach (string file in Directory.GetFiles(configDir, "*.conf").OrderBy(x => x))
        {
            ConfigComboBox.Items.Add(IOPath.GetRelativePath(ProjectFolder, file));
        }

        if (ConfigComboBox.Items.Count > 0)
        {
            ConfigComboBox.SelectedIndex = 0;
            StatusTextBlock.Text = $"Loaded {ConfigComboBox.Items.Count} config profile(s).";
        }
    }

    private void ConfigComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (ConfigComboBox.SelectedItem is not string selectedConfig)
        {
            return;
        }

        string configPath = IOPath.Combine(ProjectFolder, selectedConfig);
        ConfigPreviewTextBox.Text = File.Exists(configPath) ? File.ReadAllText(configPath) : "";
        Dictionary<string, string> values = ReadSimpleConfig(configPath);

        if (values.TryGetValue("cycles", out string? cycles))
        {
            CyclesTextBox.Text = cycles;
        }

        _lastPrefix = values.TryGetValue("out_prefix", out string? prefix)
            ? prefix
            : values.TryGetValue("band", out string? band)
                ? band
                : IOPath.GetFileNameWithoutExtension(selectedConfig);

        OutPrefixTextBox.Text = "";
        GainDbTextBox.Text = "";
    }

    private void EditConfig_Click(object sender, RoutedEventArgs e)
    {
        if (ConfigComboBox.SelectedItem is not string selectedConfig)
        {
            return;
        }

        string configPath = IOPath.Combine(ProjectFolder, selectedConfig);

        if (File.Exists(configPath))
        {
            Process.Start(new ProcessStartInfo { FileName = "notepad.exe", Arguments = Quote(configPath), UseShellExecute = true });
        }
    }

    private async void RunButton_Click(object sender, RoutedEventArgs e) => await RunSessionAsync();

    private async Task RunSessionAsync()
    {
        if (_currentProcess != null)
        {
            MessageBox.Show(this, "A scan session is already running.");
            return;
        }

        if (!File.Exists(ScanSessionExe))
        {
            MessageBox.Show(this,
                $"Could not find:\n{ScanSessionExe}\n\nBuild the native tools after the repo layout change.",
                "Missing executable",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
            return;
        }

        if (ConfigComboBox.SelectedItem is not string selectedConfig)
        {
            MessageBox.Show(this, "Select a config profile first.");
            return;
        }

        string args = BuildArguments(selectedConfig);

        OutputTextBox.Clear();
        AppendOutputLine("Running:");
        AppendOutputLine($"{ScanSessionExe} {args}");
        AppendOutputLine("");

        var psi = new ProcessStartInfo
        {
            FileName = ScanSessionExe,
            Arguments = args,
            WorkingDirectory = ProjectFolder,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true
        };

        AddMsys2RuntimePath(psi);

        var process = new Process { StartInfo = psi, EnableRaisingEvents = true };

        process.OutputDataReceived += (_, eventArgs) =>
        {
            if (eventArgs.Data != null)
            {
                Dispatcher.Invoke(() => AppendOutputLine(eventArgs.Data));
            }
        };

        process.ErrorDataReceived += (_, eventArgs) =>
        {
            if (eventArgs.Data != null)
            {
                Dispatcher.Invoke(() => AppendOutputLine(eventArgs.Data));
            }
        };

        try
        {
            _currentProcess = process;
            RunButton.IsEnabled = false;
            StopButton.IsEnabled = true;
            StatusTextBlock.Text = "Running scan session...";

            process.Start();
            process.BeginOutputReadLine();
            process.BeginErrorReadLine();

            await process.WaitForExitAsync();

            AppendOutputLine("");
            AppendOutputLine($"Process exited with code {process.ExitCode}");

            if (process.ExitCode == 0)
            {
                StatusTextBlock.Text = "Scan session complete.";
                LoadSummaryCsvIfPresent();
                RefreshReports();

                if (OpenReportAfterRunCheckBox.IsChecked == true)
                {
                    OpenReportForLastPrefix(showError: false);
                }
            }
            else
            {
                StatusTextBlock.Text = $"Scan session failed: {process.ExitCode}";
            }
        }
        finally
        {
            process.Dispose();
            _currentProcess = null;
            RunButton.IsEnabled = true;
            StopButton.IsEnabled = false;
        }
    }

    private string BuildArguments(string selectedConfig)
    {
        var args = new List<string> { "--config", Quote(selectedConfig) };

        string cycles = CyclesTextBox.Text.Trim();
        if (!string.IsNullOrWhiteSpace(cycles))
        {
            args.Add("--cycles");
            args.Add(cycles);
        }

        string outPrefix = OutPrefixTextBox.Text.Trim();
        if (!string.IsNullOrWhiteSpace(outPrefix))
        {
            args.Add("--out-prefix");
            args.Add(Quote(outPrefix));
            _lastPrefix = outPrefix;
        }
        else
        {
            _lastPrefix = ReadOutPrefixFromConfig(IOPath.Combine(ProjectFolder, selectedConfig));
        }

        string gainDb = GainDbTextBox.Text.Trim();
        if (!string.IsNullOrWhiteSpace(gainDb))
        {
            args.Add("--gain-db");
            args.Add(gainDb);
        }

        if (DryRunCheckBox.IsChecked == true)
        {
            args.Add("--dry-run");
        }

        string extra = ExtraArgsTextBox.Text.Trim();
        if (!string.IsNullOrWhiteSpace(extra))
        {
            args.Add(extra);

            string? overridePrefix = GetOutPrefixOverride(extra);
            if (!string.IsNullOrWhiteSpace(overridePrefix))
            {
                _lastPrefix = overridePrefix;
            }
        }

        return string.Join(" ", args);
    }

    private void StopButton_Click(object sender, RoutedEventArgs e)
    {
        if (_currentProcess != null && !_currentProcess.HasExited)
        {
            _currentProcess.Kill(entireProcessTree: true);
            AppendOutputLine("Stop requested.");
        }
    }

    private void OpenReport_Click(object sender, RoutedEventArgs e) => OpenReportForLastPrefix(showError: true);

    private void OpenReportForLastPrefix(bool showError)
    {
        string report = IOPath.Combine(ProjectFolder, $"{_lastPrefix}_report.html");

        if (!File.Exists(report))
        {
            if (showError)
            {
                MessageBox.Show(this, $"Report not found:\n{report}");
            }

            return;
        }

        Process.Start(new ProcessStartInfo { FileName = report, UseShellExecute = true });
    }

    private void LoadSummary_Click(object sender, RoutedEventArgs e) => LoadSummaryCsvIfPresent(showError: true);

    private void LoadSummaryCsvIfPresent(bool showError = false)
    {
        string summary = IOPath.Combine(ProjectFolder, $"{_lastPrefix}_summary.csv");

        if (!File.Exists(summary))
        {
            if (showError)
            {
                MessageBox.Show(this, $"Summary CSV not found:\n{summary}");
            }

            return;
        }

        SummaryDataGrid.ItemsSource = LoadCsvAsDataTable(summary).DefaultView;
        StatusTextBlock.Text = $"Loaded summary: {IOPath.GetFileName(summary)}";
    }

    private static DataTable LoadCsvAsDataTable(string filename)
    {
        var table = new DataTable();

        using var reader = new StreamReader(filename);
        string? headerLine = reader.ReadLine();

        if (headerLine == null)
        {
            return table;
        }

        foreach (string header in ParseCsvLine(headerLine))
        {
            string name = string.IsNullOrWhiteSpace(header) ? $"Column{table.Columns.Count + 1}" : header;
            if (table.Columns.Contains(name))
            {
                name = $"{name}_{table.Columns.Count + 1}";
            }
            table.Columns.Add(name);
        }

        while (!reader.EndOfStream)
        {
            string? line = reader.ReadLine();
            if (string.IsNullOrWhiteSpace(line))
            {
                continue;
            }

            List<string> fields = ParseCsvLine(line);
            DataRow row = table.NewRow();

            for (int i = 0; i < table.Columns.Count; i++)
            {
                row[i] = i < fields.Count ? fields[i] : "";
            }

            table.Rows.Add(row);
        }

        return table;
    }

    private static List<string> ParseCsvLine(string line)
    {
        var fields = new List<string>();
        var sb = new StringBuilder();
        bool inQuotes = false;

        for (int i = 0; i < line.Length; i++)
        {
            char c = line[i];

            if (c == '"')
            {
                if (inQuotes && i + 1 < line.Length && line[i + 1] == '"')
                {
                    sb.Append('"');
                    i++;
                }
                else
                {
                    inQuotes = !inQuotes;
                }
            }
            else if (c == ',' && !inQuotes)
            {
                fields.Add(sb.ToString());
                sb.Clear();
            }
            else
            {
                sb.Append(c);
            }
        }

        fields.Add(sb.ToString());
        return fields;
    }

    private void AppendOutputLine(string text)
    {
        OutputTextBox.AppendText(text + Environment.NewLine);
        OutputTextBox.ScrollToEnd();
    }

    private void ClearOutput_Click(object sender, RoutedEventArgs e) => OutputTextBox.Clear();

    private void CopyOutput_Click(object sender, RoutedEventArgs e) => Clipboard.SetText(OutputTextBox.Text);

    private void RefreshReports_Click(object sender, RoutedEventArgs e) => RefreshReports();

    private void RefreshReports()
    {
        ReportsListView.Items.Clear();

        if (!Directory.Exists(ProjectFolder))
        {
            return;
        }

        foreach (string file in Directory.GetFiles(ProjectFolder, "*_report.html").OrderByDescending(File.GetLastWriteTime))
        {
            var info = new FileInfo(file);
            ReportsListView.Items.Add(new ReportItem
            {
                Name = info.Name,
                Modified = info.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss"),
                SizeKb = Math.Max(1, info.Length / 1024).ToString(),
                FullPath = info.FullName
            });
        }
    }

    private void OpenSelectedReport_Click(object sender, RoutedEventArgs e) => OpenSelectedReport();

    private void OpenLatestReport_Click(object sender, RoutedEventArgs e)
    {
        RefreshReports();
        if (ReportsListView.Items.Count > 0)
        {
            ReportsListView.SelectedIndex = 0;
            OpenSelectedReport();
        }
    }

    private void ReportsListView_MouseDoubleClick(object sender, MouseButtonEventArgs e) => OpenSelectedReport();

    private void OpenSelectedReport()
    {
        if (ReportsListView.SelectedItem is ReportItem report && File.Exists(report.FullPath))
        {
            Process.Start(new ProcessStartInfo { FileName = report.FullPath, UseShellExecute = true });
        }
    }

    private static Dictionary<string, string> ReadSimpleConfig(string configPath)
    {
        var values = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        if (!File.Exists(configPath))
        {
            return values;
        }

        foreach (string rawLine in File.ReadLines(configPath))
        {
            string line = rawLine;
            int hash = line.IndexOf('#');
            if (hash >= 0) line = line[..hash];
            int semi = line.IndexOf(';');
            if (semi >= 0) line = line[..semi];

            line = line.Trim();
            int equals = line.IndexOf('=');

            if (equals < 0)
            {
                continue;
            }

            string key = line[..equals].Trim();
            string value = line[(equals + 1)..].Trim().Trim('"', '\'');

            values[key] = value;
        }

        return values;
    }

    private static string ReadOutPrefixFromConfig(string configPath)
    {
        Dictionary<string, string> values = ReadSimpleConfig(configPath);

        if (values.TryGetValue("out_prefix", out string? prefix) && !string.IsNullOrWhiteSpace(prefix))
        {
            return prefix;
        }

        if (values.TryGetValue("band", out string? band) && !string.IsNullOrWhiteSpace(band))
        {
            return band;
        }

        return IOPath.GetFileNameWithoutExtension(configPath);
    }

    private static string? GetOutPrefixOverride(string args)
    {
        string[] parts = SplitCommandLineLike(args);

        for (int i = 0; i < parts.Length - 1; i++)
        {
            if (parts[i] == "--out-prefix")
            {
                return parts[i + 1];
            }
        }

        return null;
    }

    private static string[] SplitCommandLineLike(string text)
    {
        var parts = new List<string>();
        var sb = new StringBuilder();
        bool inQuotes = false;

        foreach (char c in text)
        {
            if (c == '"')
            {
                inQuotes = !inQuotes;
            }
            else if (char.IsWhiteSpace(c) && !inQuotes)
            {
                if (sb.Length > 0)
                {
                    parts.Add(sb.ToString());
                    sb.Clear();
                }
            }
            else
            {
                sb.Append(c);
            }
        }

        if (sb.Length > 0)
        {
            parts.Add(sb.ToString());
        }

        return parts.ToArray();
    }

    private static void AddMsys2RuntimePath(ProcessStartInfo psi)
    {
        string oldPath = psi.EnvironmentVariables["PATH"] ?? "";
        psi.EnvironmentVariables["PATH"] = $@"C:\msys64\ucrt64\bin;C:\msys64\usr\bin;{oldPath}";
    }

    private static string Quote(string value) => "\"" + value.Replace("\"", "\\\"") + "\"";

    private sealed class ReportItem
    {
        public string Name { get; set; } = "";
        public string Modified { get; set; } = "";
        public string SizeKb { get; set; } = "";
        public string FullPath { get; set; } = "";
    }
}
