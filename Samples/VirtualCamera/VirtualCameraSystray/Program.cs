// Copyright (C) Microsoft Corporation. All rights reserved.

using System;
using System.Diagnostics;
using System.Security.Principal;
using System.Threading;
using System.Windows.Forms;

namespace VirtualCameraSystray
{
    static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            // Check if running as administrator
            if (!IsRunningAsAdministrator())
            {
                // Request elevation
                try
                {
                    ProcessStartInfo startInfo = new ProcessStartInfo
                    {
                        UseShellExecute = true,
                        WorkingDirectory = Environment.CurrentDirectory,
                        FileName = Application.ExecutablePath,
                        Verb = "runas"
                    };

                    Process.Start(startInfo);
                    return; // Exit current non-elevated instance
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Failed to request administrator privileges: {ex.Message}", 
                                  "Elevation Required", 
                                  MessageBoxButtons.OK, 
                                  MessageBoxIcon.Warning);
                    return;
                }
            }

            Mutex mutex = null;
            if (!Mutex.TryOpenExisting("VirtualCameraSystrayMutex", out mutex))
            {
                mutex = new Mutex(false, "VirtualCameraSystrayMutex");
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Application.Run(new SystrayApplicationContext());
                mutex.Close();
            }
        }

        /// <summary>
        /// Check if the current process is running as administrator
        /// </summary>
        /// <returns>True if running as administrator, false otherwise</returns>
        private static bool IsRunningAsAdministrator()
        {
            try
            {
                WindowsIdentity identity = WindowsIdentity.GetCurrent();
                WindowsPrincipal principal = new WindowsPrincipal(identity);
                return principal.IsInRole(WindowsBuiltInRole.Administrator);
            }
            catch
            {
                return false;
            }
        }
    }
}
