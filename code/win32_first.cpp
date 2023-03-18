//~ START
#if _RELEASE_BUILD && _INTERNAL_BUILD
#error "Making a release in debug mode"
#endif

#if _RELEASE_BUILD
#define TITLE_SUFFIX
#else
#define TITLE_SUFFIX " - dev"
#endif

#define _INTERNAL_PROFILE 0
#if _INTERNAL_PROFILE
#include "spall.h"
#endif

#include "showmuted.cpp"

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <windows.h>
#include <timeapi.h>
#include <hidusage.h>

#if _INTERNAL_PROFILE
double spall_get_time_in_micros() {
    local_persist double invfreq;
	if (!invfreq) {
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		invfreq = 1000000.0 / frequency.QuadPart;
	}
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	return counter.QuadPart * invfreq;
}

global SpallProfile g_spall_ctx;
global SpallBuffer  g_spall_buffer;
global b32 g_spall_should_profile;

#define TIMED_BLOCK_BEGIN(name) if(g_spall_should_profile) spall_buffer_begin(&g_spall_ctx, &g_spall_buffer, name, sizeof(name)-1, spall_get_time_in_micros())
#define TIMED_BLOCK_END()       if(g_spall_should_profile) spall_buffer_end(&g_spall_ctx, &g_spall_buffer, spall_get_time_in_micros())

struct Timed_Scope {
    Timed_Scope(const char *name, signed long name_len) {
        if(g_spall_should_profile) spall_buffer_begin(&g_spall_ctx, &g_spall_buffer, name, name_len, spall_get_time_in_micros());
    }
    ~Timed_Scope() {
        if(g_spall_should_profile) spall_buffer_end(&g_spall_ctx, &g_spall_buffer, spall_get_time_in_micros());
    }
};

#define TIMED_SCOPE(name) Timed_Scope ___Concat(__timed_scope_, __LINE__)(name, sizeof(name)-1)
#define TIMED_PROC()      Timed_Scope ___Concat(__timed_proc_, __LINE__)(__FUNCTION__, sizeof(__FUNCTION__) - 1)
#else
#define TIMED_BLOCK_BEGIN(...) {}
#define TIMED_BLOCK_END(...) {}
#define TIMED_SCOPE(...) {}
#define TIMED_PROC(...) {}
#endif

#define INI_MALLOC(ctx, size) ( talloc(size) )
#define INI_FREE(ctx, ptr) {}

//~ ini

#define INI_IMPLEMENTATION
#include "ini.h"

internal int
ini_find_section( ini_t* ini, string name ) {
    return ini_find_section(ini, name.data, cast(int)name.count);
}

internal int
ini_find_property( ini_t* ini, int section, string name ) {
    return ini_find_property(ini, section, name.data, cast(int)name.count);
}

internal int
ini_section_add( ini_t* ini, string name ) {
    return ini_section_add(ini, name.data, cast(int)name.count);
}
internal void
ini_property_add( ini_t* ini, int section, string name, string value ) {
    return ini_property_add(ini, section, name.data, cast(int)name.count, value.data, cast(int)value.count);
}

internal void
ini_section_name_set( ini_t* ini, int section, string name ) {
    return ini_section_name_set(ini, section, name.data, cast(int)name.count);
}
internal void
ini_property_name_set( ini_t* ini, int section, int property, string name ) {
    return ini_property_name_set(ini, section, property, name.data, cast(int)name.count);
}
internal void
ini_property_value_set( ini_t* ini, int section, int property, string value  ) {
    return ini_property_value_set(ini, section, property, value.data, cast(int)value.count);
}

internal string
ini_property_value( ini_t* ini, int section, string property_name, b32 *_success) {
    b32 success = false;
    string value = {};
    int index    = ini_find_property(ini, section, property_name);
    if(index != INI_NOT_FOUND) {
        char *cstring = ini_property_value(ini, section, index);
        if(cstring) {
            value = c_style_to_string(cstring);
            success = true;
        }
    }
    if(_success) *_success = success;
    return value;
}

//~ Types

template<typename T>
struct Array {
    s64 count;
    T *data;
    s64 capacity;
    
    void resize(s64 new_cap) {
        if(new_cap <= 0) {
            if(this->data) free(this->data);
            *this = {};
            return;
        }
        if(new_cap > this->capacity) {
            SetIfMax(new_cap, this->capacity * 2);
            this->data = cast(T *)realloc(this->data, new_cap*sizeof(T));
            AssertM(this->data, "Failed to realloc an Array");
        }
        this->capacity = new_cap;
        SetIfMin(this->count, this->capacity);
    }
    
    inline void add_new_elements(s64 number_of_elements_to_add) {
        s64 new_count = this->count + number_of_elements_to_add;
        if(new_count > this->capacity) {
            this->resize(new_count);
        }
        this->count = new_count;
    }
    
    inline void push(T value) {
        this->add_new_elements(1);
        this->data[this->count - 1] = value;
    }
    
    inline T pop() {
        assert(this->count > 0);
        this->count -= 1;
        return this->data[this->count];
    }
};


// TODO(bo): this is weird, but hey it works for now
template <typename T, int size>
struct Buffer {
    s64 count;
    T data[size];
    enum { capacity = size };
};

struct String_Builder {
    s64 count;
    char *data;
    s64 capacity;
};

#define BufferToStringBuilder(a) String_Builder{(a).count, cast(char *)(a).data, (a).capacity};

struct Bind_Code {
    u8 is_mouse;
    u8 code;
};

// NOTE(bo): Discord bind combos are max 4
global const u32 MAX_MUTE_BIND_COUNT = 4;

struct Mute_Bind {
    int count;
    union {
        struct {
            Bind_Code b1, b2, b3, b4;
        };
        Bind_Code b[MAX_MUTE_BIND_COUNT];
    };
    b32 is_down[MAX_MUTE_BIND_COUNT];
};

struct Layout {
    b32 is_used;
    b32 maximized;
    RECT rect;
};


// When changes happen in the future to Settings, save the old versions as _Settings_{OLD_VERSION_NUMBER}

// Either make converter functions to convert to the next adjacent version,
// or have all convertion be to the newest version directly.
// Or continue with the 'one version at a time' until you reach X versions,
// then use version X as a landmark for previous version to convert to.
// Then newer version go with 'one verstion at a time" until it reaches 2X,
// and make 2X a landmark for [X+1, 2x).

const s32 SETTINGS_VERSION = 0;

struct Settings {
    s32 version;
    Mute_Bind toggle_mute_bind;
    Mute_Bind push_to_mute_bind;
    Layout settings_window_layout;
    Layout color_window_layout;
    Layout layouts[8];
    b32 color_mode_hsv;
    v3i unmuted_rgb;
    v3i   muted_rgb;
    s32 unmuted_preserved_hue;
    s32   muted_preserved_hue;
    s32 font_size;
};

enum struct Settings_State {
    MAIN,
    BIND_TOGGLE_MUTE,
    BIND_PUSH_TO_MUTE,
    LAYOUTS,
    COLORS,
    
    _TERMINATOR,
};

struct Ui_Colors {
    COLORREF main;
    COLORREF menu_background;
    COLORREF normal_button;
    COLORREF hot_button;
    
    COLORREF normal_text;
    COLORREF active_text;
};

struct Bitmap {
    HBITMAP handle;
    HDC dc;
    int width;
    int height;
};

enum Button_Flags {
    HOT = 0x1,
    ACTIVE = 0x2,
};

struct Rect {
    v2i p0;
    v2i p1;
    COLORREF color;
    
    string text;
    v2i text_pad; // only used if text isn't empty
    COLORREF text_color;
};

struct Button {
    Rect rect;
    
    int id;
    u32 flags;
};

struct Slider {
    int id;
    string label;
    u32 flags;
    
    struct {
        v2i p0;
        v2i p1;
        COLORREF color;
    } line;
    
    struct {
        v2i p0;
        v2i p1;
        COLORREF color;
    } cursor;
};

struct Menu {
    HDC dc;
    int id_count;
    int active_id;
    v2i dim;
    Ui_Colors colors;
    
    v2i topleft;
    s32 spacing;
    v2i pad;
    
    v2i p0;
    v2i p1;
    s32 max_p1_x;
    
    b32 is_horizontal_mode;
    s32 hori_max_p1_y;
    s32 saved_x;
    
    // NOTE(bo): having most ui elements be buttons, even if it isn't clickable, is weird. but it works and i'm not gonna change it prematurely :UiElementWeirdness
    Array<Button> buttons;
    Array<Slider> sliders;
};

enum Mouse_State {
    Mouse_Nochange,
    Mouse_Wentdown,
    Mouse_Wentup,
};

struct Mouse {
    v2i p;
    Mouse_State state;
};

struct Active_Slider_State {
    s32 start_mouse_x;
    s32 start_cursor_x;
};


struct Undo {
    Layout layout;
    v3i unmuted_rgb;
    v3i   muted_rgb;
    Hsvi unmuted_hsv;
    Hsvi   muted_hsv;
};

global const u8 MOUSE_2 = 2;
global const u8 MOUSE_3 = 3;
global const u8 MOUSE_4 = 4;

global const USHORT LSHIFT_MAKECODE = 0x2a;
global const USHORT RSHIFT_MAKECODE = 0x36;


#define GRAY(g) RGB(g, g, g)




// TODO(bo): make buttons_to_draw, buttons_to_draw_count, active_index, into to Button_Buffer struct






//~ Globals

// TODO(bo): clean up all this global state. it's too much and too messy

// Misc.
global s64 g_perf_counter_frequency;

// UI
global Ui_Colors g_unmuted_colors;
global Ui_Colors g_muted_colors;
global Ui_Colors g_settings_colors;
global Ui_Colors g_error_colors;

global Menu g_color_menu;
global Menu g_error_menu;
global Menu g_settings_menu;

global HFONT g_font;
global s32 g_em_s32;
global f32 g_em;

// Windows
global b32 g_color_window_active;
global HWND g_color_window;
global Bitmap g_color_bitmap;

global b32 g_settings_window_active;
global HWND g_settings_window;
global Bitmap g_settings_bitmap;

global WNDCLASSA g_settings_window_class;

// Settings menu
global Settings_State g_settings_state;

global Hsvi g_unmuted_hsv;
global Hsvi g_muted_hsv;

global Undo g_undo;
global Active_Slider_State g_active_slider_state;

// Settings and binds
global char *g_settings_file_path;

global Settings g_settings;
global Mute_Bind g_new_mute_bind;

global b32 g_toggle_mute_bind_was_down;

// Global state
global b32 g_running;

global b32 g_paused;
global b32 g_toggle_mute_is_on;
global b32 g_push_to_mute_is_pressed;

global b32 g_error_occurred;
global Buffer<char, 2000> g_error_message;

//~ String Builder

internal inline String_Builder
XXX_temp_arena__get_free_memory_as_string_builder(Temp_Arena *arena) {
    string_u8 free = XXX_temp_arena__get_free_memory(arena);
    String_Builder sb = {};
    sb.capacity = free.count;
    sb.data     = cast(char *)free.data;
    return sb;
}

internal void
v_append_string(String_Builder *sb, char *format, va_list args) {
    if(sb->count >= sb->capacity) return;
    sb->count += stbsp_vsnprintf(sb->data+sb->count, cast(int)(sb->capacity-sb->count), format, args);
}

internal void
append_string(String_Builder *sb, char *format, ...) {
    va_list args;
    va_start(args, format);
    v_append_string(sb, format, args);
    va_end(args);
}

//~ Misc: home for uncategorised functions


internal inline string
tprint_layout(Layout layout) {
    string s = {};
    if(layout.maximized) {
        s = tprint("Maximized");
    } else {
        s = tprint("%d, %d, %d, %d",
                   layout.rect.left,  layout.rect.top,
                   layout.rect.right, layout.rect.bottom);
    }
    return s;
}

internal inline b32
operator==(RECT a, RECT b) {
    b32 result = (a.left   == b.left  &&
                  a.top    == b.top   &&
                  a.right  == b.right &&
                  a.bottom == b.bottom);
	return result;
}

internal inline b32
operator!=(RECT a, RECT b) {
	return !(a == b);
}

internal inline LONG
rect_width(RECT a) {
    return a.right - a.left;
}

internal inline LONG
rect_height(RECT a) {
    return a.bottom - a.top;
}

internal inline RECT
rect_p_dim(v2i p, v2i dim) {
    RECT result = {};
    result.left   = p.x;
    result.top    = p.y;
    result.right  = p.x + dim.x;
    result.bottom = p.y + dim.y;
    return result;
}

internal inline void
menu_reset(Menu *menu) {
    // NOTE(bo): most stuff gets handled by menu_preamble
    menu->active_id = -1;
}

internal inline void
invalidate_color_window() {
    InvalidateRect(g_color_window, null, false);
}

internal void
report_error(char *format, ...) {
    g_error_occurred = true;
    va_list args;
    va_start(args, format);
    auto sb = BufferToStringBuilder(g_error_message);
    v_append_string(&sb, format, args);
    g_error_message.count = sb.count;
    va_end(args);
    invalidate_color_window();
}

internal inline void
clear_error() {
    g_error_message.count = 0;
    g_error_occurred = false;
    menu_reset(&g_error_menu);
}

// @Important: should only be called on startup, before the run loop
internal void
fatal_error(char *format, ...) {
    va_list args;
    va_start(args, format);
    auto s = vtprint(format, args);
    va_end(args);
    MessageBoxA(0, s.data, "Show Muted big error", MB_ICONERROR);
}


internal inline b32
is_in_rect(int x0, int y0, int x1, int y1, v2i point) {
    return (point.x >= x0 && point.x < x1 &&
            point.y >= y0 && point.y < y1);
}

