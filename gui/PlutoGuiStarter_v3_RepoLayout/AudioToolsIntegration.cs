using System;
using System.Diagnostics;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace PlutoGuiStarter
{
    public static class AudioToolsIntegration
    {
        private const string InjectedMarker = "PlutoAudioToolsInjected";

        public static void Attach(Window window)
        {
            if (window == null) return;
            window.Loaded -= OnWindowLoaded;
            window.Loaded += OnWindowLoaded;
        }

        private static void OnWindowLoaded(object sender, RoutedEventArgs e)
        {
            if (sender is Window window) InjectToolbar(window);
        }

        private static void InjectToolbar(Window window)
        {
            if (window.Content is DockPanel existingDock && existingDock.Tag is string tag && tag == InjectedMarker) return;
            if (window.Content is not UIElement originalContent) return;

            var toolbar = BuildToolbar();
            var dock = new DockPanel { LastChildFill = true, Tag = InjectedMarker };
            DockPanel.SetDock(toolbar, Dock.Top);
            window.Content = null;
            dock.Children.Add(toolbar);
            dock.Children.Add(originalContent);
            window.Content = dock;
        }

        private static Border BuildToolbar()
        {
            var panel = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(8, 6, 8, 6) };
            panel.Children.Add(new TextBlock
            {
                Text = "Audio Tools:",
                VerticalAlignment = VerticalAlignment.Center,
                FontWeight = FontWeights.SemiBold,
                Margin = new Thickness(0, 0, 8, 0)
            });
            panel.Children.Add(MakeButton("Audio Menu", "Open the NOAA / Airband / FM audio menu", () => RunLauncher("run_audio_menu.cmd")));
            panel.Children.Add(MakeButton("Audio Report", "Generate and open sessions\\audio_report.html", () => RunLauncher("make_audio_report.cmd")));
            panel.Children.Add(MakeButton("Sessions Folder", "Open the sessions output folder", OpenSessionsFolder));

            return new Border
            {
                BorderBrush = new SolidColorBrush(Color.FromRgb(210, 210, 210)),
                BorderThickness = new Thickness(0, 0, 0, 1),
                Background = new SolidColorBrush(Color.FromRgb(245, 247, 250)),
                Child = panel
            };
        }

        private static Button MakeButton(string text, string toolTip, Action onClick)
        {
            var button = new Button
            {
                Content = text,
                ToolTip = toolTip,
                Padding = new Thickness(10, 4, 10, 4),
                Margin = new Thickness(0, 0, 6, 0),
                MinWidth = 92
            };
            button.Click += (_, _) =>
            {
                try { onClick(); }
                catch (Exception ex)
                {
                    MessageBox.Show(ex.Message, "Pluto+ Audio Tools", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            };
            return button;
        }

        private static void RunLauncher(string launcherName)
        {
            var root = FindToolkitRoot();
            if (root == null)
            {
                MessageBox.Show("Could not find the Pluto+ toolkit root folder.\n\nExpected launchers\\" + launcherName + " near the GUI executable, or in a parent folder.", "Pluto+ Audio Tools", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            var launcher = Path.Combine(root, "launchers", launcherName);
            if (!File.Exists(launcher))
            {
                MessageBox.Show("Launcher not found:\n" + launcher, "Pluto+ Audio Tools", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            Process.Start(new ProcessStartInfo
            {
                FileName = "cmd.exe",
                Arguments = "/c start \"\" \"" + launcher + "\"",
                WorkingDirectory = Path.GetDirectoryName(launcher) ?? root,
                UseShellExecute = false,
                CreateNoWindow = true
            });
        }

        private static void OpenSessionsFolder()
        {
            var root = FindToolkitRoot();
            if (root == null)
            {
                MessageBox.Show("Could not find the Pluto+ toolkit root folder.", "Pluto+ Audio Tools", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }
            var sessions = Path.Combine(root, "sessions");
            Directory.CreateDirectory(sessions);
            Process.Start(new ProcessStartInfo { FileName = sessions, UseShellExecute = true });
        }

        private static string? FindToolkitRoot()
        {
            var dir = new DirectoryInfo(AppContext.BaseDirectory);
            while (dir != null)
            {
                if (LooksLikeReleaseRoot(dir.FullName) || LooksLikeRepoRoot(dir.FullName)) return dir.FullName;
                dir = dir.Parent;
            }
            return null;
        }

        private static bool LooksLikeReleaseRoot(string dir)
        {
            return File.Exists(Path.Combine(dir, "launchers", "run_audio_menu.cmd")) &&
                   File.Exists(Path.Combine(dir, "bin", "native", "pluto_audio_monitor.exe"));
        }

        private static bool LooksLikeRepoRoot(string dir)
        {
            return File.Exists(Path.Combine(dir, "launchers", "run_audio_menu.cmd")) &&
                   File.Exists(Path.Combine(dir, "build", "native", "pluto_audio_monitor.exe"));
        }
    }
}
