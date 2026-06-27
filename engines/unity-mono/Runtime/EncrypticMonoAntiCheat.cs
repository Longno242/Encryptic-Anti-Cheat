using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using UnityEngine;
using Debug = UnityEngine.Debug;

namespace Encryptic.AntiCheat.Mono
{
    [Serializable]
    public class EncrypticMonoConfig
    {
        public bool MonitorAssemblyLoads = true;
        public bool BlockDebugger = true;
        public bool ScanCheatProcesses = true;
        public bool UseNativeCore = false;
        public bool UseConfigFile = true;
        public string ConfigFileName = "encryptic_config.json";
        public string[] AllowedAssemblies = { "Assembly-CSharp", "UnityEngine", "mscorlib" };
        public string[] CheatProcessNames = {
            "cheatengine", "cheat engine", "x64dbg", "x32dbg", "ida", "ida64",
            "processhacker", "reclass", "artmoney", "tsearch", "wemod", "xenos",
            "extremeinjector", "trainer", "speedhack", "dnspy", "hxd", "ollydbg"
        };
        public EncrypticWatermarkSettings Watermark = new EncrypticWatermarkSettings();
        public Action<string> OnViolation;
    }

    [Serializable]
    public class EncrypticWatermarkSettings
    {
        public bool Enabled = true;
        public string GameTitle = "Your Game";
    }

    internal static class EncrypticNativeMono
    {
        private const string DllName = "encryptic_core";

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void encryptic_start_default();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int encryptic_start_from_file([MarshalAs(UnmanagedType.LPStr)] string path);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void encryptic_tick();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void encryptic_stop();
    }

    [DefaultExecutionOrder(-10000)]
    public sealed class EncrypticMonoAntiCheat : MonoBehaviour
    {
        private static EncrypticMonoAntiCheat _instance;
        private static EncrypticMonoConfig _config;
        private static readonly HashSet<string> LoadedAssemblies = new(StringComparer.OrdinalIgnoreCase);

        public static void StartFromConfig(string configFileName = "encryptic_config.json")
        {
            Start(new EncrypticMonoConfig { UseNativeCore = true, UseConfigFile = true, ConfigFileName = configFileName });
        }

        public static void Start(EncrypticMonoConfig config)
        {
            _config = config ?? new EncrypticMonoConfig();

            if (_instance == null)
            {
                var go = new GameObject(nameof(EncrypticMonoAntiCheat));
                _instance = go.AddComponent<EncrypticMonoAntiCheat>();
                DontDestroyOnLoad(go);
            }

            _instance.Initialize();
        }

        private void Initialize()
        {
            foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
            {
                LoadedAssemblies.Add(asm.GetName().Name);
            }

            if (_config.MonitorAssemblyLoads)
            {
                AppDomain.CurrentDomain.AssemblyLoad += OnAssemblyLoad;
            }

            if (_config.UseNativeCore)
            {
                try
                {
                    if (_config.UseConfigFile)
                    {
                        var path = Path.Combine(Application.streamingAssetsPath, _config.ConfigFileName);
                        if (!File.Exists(path))
                        {
                            path = Path.Combine(Application.dataPath, "..", _config.ConfigFileName);
                        }

                        if (File.Exists(path) && EncrypticNativeMono.encryptic_start_from_file(path) == 1)
                        {
                            Debug.Log($"[Encryptic] Started from {path}");
                        }
                        else
                        {
                            EncrypticNativeMono.encryptic_start_default();
                        }
                    }
                    else
                    {
                        EncrypticNativeMono.encryptic_start_default();
                    }
                }
                catch (DllNotFoundException ex)
                {
                    Debug.LogWarning($"[Encryptic] Native core unavailable: {ex.Message}");
                }
            }
        }

        private static void OnAssemblyLoad(object sender, AssemblyLoadEventArgs args)
        {
            var name = args.LoadedAssembly.GetName().Name;
            if (LoadedAssemblies.Add(name) && !IsAllowedAssembly(name))
            {
                Report($"Unauthorized assembly load: {name}");
            }
        }

        private static bool IsAllowedAssembly(string name)
        {
            if (_config?.AllowedAssemblies == null) return true;
            return _config.AllowedAssemblies.Any(a =>
                name.StartsWith(a, StringComparison.OrdinalIgnoreCase));
        }

        private void Update()
        {
            if (_config == null) return;

            if (_config.BlockDebugger && Debugger.IsAttached)
            {
                Report("Managed debugger attached");
            }

            if (_config.ScanCheatProcesses)
            {
                ScanProcesses();
            }

            if (_config.UseNativeCore)
            {
                try { EncrypticNativeMono.encryptic_tick(); }
                catch (DllNotFoundException) { }
            }
        }

        private static void ScanProcesses()
        {
            try
            {
                foreach (var proc in Process.GetProcesses())
                {
                    string name;
                    try { name = proc.ProcessName; }
                    catch { continue; }

                    if (_config.CheatProcessNames.Any(c =>
                            name.IndexOf(c, StringComparison.OrdinalIgnoreCase) >= 0))
                    {
                        Report($"Cheat process detected: {name}");
                    }
                }
            }
            catch (Exception ex)
            {
                Debug.LogWarning($"[Encryptic] Process scan failed: {ex.Message}");
            }
        }

        private static void Report(string message)
        {
            Debug.LogWarning($"[Encryptic] {message}");
            _config?.OnViolation?.Invoke(message);
        }

        private void OnDestroy()
        {
            AppDomain.CurrentDomain.AssemblyLoad -= OnAssemblyLoad;
            if (_config?.UseNativeCore == true)
            {
                try { EncrypticNativeMono.encryptic_stop(); } catch { }
            }
        }
    }
}
