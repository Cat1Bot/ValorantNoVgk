using System.Diagnostics;
using System.ServiceProcess;

namespace NoVgkInstaller;

internal class Setup
{
    public static void TerminateRiotServices()
    {
        string[] riotProcesses = ["RiotClientServices", "LeagueClient", "VALORANT-Win64-Shipping", "Lion-Win64-Shipping"];

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

    internal static bool EnsurevServiceInstalled()
    {
        if (!RemoveVanguard())
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

            Console.WriteLine(" [INFO] Registering vService.exe...");

            string exePath = Path.Combine(AppContext.BaseDirectory, "vService.exe");

            if (!File.Exists(exePath))
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine($" [ERROR] vService.exe not found in {exePath}");
                Console.ResetColor();
                return false;
            }

            string scCreate = $"sc create vgc binPath= \"{exePath}\" start= demand DisplayName= \"vService\"";
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
                Console.WriteLine(" [SUCCESS] vService successfully registered.");
                Console.ResetColor();
                return true;
            }
            else
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine(" [ERROR] Failed to register vService.");
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
