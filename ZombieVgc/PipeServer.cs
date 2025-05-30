using System;
using System.Diagnostics;
using System.IO.Pipes;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Security.AccessControl;
using System.Security.Principal;

namespace vgc
{
    public class VgcPipeServer
    {
        private const string PipeName = "933823D3-C77B-4BAE-89D7-A92B567236BC";
        private CancellationTokenSource _cts;
        private Task _listeningTask;

        public void Start()
        {
            if (_cts != null)
            {
                Trace.WriteLine("[WARN] Pipe server already running. Restarting.");
                Stop();
            }

            _cts = new CancellationTokenSource();
            _listeningTask = Task.Run(() => RunAsync(_cts.Token));
        }

        public void Stop()
        {
            if (_cts == null)
            {
                Trace.WriteLine("[WARN] Pipe server is not running.");
                return;
            }

            _cts.Cancel();

            try
            {
                _listeningTask?.Wait(3000); // Wait briefly, avoid deadlocks
            }
            catch (Exception ex)
            {
                Trace.WriteLine("[ERROR] Exception while stopping pipe server: " + ex.Message);
            }

            _cts.Dispose();
            _cts = null;
        }

        private async Task RunAsync(CancellationToken token)
        {
            Trace.WriteLine("[INFO] Pipe server listening thread started.");

            while (!token.IsCancellationRequested)
            {
                PipeSecurity ps = new PipeSecurity();
                ps.AddAccessRule(new PipeAccessRule(
                    new SecurityIdentifier(WellKnownSidType.WorldSid, null),
                    PipeAccessRights.ReadWrite,
                    AccessControlType.Allow));

                var server = new NamedPipeServerStream(
                    PipeName,
                    PipeDirection.InOut,
                    NamedPipeServerStream.MaxAllowedServerInstances,
                    PipeTransmissionMode.Message,
                    PipeOptions.Asynchronous,
                    0,
                    0,
                    ps);

                try
                {
                    Trace.WriteLine("[INFO] Waiting for pipe client...");
                    await server.WaitForConnectionAsync(token);
                    Trace.WriteLine("[INFO] Client connected.");

                    await HandleClient(server, token); // <-- Wait for handler to complete before looping
                }
                catch (OperationCanceledException)
                {
                    // Do nothing, just exit
                    break;
                }
                catch (Exception ex)
                {
                    Trace.WriteLine("[ERROR] Exception while handling pipe client: " + ex.Message);
                }
            }

            Trace.WriteLine("[INFO] Pipe server thread exiting.");
        }

        private async Task HandleClient(NamedPipeServerStream server, CancellationToken token)
        {
            try
            {
                byte[] buffer = new byte[4096];
                int messageCount = 0;

                while (server.IsConnected && !token.IsCancellationRequested)
                {
                    int bytesRead = await server.ReadAsync(buffer, 0, buffer.Length, token);
                    if (bytesRead > 0)
                    {
                        messageCount++;
                        Trace.WriteLine($"[INFO] Received message #{messageCount} ({bytesRead} bytes)");

                        byte[] received = new byte[bytesRead];
                        Array.Copy(buffer, received, bytesRead);
                        Trace.WriteLine(BitConverter.ToString(received).Replace("-", " "));

                        if (messageCount >= 3)
                        {
                            await server.WriteAsync(received, 0, received.Length, token);
                            await server.FlushAsync(token);
                            Trace.WriteLine($"[DEBUG] Echoed message #{messageCount}");
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
            catch (OperationCanceledException) { }
            catch (Exception ex)
            {
                Trace.WriteLine("[ERROR] Pipe handler exception: " + ex.Message);
            }
            finally
            {
                server.Dispose();
                Trace.WriteLine("[INFO] Client disconnected.");
            }
        }
    }
}
