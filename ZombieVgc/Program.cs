using System;
using System.Diagnostics;
using System.ServiceProcess;

namespace vgc
{
    public class FakeVgc : ServiceBase
    {
        private VgcPipeServer _pipeServer;

        public FakeVgc()
        {
            ServiceName = "vgc";
            CanStop = true;
            CanPauseAndContinue = false;
            AutoLog = true;

            try
            {
                var logPath = AppDomain.CurrentDomain.BaseDirectory + "vgc_service_log.txt";
                Trace.Listeners.Add(new TextWriterTraceListener(logPath));
                Trace.AutoFlush = true;
                Trace.WriteLine("=== Vanguard Fake Service Initialized ===");
            }
            catch (Exception ex)
            {
                EventLog.WriteEntry("vgc", "Failed to initialize trace listener: " + ex.Message, EventLogEntryType.Error);
            }
        }

        protected override void OnStart(string[] args)
        {
            Trace.WriteLine("[INFO] FakeVgc service starting...");
            _pipeServer = new VgcPipeServer();
            _pipeServer.Start();
        }

        protected override void OnStop()
        {
            Trace.WriteLine("[INFO] FakeVgc service stopping...");
            if (_pipeServer != null)
            {
                _pipeServer.Stop();
                _pipeServer = null;
            }
        }

        public static void Main()
        {
            ServiceBase.Run(new FakeVgc());
        }
    }
}
