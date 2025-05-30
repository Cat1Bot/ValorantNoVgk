using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.Versioning;
using System.ServiceProcess;
using System.Text;
using System.Threading.Tasks;

namespace NoVgkLauncher;

internal class Setup
{
    public static void TerminateRiotServices()
    {
        string[] riotProcesses = ["RiotClientServices", "LeagueClient", "vgc"];

        foreach (var processName in riotProcesses)
        {
            try
            {
                var processes = Process.GetProcessesByName(processName);

                foreach (var process in processes)
                {
                    process.Kill();
                    process.WaitForExit();
                }
            }
            catch (Exception)
            {
                Console.ForegroundColor = ConsoleColor.DarkYellow;
                Console.WriteLine($" [WARN] Could not terminate {processName}, try running this app as administrator.");
                Console.ResetColor();
            }
        }
    }
    [SupportedOSPlatform("windows")]
    private static bool IsRealVanguardPresent()
    {
        try
        {
            using var controller = new ServiceController("vgk");
            var status = controller.Status;
            return true;
        }
        catch (InvalidOperationException)
        {
            return false;
        }
        catch (Exception ex)
        {
            Console.ForegroundColor = ConsoleColor.DarkYellow;
            Console.WriteLine($" [WARN] Unexpected error while checking for vgk presense: {ex.Message}");
            Console.ResetColor();
            return false;
        }
    }

    [SupportedOSPlatform("windows")]
    internal static bool EnsureZombieVgcInstalled()
    {
        if (IsRealVanguardPresent())
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine(" [ERROR] Cannot install zombie Vanguard Client because real Vanguard failed to uninstall. Please manually uninstall Vanguard with Revo uninstaller before continuing.");
            Console.ResetColor();
            return false;
        }

        try
        {
            if (ServiceController.GetServices().Any(s => s.ServiceName == "vgc"))
                return true;

            Console.WriteLine(" [INFO] Fake vgc service not found. Registering...");

            string exePath = Path.Combine(AppContext.BaseDirectory, "vgc.exe");

            if (!File.Exists(exePath))
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine(" [ERROR] zombie vgc.exe not found. Please place zombie vgc.exe in the same directory as this app and try again.");
                Console.ResetColor();
                return false;
            }

            string scCreate = $"sc create vgc binPath= \"{exePath}\" start= demand DisplayName= \"vgc\"";
            string scSdset = "sc sdset vgc D:(A;;CCLCSWRPWPDTLOCRRC;;;SY)" +
                             "(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;BA)" +
                             "(A;;CCLCSWLOCRRC;;;IU)(A;;CCLCSWLOCRRC;;;SU)" +
                             "(A;;RPLOCRRC;;;S-1-5-32-545)";

            var psi = new ProcessStartInfo
            {
                FileName = "cmd.exe",
                Arguments = $"/c \"{scCreate} && {scSdset}\"",
                Verb = "runas",
                UseShellExecute = true,
                WindowStyle = ProcessWindowStyle.Hidden
            };

            using var process = Process.Start(psi);
            process?.WaitForExit();

            bool serviceExists = ServiceController.GetServices().Any(s => s.ServiceName == "vgc");
            if (serviceExists)
            {
                Console.ForegroundColor = ConsoleColor.Green;
                Console.WriteLine(" [SUCCESS] Fake vgc service successfully registered.");
                Console.ResetColor();
                return true;
            }
            else
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine(" [ERROR] Failed to register fake vgc service.");
                Console.ResetColor();
                return false;
            }
        }
        catch (Exception ex)
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine($" [ERROR] Failed to create or verify zombie Vanguard Client registration: {ex.Message}");
            Console.ResetColor();
            return false;
        }
    }
    public static bool RemoveVanguard()
    {
        string vgkpath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Riot Vanguard", "installer.exe");

        if (!File.Exists(vgkpath))
        {
            return true;
        }

        try
        {
            using var process = Process.Start(vgkpath, "--quiet");
            if (process != null)
            {
                Console.WriteLine(" [INFO] Attempting to uninstall Vanguard...");

                process.WaitForExit();
                Thread.Sleep(5000);

                for (int i = 0; i < 30; i++)
                {
                    if (!File.Exists(vgkpath))
                    {
                        Console.ForegroundColor = ConsoleColor.Green;
                        Console.WriteLine(" [SUCCESS] Vanguard uninstallation completed, starting now...");
                        Console.ResetColor();
                        return true;
                    }

                    Thread.Sleep(1000);
                }

                Console.ForegroundColor = ConsoleColor.DarkYellow;
                Console.WriteLine(" [WARN] Vanguard uninstallation failed, try running this app as administrator.");
                Console.ResetColor();
                return false;
            }
            else
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine(" [ERROR] Failed to start Vanguard uninstallation process.");
                Console.ResetColor();
                return false;
            }
        }
        catch (Exception ex)
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine($" [ERROR] Vanguard uninstallation aborted: {ex}");
            Console.ResetColor();
            return false;
        }
    }
}