internal inline b32
is_in_rect(v2i p0, v2i p1, v2i point) {
    return is_in_rect(p0.x, p0.y, p1.x, p1.y, point);
}


internal inline v2i
point_to_v2i(POINT p) {
    return v2i{p.x, p.y};
}

internal inline b32
is_point_in_rect(POINT p, RECT r) {
    return (p.x >= 0 && p.x < r.right &&
            p.y >= 0 && p.y < r.bottom);
}

internal inline b32
is_point_in_rect(v2i p, RECT r) {
    return (p.x >= 0 && p.x < r.right &&
            p.y >= 0 && p.y < r.bottom);
}


internal inline f32
em_factor(f32 a) {
    f32 result = round_f32_f32(g_em*a);
    return result;
}

internal inline s32
em_factor_s32(f32 a) {
    s32 result = cast(s32)em_factor(a);
    return result;
}

internal inline v2i
em_v2i(f32 x, f32 y) {
    v2i result = v2i{cast(s32)em_factor(x), cast(s32)em_factor(y)};
    return result;
}


internal inline b32
is_muted() {
    return g_toggle_mute_is_on || g_push_to_mute_is_pressed;
}


internal inline LARGE_INTEGER
get_perf_counter() {
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}

internal inline f32
get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end) {
    return (cast(f32)(end.QuadPart - start.QuadPart) / cast(f32)g_perf_counter_frequency);
}

//~ Windows misc

#if !_RELEASE_BUILD
internal void
output_debug_string(char *format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    stbsp_vsnprintf(buffer, 256, format, args);
    va_end(args);
    OutputDebugStringA(buffer);
}
#endif

internal void
set_window_pos(HWND window, Layout layout, char *error_message) {
    WINDOWPLACEMENT window_placement = {sizeof(WINDOWPLACEMENT)};
    
    if(layout.maximized) {
        window_placement.flags = WPF_RESTORETOMAXIMIZED;
        window_placement.showCmd = SW_SHOWMAXIMIZED;
    } else {
        window_placement.showCmd = SW_NORMAL;
    }
    window_placement.ptMinPosition = POINT{.x = -1, .y = -1};
    window_placement.ptMaxPosition = POINT{.x = -1, .y = -1};
    window_placement.rcNormalPosition = layout.rect;
    
    if(!SetWindowPlacement(window, &window_placement)) {
        if(error_message) {
            report_error("SetWindowPlacement failed: %d\n%s\n", GetLastError(), error_message);
        }
    }
}

internal void
set_window_layout(HWND window, Layout layout, char *error_message) {
    if(layout.maximized) {
        ShowWindow(window, SW_SHOWMAXIMIZED);
    } else {
        set_window_pos(window, layout, error_message);
    }
}

internal Layout
get_window_pos(HWND window, char *error_message) {
    Layout result = {};
    
    RECT rect;
    WINDOWPLACEMENT place;
    if(!GetWindowRect(window, &rect)) {
        if(error_message) {
            report_error("GetWindowRect failed: %d\n%s\n", GetLastError(), error_message);
        }
    } else if(!GetWindowPlacement(window, &place)) {
        if(error_message) {
            report_error("GetWindowPlacement failed: %d\n%s\n", GetLastError(), error_message);
        }
    } else {
        result.is_used = true;
        
        if(place.showCmd == SW_SHOWMAXIMIZED) {
            result.maximized = true;
        } else if(place.showCmd == SW_NORMAL) {
            place.rcNormalPosition = rect;
        }
        
        result.rect = place.rcNormalPosition;
    }
    return result;
}

#if 0
internal Layout
get_window_pos(HWND window, char *error_message) {
    Layout result = {};
    WINDOWPLACEMENT place = {};
    GetWindowPlacement(window, &place);
    RECT window_rect;
    if(!GetWindowRect(window, &window_rect)) {
        if(error_message) {
            report_error("GetWindowRect failed: %d\n%s\n", GetLastError(), error_message);
        }
    } else {
        result.x      = window_rect.left;
        result.y      = window_rect.top;
        result.width  = window_rect.right  - window_rect.left;
        result.height = window_rect.bottom - window_rect.top;
        result.is_used = true;
    }
    return result;
}
#endif


global const u64 PRECISE_TIME_SECOND   = 10000000;
global const u64 PRECISE_TIME_MS       = PRECISE_TIME_SECOND/1000;

#define PreciseTimeToUS(time)      ((time)/(PRECISE_TIME_SECOND/1000000ull))
#define PreciseTimeToMS(time)      ((time)/(PRECISE_TIME_SECOND/1000ull))
#define PreciseTimeToSeconds(time) ((time)/(PRECISE_TIME_SECOND))

internal inline u64
get_precise_time() {
    FILETIME t = {};
    GetSystemTimePreciseAsFileTime(&t);
    u64 result = ((cast(u64)t.dwHighDateTime) << 32) | (cast(u64)t.dwLowDateTime);
    return result;
}


internal void
display_buffer_in_window(HDC window_dc, HDC bitmap_dc, int width, int height) {
    auto result = BitBlt(window_dc, 0, 0, width, height,
                         bitmap_dc, 0, 0, SRCCOPY);
}

internal void
resize_bitmap(Bitmap *bitmap, HWND window, int width, int height) {
    auto window_dc = GetDC(window);
    defer { ReleaseDC(window, window_dc); };
    
    if(bitmap->handle) {
        DeleteObject(bitmap->handle);
    }
    if(!bitmap->dc) {
        bitmap->dc = CreateCompatibleDC(window_dc);
        SelectObject(bitmap->dc, GetStockObject(DC_PEN));
        SelectObject(bitmap->dc, GetStockObject(DC_BRUSH));
    }
    
    bitmap->width  = width;
    bitmap->height = height;
    bitmap->handle = CreateCompatibleBitmap(window_dc, width, height);
    SelectObject(bitmap->dc, bitmap->handle);
}

internal inline void
resize_bitmap(Bitmap *bitmap, HWND window, RECT rect) {
    Assert(rect.left == 0 && rect.top == 0);
    resize_bitmap(bitmap, window, rect.right, rect.bottom);
}

internal inline void
on_settings_window_close() {
    g_settings.settings_window_layout = get_window_pos(g_settings_window, null);
}

//~ File stuff

internal inline void
free_file_memory(void *memory) {
    if(!memory) return;
    free(memory);
}

