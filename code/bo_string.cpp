/*
// wchar_t versions
#define BO_STRING_WIDE    1
// C-string functions
#define BO_STRING_C_STYLE 1
// s/sn/vsnprint for modified stb_sprintf.h
#define BO_STRING_SPRINT  1
*/

// @Volatile: must match defines at bottom of file
#if BO_STRING_WIDE
# define char wchar_t
# define string stringw
# define split_string split_stringw

# define null_stringw (stringw{})
# define null_string null_stringw
#else
# define null_string (string{})
#endif


struct string {
    s64 count;
    char *data;
};


#define AssertGoodString(a) Assert((a).count == 0 || ((a).count > 0 && (a).data))
#define AssertGoodString2(a, b) AssertGoodString(a); AssertGoodString(b)
#define ForS(i, s) ForI(i, 0, (s).count)


internal b32
operator ==(string a, string b) {
    AssertGoodString2(a, b);
    if(a.count != b.count) return false;
    //if(a.data == b.data)   return true;
    
    ForS(i, b) {
        if(a.data[i] != b.data[i]) return false;
    }
    
    return true;
}

internal inline b32
operator !=(string a, string b) {
    return !(a == b);
}

internal b32
_match(string a, string b) { //assert(a.count == b.count);
    ForS(i, b) {
        if(a.data[i] != b.data[i]) return false;
    }
    
    return true;
}

#define match(a, b) (((a).count == (b).count) && _match((a), (b)))
//#define match(a, b) (((a).count == (b).count) && (((a).data == (b).data) || _match((a), (b))))

#define inlined_strings_match_loop(a, b) \
ForS(i, (b)) { \
if((a).data[i] != (b).data[i]) return false; \
} \
return true;

#define _advance_one(s) { (s).data += 1; (s).count -= 1; }

internal inline void
advance(string *s, s64 delta) {
    Assert(s && s->data);
    if(delta > s->count) delta = s->count;
    s->data += delta;
    s->count -= delta;
    Assert(s->count >= 0);
}

must_receive internal inline string
advance(string s, s64 delta) {
    Assert(s.data);
    if(delta > s.count) delta = s.count;
    s.data += delta;
    s.count -= delta;
    Assert(s.count >= 0);
    return s;
}

internal b32
starts_with(string s, string prefix) {
    AssertGoodString2(s, prefix);
    
    if(s.count < prefix.count) return false;
    inlined_strings_match_loop(s, prefix);
}

internal b32
ends_with(string s, string suffix) {
    AssertGoodString2(s, suffix);
    
    if(s.count < suffix.count) return false;
    
    s = advance(s, s.count - suffix.count);
    Assert(s.count == suffix.count);
    
    inlined_strings_match_loop(s, suffix);
}

// used to be called starts_with_and_seek
internal b32
eat_prefix(string *sp, string prefix) {
    Assert(sp);
    if(!starts_with(*sp, prefix)) return false;
    
    advance(sp, prefix.count);
    return true;
}

internal b32
eat_suffix(string *sp, string suffix) {
    Assert(sp);
    if(!ends_with(*sp, suffix)) return false;
    
    sp->count -= suffix.count;
    return true;
}


#if 1
//
// TEXT SPECIFIC
//
// @Incomplete: remove crt


#if BO_STRING_WIDE
# define slw(s) (stringw{ArrayCount(L##s)-1, (L##s)})
#else // BO_STRING_WIDE
# define sl(s) (string{ArrayCount(s)-1, (s)})
//#define stringf(s) (cast(int)((s).count)), ((s).data) // only need stringf once
#endif // BO_STRING_WIDE


internal b32
case_insensitive_match(string a, string b) {
    AssertGoodString2(a, b);
    if(a.count != b.count) return false;
    if(a.data == b.data)   return true;
    
    ForS(i, b) {
        auto a_char = a.data[i];
        if(a_char >= 'A' && a_char <= 'Z') a_char += ('a'-'A');
        
        auto b_char = b.data[i];
        if(b_char >= 'A' && b_char <= 'Z') b_char += ('a'-'A');
        
        if(a_char != b_char) return false;
    }
    
    return true;
}

internal b32
starts_with_case_insensitive(string s, string prefix) {
    AssertGoodString2(s, prefix);
    
    if(s.count < prefix.count) return false;
    
    ForS(i, prefix) {
        auto s_char = s.data[i];
        if(s_char >= 'A' && s_char <= 'Z') s_char += ('a'-'A');
        
        auto prefix_char = prefix.data[i];
        if(prefix_char >= 'A' && prefix_char <= 'Z') prefix_char += ('a'-'A');
        
        if(s_char != prefix_char) return false;
    }
    
    return true;
}

