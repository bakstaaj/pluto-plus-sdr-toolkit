#!/usr/bin/env python3
from __future__ import annotations
import argparse, re
from pathlib import Path

AUDIO_TOOLS_TEMPLATE = r'''using System;
using System.Diagnostics;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace __NAMESPACE__
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
'''

def detect_namespace(text: str) -> str:
    m = re.search(r'^\s*namespace\s+([A-Za-z_][A-Za-z0-9_.]*)\s*;', text, re.MULTILINE)
    if m: return m.group(1)
    m = re.search(r'^\s*namespace\s+([A-Za-z_][A-Za-z0-9_.]*)\s*\{', text, re.MULTILINE)
    if m: return m.group(1)
    return 'PlutoGuiStarter'

def patch_mainwindow(main_cs: Path) -> None:
    text = main_cs.read_text(encoding='utf-8')
    if 'AudioToolsIntegration.Attach(this);' in text:
        print('MainWindow.xaml.cs already calls AudioToolsIntegration.Attach(this);')
        return
    needle='InitializeComponent();'
    idx=text.find(needle)
    if idx < 0:
        raise RuntimeError('Could not find InitializeComponent(); in MainWindow.xaml.cs')
    le='\r\n' if '\r\n' in text else '\n'
    backup=main_cs.with_suffix(main_cs.suffix + '.bak_audio_tools')
    if not backup.exists(): backup.write_text(text, encoding='utf-8', newline='')
    text=text[:idx] + needle + le + '            AudioToolsIntegration.Attach(this);' + text[idx+len(needle):]
    main_cs.write_text(text, encoding='utf-8', newline='')
    print(f'Patched {main_cs}')
    print(f'Backup: {backup}')

def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument('--gui-dir', default='gui/PlutoGuiStarter_v3_RepoLayout')
    args=ap.parse_args()
    gui_dir=Path(args.gui_dir)
    main_cs=gui_dir/'MainWindow.xaml.cs'
    if not gui_dir.exists(): raise SystemExit(f'ERROR: GUI folder not found: {gui_dir}')
    if not main_cs.exists(): raise SystemExit(f'ERROR: MainWindow.xaml.cs not found: {main_cs}')
    ns=detect_namespace(main_cs.read_text(encoding='utf-8'))
    out=gui_dir/'AudioToolsIntegration.cs'
    out.write_text(AUDIO_TOOLS_TEMPLATE.replace('__NAMESPACE__', ns), encoding='utf-8', newline='\r\n')
    print(f'Wrote {out}')
    print(f'Detected namespace: {ns}')
    patch_mainwindow(main_cs)
    print('\nDone. Build test:')
    print(f'  cd {gui_dir.as_posix()}')
    print('  dotnet build')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
