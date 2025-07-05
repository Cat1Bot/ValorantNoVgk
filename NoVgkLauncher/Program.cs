using System;
using System.Diagnostics;
using System.ServiceProcess;
using System.IO.Pipes;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Runtime.Versioning;

namespace NoVgkLauncher;
[SupportedOSPlatform("windows")]
class Program
{
    private static async Task Main(string[] args)
    {
        if (!Setup.RemoveVanguard())
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

        if (!Setup.EnsureZombieVgcInstalled())
        {
            while (true)
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine(" [ERROR] Failed to register zombie Vanguard Client. Please make sure FakeVgc.exe is placed in same directory as this app and try running this app as administrator if issue persist.");
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

        var process = leagueProxy.LaunchRCS(args);
        if (process is null)
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine(" [ERROR] Failed to launch RCS process. Try running this app as administrator.");
            Console.ResetColor();
            leagueProxy.Stop();
            return;
        }

        await process.WaitForExitAsync();
        leagueProxy.Stop();
    }
}