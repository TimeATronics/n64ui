// The curated settings table (see DESIGN.md section 4.5).
#include "config/Settings.h"

namespace n64ui {

const Setting kSettings[] = {
    // --- Core ---
    {"Core", "R4300Emulator", SettingKind::Choice, "CPU emulator", "2", 0, 2, 1,
     "Pure Interpreter,Cached Interpreter,Dynamic Recompiler"},
    {"Core", "CountPerOp", SettingKind::Int, "Counts per op", "0", 0, 64, 1, ""},
    {"Core", "NoCompiledJump", SettingKind::Toggle, "No compiled jump", "0", 0, 1, 1, ""},
    {"Core", "DisableExtraMem", SettingKind::Toggle, "Disable 4MB expansion", "0", 0,
     1, 1, ""},
    {"Core", "OnScreenDisplay", SettingKind::Toggle, "On-screen display", "1", 0, 1,
     1, ""},
    {"Core", "SaveFilenameFormat", SettingKind::Choice, "Save file name format", "0",
     0, 1, 1, "Header name,GoodName + CRC"},
    {"Core", "SaveStatePath", SettingKind::Text, "Savestate path", "Save", 0, 0, 0,
     ""},
    {"Core", "SaveSRAMPath", SettingKind::Text, "SRAM path", "Save", 0, 0, 0, ""},
    {"Core", "ScreenshotPath", SettingKind::Text, "Screenshot path", "Screens", 0, 0,
     0, ""},

    // --- Video-General ---
    {"Video-General", "Fullscreen", SettingKind::Toggle, "Fullscreen", "True", 0, 1,
     1, ""},
    {"Video-General", "ScreenWidth", SettingKind::Int, "Width", "1024", 320, 1024, 1,
     ""},
    {"Video-General", "ScreenHeight", SettingKind::Int, "Height", "768", 240, 768, 1,
     ""},
    {"Video-General", "VerticalSync", SettingKind::Toggle, "Vertical sync", "False",
     0, 1, 1, ""},

    // --- Video-Glide64mk2 ---
    {"Video-Glide64mk2", "aspect", SettingKind::Choice, "Aspect ratio", "0", 0, 2, 1,
     "4:3,16:9,Adjust"},
    {"Video-Glide64mk2", "vsync", SettingKind::Toggle, "VSync", "False", 0, 1, 1,
     ""},
    {"Video-Glide64mk2", "show_fps", SettingKind::Int, "Show FPS", "0", 0, 4, 1, ""},
    {"Video-Glide64mk2", "filtering", SettingKind::Choice, "Texture filtering", "-1",
     -1, 3, 1, "Automatic,None,Linear,Bilinear"},
    {"Video-Glide64mk2", "fog", SettingKind::Toggle, "Fog", "False", 0, 1, 1, ""},
    {"Video-Glide64mk2", "swapmode", SettingKind::Choice, "Buffer swap mode", "-1",
     -1, 2, 1, "Automatic,Old-school,New-style"},
    {"Video-Glide64mk2", "autoframeskip", SettingKind::Toggle, "Auto frame skip",
     "False", 0, 1, 1, ""},
    {"Video-Glide64mk2", "maxframeskip", SettingKind::Int, "Max frame skip", "0", 0,
     10, 1, ""},
    {"Video-Glide64mk2", "fb_hires", SettingKind::Toggle, "FB: hi-res", "False", 0,
     1, 1, ""},
    {"Video-Glide64mk2", "fb_smart", SettingKind::Toggle, "FB: smart", "False", 0,
     1, 1, ""},
    {"Video-Glide64mk2", "ghq_fltr", SettingKind::Choice, "HQ texture filter", "0",
     0, 3, 1, "None,2xSaI,HQ2x,HQ4x"},
    {"Video-Glide64mk2", "ghq_cmpr", SettingKind::Toggle, "HQ compressed textures",
     "False", 0, 1, 1, ""},
    {"Video-Glide64mk2", "ghq_hirs", SettingKind::Choice, "Hi-res textures", "0", 0,
     2, 1, "Off,Lookup,Full"},
    {"Video-Glide64mk2", "fast_crc", SettingKind::Int, "Fast CRC", "1", 0, 3, 1, ""},
    {"Video-Glide64mk2", "wrpFBO", SettingKind::Toggle, "Framebuffer objects",
     "True", 0, 1, 1, ""},
    {"Video-Glide64mk2", "wrpResolution", SettingKind::Int, "Wrapper resolution",
     "1", 0, 4, 1, ""},
    {"Video-Glide64mk2", "card_id", SettingKind::Int, "Card ID", "0", 0, 7, 1, ""},

    // --- Audio-SDL ---
    {"Audio-SDL", "Volume", SettingKind::Int, "Volume", "100", 0, 100, 5, ""},
    {"Audio-SDL", "Muted", SettingKind::Toggle, "Muted", "0", 0, 1, 1, ""},
    {"Audio-SDL", "Resampler", SettingKind::Choice, "Resampler", "0", 0, 2, 1,
     "Trivial,SRC linear,SRC sinc"},
    {"Audio-SDL", "PrimaryBufferSize", SettingKind::Int, "Primary buffer", "2048",
     256, 8192, 256, ""},
    {"Audio-SDL", "SecondaryBufferSize", SettingKind::Int, "Secondary buffer", "0",
     0, 16384, 1024, ""},
    {"Audio-SDL", "SwapChannels", SettingKind::Toggle, "Swap channels", "0", 0, 1,
     1, ""},
    {"Audio-SDL", "Synchronize", SettingKind::Toggle, "Synchronize audio", "True",
     0, 1, 1, ""},

    // --- Input-SDL-Control1 ---
    {"Input-SDL-Control1", "plugged", SettingKind::Toggle, "Controller plugged",
     "True", 0, 1, 1, ""},
    {"Input-SDL-Control1", "device", SettingKind::Text, "Device name", "", 0, 0, 0,
     ""},
    {"Input-SDL-Control1", "AnalogDeadzone", SettingKind::Text, "Analog deadzone",
     "4096,4096", 0, 0, 0, ""},
    {"Input-SDL-Control1", "AnalogPeak", SettingKind::Text, "Analog peak",
     "32768,32768", 0, 0, 0, ""},
};

const int kSettingsCount = static_cast<int>(sizeof(kSettings) / sizeof(kSettings[0]));

}  // namespace n64ui