// eat_u64 or consume_u64 instead?
internal u64
string_to_u64_and_substring(string *s_pointer) {
    Assert(s_pointer);
    u64 result = 0;
    string s = *s_pointer;
    AssertGoodString(s);
    
    ForS(i, s) {
        auto digit = s.data[i];
        if(digit < '0' || digit > '9') {
            s.count = i;
            break;
        }
        result *= 10;
        result += digit - '0';
    }
    *s_pointer = s;
    return result;
}

internal u64
string_to_u64(string s) {
    AssertGoodString(s);
    u64 result = 0;
    ForS(i, s) {
        auto digit = s.data[i];
        if(digit < '0' || digit > '9') break;
        result *= 10;
        result += digit - '0';
    }
    return result;
}

internal inline u32
string_to_u32(string s) {
    u64 result_u64 = string_to_u64(s);
    Assert(result_u64 <= U32_MAX); // clamp it instead?
    u32 result = cast(u32)result_u64;
    return result;
}

internal s64
string_to_s64(string s) {
    AssertGoodString(s);
    b32 negative = false;
    if(s.count > 0 && s.data[0] == '-') {
        negative = true;
        s = advance(s, 1);
    } else if(s.count > 0 && s.data[0] == '+') {
        s = advance(s, 1);
    }
    u64 result_u64 = string_to_u64(s);
    s64 result;
    if(negative) {
        Assert(result_u64 <= (u64)S64_MAX+1); // clamp it instead?
        result = -cast(s64)result_u64;
    } else {
        Assert(result_u64 <= S64_MAX); // clamp it instead?
        result =  cast(s64)result_u64;
    }
    return result;
}

internal inline s32
string_to_s32(string s) {
    s64 result_s64 = string_to_s64(s);
    Assert(result_s64 >= S32_MIN && result_s64 <= S32_MAX); // clamp it instead?
    s32 result = cast(s32)result_s64;
    return result;
}

internal b32
string_to_b32(string s) {
    AssertGoodString(s);
    
    if(s == sl("true")) return true;
    return false;
}

internal s64
index_of_first_char(string s, char c) {
    AssertGoodString(s);
    
    ForS(i, s) {
        if(s.data[i] == c) {
            return i;
        }
    }
    return -1;
}

internal s64
index_of_last_char(string s, char c) {
    AssertGoodString(s);
    
    for(s64 i = s.count-1; i >= 0; i -= 1) {
        if(s.data[i] == c) {
            return i;
        }
    }
    return -1;
}

internal string
seek_to_char(string s, char c) {
    auto i = index_of_first_char(s, c);
    if(i >= 0) return advance(s, i);
    return null_string;
}

internal string
seek_past_char(string s, char c) {
    s = seek_to_char(s, c);
    if(s.count > 0) {
        return advance(s, 1);
    }
    return null_string;
}

internal string
seek_to_last_char(string s, char c) {
    AssertGoodString(s);
    
    for(s64 i = s.count-1; i >= 0; i -= 1) {
        if(s.data[i] == c) {
            return advance(s, i);
        }
    }
    return null_string;
}

internal string
seek_past_last_char(string s, char c) {
    s = seek_to_last_char(s, c);
    if(s.count > 0) {
        return advance(s, 1);
    }
    return null_string;
}

struct split_string {
    string lhs, rhs;
};

#define ss_decl(  a, b, expr) string a, b; { auto __split_ = (expr); a = __split_.lhs; b = __split_.rhs; }
#define ss_decl_1(a, b, expr) string a;    { auto __split_ = (expr); a = __split_.lhs; b = __split_.rhs; }
#define ss_assign(a, b, expr)              { auto __split_ = (expr); a = __split_.lhs; b = __split_.rhs; }

internal split_string
break_by_spaces(string s) {
    AssertGoodString(s);
    split_string result = {};
    if(s.count > 0) {
        Assert(s.data);
        result.lhs = s;
        result.rhs = seek_to_char(s, ' ');
        
        result.lhs.count -= result.rhs.count;
        while(result.rhs.count > 0 && result.rhs.data[0] == ' ') {
            result.rhs = advance(result.rhs, 1);
        }
    }
    return result;
}

// if didn't find char:  lhs = s, rhs = null_string
internal auto
break_by_char(string s, char c) {
    AssertGoodString(s);
    split_string result = {};
    if(s.count > 0) {
        Assert(s.data);
        result.lhs = s;
        result.rhs = seek_to_char(s, c);
        
        if(result.rhs.count > 0 && result.rhs.data[0] == c) {
            result.lhs.count -= result.rhs.count;
            result.rhs = advance(result.rhs, 1);
        }
    }
    return result;
}