must_receive internal string
read_entire_file_if_exists(char *file_name) {
    auto file_handle = CreateFileA(file_name, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if(file_handle == INVALID_HANDLE_VALUE) {
        auto error = GetLastError();
        if(error != ERROR_FILE_NOT_FOUND) {
            report_error("Failed to open a file\nCreateFileA %d\n", error);
        }
        return {};
    }
    defer { CloseHandle(file_handle); };
    
    LARGE_INTEGER large_file_size;
    auto success = GetFileSizeEx(file_handle, &large_file_size);
    if(!success) {
        report_error("Failed to open a file.\nGetFileSizeEx %d\n", GetLastError());
        return {};
    }
    
    if(large_file_size.QuadPart > U32_MAX) {
        report_error("Failed to open a file.\nFile too big %lld\n", large_file_size.QuadPart);
        return {};
    }
    string result = {};
    result.count = large_file_size.QuadPart;
    
    result.data = cast(char *)malloc(result.count);
    DWORD bytes_read;
    auto read_success = ReadFile(file_handle, result.data, cast(DWORD)result.count, &bytes_read, 0);
    if(read_success && (result.count == bytes_read)) {
        return result;
    }
    free(result.data);
    report_error("Failed to load settings from file: ReadFile %d, %d, %d\n", bytes_read, result.count, GetLastError());
    return {};
}

internal b32
write_entire_file(char *file_name, string data) {
    auto file_handle = CreateFileA(file_name, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if(file_handle == INVALID_HANDLE_VALUE) {
        report_error("Failed to write to a file.\nCreateFileA %d\n", GetLastError());
        return false;
    }
    
    defer { CloseHandle(file_handle); };
    DWORD bytes_written;
    b32 write_success = WriteFile(file_handle, data.data, cast(DWORD)data.count, &bytes_written, 0);
    if(write_success && (data.count == bytes_written)) return true;
    
    report_error("Failed to write to a file.\nWriteFile %d, data.count %lld, bytes_written %d\n",
                 GetLastError(), data.count, bytes_written);
    return false;
}

//~ Settings file

#define NO_SETTINGS_ERROR_MESSAGE "You can still use the program, but the settings won't be saved for next time you open it"

internal char *
init_settings_file_path() {
    char buffer[32*1024];
    
    string exe_absolute_path = {};
    exe_absolute_path.count = GetModuleFileNameA(0, buffer, ArrayCount(buffer));
    exe_absolute_path.data = buffer;
    
    if(exe_absolute_path.count == 0) {
        report_error("GetModuleFileNameA error: %d\n\n%s\n", GetLastError(), NO_SETTINGS_ERROR_MESSAGE);
        return {};
    } else if(exe_absolute_path.count == ArrayCount(buffer)) {
        report_error("GetModuleFileNameA somehow returned a path that was too long: %d\n\n%s\n", exe_absolute_path.count, NO_SETTINGS_ERROR_MESSAGE);
        return {};
    } else if(exe_absolute_path.count <= 0) {
        report_error("GetModuleFileNameA somehow returned a path with no backslashes\n\n%s\n", NO_SETTINGS_ERROR_MESSAGE);
        return {};
    }
    
    
    string exe_dir = break_by_last_char(exe_absolute_path, '\\').lhs;
    string settings_file_name = sl("settings.ini");
    
    s64 count = exe_dir.count + 1 + settings_file_name.count;
    char *result = cast(char *)malloc(count + 1);
    AssertM(result, "init_settings_file_path malloc failed");
    
    snprint(result, cast(int)(count + 1), "%S\\%S", exe_dir, settings_file_name);
    
    return result;
}

internal b32
load_settings_from_file(string file) {
    ini_t *ini = ini_load(file.data, null);
    
    Settings st = {};
    
    int sct = 0;
    b32 ok = false;
    
    st.version = string_to_s32(ini_property_value(ini, sct, sl("version"), &ok));
    
    b32 settings_read_success = false;
    if(ok) {
        if(st.version > SETTINGS_VERSION) {
            report_error("Failed to load settings from file.\nSettings file version too high %d %d\n", st.version, SETTINGS_VERSION);
        } else if(st.version < 0) {
            report_error("Failed to load settings from file.\nSettings file version too low %d %d\n", st.version, SETTINGS_VERSION);
        } else if(st.version != SETTINGS_VERSION) {
            InvalidCodePath;
            //settings_read_success = convert_settings(st.version, file);
        } else {
            settings_read_success = true;
        }
    }
    if(!settings_read_success) {
        MoveFile(g_settings_file_path, tprint("%s.failed_to_read", g_settings_file_path).data);
        return false;
    }
    
    g_settings.version = SETTINGS_VERSION; // should always be SETTINGS_VERSION?
    st.color_mode_hsv = string_to_s32(ini_property_value(ini, sct, sl("color_mode_hsv"), &ok));
    if(ok) g_settings.color_mode_hsv = st.color_mode_hsv;
    
    string utext = ini_property_value(ini, sct, sl("unmuted_color"), &ok);
    if(ok) {
        ss_decl_1(ur_text, utext, break_by_spaces(utext));
        ss_decl_1(ug_text, utext, break_by_spaces(utext));
        ss_decl_1(ub_text, utext, break_by_spaces(utext));
        ss_decl_1(uh_text, utext, break_by_spaces(utext));
        
        st.unmuted_rgb.r = string_to_u32(ur_text);
        st.unmuted_rgb.g = string_to_u32(ug_text);
        st.unmuted_rgb.b = string_to_u32(ub_text);
        st.unmuted_preserved_hue = string_to_u32(uh_text);
        g_settings.unmuted_rgb           = st.unmuted_rgb;
        g_settings.unmuted_preserved_hue = st.unmuted_preserved_hue;
    }
    
    string mtext = ini_property_value(ini, sct, sl("muted_color"), &ok);
    if(ok) {
        ss_decl_1(mr_text, mtext, break_by_spaces(mtext));
        ss_decl_1(mg_text, mtext, break_by_spaces(mtext));
        ss_decl_1(mb_text, mtext, break_by_spaces(mtext));
        ss_decl_1(mh_text, mtext, break_by_spaces(mtext));
        
        st.muted_rgb.r = string_to_u32(mr_text);
        st.muted_rgb.g = string_to_u32(mg_text);
        st.muted_rgb.b = string_to_u32(mb_text);
        st.muted_preserved_hue = string_to_u32(mh_text);
        g_settings.muted_rgb           = st.muted_rgb;
        g_settings.muted_preserved_hue = st.muted_preserved_hue;
    }
    
    st.font_size = string_to_s32(ini_property_value(ini, sct, sl("font_size"), &ok));
    if(ok) g_settings.font_size = st.font_size;
    
    sct = ini_find_section(ini, sl("toggle_mute_bind"));
    if(sct != INI_NOT_FOUND) {
        ForI(i, 0, MAX_MUTE_BIND_COUNT) {
            string text = ini_property_value(ini, sct, tprint("%d", i), &ok);
            if(!ok) break;
            
            ss_decl_1(is_mouse, text, break_by_spaces(text));
            ss_decl_1(code, text, break_by_spaces(text));
            st.toggle_mute_bind.b[i].is_mouse = cast(u8)string_to_u32(is_mouse);
            st.toggle_mute_bind.b[i].code     = cast(u8)string_to_u32(code);
            st.toggle_mute_bind.count += 1;
        }
        g_settings.toggle_mute_bind = st.toggle_mute_bind;
    }
    
    sct = ini_find_section(ini, sl("push_to_mute_bind"));
    if(sct != INI_NOT_FOUND) {
        ForI(i, 0, MAX_MUTE_BIND_COUNT) {
            string text = ini_property_value(ini, sct, tprint("%d", i), &ok);
            if(!ok) break;
            
            ss_decl_1(is_mouse, text, break_by_spaces(text));
            ss_decl_1(code, text, break_by_spaces(text));
            st.push_to_mute_bind.b[i].is_mouse = cast(u8)string_to_u32(is_mouse);
            st.push_to_mute_bind.b[i].code     = cast(u8)string_to_u32(code);
            st.push_to_mute_bind.count += 1;
        }
        g_settings.push_to_mute_bind = st.push_to_mute_bind;
    }
    
    // window pos
    sct = ini_find_section(ini, sl("settings_window_layout"));
    if(sct != INI_NOT_FOUND) {
        st.settings_window_layout.is_used = true;
        g_settings.settings_window_layout.is_used = true;
        st.settings_window_layout.maximized = string_to_s32(ini_property_value(ini, sct, sl("maximized"), &ok));
        if(ok) g_settings.settings_window_layout.maximized = st.settings_window_layout.maximized;
        
        auto text = ini_property_value(ini, sct, sl("pos"), &ok);
        if(ok) {
            ss_decl_1(l_text, text, break_by_spaces(text));
            ss_decl_1(t_text, text, break_by_spaces(text));
            ss_decl_1(r_text, text, break_by_spaces(text));
            ss_decl_1(b_text, text, break_by_spaces(text));
            
            st.settings_window_layout.rect.left   = string_to_s32(l_text);
            st.settings_window_layout.rect.top    = string_to_s32(t_text);
            st.settings_window_layout.rect.right  = string_to_s32(r_text);
            st.settings_window_layout.rect.bottom = string_to_s32(b_text);
            g_settings.settings_window_layout.rect = st.settings_window_layout.rect;
        }
    }
    
    // window pos
    sct = ini_find_section(ini, sl("color_window_layout"));
    if(sct != INI_NOT_FOUND) {
        st.color_window_layout.is_used = true;
        st.color_window_layout.maximized = string_to_s32(ini_property_value(ini, sct, sl("maximized"), &ok));
        if(ok) g_settings.color_window_layout.maximized = st.color_window_layout.maximized;
        auto text = ini_property_value(ini, sct, sl("pos"), &ok);
        if(ok) {
            ss_decl_1(l_text, text, break_by_spaces(text));
            ss_decl_1(t_text, text, break_by_spaces(text));
            ss_decl_1(r_text, text, break_by_spaces(text));
            ss_decl_1(b_text, text, break_by_spaces(text));
            
            st.color_window_layout.rect.left   = string_to_s32(l_text);
            st.color_window_layout.rect.top    = string_to_s32(t_text);
            st.color_window_layout.rect.right  = string_to_s32(r_text);
            st.color_window_layout.rect.bottom = string_to_s32(b_text);
            g_settings.color_window_layout.rect = st.color_window_layout.rect;
        }
    }
    
    // layout
    sct = ini_find_section(ini, sl("layouts"));
    if(sct != INI_NOT_FOUND) {
        ForA(i, st.layouts) { auto it = st.layouts[i];
            string text = ini_property_value(ini, sct, tprint("%d", i), &ok);
            if(!ok) continue;
            
            st.layouts[i].is_used = true;
            if(text == sl("maximized")) {
                st.layouts[i].maximized = true;
            } else {
                ss_decl_1(l_text, text, break_by_spaces(text));
                ss_decl_1(t_text, text, break_by_spaces(text));
                ss_decl_1(r_text, text, break_by_spaces(text));
                ss_decl_1(b_text, text, break_by_spaces(text));
                
                st.layouts[i].rect.left   = string_to_s32(l_text);
                st.layouts[i].rect.top    = string_to_s32(t_text);
                st.layouts[i].rect.right  = string_to_s32(r_text);
                st.layouts[i].rect.bottom = string_to_s32(b_text);
            }
        }
        memcpy(&g_settings.layouts, &st.layouts, sizeof(st.layouts));;
    }
    
    return true;
}

internal string
save_settings_as_ini() {
    auto st = g_settings;
    ini_t *ini = ini_create(null);
    
    ini_property_add(ini, 0, sl("version"), tprint("%d", st.version));
    ini_property_add(ini, 0, sl("color_mode_hsv"), tprint("%d", st.color_mode_hsv));
    ini_property_add(ini, 0, sl("unmuted_color"), tprint("%d %d %d %d", st.unmuted_rgb.r, st.unmuted_rgb.g, st.unmuted_rgb.b, st.unmuted_preserved_hue));
    ini_property_add(ini, 0, sl("muted_color"), tprint("%d %d %d %d", st.muted_rgb.r, st.muted_rgb.g, st.muted_rgb.b, st.muted_preserved_hue));
    ini_property_add(ini, 0, sl("font_size"), tprint("%d", st.font_size));
    {
        int sct = ini_section_add(ini, sl("toggle_mute_bind"));
        ForI(i, 0, st.toggle_mute_bind.count) { auto it = st.toggle_mute_bind.b[i];
            ini_property_add(ini, sct, tprint("%d", i), tprint("%d %d", it.is_mouse, it.code));
        }
    }{
        int sct = ini_section_add(ini, sl("push_to_mute_bind"));
        ForI(i, 0, st.push_to_mute_bind.count) { auto it = st.push_to_mute_bind.b[i];
            ini_property_add(ini, sct, tprint("%d", i), tprint("%d %d", it.is_mouse, it.code));
        }
    }{ // window pos
        int sct = ini_section_add(ini, sl("settings_window_layout"));
        auto it = st.settings_window_layout;
        if(it.is_used) {
            ini_property_add(ini, sct, tprint("maximized"), tprint("%d", it.maximized));
            ini_property_add(ini, sct, tprint("pos"), tprint("%d %d %d %d", it.rect.left, it.rect.top, it.rect.right, it.rect.bottom));
        }
    }{ // window pos
        int sct = ini_section_add(ini, sl("color_window_layout"));
        auto it = st.color_window_layout;
        if(it.is_used) {
            ini_property_add(ini, sct, tprint("maximized"), tprint("%d", it.maximized));
            ini_property_add(ini, sct, tprint("pos"), tprint("%d %d %d %d", it.rect.left, it.rect.top, it.rect.right, it.rect.bottom));
        }
    }{ // layout
        int sct = ini_section_add(ini, sl("layouts"));
        ForA(i, st.layouts) { auto it = st.layouts[i];
            if(it.is_used) {
                string value;
                if(it.maximized) {
                    value = sl("maximized");
                } else {
                    value = tprint("%d %d %d %d", it.rect.left, it.rect.top, it.rect.right, it.rect.bottom);
                }
                ini_property_add(ini, sct, tprint("%d", i), value);
            }
        }
    }
    
    auto arena = &g_frame_arena;
    string result = {};
    
    // TODO(bo): lock_arena unlock_arena instead of get_free commit_used?
    // Doing this means you need to remember that you can't use any temp arena
    // stuff before the commit which is awful.
    // Or pass in a Scoped_Arena that's decl'd where you wanna use it.
    
    // @Volatile: don't alloc from temp arena before the commit
    string_s32 free = XXX_temp_arena__get_free_memory_s32(arena);
    result.data = free.data;
    
    // size includes zero-terminator
    int size = ini_save(ini, result.data, free.count);
    
    if(size <= free.count) {
        XXX_temp_arena__commit_used_bytes(arena, size);
    } else {
        result.data = cast(char *)temp_arena_alloc(arena, size);
        ini_save(ini, result.data, size);
    }
    result.count = size - 1;
    Assert(result.data[result.count] == '\0');
    return result;
}

internal void
save_settings_to_file(char *settings_file_path) {
    g_settings.unmuted_preserved_hue = g_unmuted_hsv.h;
    g_settings.muted_preserved_hue   = g_muted_hsv.h;
    if(settings_file_path) {
        string data = save_settings_as_ini();
        if(!write_entire_file(settings_file_path, data)) {
            report_error("Somehow failed to save settings to file\n\n%s\n", NO_SETTINGS_ERROR_MESSAGE);
        }
    }
}

#if 0
internal void
convert_settings(s32 settings_version, string file) {
    Assert(settings_version < SETTINGS_VERSION);
    if(settings_version == 0) {
    } else {
        Assert(!"Unknown settings version");
    }
}
#endif

//~ Bind and rawinput stuff

internal inline b32
bind_codes_match(Bind_Code a, Bind_Code b) {
    return a.is_mouse == b.is_mouse && a.code == b.code;
}

internal b32
mute_binds_match(Mute_Bind a, Mute_Bind b) {
    if(a.count != b.count) return false;
    
    ForI(i, 0, a.count) {
        if(!bind_codes_match(a.b[i], b.b[i])) return false;
    }
    
    return true;
}

internal inline b32
is_accepted_bind_code(Bind_Code b) {
    if(b.is_mouse) return true;
    return b.code != 0x07 && b.code != 0xff && b.code != VK_SHIFT && b.code != VK_CONTROL && b.code != VK_MENU;
}

internal inline b32
is_accepted_mouse_flags(USHORT flags) {
    return (flags & (RI_MOUSE_BUTTON_3_DOWN|RI_MOUSE_BUTTON_3_UP|
                     RI_MOUSE_BUTTON_4_DOWN|RI_MOUSE_BUTTON_4_UP|
                     RI_MOUSE_BUTTON_5_DOWN|RI_MOUSE_BUTTON_5_UP)) != 0;
}

internal inline b32
is_accepted_mouse_input(RAWMOUSE *input) {
    return is_accepted_mouse_flags(input->usButtonFlags);
}

internal inline b32
is_accepted_vk_code(USHORT vk) {
    return vk >= 0x00 && vk <= 0xfe && vk != 0x07;
}

internal inline b32
is_accepted_keyboard_input(RAWKEYBOARD *input) {
    return is_accepted_vk_code(input->VKey);
}

internal inline b32
is_accepted_rawinput(RAWINPUT *raw) {
    if(raw->header.dwType == RIM_TYPEMOUSE) {
        return is_accepted_mouse_input(&raw->data.mouse);
    } else if(raw->header.dwType == RIM_TYPEKEYBOARD) {
        return is_accepted_keyboard_input(&raw->data.keyboard);
    }
    return false;
}

internal Bind_Code
get_mouse_bind_code(RAWMOUSE *input) {
    USHORT flags = input->usButtonFlags;
    Assert(is_accepted_mouse_flags(flags));
    
    Bind_Code bind = {};
    bind.is_mouse = true;
    if(flags & (RI_MOUSE_BUTTON_3_DOWN|RI_MOUSE_BUTTON_3_UP)) {
        bind.code = MOUSE_2;
    } else if(flags & (RI_MOUSE_BUTTON_4_DOWN|RI_MOUSE_BUTTON_4_UP)) {
        bind.code = MOUSE_3;
    } else if(flags & (RI_MOUSE_BUTTON_5_DOWN|RI_MOUSE_BUTTON_5_UP)) {
        bind.code = MOUSE_4;
    }
    Assert(bind.code);
    return bind;
}

internal Bind_Code
get_key_bind_code(RAWKEYBOARD *input) {
    auto vk = input->VKey;
    b32 is_right = (input->Flags & RI_KEY_E0) != 0;
    Assert(is_accepted_vk_code(vk));
    if(vk == VK_SHIFT) {
        //assert(input->MakeCode == LSHIFT_MAKECODE || input->MakeCode == RSHIFT_MAKECODE);
    }
    
    // NOTE(bo): Unsure if this is the actual wanted behaviour, or just an artifact of SentInput messing with things
    // @Incomplete @Testing: Use SendMessage or something instead of SendInput and re-test
    Bind_Code key = {};
    if(vk == VK_SHIFT && input->MakeCode != RSHIFT_MAKECODE) {
        key.code = VK_LSHIFT;
    } else if(vk == VK_SHIFT && input->MakeCode == RSHIFT_MAKECODE || vk == VK_LSHIFT || vk == VK_RSHIFT) {
        key.code = VK_RSHIFT;
    } else if(vk == VK_CONTROL && !is_right || vk == VK_LCONTROL || vk == VK_RCONTROL) {
        key.code = VK_LCONTROL;
    } else if(vk == VK_CONTROL && is_right) {
        key.code = VK_RCONTROL;
    } else if(vk == VK_MENU && !is_right || vk == VK_LMENU || vk == VK_RMENU) {
        key.code = VK_LMENU;
    } else if(vk == VK_MENU && is_right) {
        key.code = VK_RMENU;
    } else if(vk == 0xe4 && is_right) {
        key.code = '0';
    } else {
        key.code = vk & 0xff;
    }
    return key;
}

internal inline Bind_Code
get_rawinput_bind_code(RAWINPUT *raw) {
    if(raw->header.dwType == RIM_TYPEMOUSE) {
        return get_mouse_bind_code(&raw->data.mouse);
    } else if(raw->header.dwType == RIM_TYPEKEYBOARD) {
        return get_key_bind_code(&raw->data.keyboard);
    }
    InvalidCodePath;
    return {};
}

internal inline b32
is_key_down(RAWKEYBOARD *input) {
    return (input->Flags & RI_KEY_BREAK) == 0;
}

internal inline b32
is_mouse_down(RAWMOUSE *input) {
    return (input->usButtonFlags & (RI_MOUSE_BUTTON_3_DOWN|RI_MOUSE_BUTTON_4_DOWN|RI_MOUSE_BUTTON_5_DOWN)) != 0;
}

internal inline b32
is_rawinput_down(RAWINPUT *raw) {
    if(raw->header.dwType == RIM_TYPEMOUSE) {
        return is_mouse_down(&raw->data.mouse);
    } else if(raw->header.dwType == RIM_TYPEKEYBOARD) {
        return is_key_down(&raw->data.keyboard);
    }
    InvalidCodePath;
    return false;
}

internal void
append_bind_code(String_Builder *sb, Bind_Code bind) {
    if(bind.is_mouse) {
        int number = -1;
        if(bind.code == MOUSE_2) {
            number = 2;
        } else if(bind.code == MOUSE_3) {
            number = 3;
        } else if(bind.code == MOUSE_4) {
            number = 4;
        } else Assert(!"Unhandled mouse button");
        append_string(sb, "MOUSE%d", number);
    } else {
        string s = {};
        switch(bind.code) {
            // Found with actual button presses on full 100% keyboard US layout:
            Case VK_ESCAPE:     s = sl("ESCAPE");
            Case VK_SNAPSHOT:   s = sl("PRINT SCREEN");
            Case VK_SCROLL:     s = sl("SCROLL LOCK");
            Case VK_PAUSE:      s = sl("BREAK");
            Case VK_OEM_3:      s = sl("`");
            Case VK_OEM_MINUS:  s = sl("-");
            Case VK_OEM_PLUS:   s = sl("=");
            Case VK_BACK:       s = sl("BACKSPACE");
            Case VK_INSERT:     s = sl("INSERT");
            Case VK_HOME:       s = sl("HOME");
            Case VK_PRIOR:      s = sl("PAGE UP");
            Case VK_TAB:        s = sl("TAB");
            Case VK_OEM_4:      s = sl("[");
            Case VK_OEM_6:      s = sl("]");
            Case VK_OEM_5:      s = sl("\\");
            Case VK_DELETE:     s = sl("DEL");
            Case VK_END:        s = sl("END");
            Case VK_NEXT:       s = sl("PAGE DOWN");
            Case VK_CAPITAL:    s = sl("CAPS LOCK");
            Case VK_OEM_1:      s = sl(";");
            Case VK_OEM_7:      s = sl("'");
            Case VK_RETURN:     s = sl("ENTER");
            Case VK_OEM_COMMA:  s = sl(",");
            Case VK_OEM_PERIOD: s = sl(".");
            Case VK_OEM_2:      s = sl("/");
            Case VK_UP:         s = sl("UP");
            Case VK_LWIN:       s = sl("META");
            Case VK_SPACE:      s = sl("SPACE");
            Case VK_APPS:       s = sl("RIGHT META");
            Case VK_LEFT:       s = sl("LEFT");
            Case VK_DOWN:       s = sl("DOWN");
            Case VK_RIGHT:      s = sl("RIGHT");
            Case VK_NUMLOCK:    s = sl("NUMPAD CLEAR");
            Case VK_DIVIDE:     s = sl("NUMPAD /");
            Case VK_MULTIPLY:   s = sl("NUMPAD *");
            Case VK_SUBTRACT:   s = sl("NUMPAD -");
            Case VK_ADD:        s = sl("NUMPAD +");
            Case VK_DECIMAL:    s = sl("NUMPAD .");
            
            // Found with SendInput():
            Case VK_LSHIFT: s = sl("SHIFT");
            Case VK_RSHIFT: s = sl("RIGHT SHIFT"); // need @Testing
            Case VK_LCONTROL: s = sl("CTRL");
            Case VK_RCONTROL: s = sl("RIGHT CTRL"); // need @Testing
            Case VK_LMENU: s = sl("ALT");
            Case VK_RMENU: s = sl("RIGHT ALT"); // need @Testing
            Case VK_MEDIA_NEXT_TRACK: s = sl("FAST FORWARD");
            Case VK_MEDIA_PREV_TRACK: s = sl("REWIND");
            Case VK_MEDIA_PLAY_PAUSE: s = sl("PLAY");
            Case VK_OEM_102: s = sl("NUMPAD =");
            Case VK_OEM_8: s = sl("`");
        }
        if(s.data) {
            append_string(sb, "%S", s);
        } else {
            if(bind.code >= '0' && bind.code <= '9' || bind.code >= 'A' && bind.code <= 'Z') {
                append_string(sb, "%c", bind.code);
            } else if(bind.code >= VK_F1 && bind.code <= VK_F19) { // F20-F24 aren't handled by discord i guess
                append_string(sb, "F%d", (bind.code - VK_F1) + 1);
            } else if(bind.code >= VK_NUMPAD0 && bind.code <= VK_NUMPAD9) {
                append_string(sb, "NUMPAD %d", bind.code - VK_NUMPAD0);
            } else {
                Assert(is_accepted_bind_code(bind));
                append_string(sb, "UNK%d", bind.code);
            }
        }
    }
}

internal void
append_bind(String_Builder *sb, Mute_Bind bind) {
    if(bind.count > 0) {
        int i = 0;
        while(true) {
            append_bind_code(sb, bind.b[i]);
            i += 1;
            if(i >= bind.count) break;
            append_string(sb, " + ");
        }
    } else {
        append_string(sb, "'unbound'");
    }
}

internal b32
is_whole_bind_pressed(Mute_Bind *bind) {
    if(bind->count == 0) return false;
    b32 whole_bind_is_pressed = true;
    ForI(i, 0, bind->count) {
        if(!bind->is_down[i]) {
            whole_bind_is_pressed = false;
            break;
        }
    }
    return whole_bind_is_pressed;
}

internal b32
bind_activated(Mute_Bind *bind, b32 *whole_bind_was_down) {
    if(is_whole_bind_pressed(bind)) {
        *whole_bind_was_down = true;
    } else {
        if(*whole_bind_was_down) {
            *whole_bind_was_down = false;
            return true;
        }
    }
    return false;
}

internal b32
maybe_add_bind_code_to_bind(Mute_Bind *new_mute_bind, Bind_Code new_bind) {
    if(new_mute_bind->count < 4) {
        b32 already_bound = false;
        ForI(i, 0, new_mute_bind->count) {
            if(bind_codes_match(new_bind, new_mute_bind->b[i])) {
                already_bound = true;
                break;
            }
        }
        if(!already_bound) {
            new_mute_bind->b[new_mute_bind->count] = new_bind;
            new_mute_bind->count += 1;
            return true;
        }
    }
    return false;
}

//~ UI_COlors

internal inline COLORREF
s32_color_lerp(s32 r, s32 g, s32 b, f32 t) {
    r = cast(s32) lerp(t, cast(f32)r, 0.0f);
    g = cast(s32) lerp(t, cast(f32)g, 0.0f);
    b = cast(s32) lerp(t, cast(f32)b, 0.0f);
    
    Assert(r >= 0 && r <= 255);
    Assert(g >= 0 && g <= 255);
    Assert(b >= 0 && b <= 255);
    
    return RGB(r, g, b);
}

internal Ui_Colors
make_fake_transparency_colors(s32 r, s32 g, s32 b) {
    Ui_Colors colors = {};
    const f32 menu_t       = 0.5f;
    const f32 button_t     = 0.72f;
    const f32 hot_button_t = 0.8f;
    
    colors.main            = RGB(r, g, b);
    colors.menu_background = s32_color_lerp(r, g, b, menu_t);
    colors.normal_button   = s32_color_lerp(r, g, b, button_t);
    colors.hot_button      = s32_color_lerp(r, g, b, hot_button_t);
    
    colors.normal_text = GRAY(230);
    colors.active_text = GRAY(255);
    
    return colors;
}

internal inline Ui_Colors
make_fake_transparency_colors(v3i rgb) {
    return make_fake_transparency_colors(rgb.r, rgb.g, rgb.b);
}

internal inline v3i
f32_1_color_to_v3_255(f32 r, f32 g, f32 b) {
    return v3i{cast(s32)(255.0f*r), cast(s32)(255.0f*g), cast(s32)(255.0f*b)};
}

//~ Drawing

internal inline void
set_draw_color(HDC dc, COLORREF color) {
    SetDCBrushColor(dc, color);
    SetDCPenColor(dc, color);
}

internal inline void
draw_rect(HDC dc, s32 x0, s32 y0, s32 x1, s32 y1, COLORREF color) {
    set_draw_color(dc, color);
    Rectangle(dc, x0, y0, x1, y1);
}

internal inline void
draw_rect(HDC dc, v2i p0, v2i p1, COLORREF color) {
    draw_rect(dc, p0.x, p0.y, p1.x, p1.y, color);
}

internal inline void
draw_rect(HDC dc, Rect r) {
    draw_rect(dc, r.p0.x, r.p0.y, r.p1.x, r.p1.y, r.color);
}

internal void
draw_menu(HDC dc, Ui_Colors ui_colors, Menu menu) {
    using enum Button_Flags;
    TIMED_BLOCK_BEGIN("menu background");
    draw_rect(dc, menu.topleft, menu.dim, ui_colors.menu_background);
    TIMED_BLOCK_END();
    TIMED_BLOCK_BEGIN("menu buttons");
    Assert(menu.buttons.count <= menu.buttons.capacity);
    ForI(i, 0, menu.buttons.count) { auto it = menu.buttons.data[i];
        draw_rect(dc, it.rect);
        
        if(it.rect.text.count) {
            SetBkColor(dc, it.rect.color);
            SetTextColor(dc, it.rect.text_color);
            TextOutA(dc, it.rect.p0.x + it.rect.text_pad.x, it.rect.p0.y + it.rect.text_pad.y, it.rect.text.data, cast(int)it.rect.text.count);
        }
    }
    
    TIMED_BLOCK_END();
    TIMED_BLOCK_BEGIN("menu sliders");
    Assert(menu.sliders.count <= menu.sliders.capacity);
    ForI(i, 0, menu.sliders.count) { auto it = menu.sliders.data[i];
        draw_rect(dc, it.line.p0, it.line.p1, it.line.color);
        draw_rect(dc, it.cursor.p0, it.cursor.p1, it.cursor.color);
    }
    TIMED_BLOCK_END();
}

internal void
draw_base_color(HDC dc, Ui_Colors ui_colors, RECT client_rect) {
    set_draw_color(dc, ui_colors.main);
    Rectangle(dc, client_rect.left, client_rect.top, client_rect.right, client_rect.bottom);
}

//~ UI

internal inline void
menu_horizontal_begin(Menu *menu) {
    Assert(!menu->is_horizontal_mode);
    menu->is_horizontal_mode = true;
    menu->saved_x = menu->p0.x;
    menu->hori_max_p1_y = menu->p0.y;
}

internal inline void
menu_horizontal_end(Menu *menu, s32 spacing = 1) {
    Assert(menu->is_horizontal_mode);
    menu->is_horizontal_mode = false;
    menu->p0.x = menu->saved_x;
    menu->p0.y = menu->hori_max_p1_y + spacing*menu->spacing;;
}

#define ModalScope(begin, end) for(int _i_ = ((begin), 0); !_i_; _i_ += 1, (end))

#define menu_horizontal_scope(menu, ...) \
ModalScope(menu_horizontal_begin(menu), \
menu_horizontal_end(menu, __VA_ARGS__))

internal inline void
menu_extra_advance(Menu *menu, v2i *p0, s32 extra_button_spacings) {
    if(menu->is_horizontal_mode) {
        p0->x += extra_button_spacings*menu->spacing;
    } else {
        p0->y += extra_button_spacings*menu->spacing;
    }
}

struct Scoped_Pushpop_menu_color {
    Menu *menu;
    Ui_Colors colors;
    Scoped_Pushpop_menu_color(Menu *menu) {
        this->menu = menu;
        this->colors = menu->colors;
    }
    ~Scoped_Pushpop_menu_color() {
        this->menu->colors = this->colors;
    }
};

internal inline void
menu_preamble(Menu *menu) {
    menu->buttons.count = 0;
    menu->sliders.count = 0;
    menu->id_count = 0;
    
    menu->pad = em_v2i(0.45f, 0.57f);
    menu->spacing = em_factor_s32(0.35f);
    
    menu->max_p1_x = 0;
}

internal inline void
menu_postamble(Menu *menu, s32 p1_y) {
    menu->dim = v2i{menu->max_p1_x, p1_y} + menu->pad;
}

internal s32
do_rect(Menu *menu, v2i rect_dim) {
    
    Button b = {};
    b.id = menu->id_count++;
    b.rect.p0 = menu->p0;
    b.rect.p1 = menu->p0 + rect_dim;
    b.rect.color = menu->colors.normal_button;
    
    menu->p1 = b.rect.p1;
    
    SetIfMax(menu->max_p1_x, menu->p1.x);
    
    if(menu->is_horizontal_mode) {
        SetIfMax(menu->hori_max_p1_y, menu->p1.y);
        menu->p0.x = menu->p1.x + menu->spacing;
    } else {
        menu->p0.y = menu->p1.y + menu->spacing;
    }
    
    menu->buttons.push(b);
    return cast(s32)menu->buttons.count - 1;
}

// NOTE(bo): :UiElementWeirdness also weird that everything goes through textbox
internal s32
do_textbox(Menu *menu, string s, s32 min_width = 0) {
    v2i text_pad = em_v2i(0.35f, 0.3f);
    
    SIZE text_dim = {};
    GetTextExtentPoint32A(menu->dc, s.data, cast(int)s.count, &text_dim);
    auto rect_dim = text_pad*2 + v2i{text_dim.cx, text_dim.cy + 1};
    
    if(min_width == 0 && rect_dim.x < rect_dim.y) min_width = rect_dim.y;
    
    if(rect_dim.x < min_width) {
        s32 extra_width = min_width - rect_dim.x;
        s32 extra_left_pad = extra_width/2;
        text_pad.x += extra_left_pad;
        rect_dim.x = min_width;
    }
    
    auto index = do_rect(menu, rect_dim);
    auto b = &menu->buttons.data[index];
    b->rect.text = s;
    b->rect.text_pad = text_pad;
    b->rect.text_color = menu->colors.normal_text;
    return index;
}

internal inline s32
do_text(Menu *menu, string s) {
    auto saved = menu->colors.normal_button;
    menu->colors.normal_button = menu->colors.menu_background;
    auto result = do_textbox(menu, s);
    menu->colors.normal_button = saved;
    return result;
}

internal b32
do_button(Menu *menu, Mouse mouse, string s) {
    auto index = do_textbox(menu, s);
    auto b = &menu->buttons.data[index];
    
    b32 result = false;
    b32 is_hot = is_in_rect(b->rect.p0, b->rect.p1, mouse.p);
    
    if(menu->active_id == b->id) {
        if(mouse.state == Mouse_Wentup) {
            if(is_hot) result = true;
            menu->active_id = -1;
        }
    } else if(is_hot) {
        if(mouse.state == Mouse_Wentdown) {
            menu->active_id = b->id;
        }
    }
    
    if(menu->active_id == b->id) {
        b->flags |= Button_Flags::ACTIVE;
        b->rect.text_color = menu->colors.active_text; // do_textbox already set it to normal_text
    }
    if(is_hot) {
        b->flags |= Button_Flags::HOT;
        b->rect.color      = menu->colors.hot_button;  // do_textbox already set it to normal_color
    }
    
    return result;
}

internal void
do_slider(Menu *menu, Mouse mouse, string s, Ui_Colors slider_colors, s32 *_value, s32 max_value, s32 slider_width) {
    if(slider_width == 0) slider_width = max_value+1;
    
    menu_horizontal_begin(menu);
    
    Slider slider = {};
    slider.id = menu->id_count++;
    
    s32 value = 0;
    s32 cursor_x;
    
    if(menu->active_id == slider.id) {
        s32 mouse_cursor_x = g_active_slider_state.start_cursor_x;
        mouse_cursor_x += (mouse.p.x - g_active_slider_state.start_mouse_x);
        mouse_cursor_x = clamp(mouse_cursor_x, 0, slider_width-1);
        value = linear_remap_s32(mouse_cursor_x,  0, slider_width-1,  0, max_value);
    } else if(_value) {
        value = *_value;
    }
    cursor_x = linear_remap_s32(value,  0, max_value,  0, slider_width-1);
    
    
    v2i slider_pad = em_v2i(0.2f, 0.2f);
    v2i line_dim = v2i{slider_width, em_factor_s32(0.2f)};
    
    v2i cursor_half_dim = em_v2i(0.2f, 0.45f);
    v2i cursor_dim = v2i{1, line_dim.y} + cursor_half_dim*2;
    
    slider.line.p0 = menu->p0 + slider_pad + cursor_half_dim;
    slider.line.p1 = slider.line.p0 + line_dim;
    
    slider.cursor.p0 = menu->p0 + slider_pad + v2i{cursor_x, 0};
    slider.cursor.p1 = slider.cursor.p0 + cursor_dim;
    
    b32 is_cursor_hot = is_in_rect(slider.cursor.p0, slider.cursor.p1, mouse.p);
    b32 clicked_directly_on_cursor = false;
    
    if(menu->active_id == slider.id) {
        if(mouse.state == Mouse_Wentup) {
            menu->active_id = -1;
        }
    } else if(is_cursor_hot) {
        if(mouse.state == Mouse_Wentdown) {
            menu->active_id = slider.id;
            clicked_directly_on_cursor = true;
        }
    }
    
    if(clicked_directly_on_cursor) {
        g_active_slider_state.start_mouse_x = mouse.p.x;
        g_active_slider_state.start_cursor_x = cursor_x;
    }
    
    if(menu->active_id != slider.id) {
        v2i clickable_p0 = v2i{slider.line.p0.x, slider.cursor.p0.y};
        v2i clickable_p1 = v2i{slider.line.p1.x, slider.cursor.p1.y};
        b32 in_clickable = is_in_rect(clickable_p0, clickable_p1, mouse.p);
        if(in_clickable) {
            if(mouse.state == Mouse_Wentdown) {
                auto current_cursor_center_x = slider.cursor.p0.x + cursor_half_dim.x;
                auto diff = (mouse.p.x - current_cursor_center_x);
                cursor_x += diff;
                g_active_slider_state.start_mouse_x = mouse.p.x;
                g_active_slider_state.start_cursor_x = cursor_x;
                menu->active_id = slider.id;
                is_cursor_hot = true;
                // NOTE(bo): recalculate cursor position since we changed cursor_x
                slider.cursor.p0 = menu->p0 + slider_pad + v2i{cursor_x, 0};
                slider.cursor.p1 = slider.cursor.p0 + cursor_dim;
            }
        }
    }
    
    if(menu->active_id == slider.id) {
        slider.flags |= Button_Flags::ACTIVE;
    }
    if(is_cursor_hot) {
        slider.flags |= Button_Flags::HOT;
    }
    
    slider.line.color = slider_colors.normal_button;
    slider.cursor.color = (slider.flags & (ACTIVE|HOT)) ? slider_colors.hot_button : slider_colors.normal_button;
    
    if(_value) *_value = linear_remap_s32(cursor_x,  0, slider_width-1,  0, max_value);
    
    menu->p1 = v2i{slider.line.p1.x + cursor_half_dim.x, slider.cursor.p1.y};
    auto max_p1 = menu->p1;;
    
    menu->sliders.push(slider);
    
    SetIfMax(menu->max_p1_x, menu->p1.x);
    
    if(menu->is_horizontal_mode) {
        SetIfMax(menu->hori_max_p1_y, menu->p1.y);
        menu->p0.x = menu->p1.x + menu->spacing;
    } else {
        menu->p0.y = menu->p1.y + menu->spacing;
    }
    
    //- end of slider stuff
    
    if(s.count) {
        string label = tprint("%S: %3d", s, value);
        do_text(menu, label);
        SetIfMax(max_p1.x, menu->p1.x);
        SetIfMax(max_p1.y, menu->p1.y);
    }
    
    menu->p1 = max_p1;
    menu_horizontal_end(menu);
}

#include "math.h" // TODO(bo): remove this

// m is the base color value for every channel
// C is the value added on top of that for the 'constant' color channel
// X is the value added on top of that for the 'variable' color
internal v3i
hsv_to_rgb(s32 hi, s32 si, s32 vi) {
    if(!(0 <= hi && hi <= 360) ||
       !(0 <= si && si <= 100) ||
       !(0 <= vi && vi <= 100))
    {
        Assert(!"Invalid HSV values");
        return {};
    }
    f32 s = si/100.0f;
    f32 v = vi/100.0f;
    
    // c is the value of the colour channel that is 'constantly high' within the given hue range
    // x is the colour channel that goes up and down within the given hue range
    f32 c = s*v;
    f32 x = c*(1.0f - fabsf(fmodf(hi/60.0f, 2.0f) - 1.0f));
    f32 r, g, b;
    
    if(         0 <= hi && hi <  60) { // red to yellow
        r = c;
        g = x;
        b = 0;
    } else if( 60 <= hi && hi < 120) { // yellow to green
        r = x;
        g = c;
        b = 0;
    } else if(120 <= hi && hi < 180) { // green to cyan
        r = 0;
        g = c;
        b = x;
    } else if(180 <= hi && hi < 240) { // cyan to blue
        r = 0;
        g = x;
        b = c;
    } else if(240 <= hi && hi < 300) { // blue to magenta
        r = x;
        g = 0;
        b = c;
    } else {                           // magenta to red
        r = c;
        g = 0;
        b = x;
    }
    
    f32 m = v - c; // m is washoutedness (negative saturation) scaled by v (value)
    auto result = v3i{
        round_f32_s32((r+m)*255.0f),
        round_f32_s32((g+m)*255.0f),
        round_f32_s32((b+m)*255.0f),
    };
    return result;
}

internal inline v3i
hsv_to_rgb(Hsvi a) {
    return hsv_to_rgb(a.h, a.s, a.v);
}

internal Hsvi
rgb_to_hsv(f32 r, f32 g, f32 b, f32 preserved_hue = 0.0f) {
    // r, g, b assumed to be [0,1]
    // preserved_hue [0,360]
    
    f32 cmax = Max(r, Max(g, b)); // max of r, g, b
    f32 cmin = Min(r, Min(g, b)); // min of r, g, b
    f32 diff = cmax - cmin;
    f32 h = -1.0f;
    f32 s = -1.0f;
    
    // If all rgb values are equal then we can't determine hue.
    // Set preserved_hue to preseve any hue represenation you might have.
    if(cmax == cmin)   h = preserved_hue;
    else if(cmax == r) h = fmodf((((g - b) / diff) * 60.0f) + 360.0f, 360.0f);
    else if(cmax == g) h = fmodf((((b - r) / diff) * 60.0f) + 120.0f, 360.0f);
    else if(cmax == b) h = fmodf((((r - g) / diff) * 60.0f) + 240.0f, 360.0f);
    
    if(cmax == 0.0f) {
        s = 100.0f; // As rgb goes toward black, hsv.s usually goes toward 100
    } else {
        s = (diff / cmax) * 100.0f;
    }
    
    f32 v = cmax * 100.0f;
    
    auto result = Hsvi{
        round_f32_s32(h),
        round_f32_s32(s),
        round_f32_s32(v),
    };
    return result;
}

internal inline Hsvi
rgb_to_hsv(v3i a, s32 preserved_hue = 0) {
    return rgb_to_hsv(a.r/255.0f, a.g/255.0f, a.b/255.0f, cast(f32)preserved_hue);
}

internal void
do_settings_menu(Menu *menu, HDC dc, Mouse mouse) {
    menu->dc = dc;
    
    SelectObject(dc, g_font);
    
    auto title_p  = em_v2i(1.5f, 0.88f);
    
    menu_preamble(menu);
    menu->topleft = em_v2i(0.88f, 3.2f);
    menu->colors = g_settings_colors;
    
    menu->p0 = title_p;
    menu->p1 = {};
    
    switch(g_settings_state) {
        Case Settings_State::MAIN: {
            do_textbox(menu, sl("Settings"));
            
            menu->p0 = menu->topleft + menu->pad;
            
            if(do_button(menu, mouse, sl("Close Settings"))) {
                on_settings_window_close();
                DestroyWindow(g_settings_window);
            }
            menu_extra_advance(menu, &menu->p0, 2);
            
            if(do_button(menu, mouse, sl("Change Toggle-Mute Bind"))) {
                g_settings_state = Settings_State::BIND_TOGGLE_MUTE;
                g_new_mute_bind = g_settings.toggle_mute_bind;
            }
            if(do_button(menu, mouse, sl("Change Push-to-Mute Bind"))) {
                g_settings_state = Settings_State::BIND_PUSH_TO_MUTE;
                g_new_mute_bind = g_settings.push_to_mute_bind;
            }
            if(do_button(menu, mouse, sl("Layouts"))) {
                g_settings_state = Settings_State::LAYOUTS;
                g_undo.layout.is_used = false;
            }
            if(do_button(menu, mouse, sl("Colors"))) {
                g_settings_state = Settings_State::COLORS;
                g_undo.unmuted_rgb = g_settings.unmuted_rgb;
                g_undo.muted_rgb   = g_settings.muted_rgb;
                g_undo.unmuted_hsv = rgb_to_hsv(g_settings.unmuted_rgb, g_unmuted_hsv.h);
                g_undo.muted_hsv   = rgb_to_hsv(g_settings.muted_rgb,   g_muted_hsv.h);
            }
            
            int font_change = 0;
            menu_horizontal_scope(menu) {
                do_text(menu, tprint("Font Size: %d", g_settings.font_size));
                
                if(do_button(menu, mouse, sl("-"))) {
                    font_change = -1;
                }
                if(do_button(menu, mouse, sl("+"))) {
                    font_change = +1;
                }
            }
            
            if(font_change != 0) {
                int font_size = g_settings.font_size;
                font_size += font_change;
                
                ForI(safety, 0, 10) {
                    if(font_size < 10 || font_size > 70) break;
                    auto font = CreateFontA(font_size, 0, 0, 0, FW_DONTCARE, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, "Arial");
                    SelectObject(dc, font);
                    
                    TEXTMETRICA metrics;
                    GetTextMetrics(dc, &metrics);
                    s32 em_s32 = metrics.tmHeight;
                    if(em_s32 <= 0) {
                        DeleteObject(font);
                        break;
                    }
                    
                    if(em_s32 != g_em_s32) {
                        DeleteObject(g_font);
                        g_font = font;
                        g_settings.font_size = em_s32;
                        g_em_s32 = em_s32;
                        g_em     = cast(f32)g_em_s32;
                        break;
                    }
                    DeleteObject(font);
                    font_size += font_change;
                }
                SelectObject(dc, g_font);
            }
            
        }
        
        Case   Settings_State::BIND_TOGGLE_MUTE:
        OrCase Settings_State::BIND_PUSH_TO_MUTE: {
            string title;
            Mute_Bind *current_bind;
            if(g_settings_state == Settings_State::BIND_TOGGLE_MUTE) {
                title = sl("Toggle-Mute Bind");
                current_bind = &g_settings.toggle_mute_bind;
            } else {
                title = sl("Push-to-Mute Bind");
                current_bind = &g_settings.push_to_mute_bind;
            }
            
            do_textbox(menu, title);
            
            menu->p0 = menu->topleft + menu->pad;
            
            menu_horizontal_scope(menu) {
                if(do_button(menu, mouse, sl("Back"))) {
                    g_settings_state = Settings_State::MAIN;
                }
                do_text(menu, sl("Press your bind now"));
            }
            menu_extra_advance(menu, &menu->p0, 2);
            {
                // @Volatile: don't alloc from temp arena before the commit
                auto sb = XXX_temp_arena__get_free_memory_as_string_builder(&g_frame_arena);
                append_string(&sb, "Bind: ");
                append_bind( &sb, g_new_mute_bind);
                auto s = string{.count = sb.count, .data = sb.data};
                XXX_temp_arena__commit_used_bytes(&g_frame_arena, sb.count);
                
                do_text(menu, s);
            }
            menu_extra_advance(menu, &menu->p0, 2);
            menu_horizontal_scope(menu) {
                if(do_button(menu, mouse, sl("Save"))) {
                    *current_bind = g_new_mute_bind;
                    //g_new_mute_bind.count = 0;
                    
                    save_settings_to_file(g_settings_file_path);
                }
                if(do_button(menu, mouse, sl("Clear"))) {
                    g_new_mute_bind.count = 0;
                }
            }
            if(!mute_binds_match(g_new_mute_bind, *current_bind)) {
                menu_extra_advance(menu, &menu->p0, 2);
                // @Volatile: don't alloc from temp arena before the commit
                auto sb = XXX_temp_arena__get_free_memory_as_string_builder(&g_frame_arena);
                append_string(&sb, "Restore: ");
                append_bind( &sb, *current_bind);
                auto s = string{.count = sb.count, .data = sb.data};
                XXX_temp_arena__commit_used_bytes(&g_frame_arena, sb.count);
                
                if(do_button(menu, mouse, s)) {
                    g_new_mute_bind = *current_bind;
                }
            }
        }
        
        Case Settings_State::LAYOUTS: {
            do_textbox(menu, sl("Layouts"));
            
            menu->p0 = menu->topleft + menu->pad;
            
            if(do_button(menu, mouse, sl("Back"))) {
                g_settings_state = Settings_State::MAIN;
            }
            menu_extra_advance(menu, &menu->p0, 2);
            
            ForA(i, g_settings.layouts) {
                
                menu_horizontal_scope(menu) {
                    if(do_button(menu, mouse, sl("Save"))) {
                        g_settings.layouts[i] = get_window_pos(g_color_window, "The layout couldn't be saved.");
                    }
                    
                    if(g_settings.layouts[i].is_used) {
                        string s = tprint("Layout %d : %S", i+1, tprint_layout(g_settings.layouts[i]));
                        if(do_button(menu, mouse, s)) {
                            if(g_settings.layouts[i].is_used) {
                                if(!g_undo.layout.is_used) {
                                    g_undo.layout = get_window_pos(g_color_window, "The layout couldn't be saved.");
                                }
                                set_window_layout(g_color_window, g_settings.layouts[i], "The layout couldn't be set.");
                            }
                        }
                    } else {
                        do_text(menu, tprint("Layout %d : Unused", i+1));
                    }
                    
                    if(g_settings.layouts[i].is_used) {
                        if(do_button(menu, mouse, sl("Clear"))) {
                            g_settings.layouts[i].is_used = false;
                        }
                    }
                }
            }
            if(g_undo.layout.is_used) {
                menu_extra_advance(menu, &menu->p0, 3);
                
                string s = tprint("Restore Window Position : %S", tprint_layout(g_undo.layout));
                if(do_button(menu, mouse, s)) {
                    set_window_pos(g_color_window, g_undo.layout, "The layout couldn't be set.");
                    g_undo.layout.is_used = false;
                }
            }
        }
        
        Case Settings_State::COLORS: {
            b32 should_redraw = false;
            
            do_textbox(menu, sl("Colors"));
            
            menu->p0 = menu->topleft + menu->pad;
            
            if(do_button(menu, mouse, sl("Back"))) {
                g_settings_state = Settings_State::MAIN;
            }
            menu_extra_advance(menu, &menu->p0, 2);
            
            if(do_button(menu, mouse, sl("Manual Toggle"))) {
                g_toggle_mute_is_on = !g_toggle_mute_is_on;
                should_redraw = true;
            }
            
            string swap_text = g_settings.color_mode_hsv ? sl("Swap to RGB") : sl("Swap to HSV");
            if(do_button(menu, mouse, swap_text)) {
                g_settings.color_mode_hsv = !g_settings.color_mode_hsv;
            }
            menu_extra_advance(menu, &menu->p0, 2);
            
            v3i unmuted_rgb = g_settings.unmuted_rgb;
            v3i   muted_rgb = g_settings.muted_rgb;
            s32 unmuted_color_label_p1_x;
            menu_horizontal_scope(menu) {
                
                do_text(menu, sl("Unmuted Color:"));
                unmuted_color_label_p1_x = menu->p1.x;
                
                {   Scoped_Pushpop_menu_color asdf(menu);
                    menu->colors.normal_button = RGB(unmuted_rgb.r, unmuted_rgb.g, unmuted_rgb.b);
                    do_textbox(menu, sl(" "), em_factor_s32(3.0f));
                }
                // TODO(bo): text input
                string s = tprint("#%02x%02x%02x", unmuted_rgb.r, unmuted_rgb.g, unmuted_rgb.b);
                do_textbox(menu, s);
            }
            
            auto unmuted_hsv = g_unmuted_hsv;
            auto   muted_hsv =   g_muted_hsv;
            
            auto sliderc = menu->colors;
            
            if(g_settings.color_mode_hsv) {
                sliderc.normal_button = GRAY(70);
                sliderc.hot_button    = GRAY(90);
                do_slider(menu, mouse, sl("H"), sliderc, &unmuted_hsv.h, 360, 361);
                do_slider(menu, mouse, sl("S"), sliderc, &unmuted_hsv.s, 100, 361);
                do_slider(menu, mouse, sl("V"), sliderc, &unmuted_hsv.v, 100, 361);
            } else {
                const int c = 210; // cold
                const int h = 255; // hot
                const int b = 60; // base
                sliderc.normal_button = RGB(c, b, b);
                sliderc.hot_button    = RGB(h, b, b);
                do_slider(menu, mouse, sl("R"), sliderc, &unmuted_rgb.r, 255, 361);
                sliderc.normal_button = RGB(b, c, b);
                sliderc.hot_button    = RGB(b, h, b);
                do_slider(menu, mouse, sl("G"), sliderc, &unmuted_rgb.g, 255, 361);
                sliderc.normal_button = RGB(b, b, c);
                sliderc.hot_button    = RGB(b, b, h);
                do_slider(menu, mouse, sl("B"), sliderc, &unmuted_rgb.b, 255, 361);
            }
            
            menu_horizontal_scope(menu) {
                
                do_text(menu, sl("Muted Color:"));
                menu->p0.x = unmuted_color_label_p1_x + menu->spacing;
                
                {   Scoped_Pushpop_menu_color asdf(menu);
                    menu->colors.normal_button = RGB(muted_rgb.r, muted_rgb.g, muted_rgb.b);
                    do_textbox(menu, sl(" "), em_factor_s32(3.0f));
                }
                // TODO(bo): text input
                string s = tprint("#%02x%02x%02x", muted_rgb.r, muted_rgb.g, muted_rgb.b);
                do_textbox(menu, s);
            }
            
            sliderc = menu->colors;
            if(g_settings.color_mode_hsv) {
                sliderc.normal_button = GRAY(70);
                sliderc.hot_button    = GRAY(90);
                do_slider(menu, mouse, sl("H"), sliderc, &muted_hsv.h, 360, 361);
                do_slider(menu, mouse, sl("S"), sliderc, &muted_hsv.s, 100, 361);
                do_slider(menu, mouse, sl("V"), sliderc, &muted_hsv.v, 100, 361);
            } else {
                const int c = 210; // cold
                const int h = 255; // hot
                const int b = 60; // base
                sliderc.normal_button = RGB(c, b, b);
                sliderc.hot_button    = RGB(h, b, b);
                do_slider(menu, mouse, sl("R"), sliderc, &muted_rgb.r, 255, 361);
                sliderc.normal_button = RGB(b, c, b);
                sliderc.hot_button    = RGB(b, h, b);
                do_slider(menu, mouse, sl("G"), sliderc, &muted_rgb.g, 255, 361);
                sliderc.normal_button = RGB(b, b, c);
                sliderc.hot_button    = RGB(b, b, h);
                do_slider(menu, mouse, sl("B"), sliderc, &muted_rgb.b, 255, 361);
            }
            
            if(g_undo.unmuted_rgb != g_settings.unmuted_rgb ||
               g_undo.muted_rgb   != g_settings.muted_rgb)
            {
                string s = {};
                if(g_settings.color_mode_hsv) {
                    s = tprint("Restore - HSV(%d, %d, %d), HSV(%d, %d, %d)",
                               g_undo.unmuted_hsv.h, g_undo.unmuted_hsv.s, g_undo.unmuted_hsv.v,
                               g_undo.  muted_hsv.h, g_undo.  muted_hsv.s, g_undo.  muted_hsv.v);
                } else {
                    s = tprint("Restore - RGB(%d, %d, %d), RGB(%d, %d, %d)",
                               g_undo.unmuted_rgb.r, g_undo.unmuted_rgb.g, g_undo.unmuted_rgb.b,
                               g_undo.  muted_rgb.r, g_undo.  muted_rgb.g, g_undo.  muted_rgb.b);
                }
                menu_extra_advance(menu, &menu->p0, 3);
                if(do_button(menu, mouse, s)) {
                    unmuted_rgb = g_undo.unmuted_rgb;
                    muted_rgb   = g_undo.muted_rgb;
                    // NOTE(bo): hsv will be taken care of below
                    should_redraw = true;
                }
            }
            
            if(g_settings.unmuted_rgb != unmuted_rgb) {
                g_settings.unmuted_rgb = unmuted_rgb;
                g_unmuted_hsv = unmuted_hsv = rgb_to_hsv(unmuted_rgb, g_unmuted_hsv.h);
                g_unmuted_colors = make_fake_transparency_colors(unmuted_rgb);
                should_redraw = true;
            }
            if(g_unmuted_hsv != unmuted_hsv) {
                g_unmuted_hsv = unmuted_hsv;
                g_settings.unmuted_rgb = unmuted_rgb = hsv_to_rgb(unmuted_hsv);
                g_unmuted_colors = make_fake_transparency_colors(unmuted_rgb);
                should_redraw = true;
            }
            
            if(g_settings.muted_rgb != muted_rgb) {
                g_settings.muted_rgb = muted_rgb;
                g_muted_hsv = muted_hsv = rgb_to_hsv(muted_rgb, g_muted_hsv.h);
                g_muted_colors = make_fake_transparency_colors(muted_rgb);
                should_redraw = true;
            }
            
            if(g_muted_hsv != muted_hsv) {
                g_muted_hsv = muted_hsv;
                g_settings.muted_rgb = muted_rgb = hsv_to_rgb(muted_hsv);
                g_muted_colors = make_fake_transparency_colors(muted_rgb);
                should_redraw = true;
            }
            
            if(should_redraw) invalidate_color_window();
        }
        InvalidDefaultCase;
    }
    menu_postamble(menu, menu->p1.y);
}

internal void
do_color_menu(Menu *menu, HDC dc, Mouse mouse) {
    menu->dc = dc;
    
    SelectObject(dc, g_font); // done here so do_button can get text dim
    
    menu_preamble(menu);
    menu->topleft = v2i{0, 0};
    
    menu->p0 = menu->topleft + menu->pad;
    menu->p1 = {};
    
    menu->colors = is_muted() ? g_muted_colors : g_unmuted_colors;
    
    if(g_paused) {
        if(do_button(menu, mouse, sl("Unpause"))) {
            g_paused = false;
        }
    } else {
        if(do_button(menu, mouse, sl("Pause"))) {
            g_paused = true;
        }
    }
    if(do_button(menu, mouse, sl("Manual Toggle"))) {
        g_toggle_mute_is_on = !g_toggle_mute_is_on;
    }
    if(do_button(menu, mouse, sl("Settings"))) {
        if(g_settings_window) {
            SetForegroundWindow(g_settings_window);
        } else {
            g_settings_window = CreateWindowExA(0, g_settings_window_class.lpszClassName, "Show Muted Settings" TITLE_SUFFIX,
                                                WS_OVERLAPPEDWINDOW,
                                                CW_USEDEFAULT, CW_USEDEFAULT,
                                                rect_width( g_settings.settings_window_layout.rect), rect_height(g_settings.settings_window_layout.rect),
                                                0, 0, g_settings_window_class.hInstance, 0);
            if(!g_settings_window) {
                report_error("Big error\nFailed to create window: %d\n", GetLastError());
            } else {
                set_window_pos(g_settings_window, g_settings.settings_window_layout, null);
                
                RECT client_rect;
                GetClientRect(g_settings_window, &client_rect);
                resize_bitmap(&g_settings_bitmap, g_settings_window, client_rect);
                menu_reset(&g_settings_menu);
                ShowWindow(g_settings_window, SW_SHOW);
            }
        }
    }
    
#if _INTERNAL_BUILD
    if(do_button(menu, mouse, sl("Fake Error"))) {
        g_error_occurred = true;
    }
    if(do_button(menu, mouse, sl("get"))) {
        if(g_settings_window) {
            g_settings.settings_window_layout = get_window_pos(g_settings_window, null);
        }
    }
#endif
    
    menu_postamble(menu, menu->p1.y);
}

internal void
do_error_menu(Menu *menu, HDC dc, Mouse mouse) {
    menu->dc = dc;
    
    SelectObject(dc, g_font);
    
    auto title_p = em_v2i(0.66f, 1.0f);
    
    menu_preamble(menu);
    menu->colors = g_error_colors;
    
    menu->p0 = title_p;
    menu->p1 = {};
    
    do_textbox(menu, sl("Error occurred"));
    
    menu->topleft = v2i{0, menu->p1.y + title_p.y};
    
    menu->p0 = menu->topleft + menu->pad;
    
    if(do_button(menu, mouse, sl("Continue"))) {
        clear_error();
    }
    if(do_button(menu, mouse, sl("Show error"))) {
        MessageBoxA(0, g_error_message.data, "Show Muted error", MB_ICONERROR);
    }
    
    menu_postamble(menu, menu->p1.y);
}

//~ Window callbacks

LRESULT CALLBACK
settings_window_callback(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;
    
    switch(message) {
        Case WM_DESTROY: {
            g_settings_window = 0;
            g_settings_window_active = false;
            
            g_settings_state = Settings_State::MAIN;
            save_settings_to_file(g_settings_file_path);
        }
        
        Case WM_CLOSE: {
            on_settings_window_close();
            DestroyWindow(g_settings_window);
        }
        
        Case WM_ACTIVATE: {
            auto value = LOWORD(wparam);
            g_settings_window_active = (value != WA_INACTIVE);
            if(!g_settings_window_active) {
                menu_reset(&g_settings_menu);
            }
        }
        
        Case WM_SETCURSOR: {
            SetCursor(LoadCursor(0, IDC_ARROW));
            result = DefWindowProcA(window, message, wparam, lparam);
        }
        
        Case WM_PAINT: {
            PAINTSTRUCT paint;
            auto paint_dc = BeginPaint(window, &paint);
            defer { EndPaint(window, &paint); };
            
            // @Copypaste: basically just the if(g_settings_window_active) from the main loop
            // TODO(bo): Open windows in a separate thread so the main thread doesn't get blocked on paint/resize?
            
            temp_arena_reset(&g_frame_arena);
            
            RECT client_rect;
            GetClientRect(window, &client_rect);
            Assert(client_rect.left == 0 && client_rect.top == 0);
            if(client_rect.right != g_settings_bitmap.width || client_rect.bottom != g_settings_bitmap.height) {
                resize_bitmap(&g_settings_bitmap, g_settings_window, client_rect);
            }
            do_settings_menu(&g_settings_menu, g_settings_bitmap.dc, {});
            draw_base_color(g_settings_bitmap.dc, g_settings_colors, client_rect);
            draw_menu(      g_settings_bitmap.dc, g_settings_colors, g_settings_menu);
            display_buffer_in_window(paint_dc, g_settings_bitmap.dc, g_settings_bitmap.width, g_settings_bitmap.height);
        }
        
        Default: {
            result = DefWindowProcA(window, message, wparam, lparam);
        }
    }
    
    return result;
}

LRESULT CALLBACK
main_window_callback(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;
    
    switch(message) {
        Case   WM_DESTROY:
        OrCase WM_CLOSE: {
            g_running = false;
        }
        
        Case WM_SETCURSOR: {
            SetCursor(LoadCursor(0, IDC_ARROW));
            result = DefWindowProcA(window, message, wparam, lparam);
        }
        
        Case WM_ACTIVATE: {
            auto value = LOWORD(wparam);
            g_color_window_active = (value != WA_INACTIVE);
            if(!g_color_window_active) {
                menu_reset(&g_error_menu);
                menu_reset(&g_color_menu);
            }
            invalidate_color_window();
        }
        
        Case WM_PAINT: {
            PAINTSTRUCT paint;
            auto paint_dc = BeginPaint(window, &paint);
            defer { EndPaint(window, &paint); };
            
            // @Copypaste: basically just the if(g_color_window_active) from the main loop
            // TODO(bo): Open windows in a separate thread so the main thread doesn't get blocked on paint/resize?
            
            temp_arena_reset(&g_frame_arena);
            
            SelectObject(paint_dc, GetStockObject(DC_PEN));
            SelectObject(paint_dc, GetStockObject(DC_BRUSH));
            
            RECT client_rect;
            GetClientRect(window, &client_rect);
            if(g_error_occurred) {
                do_error_menu(&g_error_menu, paint_dc, {});
                draw_base_color(paint_dc,   g_error_colors, client_rect);
                draw_menu(paint_dc, g_error_colors, g_error_menu);
            } else if(is_muted()) {
                draw_base_color(paint_dc,   g_muted_colors, client_rect);
            } else {
                draw_base_color(paint_dc, g_unmuted_colors, client_rect);
            }
        }
        
        Case WM_INPUT: {
            b32 window_is_in_foreground = GET_RAWINPUT_CODE_WPARAM(wparam) == RIM_INPUT;
            auto raw_input_handle = cast(HRAWINPUT)lparam;
            
            RAWINPUT raw = {};
            UINT buffer_size = sizeof(RAWINPUT);
            auto bytes_copied = GetRawInputData(raw_input_handle, RID_INPUT, &raw, &buffer_size, sizeof(RAWINPUTHEADER));
#if _INTERNAL_BUILD
            if(raw.header.dwType == RIM_TYPEKEYBOARD) {
                if(raw.data.keyboard.VKey == VK_ESCAPE) {
                    if(g_settings_window_active) {
                        //DestroyWindow(g_settings_window);
                        g_running = false;
                    } else if(g_color_window_active) {
                        g_running = false;
                    }
                }
                if(raw.data.keyboard.VKey == 'W') {
                    int asdf = 21;
                }
            }
#endif
            if(is_accepted_rawinput(&raw)) {
                auto is_down = is_rawinput_down(&raw);
                
                if(g_settings_state == Settings_State::MAIN || g_settings_state == Settings_State::LAYOUTS || g_settings_state == Settings_State::COLORS) {
                    if(!g_paused && !g_error_occurred) {
                        auto bind = get_rawinput_bind_code(&raw);
                        
                        ForI(i, 0, g_settings.toggle_mute_bind.count) {
                            if(bind_codes_match(bind, g_settings.toggle_mute_bind.b[i])) {
                                g_settings.toggle_mute_bind.is_down[i] = is_down;
                                break;
                            }
                        }
                        
                        if(bind_activated(&g_settings.toggle_mute_bind, &g_toggle_mute_bind_was_down)) {
                            g_toggle_mute_is_on = !g_toggle_mute_is_on;
                            invalidate_color_window();
                        }
                        
                        ForI(i, 0, g_settings.push_to_mute_bind.count) {
                            if(bind_codes_match(bind, g_settings.push_to_mute_bind.b[i])) {
                                g_settings.push_to_mute_bind.is_down[i] = is_down;
                                break;
                            }
                        }
                        
                        auto ptm_was_pressed = g_push_to_mute_is_pressed;
                        g_push_to_mute_is_pressed = is_whole_bind_pressed(&g_settings.push_to_mute_bind);
                        if(!g_toggle_mute_is_on && g_push_to_mute_is_pressed != ptm_was_pressed) {
                            invalidate_color_window();
                        }
                    }
                } else if(g_settings_state == Settings_State::BIND_TOGGLE_MUTE || g_settings_state == Settings_State::BIND_PUSH_TO_MUTE) {
                    if(g_settings_window_active) {
                        if(is_down) {
                            maybe_add_bind_code_to_bind(&g_new_mute_bind, get_rawinput_bind_code(&raw));
                        }
                    }
                } else {
                    report_error("Internal Error: The menu state is somehow invalid\n"
                                 "State index: %d\n"
                                 "Should be in the range 0-%d\n", g_settings_state, cast(int)Settings_State::_TERMINATOR-1);
                }
            }
            
            result = DefWindowProcA(window, message, wparam, lparam);
        }
        
        Default: {
            result = DefWindowProcA(window, message, wparam, lparam);
        }
    }
    
    return result;
}

//~ main

global f32 frame_times[600];

int WINAPI
WinMain(HINSTANCE instance, HINSTANCE, PSTR, int) {
    g_frame_arena.chunk_size = KB(256);
    g_frame_arena.alignment  = 8;
    
#if _INTERNAL_PROFILE
    g_spall_ctx = spall_init_file("timings.spall", 1);
    
    const auto SPALL_BUFFER_SIZE = (32 * 1024 * 1024);
    u8 *spall_data_buffer = cast(u8 *)malloc(SPALL_BUFFER_SIZE);
    
	g_spall_buffer = SpallBuffer{
		.data = spall_data_buffer,
		.length = SPALL_BUFFER_SIZE,
	};
    
	spall_buffer_init(&g_spall_ctx, &g_spall_buffer);
#endif
    
    WNDCLASSA color_window_class = {};
    color_window_class.style         = CS_HREDRAW|CS_VREDRAW;
    color_window_class.lpfnWndProc   = main_window_callback;
    color_window_class.hInstance     = instance;
    color_window_class.hCursor       = LoadCursor(NULL, IDC_ARROW);
    color_window_class.lpszClassName = "showmuted_main";
    
    HWND existing_window = FindWindowA(color_window_class.lpszClassName, null);
    if(existing_window) {
        SetForegroundWindow(existing_window);
        return 0;
    }
    
    if(!RegisterClassA(&color_window_class)) {
        fatal_error("Big error\nFailed to register window class: %d\n", GetLastError());
        return 0;
    }
    
    g_settings_window_class.style         = CS_HREDRAW|CS_VREDRAW;
    g_settings_window_class.lpfnWndProc   = settings_window_callback;
    g_settings_window_class.hInstance     = instance;
    g_settings_window_class.hCursor       = LoadCursor(NULL, IDC_ARROW);
    g_settings_window_class.lpszClassName = "showmuted_settings";
    
    if(!RegisterClassA(&g_settings_window_class)) {
        fatal_error("Big error\nFailed to register window class: %d\n", GetLastError());
        return 0;
    }
    
    v2i default_color_window_dim = v2i{270, 270};
    
    g_color_window = CreateWindowExA(0, color_window_class.lpszClassName, "Show Muted" TITLE_SUFFIX,
                                     WS_OVERLAPPEDWINDOW,
                                     CW_USEDEFAULT, CW_USEDEFAULT,
                                     default_color_window_dim.x, default_color_window_dim.y,
                                     0, 0, instance, 0);
    if(!g_color_window) {
        fatal_error("Big error\nFailed to create window: %d\n", GetLastError());
        return 0;
    }
    
    RAWINPUTDEVICE raw_input_devices[2];
    
    raw_input_devices[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    raw_input_devices[0].usUsage = HID_USAGE_GENERIC_MOUSE;
    raw_input_devices[0].dwFlags = RIDEV_INPUTSINK;
    raw_input_devices[0].hwndTarget = g_color_window;
    
    raw_input_devices[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
    raw_input_devices[1].usUsage = HID_USAGE_GENERIC_KEYBOARD;
    raw_input_devices[1].dwFlags = RIDEV_INPUTSINK;
    raw_input_devices[1].hwndTarget = g_color_window;
    
    if(!RegisterRawInputDevices(raw_input_devices, ArrayCount(raw_input_devices), sizeof(RAWINPUTDEVICE))) {
        fatal_error("Big error\nFailed to register for raw input messages: %d\n", GetLastError());
        return 0;
    }
    
    {
        LARGE_INTEGER perf_counter_frequency_result;
        QueryPerformanceFrequency(&perf_counter_frequency_result);
        g_perf_counter_frequency = perf_counter_frequency_result.QuadPart;
    }
    UINT wanted_scheduler_ms = 1;
    b32 sleep_is_granular = (timeBeginPeriod(wanted_scheduler_ms) == TIMERR_NOERROR);
    Assert(sleep_is_granular);
    { // Default settings
        g_settings.version = SETTINGS_VERSION;
        g_settings.settings_window_layout.rect = rect_p_dim({100, 100}, {550, 750});
        g_settings.color_window_layout.rect = rect_p_dim({100, 100}, default_color_window_dim);
        g_settings.unmuted_rgb = v3i{34, 171, 54};
        g_settings.muted_rgb   = v3i{171, 34, 34};
        g_settings.font_size = 19;
        
#if 0
        g_settings.unmuted_rgb = v3i{46, 135, 70};
        g_settings.muted_rgb   = v3i{135, 46, 70};
#endif
    }
    g_settings_file_path = init_settings_file_path();
    b32 settings_read_success = false;
    if(g_settings_file_path) {
        auto file = read_entire_file_if_exists(g_settings_file_path);
        settings_read_success = load_settings_from_file(file);
        free_file_memory(file.data);
    }
    if(settings_read_success) {
        set_window_pos(g_color_window, g_settings.color_window_layout, "The startup layout couldn't be set.");
    }
    
    { // Set up ui globals
        g_unmuted_colors = make_fake_transparency_colors(g_settings.unmuted_rgb);
        g_muted_colors   = make_fake_transparency_colors(g_settings.muted_rgb);
        
        g_unmuted_hsv = rgb_to_hsv(g_settings.unmuted_rgb, g_settings.unmuted_preserved_hue);
        g_muted_hsv   = rgb_to_hsv(g_settings.muted_rgb, g_settings.muted_preserved_hue);
        
        {   // init g_settings_colors
            g_settings_colors.main            = GRAY(20);
            g_settings_colors.menu_background = GRAY(35);
            g_settings_colors.normal_button   = GRAY(55);
            g_settings_colors.hot_button      = GRAY(65);
            
            g_settings_colors.normal_text = GRAY(215);
            g_settings_colors.active_text = GRAY(255);
            
            // init g_error_colors
            g_error_colors.main            = GRAY(10);
            g_error_colors.menu_background = GRAY(10);
            g_error_colors.normal_button   = GRAY(35);
            g_error_colors.hot_button      = GRAY(45);
            
            g_error_colors.normal_text = RGB(255, 0, 0);
            g_error_colors.active_text = RGB(255, 0, 0);
        }
        
        g_font = CreateFontA(g_settings.font_size, 0, 0, 0, FW_DONTCARE, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, "Arial");
        
        g_color_menu.active_id = -1;
        g_settings_menu.active_id = -1;
        g_error_menu.active_id = -1;
        
        auto dc = GetDC(g_color_window);
        defer { ReleaseDC(g_color_window, dc); };
        
        SelectObject(dc, g_font);
        
        TEXTMETRICA metrics;
        GetTextMetrics(dc, &metrics);
        g_em_s32 = metrics.tmHeight;
        g_em     = cast(f32)g_em_s32;
    }
    
    {
        RECT client_rect;
        GetClientRect(g_color_window, &client_rect);
        resize_bitmap(&g_color_bitmap, g_color_window, client_rect);
    }
    ShowWindow(g_color_window, SW_SHOW);
    
    //- main loop
    
    g_running = true;
    
    b32 mouse_was_down = false;;
    
    // NOTE(bo): so the user doesn't accidentally click the invisible buttons when they click onto the window
    b32 color_window_was_active_last_loop = false;
    
    s32 frame_index = 0;
    
    auto frame_start_counter = get_perf_counter();
    while(g_running) {
        while(!(g_color_window_active || g_settings_window_active)) {
            //- passive background loop
            color_window_was_active_last_loop = false;
            mouse_was_down = false;
            
            MSG message;
            auto status = GetMessageA(&message, 0, 0, 0);
            
            if(status == -1) {
                report_error("GetMessage error: %d\n", GetLastError());
                break;
            }
            
            DispatchMessageA(&message);
        }
        
        //- active foreground loop with UI
        
        TIMED_SCOPE("main loop");
        
        while(true) {
            MSG message;
            auto got_message = PeekMessage(&message, 0, 0, 0, PM_REMOVE);
            if(!got_message) break;
            
            DispatchMessageA(&message);
        }
        
        temp_arena_reset(&g_frame_arena);
        
        if(!g_running) break;
        TIMED_BLOCK_BEGIN("input");
        POINT screen_mouse_p = {};
        GetCursorPos(&screen_mouse_p);
        
        b32 mouse_down = (GetKeyState(VK_LBUTTON) & (1 << 15)) != 0;
        b32 mouse_transitioned = mouse_down != mouse_was_down;
        
        if(mouse_transitioned) mouse_was_down = mouse_down;
        
        Mouse_State mouse_state = Mouse_Nochange;
        if(mouse_transitioned) {
            mouse_state = mouse_down ? Mouse_Wentdown : Mouse_Wentup;
        }
        TIMED_BLOCK_END();
        TIMED_BLOCK_BEGIN("color window");
        if(g_color_window_active) {
            RECT color_client_rect = {};
            GetClientRect(g_color_window, &color_client_rect);
            
            Assert(color_client_rect.left == 0 && color_client_rect.top == 0);
            if(color_client_rect.right  != g_color_bitmap.width ||
               color_client_rect.bottom != g_color_bitmap.height)
            {
                resize_bitmap(&g_color_bitmap, g_color_window, color_client_rect);
            }
            
            Mouse mouse = {};
            mouse.state = mouse_state;
            if(color_window_was_active_last_loop) {
                auto color_window_mouse_p = screen_mouse_p;
                ScreenToClient(g_color_window, &color_window_mouse_p);
                
                mouse.p = point_to_v2i(color_window_mouse_p);
            }
            if(g_error_occurred) {
                do_error_menu(&g_error_menu, g_color_bitmap.dc, mouse);
                draw_base_color(g_color_bitmap.dc, g_error_colors, color_client_rect);
                draw_menu(      g_color_bitmap.dc, g_error_colors, g_error_menu);
            } else {
                do_color_menu(&g_color_menu, g_color_bitmap.dc, mouse);
                
                if(is_muted()) {
                    draw_base_color(g_color_bitmap.dc,   g_muted_colors, color_client_rect);
                    draw_menu(      g_color_bitmap.dc,   g_muted_colors, g_color_menu);
                } else {
                    draw_base_color(g_color_bitmap.dc, g_unmuted_colors, color_client_rect);
                    draw_menu(      g_color_bitmap.dc, g_unmuted_colors, g_color_menu);
                }
            }
        }
        TIMED_BLOCK_END();
        TIMED_BLOCK_BEGIN("settings window");
        if(g_settings_window_active) {
            RECT settings_client_rect = {};
            GetClientRect(g_settings_window, &settings_client_rect);
            
            Assert(settings_client_rect.left == 0 && settings_client_rect.top == 0);
            if(settings_client_rect.right  != g_settings_bitmap.width ||
               settings_client_rect.bottom != g_settings_bitmap.height)
            {
                resize_bitmap(&g_settings_bitmap, g_settings_window, settings_client_rect);
            }
            
            Mouse mouse = {};
            mouse.state = mouse_state;
            if(g_settings_window_active) {
                auto settings_window_mouse_p = screen_mouse_p;
                ScreenToClient(g_settings_window, &settings_window_mouse_p);
                
                mouse.p = point_to_v2i(settings_window_mouse_p);
            }
            TIMED_BLOCK_BEGIN("do_settings_menu");
            do_settings_menu(&g_settings_menu, g_settings_bitmap.dc, mouse);
            TIMED_BLOCK_END();
            TIMED_BLOCK_BEGIN("draw_base_color");
            draw_base_color(g_settings_bitmap.dc, g_settings_colors, settings_client_rect);
            TIMED_BLOCK_END();
            TIMED_BLOCK_BEGIN("draw_menu");
            draw_menu(      g_settings_bitmap.dc, g_settings_colors, g_settings_menu);
            TIMED_BLOCK_END();
        }
        
        TIMED_BLOCK_END();
        TIMED_BLOCK_BEGIN("color window display");
        if(g_color_window_active) {
            auto window_dc = GetDC(g_color_window);
            display_buffer_in_window(window_dc, g_color_bitmap.dc, g_color_bitmap.width, g_color_bitmap.height);
            ReleaseDC(g_color_window, window_dc);
        }
        TIMED_BLOCK_END();
        TIMED_BLOCK_BEGIN("settings window display");
        if(g_settings_window) {
            auto window_dc = GetDC(g_settings_window);
            display_buffer_in_window(window_dc, g_settings_bitmap.dc, g_settings_bitmap.width, g_settings_bitmap.height);
            ReleaseDC(g_settings_window, window_dc);
        }
        TIMED_BLOCK_END();
        TIMED_BLOCK_BEGIN("sleep");
        
        color_window_was_active_last_loop = g_color_window_active;
        
        //- frame timings and sleep
        
        auto end_counter = get_perf_counter();
        f32 seconds_elapsed_for_work = get_seconds_elapsed(frame_start_counter, end_counter);
        f32 seconds_elapsed_for_frame = seconds_elapsed_for_work;
        f32 frame_seconds_for_framerate = 1.0f/120.0f;
        s32 ms_slept = 0;
        if(seconds_elapsed_for_frame < frame_seconds_for_framerate) {
            if(sleep_is_granular) {
                s32 sleep_ms = cast(s32)(1000.0f * (frame_seconds_for_framerate - seconds_elapsed_for_frame - 0.001f));
                if(sleep_ms > 0) {
                    Sleep((DWORD)sleep_ms);
                    end_counter = get_perf_counter();
                }
            }
            while(true) {
                seconds_elapsed_for_frame = get_seconds_elapsed(frame_start_counter, end_counter);
                if(seconds_elapsed_for_frame >= frame_seconds_for_framerate) break;
                end_counter = get_perf_counter();
            }
        } else {
            // somehow managed to take more than a 60th of a second to do nothing
        }
        
        frame_start_counter = end_counter;
        
#if 0 // toggle frame time printout
        f32 fps = 1.0f / seconds_elapsed_for_frame;
        f32 msw = seconds_elapsed_for_work * 1000.0f;
        f32 msf = seconds_elapsed_for_frame * 1000.0f;
        
        frame_times[frame_index++] = msf;
        frame_index = frame_index % ArrayCount(frame_times);
        f64 avgms = 0;
        ForA(i, frame_times) {
            avgms += frame_times[i];
        }
        avgms /= ArrayCount(frame_times);
        f64 avgfps = 1000.0 / avgms;
        
        output_debug_string("%2.2fms/w, %2.2fms/f, %4.ffps, %2.2fams, %4.fafps\n", msw, msf, fps, avgms, avgfps);
        
#if _INTERNAL_PROFILE
        if(frame_index == ArrayCount(frame_times)) {
            if(!g_spall_should_profile) {
                g_spall_should_profile = true;
            } else {
                g_running = false;
            }
        }
#endif
#endif
        TIMED_BLOCK_END();
    }
    
    if(g_settings_window) on_settings_window_close();
    if(g_settings_file_path) {
        g_settings.color_window_layout = get_window_pos(g_color_window, "The window position may not have been saved if you changed it.");
        save_settings_to_file(g_settings_file_path);
    }
    
#if _INTERNAL_PROFILE
	spall_buffer_quit(&g_spall_ctx, &g_spall_buffer);
	spall_quit(&g_spall_ctx);
#endif
    
    return 0;
}

//~ Assert

#if _INTERNAL_BUILD
#include <dbghelp.h>

internal void
do_assert(char *expression, char *assert_file_path, int assert_line_number) {
    // NOTE(bo): using fixed array since Temp_Arena etc could have asserts
    char buffer[KB(4)];
    String_Builder sb = {};
    sb.data     = buffer;
    sb.capacity = ArrayCount(buffer);
    
    append_string(&sb,
                  "Assert failed: %s\n\n"
                  "In file: %s\n"
                  "On line: %d\n\n"
                  "Stack trace:\n", expression, assert_file_path, assert_line_number);
    
    auto machine_type = IMAGE_FILE_MACHINE_AMD64;
    auto process = GetCurrentProcess();
    auto thread = GetCurrentThread();
    if(SymInitialize(process, null, TRUE) == FALSE) {
        snprint(sb.data, cast(int)sb.capacity, "ERROR: SymInitialize failed. Can't show stack trace.\n");
        MessageBoxA(0, sb.data, "Show Muted error", 0);
        BadAssert(false);
        return;
    }
    SymSetOptions(SYMOPT_LOAD_LINES);
    
    CONTEXT context_record = {};
    context_record.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&context_record);
    
    STACKFRAME stack_frame = {};
    stack_frame.AddrPC.Mode      = AddrModeFlat;
    stack_frame.AddrFrame.Mode   = AddrModeFlat;
    stack_frame.AddrStack.Mode   = AddrModeFlat;
    stack_frame.AddrPC.Offset    = context_record.Rip;
    stack_frame.AddrFrame.Offset = context_record.Rbp;
    stack_frame.AddrStack.Offset = context_record.Rsp;
    
    while(StackWalk(machine_type, process, thread, &stack_frame, &context_record, null, SymFunctionTableAccess, SymGetModuleBase, null)) {
#if 0
        string module_string = sl("MODULE_NAME_ERROR");
        char module_string_buffer[MAX_PATH + 1];
        
        DWORD64 module_base = SymGetModuleBase(process, stack_frame.AddrPC.Offset);
        if(module_base) {
            auto length = GetModuleFileNameA((HINSTANCE)module_base, module_string_buffer, ArrayCount(module_string_buffer));
            if(length > 0) {
                // length is size of buffer and includes null if buffer was too small
                if(length == ArrayCount(module_string_buffer)) length -= 1;
                module_string = {length, module_string_buffer};
                ForIIR(i, cast(int)(module_string.count-1), 0) {
                    if(module_string.data[i] == '\\') {
                        module_string = advance(module_string, i + 1);
                        break;
                    }
                }
            }
        }
#endif
        
        string proc_name = sl("PROC_NAME_ERROR");
        
        // NOTE(bo): 'CHAR Name[1];' is the last member of SYMBOL_INFO
        const int bytes_reserved_for_symbol_name = 256;
        char symbol_info_buffer[SizeOfArrayStructThing(SYMBOL_INFO, Name, bytes_reserved_for_symbol_name)] = {};
        
        auto symbol_info = (SYMBOL_INFO *)symbol_info_buffer;
        symbol_info->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol_info->MaxNameLen = bytes_reserved_for_symbol_name;
        
        DWORD64 __ignored_offset = 0;
        if(SymFromAddr(process, stack_frame.AddrPC.Offset, &__ignored_offset, symbol_info)) {
            proc_name = {symbol_info->NameLen, symbol_info->Name};
        }
        
        char *file_path = "FILE_NAME_ERROR";
        s32 line_number = -1;
        
        IMAGEHLP_LINE file_and_line_info = {sizeof(IMAGEHLP_LINE)};
        DWORD __ignored_line_offset = 0;
        if(SymGetLineFromAddr(process, stack_frame.AddrPC.Offset, &__ignored_line_offset, &file_and_line_info)) {
            file_path = file_and_line_info.FileName;
            line_number = file_and_line_info.LineNumber;
        }
        
        append_string(&sb, "%s(%d): %S\n", file_path, line_number, proc_name);
    }
    Assert(sb.count <= sb.capacity);
    sb.data[sb.count] = 0;
    MessageBoxA(0, sb.data, "Show Muted error", 0);
}
#endif

//~ END

#if _VK_TESTING
#include "vk_testing.cpp"
#endif