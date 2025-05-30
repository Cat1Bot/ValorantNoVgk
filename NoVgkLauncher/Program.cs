using System;
using System.Diagnostics;
using System.ServiceProcess;
using System.IO.Pipes;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Runtime.Versioning;

namespace NoVgkLauncher;

class Program
{
    private static readonly bool IsWindows = OperatingSystem.IsWindows();

    public static async Task Main(string[] args)
    {
        string[] launchArgs;
        Console.Clear();
        while (true)
        {
            Console.ForegroundColor = ConsoleColor.Cyan;
            Console.WriteLine(" ╔═══════════════════════════════════════╗");
            Console.WriteLine(" ║         Select a game to launch       ║");
            Console.WriteLine(" ╠═══════════════════════════════════════╣");
            Console.WriteLine(" ║ [1] League of Legends                 ║");
            Console.WriteLine(" ║ [2] VALORANT                          ║");
            Console.WriteLine(" ║ [3] 2XKO                              ║");
            Console.WriteLine(" ║ [4] Riot Client                       ║");
            Console.WriteLine(" ╚═══════════════════════════════════════╝");
            Console.ForegroundColor = ConsoleColor.White;
            Console.Write(" Select an option: ");
            Console.ResetColor();
            string? choice = Console.ReadLine();

            switch (choice)
            {
                case "1":
                    launchArgs = ["--launch-product=league_of_legends", "--launch-patchline=live"];
                    goto LaunchNow;
                case "2":
                    launchArgs = ["--launch-product=valorant", "--launch-patchline=live"];
                    goto LaunchNow;
                case "3":
                    launchArgs = ["--launch-product=lion", "--launch-patchline=live"];
                    goto LaunchNow;
                case "4":
                    launchArgs = [];
                    goto LaunchNow;
                default:
                    Console.ForegroundColor = ConsoleColor.Yellow;
                    Console.WriteLine(" [WARN] Invalid input. Please try again.");
                    Console.ResetColor();
                    break;
            }
        }

    LaunchNow:
        await Launch(launchArgs);
    }
    [SupportedOSPlatform("windows")]
    private static async Task Launch(string[] args)
    {
        Console.Clear();

        if (IsWindows && !Setup.RemoveVanguard())
        {
            while (true)
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine(" [ERROR] Vanguard uninstall failed. Please manually uninstall Vanguard with Revo uninstaller or similar tool.");
                Console.ResetColor();
                Console.WriteLine(" ----------------------------------------------------");
                Console.WriteLine(" [1] Go back");
                Console.WriteLine(" [0] Exit");
                Console.WriteLine(" ====================================================");
                Console.ForegroundColor = ConsoleColor.White;
                Console.Write(" Select an option: ");
                Console.ResetColor();

                var choice = Console.ReadLine();

                if (choice == "1")
                {
                    Console.Clear();
                    return;
                }
                else if (choice == "0")
                {
                    Environment.Exit(0);
                }
                else
                {
                    Console.Clear();
                    Console.ForegroundColor = ConsoleColor.Yellow;
                    Console.WriteLine(" [WARN] Invalid input. Please enter a valid option.");
                    Console.ResetColor();
                }
            }
        }

        if (IsWindows && args.Contains("--launch-product=valorant") && !Setup.EnsureZombieVgcInstalled())
        {
            while (true)
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine(" [ERROR] Failed to register zombie Vanguard Client. Please make sure vgc.exe is placed in same directory as this app and try running this app as administrator if issue persist.");
                Console.ResetColor();
                Console.WriteLine(" ----------------------------------------------------");
                Console.WriteLine(" [1] Go back");
                Console.WriteLine(" [0] Exit");
                Console.WriteLine(" ====================================================");
                Console.ForegroundColor = ConsoleColor.White;
                Console.Write(" Select an option: ");
                Console.ResetColor();

                var choice = Console.ReadLine();

                if (choice == "1")
                {
                    Console.Clear();
                    return;
                }
                else if (choice == "0")
                {
                    Environment.Exit(0);
                }
                else
                {
                    Console.Clear();
                    Console.ForegroundColor = ConsoleColor.Yellow;
                    Console.WriteLine(" [WARN] Invalid input. Please enter a valid option.");
                    Console.ResetColor();
                }
            }
        }

        var leagueProxy = new LeagueProxy();

        leagueProxy.Start();

        Console.WriteLine(" [INFO] Starting RCS process...");

        var process = leagueProxy.LaunchRCS(args);
        if (process is null)
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine(" [ERROR] Failed to launch RCS process. Try running this app as administrator.");
            Console.ResetColor();
            leagueProxy.Stop();
            return;
        }

        Console.ForegroundColor = ConsoleColor.Cyan;
        Console.WriteLine(" [OKAY] Started RCS process.");
        Console.ResetColor();

        await process.WaitForExitAsync();
        Console.WriteLine(" [INFO] RCS process exited");
        leagueProxy.Stop();
    }
}