// if didn't find char:  lhs = s, rhs = null_string
internal inline auto
break_by_char(string s, char c, string *lhs, string *rhs) {
    auto result = break_by_char(s, c);
    *lhs = result.lhs;
    *rhs = result.rhs;
}

// if didn't find char:  lhs = s, rhs = null_string
internal split_string
break_by_last_char(string s, char c) {
    AssertGoodString(s);
    split_string result = {};
    if(s.count > 0) {
        Assert(s.data);
        result.lhs = s;
        result.rhs = seek_to_last_char(s, c);
        
        if(result.rhs.count > 0 && result.rhs.data[0] == c) {
            result.lhs.count -= result.rhs.count;
            result.rhs = advance(result.rhs, 1);
        }
    }
    return result;
}


// didn't find '\n': line = s, rest = null_string
// found '\n': line = next line without (\r)\n, s = everything after '\n'
internal inline split_string
break_by_newline(string s) {
    auto line_and_rest = break_by_char(s, '\n');
    if(line_and_rest.lhs.count > 0 && line_and_rest.lhs.data[line_and_rest.lhs.count-1] == '\r') {
        line_and_rest.lhs.count -= 1;
    }
    
    return line_and_rest;
}

// didn't find '\n': line = *s, *s = null_string
// found '\n': line = next line without (\r)\n, s = everything after '\n'
must_receive internal inline string
consume_next_line(string *s) {
    Assert(s);
    auto line_and_rest = break_by_newline(*s);
    *s = line_and_rest.rhs;
    return line_and_rest.lhs;
}



internal inline b32
is_alpha(char c) {
    b32 result = ((c >= 'a' && c <= 'z') ||
                  (c >= 'A' && c <= 'Z'));
    return result;
}


internal inline b32
is_numeric(char c) {
    b32 result = (c >= '0' && c <= '9');
    return result;
}

// @Incomplete: change to is_token_char?
internal inline b32
is_alphanumeric_or_underscore(char c) {
    b32 result = (is_alpha(c) || is_numeric(c) || c == '_');
    return result;
}

internal inline b32
is_whitespace(char c) {
    switch(c) {
        case ' ':
        case '\n':
        case '\r':
        case '\t':
        return true;
    }
    return false;
}

internal string
strip_surrounding_whitespace(string s) {
    AssertGoodString(s);
    
    while(s.count > 0 && is_whitespace(s.data[0])) {
        _advance_one(s);
    }
    
    while(s.count > 0 && is_whitespace(s.data[s.count-1])) {
        s.count -= 1;
    }
    
    return s;
}

#if BO_STRING_C_STYLE

internal s64
c_string_length(char *c_string) {
    Assert(c_string);
    auto scan = c_string;
    while(*scan) scan += 1;
    // @Speed
    s64 result = scan - c_string;
    return result;
}

internal b32
starts_with(string a, const char * const b) {
    AssertGoodString(a);
    Assert(b);
    
    ForS(i, a) {
        if(b[i] == '\0' || a.data[i] != b[i]) return false;
    }
    
    return true;
}

internal inline b32
operator ==(string a, const char * const b) {
    if(!starts_with(a, b)) return false;
    return (b[a.count] == '\0');
}

internal inline b32
operator !=(string a, const char * const b) {
    return !(a == b);
}

internal inline b32
operator ==(const char * const b, string a) {
    return a == b;
}

internal inline b32
operator !=(const char * const b, string a) {
    return !(a == b);
}

internal inline string
c_style_to_string(char *c_string) {
    Assert(c_string);
    string result;
    result.data = c_string;
    result.count = c_string_length(c_string);
    return result;
}

#endif // BO_STRING_C_STYLE

#endif // TEXT SPECIFIC

// @Volatile: must match defines at top of file
#if BO_STRING_WIDE

# undef BO_STRING_WIDE
# undef char
# undef string
# undef split_string
# undef null_string
# include "bo_string.cpp"

#else // BO_STRING_WIDE

# if BO_STRING_SPRINT
#  define STB_SPRINTF_STATIC
#  define STB_SPRINTF_IMPLEMENTATION
#  include "stb_sprintf_modified.h"
#  define sprint stbsp_sprintf
#  define snprint stbsp_snprintf
#  define vsnprint stbsp_vsnprintf
# endif // BO_STRING_SPRINT

#endif // BO_STRING_WIDE