using System;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;

namespace Encryptic.AntiCheat
{
    [Serializable]
    public class EncrypticWatermarkSettings
    {
        public bool Enabled = true;
        public string GameTitle = "Your Game";
        public string BrandLine1 = "ENCRYPTIC";
        public string BrandLine2 = "ANTI-CHEAT";
        public string StatusText = "Initializing protection modules...";
        public string FooterText = "Protected by Encryptic Anti-Cheat";
        public int DisplayDurationMs = 4000;
    }

    [Serializable]
    public class EncrypticConfig
    {
        public string Preset = "Balanced";
        public string ConfigFileName = "encryptic_config.json";
        public bool UseConfigFile = true;
        public bool DllGuard = true;
        public bool DebuggerGuard = true;
        public bool HookGuard = true;
        public bool MemoryGuard = true;
        public bool ModuleWhitelist = false;
        public bool ThreadGuard = true;
        public bool TimingGuard = true;
        public bool HandleGuard = false;
        public bool ProcessGuard = true;
        public bool VmGuard = false;
        public bool OverlayGuard = true;
        public bool InputGuard = true;
        public bool BlockUnknownDlls = false;
        public bool TerminateOnDebugger = false;
        public string[] AllowedModules = Array.Empty<string>();
        public EncrypticWatermarkSettings Watermark = new EncrypticWatermarkSettings();
        public Action<EncrypticViolation> OnViolation;
    }

    public readonly struct EncrypticViolation
    {
        public int Type { get; }
        public int Severity { get; }
        public string Message { get; }
        public string Detail { get; }

        public EncrypticViolation(int type, int severity, string message, string detail)
        {
            Type = type;
            Severity = severity;
            Message = message;
            Detail = detail;
        }
    }

    public static class EncrypticNative
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
    public sealed class EncrypticAntiCheat : MonoBehaviour
    {
        private static EncrypticAntiCheat _instance;
        private static EncrypticConfig _config;

        /// <summary>Loads StreamingAssets/encryptic_config.json — use after running setup.bat</summary>
        public static void StartFromConfig(string configFileName = "encryptic_config.json")
        {
            Start(new EncrypticConfig
            {
                UseConfigFile = true,
                ConfigFileName = configFileName,
            });
        }

        public static void Start(EncrypticConfig config)
        {
            _config = config ?? new EncrypticConfig();

            if (_instance == null)
            {
                var go = new GameObject(nameof(EncrypticAntiCheat));
                _instance = go.AddComponent<EncrypticAntiCheat>();
                DontDestroyOnLoad(go);
            }

            try
            {
                if (_config.UseConfigFile)
                {
                    var path = Path.Combine(Application.streamingAssetsPath, _config.ConfigFileName);
                    if (!File.Exists(path))
                    {
                        path = Path.Combine(Application.dataPath, "..", _config.ConfigFileName);
                    }

                    if (File.Exists(path) && EncrypticNative.encryptic_start_from_file(path) == 1)
                    {
                        Debug.Log($"[Encryptic] Started from {path}");
                    }
                    else
                    {
                        Debug.LogWarning($"[Encryptic] Config not found at {path}, using defaults.");
                        EncrypticNative.encryptic_start_default();
                    }
                }
                else
                {
                    EncrypticNative.encryptic_start_default();
                }
            }
            catch (DllNotFoundException ex)
            {
                Debug.LogError($"[Encryptic] Run setup.bat first. {ex.Message}");
            }
        }

        public static void Stop()
        {
            if (_instance != null)
            {
                EncrypticNative.encryptic_stop();
                Destroy(_instance.gameObject);
                _instance = null;
            }
        }

        private void Awake()
        {
            if (_instance != null && _instance != this)
            {
                Destroy(gameObject);
                return;
            }

            _instance = this;
            DontDestroyOnLoad(gameObject);
        }

        private void Update() => EncrypticNative.encryptic_tick();

        private void OnDestroy()
        {
            if (_instance == this)
            {
                EncrypticNative.encryptic_stop();
                _instance = null;
            }
        }

        public static void RaiseViolation(int type, int severity, string message, string detail)
        {
            _config?.OnViolation?.Invoke(new EncrypticViolation(type, severity, message, detail));
        }
    }
}
