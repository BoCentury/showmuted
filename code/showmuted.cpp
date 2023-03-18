//~ START

#define BO_JAI_MUST_RECEIVE 1
#include "bo_jai.cpp"

#if _INTERNAL_BUILD

internal void do_assert(char *expression, char *file_path, int line_number);
#define ___Stringify(a) #a
#define Assert(e) if(!(e)) { do_assert(___Stringify(e), __FILE__, __LINE__); __debugbreak(); }
#define AssertM(e, m) if(!(e)) { do_assert((m), __FILE__, __LINE__); __debugbreak(); }
#define BadAssert(expression) if(!(expression)) { *cast(int *)(0) = 0; }
#define XDebug(...) __VA_ARGS__

#else // _INTERNAL_BUILD
#define Assert(...) {}
#define AssertM(...) {}
#define BadAssert(...) {}
#define XDebug(...)
#endif // _INTERNAL_BUILD

#define InvalidDefaultCase Default: AssertM(false, "Invalid default case")
#define InvalidCodePath           AssertM(false, "Invalid code path")
#define InvalidCodePathM(message) AssertM(false, "Invalid code path: " message)

#define BO_STRING_WIDE    0
#define BO_STRING_C_STYLE 1
#define BO_STRING_SPRINT  0
#include "bo_string.cpp"

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf_modified.h"
#define snprint stbsp_snprintf


//~ Misc

#define OffsetOf(T, member) (cast(umm)&(((T *)0)->member))

// this is for allocing a struct that has a last member of 'member[1]'
#define SizeOfArrayStructThing(T, member, count) \
(OffsetOf(T, member) + (count)*sizeof((((T *)0)->member)[0]))

#define KB(x) ((x) << 10)
#define MB(x) ((x) << 20)
#define GB(x) ((x) << 30)
#define TB(x) (((s64)x) << 40)

#define Max(a, b) (((a) >= (b)) ? (a) : (b))
#define Min(a, b) (((a) <= (b)) ? (a) : (b))

//#define SetIfMax(max, a) ((max) = Max(max, a))
//#define SetIfMin(min, a) ((min) = Min(min, a))

#define SetIfMax(max, a) if((max) < (a)) { (max) = (a); }
#define SetIfMin(min, a) if((min) > (a)) { (min) = (a); }

#if _RELEASE_BUILD
#define output_debug_string(...) {}
#else
internal void
output_debug_string(char *format, ...);
#endif

internal inline s32
round_f32_s32(f32 a) {
    s32 result = cast(s32)(a + 0.5f);
    return result;
}

internal inline f32
round_f32_f32(f32 a) {
    f32 result = a + 0.5f;
    return result;
}

internal inline s32
clamp(s32 value, s32 min, s32 max) {
    if(value < min) {
        value = min;
    } else if(value > max) {
        value = max;
    }
    return value;
}

internal inline f32
lerp(f32 t, f32 a, f32 b) {
    return a + t*(b - a);
}

internal inline f32
unlerp(f32 t, f32 a, f32 b) {
    auto range = b - a;
    if(range == 0) return 0;
    return (t - a) / range;
}

internal inline f32
linear_remap(f32 a, f32 a_min, f32 a_max, f32 b_min, f32 b_max) {
    return lerp(unlerp(a, a_min, a_max), b_min, b_max);
}

internal inline s32
linear_remap_s32(s32 a, s32 a_min, s32 a_max, s32 b_min, s32 b_max) {
    return round_f32_s32(linear_remap(cast(f32)a,
                                      cast(f32)a_min, cast(f32)a_max,
                                      cast(f32)b_min, cast(f32)b_max));
}


struct string_u8 {
    s64 count;
    u8 *data;
};

struct string_u8_s32 {
    s32 count;
    u8 *data;
};

struct string_s32 {
    s32 count;
    char *data;
};

internal inline string
to_string(string_u8 s){
    return string{s.count, cast(char *)s.data};
}

internal inline string
to_string(string_u8_s32 s){
    Assert(0 <= s.count && s.count <= S32_MAX);
    return string{s.count, cast(char *)s.data};
}

