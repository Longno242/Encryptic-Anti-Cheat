#pragma once

#ifdef _WIN32
#  ifdef ENCRYPTIC_BUILD_DLL
#    define ENCRYPTIC_API __declspec(dllexport)
#  else
#    define ENCRYPTIC_API
#  endif
#else
#  define ENCRYPTIC_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EncrypticWatermarkConfig {
    int enabled;
    const char* game_title;
    const char* brand_title;
    const char* brand_subtitle;
    const char* status_text;
    const char* footer_text;
    const char* version_text;
    unsigned int display_duration_ms;
    unsigned int fade_in_ms;
    unsigned int fade_out_ms;
    unsigned char accent_r;
    unsigned char accent_g;
    unsigned char accent_b;
    int show_progress_bar;
    int show_logo_mark;
    int fullscreen_dim;
    int allow_skip;
} EncrypticWatermarkConfig;

ENCRYPTIC_API void encryptic_start_default(void);
ENCRYPTIC_API int encryptic_start_from_file(const char* path);
ENCRYPTIC_API void encryptic_tick(void);
ENCRYPTIC_API void encryptic_stop(void);
ENCRYPTIC_API void encryptic_watermark_preview(const EncrypticWatermarkConfig* config);

#ifdef __cplusplus
}
#endif
