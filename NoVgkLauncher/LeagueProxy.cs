using System.Diagnostics;
using System.Net.Sockets;
using System.Net;
using System.Runtime.InteropServices.Marshalling;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace NoVgkLauncher;

public class LeagueProxy
{
    private CancellationTokenSource? _ServerCTS;
    private readonly ConfigProxy _ConfigProxy;

    public static int ConfigPort { get; private set; }

    public LeagueProxy()
    {
        _ConfigProxy = new ConfigProxy();
    }
    private static void TerminateRiotServices()
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
            catch (Exception ex)
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine($" [ERROR] Failed to stop {processName} - {ex.Message}. Try running this app is administrator if this issue persists.");
                Console.ResetColor();
            }
        }
    }

    public void Start()
    {
        if (_ServerCTS is not null)
        {
            Console.ForegroundColor = ConsoleColor.DarkYellow;
            Console.WriteLine(" [WARN] Proxy is already running. Attempting to restart.");
            Console.ResetColor();
            Stop();
        }

        TerminateRiotServices();
        FindAvailablePorts();
        _ServerCTS = new CancellationTokenSource();

        _ConfigProxy?.RunAsync(_ServerCTS.Token);
    }

    private static void FindAvailablePorts()
    {
        int[] ports = new int[1];
        for (int i = 0; i < ports.Length; i++)
        {
            ports[i] = GetFreePort();
        }
        ConfigPort = ports[0];
    }

    private static int GetFreePort()
    {
        using var listener = new TcpListener(IPAddress.Loopback, 0);
        listener.Start();
        int port = ((IPEndPoint)listener.LocalEndpoint).Port;
        listener.Stop();
        return port;
    }

    public void Stop()
    {
        if (_ServerCTS is null)
        {
            Console.ForegroundColor = ConsoleColor.DarkYellow;
            Console.WriteLine(" [WARN] Unable to stop proxy service: Service is not running.");
            Console.ResetColor();
            return;
        }

        _ServerCTS?.Cancel();
        _ConfigProxy.Stop();
        _ServerCTS?.Dispose();
        _ServerCTS = null;

        Console.WriteLine(" [INFO] Proxy services successfully stopped.");
    }

    public Process? LaunchRCS(IEnumerable<string>? args = null)
    {
        if (_ServerCTS is null)
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine(" [ERROR] RCS launch failed: Proxies were not started due to an error.");
            Console.ResetColor();
        }
        return RiotClient.Launch(args);
    }
}