internal inline string
to_string(string_s32 s){
    return string{s.count, s.data};
}

internal inline string_s32
to_string_s32(string_u8 s){
    Assert(0 <= s.count && s.count <= S32_MAX);
    return string_s32{cast(s32)s.count, cast(char *)s.data};
}

internal s64
round_up_s64(s64 x, s64 b){
    x += b - 1;
    x -= x % b;
    return x;
}

//~ Temporary memory arena

struct Temp_Arena_Chunk {
    s64 size;
    s64 occupied; // maybe use this as an 'unreferred' counter for free_chunks?
    u8 *data;
    Temp_Arena_Chunk *next;
};

struct Temp_Arena {
    Temp_Arena_Chunk *chunks;
    Temp_Arena_Chunk *free_chunks;
    s64 alignment;
    s64 chunk_size;
    
    XDebug(s64 high_water_mark);
};

internal inline void
temp_arena_reset(Temp_Arena *arena) {
    if(arena->chunks) {
        XDebug(s64 total_occupied = 0);
        while(arena->chunks->next) {
            auto chunk = arena->chunks;
            arena->chunks = arena->chunks->next;
            
            chunk->occupied = 0;
            chunk->next = arena->free_chunks;
            arena->free_chunks = chunk;
            
            XDebug(total_occupied += arena->chunks->occupied);
        }
        XDebug(total_occupied += arena->chunks->occupied;
               if(arena->high_water_mark < total_occupied) {
                   arena->high_water_mark = total_occupied;
                   output_debug_string("\nhigh_water_mark: %d\n\n", total_occupied);
               });
        arena->chunks->occupied = 0;
        local_persist int asdf = 0;
    }
}

internal inline string_u8
XXX_temp_arena__get_free_memory(Temp_Arena *arena) {
    string_u8 s;
    s.count = arena->chunks->size - arena->chunks->occupied;
    s.data  = arena->chunks->data + arena->chunks->occupied;
    return s;
}

internal inline string_s32
XXX_temp_arena__get_free_memory_s32(Temp_Arena *arena) {
    return to_string_s32(XXX_temp_arena__get_free_memory(arena));
}

internal inline void
XXX_temp_arena__commit_used_bytes(Temp_Arena *arena, s64 bytes_used) {
    Assert(bytes_used >= 0 && bytes_used + arena->chunks->occupied <= arena->chunks->size);
    bytes_used = round_up_s64(bytes_used, arena->alignment);
    arena->chunks->occupied += bytes_used;
}

must_receive internal Temp_Arena_Chunk *
temp_arena__push_chunk(Temp_Arena *arena, s64 requested_size) {
    Temp_Arena_Chunk *chunk = arena->free_chunks;
    Temp_Arena_Chunk *prev = null;
    while(chunk) {
        if(chunk->size >= requested_size) {
            if(prev) {
                prev->next = chunk->next;
            } else {
                arena->free_chunks = chunk->next;
            }
            break;
        }
        prev = chunk;
        chunk = chunk->next;
    }
    
    if(!chunk) {
        s64 size_needed = requested_size + sizeof(Temp_Arena_Chunk);
        s64 full_size = Max(size_needed, arena->chunk_size);
        full_size = round_up_s64(full_size, KB(4));
        s64 body_size = full_size - sizeof(Temp_Arena_Chunk);
        
        u8 *data = cast(u8 *)malloc(full_size);
        Assert(data);
        
        chunk = cast(Temp_Arena_Chunk *)data;
        chunk->data = cast(u8 *)(chunk + 1);
        chunk->occupied = 0;
        chunk->size = body_size;
    }
    
    Assert(chunk->occupied == 0);
    chunk->next = arena->chunks;
    arena->chunks = chunk;
    return chunk;
}

must_receive internal inline void *
temp_arena_alloc(Temp_Arena *arena, s64 bytes) {
    Assert(bytes >= 0);
    void *result = null;
    
    if(bytes > 0) {
        bytes = round_up_s64(bytes, arena->alignment);
        
        Temp_Arena_Chunk *chunk = arena->chunks;
        if(!chunk || chunk->occupied + bytes > chunk->size) {
            chunk = temp_arena__push_chunk(arena, bytes);
        }
        
        result = chunk->data + chunk->occupied;
        chunk->occupied += bytes;
    }
    return result;
}

must_receive internal string
temp_arena_vprint(Temp_Arena *arena, char *format, va_list args) {
    // @Volatile: don't alloc from temp arena before the commit
    string_s32 free = XXX_temp_arena__get_free_memory_s32(arena);
    char *data = free.data;
    
    // length excludes zero-terminator
    int length = stbsp_vsnprintf(data, free.count, format, args);
    if(length + 1 <= free.count) {
        XXX_temp_arena__commit_used_bytes(arena, length + 1); // + 1 for zero-terminator
    } else {
        data = cast(char *)temp_arena_alloc(arena, length + 1);
        length = stbsp_vsnprintf(data, length + 1, format, args);
    }
    Assert(data[length] == '\0');
    
    return string{length, data};
}

must_receive internal string
temp_arena_print(Temp_Arena *arena, char *format, ...) {
    va_list args;
    va_start(args, format);
    auto result = temp_arena_vprint(arena, format, args);
    va_end(args);
    return result;
}

#define temp_arena_alloc_array(arena, T, count) \
(cast(T*)temp_arena_alloc((arena), sizeof(T)*(count)))

// NOTE(bo): The goal of this frame arena is to hold all scratch/temporary allocations
// you might want in a frame wihin a single chunk. The high_water_mark member is there
// for you to be able to see the max amount of bytes allocated in a frame. You can use
// this to adjust chunk_size such that the frame arena doesn't have to allocate
// additional chunks in the majority of cases.
global Temp_Arena g_frame_arena;

//- convenience functions for frame arena use

must_receive internal inline void *
talloc(s64 bytes) {
    return temp_arena_alloc(&g_frame_arena, bytes);
}

#define talloc_array(T, count) temp_arena_alloc_array(&g_frame_arena, T, (count))

must_receive internal inline string
vtprint(char *format, va_list args) {
    return temp_arena_vprint(&g_frame_arena, format, args);
}

must_receive internal string
tprint(char *format, ...) {
    va_list args;
    va_start(args, format);
    auto result = temp_arena_vprint(&g_frame_arena, format, args);
    va_end(args);
    return result;
}

//~ Vector

struct v2i {
    s32 x, y;
};

internal inline v2i
operator+(v2i a, v2i b) {
    v2i result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

internal inline v2i
operator+(v2i a, s32 b) {
    v2i result;
    result.x = a.x + b;
    result.y = a.y + b;
    return result;
}

internal inline v2i
operator+(s32 b, v2i a) {
    v2i result;
    result.x = a.x + b;
    result.y = a.y + b;
    return result;
}

internal inline v2i
operator-(v2i a, v2i b) {
    v2i result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

internal inline v2i
operator*(v2i a, s32 b) {
    v2i result;
    result.x = a.x * b;
    result.y = a.y * b;
    return result;
}


internal inline v2i
operator*(s32 b, v2i a) {
    v2i result;
    result.x = a.x * b;
    result.y = a.y * b;
    return result;
}


union v3i {
    struct {
        s32 x, y, z;
    };
    struct {
        s32 r, g, b;
    };
};


internal inline b32
operator==(v3i a, v3i b) {
    b32 result = (a.x == b.x &&
                  a.y == b.y &&
                  a.z == b.z);
    return result;
}

internal inline b32
operator!=(v3i a, v3i b) {
    return !(a == b);
}

//~ Colors

struct Rgbi {
    s32 r, g, b;
};

struct Hsvi {
    s32 h, s, v;
};

internal inline b32
operator==(Hsvi a, Hsvi b) {
    b32 result = (a.h == b.h &&
                  a.s == b.s &&
                  a.v == b.v);
    return result;
}

internal inline b32
operator!=(Hsvi a, Hsvi b) {
    return !(a == b);
}

//~ END
