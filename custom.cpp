// features
// TODO(jesper): jump location history to go back/forward
// TODO(jesper): @performance improve start-up performance of large projects, and subsequent runtime memory usage
// TODO(jesper): command history
// TODO(jesper): switch between pascalCase, CamelCase, snake_case
// TODO(jesper): remove duplicate lines
// TODO(jesper): seek matching scope need to take into account scope characters inside strings and character literals
// fleury has a version of this function that uses the lexer tokens, but it doesn't seek to the nearest one like mine does
// if the cursor isn't currently on a brace. I do think the lexer token approach is the correct one, with a text-only fallback
// for non-code files
// TODO(jesper): grab the #if <stuff> and automatically append it to the corresponding #endif. Figure out how to handle else and elseif for that
// TODO(jesper): registers for clipboard and macros. Current thinking: "c2" records macro into 2nd register, "v2" replays that macro. "vv" replays from last used register. "cc" to record into first/default register? Similar for copy and paste, though I'm not sure how I jive with having to do "pp" and "yy", which is the 99% operation. Maybe something like ctrl modifier to explicitly set register? or alt?

// bugs
// TODO(jesper): @indent figure out how to support: static WINAPI long_return_type\nproc_name(params) {} 
// TODO(jesper): with a small window, writing long comments and getting line wraps like these sometimes ends up moving the view to the right and not correctly setting it back to 0 when scrolling back to the start of line. It shouldn't scroll to begin with when line wrapping.

#include "4coder_default_include.cpp"

CUSTOM_ID(colors, defcolor_comment_note);
CUSTOM_ID(colors, defcolor_comment_todo);
CUSTOM_ID(colors, defcolor_cursor_background);
CUSTOM_ID(colors, defcolor_cursor_insert);
CUSTOM_ID(colors, defcolor_highlight_cursor_line_recording);
CUSTOM_ID(colors, defcolor_jump_buffer_background);
CUSTOM_ID(colors, defcolor_jump_buffer_background_active);
CUSTOM_ID(colors, defcolor_jump_buffer_background_cmd_executing);
CUSTOM_ID(colors, defcolor_jump_buffer_background_cmd_fail);
CUSTOM_ID(colors, defcolor_jump_buffer_foreground);
CUSTOM_ID(colors, defcolor_jump_buffer_foreground_active);
CUSTOM_ID(colors, defcolor_jump_buffer_sticky);

CUSTOM_ID(attachment, buffer_preferred_column);

#define I64_MAX (i64)(0x7fffffffffffffff)

#if defined(_WIN32)
#define FILE_SEP "\\"
#define FILE_SEP_C '\\'
#elif defined(__linux__)
#define FILE_SEP "/"
#define FILE_SEP_C '/'
#else
# error Slash not set for this platform.
#endif

#define heap_alloc_arr(heap, Type, count) (Type*)heap_allocate(heap, count * sizeof(Type))
#define heap_realloc_arr(heap, Type, ptr, old_count, new_count) (Type*)heap_realloc(heap, ptr, old_count*sizeof(Type), new_count*sizeof(Type))
#define swap(a, b) do { auto glue(tmp_a_, __LINE__) = a; a = b; b = glue(tmp_a_, __LINE__); } while(0)

typedef char CHAR;
typedef CHAR *LPSTR;

extern "C" void * memmove ( void * destination, const void * source, size_t num );
extern "C" void* memset( void* dest, int ch, size_t count );
extern "C" void* memcpy( void* dest, const void* src, size_t count );
extern "C" int strncmp ( const char * str1, const char * str2, size_t num );
extern "C" int toupper(int c);
extern "C" size_t strlen ( const char * str );
extern "C" LPSTR GetCommandLineA();

static void custom_setup_necessary_bindings(Mapping *mapping);
static void custom_setup_default_bindings(Mapping *mapping);
static void custom_setup_fonts(Application_Links *app);

static bool has_dirty_buffers(Application_Links *app);
String_Const_u8 shorten_path(String_Const_u8 path, String_Const_u8 root);
static void string_mod_lower(String_Const_u8 dst, String_Const_u8 src);
void* heap_realloc(Heap *heap, void *ptr, u64 old_size, u64 new_size);

function b32 def_get_config_b32(String_ID key, b32 default_value);

bool operator>=(String_Const_u8 lhs, String_Const_u8 rhs)
{
    int r = strncmp((char*)lhs.str, (char*)rhs.str, Min(lhs.size, rhs.size));
    return r >= 0 ? true : false;
}

bool operator<=(String_Const_u8 lhs, String_Const_u8 rhs)
{
    int r = strncmp((char*)lhs.str, (char*)rhs.str, Min(lhs.size, rhs.size));
    return r <= 0 ? true : false;
}

#include "custom_fuzzy.cpp"
#include "custom_project.cpp"
#include "custom_lister.cpp"
#include "custom_vertical_scope_annotations.cpp"

#include "4coder_fleury/4coder_fleury_ubiquitous.h"
#include "4coder_fleury/4coder_fleury_audio.h"
#include "4coder_fleury/4coder_fleury_lang.h"
#include "4coder_fleury/4coder_fleury_index.h"
#include "4coder_fleury/4coder_fleury_colors.h"
#include "4coder_fleury/4coder_fleury_render_helpers.h"
#include "4coder_fleury/4coder_fleury_brace.h"
#include "4coder_fleury/4coder_fleury_error_annotations.h"
#include "4coder_fleury/4coder_fleury_divider_comments.h"
#include "4coder_fleury/4coder_fleury_power_mode.h"
#include "4coder_fleury/4coder_fleury_cursor.h"
#include "4coder_fleury/4coder_fleury_plot.h"
#include "4coder_fleury/4coder_fleury_calc.h"
#include "4coder_fleury/4coder_fleury_pos_context_tooltips.h"
#include "4coder_fleury/4coder_fleury_code_peek.h"
#include "4coder_fleury/4coder_fleury_recent_files.h"

#include "4coder_fleury/4coder_fleury_ubiquitous.cpp"
#include "4coder_fleury/4coder_fleury_audio.cpp"
#include "4coder_fleury/4coder_fleury_lang.cpp"
#include "4coder_fleury/4coder_fleury_index.cpp"
#include "4coder_fleury/4coder_fleury_colors.cpp"
#include "4coder_fleury/4coder_fleury_render_helpers.cpp"
#include "4coder_fleury/4coder_fleury_brace.cpp"
#include "4coder_fleury/4coder_fleury_error_annotations.cpp"
#include "4coder_fleury/4coder_fleury_divider_comments.cpp"
#include "4coder_fleury/4coder_fleury_power_mode.cpp"
#include "4coder_fleury/4coder_fleury_cursor.cpp"
#include "4coder_fleury/4coder_fleury_plot.cpp"
#include "4coder_fleury/4coder_fleury_calc.cpp"
#include "4coder_fleury/4coder_fleury_pos_context_tooltips.cpp"
#include "4coder_fleury/4coder_fleury_code_peek.cpp"
#include "4coder_fleury/4coder_fleury_recent_files.cpp"

#if !defined(META_PASS)
#include "generated/managed_id_metadata.cpp"
#endif

function b32 def_get_config_b32(String_ID key, b32 default_value)
{
    Variable_Handle var = def_get_config_var(key);
    String_ID val = vars_string_id_from_var(var);
    b32 result = default_value;
    if (val != 0) result = val != vars_save_string_lit("false");
    return result;
}

void* heap_realloc(Heap *heap, void *ptr, u64 old_size, u64 new_size)
{
    void *nptr = heap_allocate(heap, new_size);
    memcpy(nptr, ptr, old_size);
    heap_free(heap, ptr);
    return nptr;
}

function void F4_DoFullLex_ASYNC_Inner(Async_Context *actx, Buffer_ID buffer_id)
{
    Application_Links *app = actx->app;
    ProfileScope(app, "[F4] Async Lex");
    Scratch_Block scratch(app);

    String_Const_u8 contents = {};
    {
        ProfileBlock(app, "[F4] Async Lex Contents (before mutex)");
        acquire_global_frame_mutex(app);
        ProfileBlock(app, "[F4] Async Lex Contents (after mutex)");
        contents = push_whole_buffer(app, scratch, buffer_id);
        release_global_frame_mutex(app);
    }

    i32 limit_factor = 10000;

    Token_List list = {};
    b32 canceled = false;

    F4_Language *language = F4_LanguageFromBuffer(app, buffer_id);

    // NOTE(rjf): Fall back to C++ if we don't have a proper language.
    if(language == 0)
    {
        language = F4_LanguageFromString(S8Lit("cpp"));
    }

    if(language != 0)
    {
        void *lexing_state = push_array_zero(scratch, u8, language->lex_state_size);
        language->LexInit(lexing_state, contents);
        for(;;)
        {
            ProfileBlock(app, "[F4] Async Lex Block");
            if(language->LexFullInput(scratch, &list, lexing_state, limit_factor))
            {
                break;
            }
            if(async_check_canceled(actx))
            {
                canceled = true;
                break;
            }
        }
    }

    if(!canceled)
    {
        ProfileBlock(app, "[F4] Async Lex Save Results (before mutex)");
        acquire_global_frame_mutex(app);
        ProfileBlock(app, "[F4] Async Lex Save Results (after mutex)");
        Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
        if(scope != 0)
        {
            Base_Allocator *allocator = managed_scope_allocator(app, scope);
            Token_Array *tokens_ptr = scope_attachment(app, scope, attachment_tokens, Token_Array);
            base_free(allocator, tokens_ptr->tokens);
            Token_Array tokens = {};
            tokens.tokens = base_array(allocator, Token, list.total_count);
            tokens.count = list.total_count;
            tokens.max = list.total_count;
            token_fill_memory_from_list(tokens.tokens, &list);
            block_copy_struct(tokens_ptr, &tokens);
        }
        buffer_mark_as_modified(buffer_id);
        release_global_frame_mutex(app);
    }
}

function void F4_DoFullLex_ASYNC(Async_Context *actx, String_Const_u8 data)
{
    if(data.size == sizeof(Buffer_ID))
    {
        Buffer_ID buffer = *(Buffer_ID*)data.str;
        F4_DoFullLex_ASYNC_Inner(actx, buffer);
    }
}

static bool has_dirty_buffers(Application_Links *app)
{
    for (Buffer_ID buffer = get_buffer_next(app, 0, Access_Always);
         buffer != 0;
         buffer = get_buffer_next(app, buffer, Access_Always))
    {
        Dirty_State dirty = buffer_get_dirty_state(app, buffer);
        if (HasFlag(dirty, DirtyState_UnsavedChanges)){
            return true;
        }
    }

    return false;
}

bool should_treat_as_code(Application_Links *app, Buffer_ID buffer)
{
    Scratch_Block scratch(app);

    String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer);
    if (file_name.size > 0){
        String_Const_u8 treat_as_code_string = def_get_config_string(scratch, vars_save_string_lit("treat_as_code"));
        String_Const_u8_Array extensions = parse_extension_line_to_extension_list(app, scratch, treat_as_code_string);
        String_Const_u8 ext = string_file_extension(file_name);
        
        for(i32 i = 0; i < extensions.count; ++i) {
            if(string_match(ext, extensions.strings[i])) {
                return true;
            }
        }
    }

    F4_Language *language = F4_LanguageFromBuffer(app, buffer);
    if (language) return true;

    return false;
}

static void string_mod_lower(String_Const_u8 dst, String_Const_u8 src)
{
    for (u64 i = 0; i < src.size; i += 1){
        dst.str[i] = character_to_lower(src.str[i]);
    }
}

static void quicksort_desc(String_Const_u8 *strs, i32 l, i32 r)
{
    if (l >= r) {
        return;
    }

    String_Const_u8 pivot = strs[r];

    i32 cnt = l;

    for (i32 i = l; i <= r; i++) {
        if (strs[i] >= pivot) {
            swap(strs[cnt], strs[i]);
            cnt++;
        }
    }

    quicksort_desc(strs, l, cnt-2);
    quicksort_desc(strs, cnt, r);
}

static void quicksort_asc(String_Const_u8 *strs, i32 l, i32 r)
{
    if (l >= r) {
        return;
    }

    String_Const_u8 pivot = strs[r];

    i32 cnt = l;

    for (i32 i = l; i <= r; i++) {
        if (strs[i] <= pivot) {
            swap(strs[cnt], strs[i]);
            cnt++;
        }
    }

    quicksort_asc(strs, l, cnt-2);
    quicksort_asc(strs, cnt, r);
}


// TODO(rjf): This is only being used to check if a font file exists because
// there's a bug in try_create_new_face that crashes the program if a font is
// not found. This function is only necessary until that is fixed.
function b32 IsFileReadable(String_Const_u8 path)
{
    b32 result = 0;
    FILE *file = fopen((char *)path.str, "r");
    if(file)
    {
        result = 1;
        fclose(file);
    }
    return result;
}


#define CMD_L(body) [](Application_Links *app) { body; }

#define BIND_MOTION(func, key) \
    Bind([](Application_Links *app) \
         {\
             set_mark(app);\
             func(app);\
         }, key);\
    Bind([](Application_Links *app)\
         {\
             func(app);\
         }, key, KeyCode_Shift)

enum ModalMode {
    MODAL_MODE_INSERT,
    MODAL_MODE_EDIT,
};

enum JumpBufferCmdType {
    JUMP_BUFFER_CMD_NONE,
    JUMP_BUFFER_CMD_SYSTEM_PROC,
    JUMP_BUFFER_CMD_BUFFER_SEARCH,
    JUMP_BUFFER_CMD_GLOBAL_SEARCH,
};

constexpr i32 JUMP_BUFFER_COUNT = 8;

struct JumpBufferCmd {
    union {
        struct {
            String_Const_u8 path;
            String_Const_u8 cmd;
            Child_Process_ID process;
            i32 status;
            bool has_exit;
        } system;
        struct {
            Buffer_ID buffer;
            String_Const_u8 query;
        } buffer_search;
        struct {
            String_Const_u8 query;
        } global_search;
    };

    char label[20];
    i32 label_size;

    JumpBufferCmdType type;
    Buffer_ID buffer_id;
    bool sticky = false;
};

static JumpBufferCmd g_jump_buffers[JUMP_BUFFER_COUNT];
static i32 g_active_jump_buffer = 0;
static View_ID g_jump_view = -1;

static ModalMode g_mode = MODAL_MODE_EDIT;

// NOTE(jesper): I really don't understand why I can't use view_get/set_preferred_x here. 
// move_vertical_pixels looks _exactly_ like set_cursor_column_to_preferred, except it gets
// its p.x from get_preferred_x. Doing view_set_preferred_x in this function doesn't solve
// that, which means it gets reset to something else somewhere else. And I've no idea where
// or why.
// If this wasn't an issue, I wouldn't have to do this in the first place. I'd only have to
// call view_set_preferred_x in my motion commands, and it'd work everywhere and be fine.
static void reset_preferred_column(Application_Links *app)
{
    View_ID view = get_active_view(app, Access_Always);

    i64 cursor_pos = view_get_cursor_pos(app, view);
    Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(cursor_pos));

    Vec2_f32 p = view_relative_xy_of_pos(app, view, cursor.line, cursor.pos);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    f32 *preferred_column = scope_attachment(app, scope, buffer_preferred_column, f32);
    *preferred_column = p.x;
}

static void set_cursor_column_to_preferred(Application_Links *app)
{
    View_ID view = get_active_view(app, Access_Always);

    i64 pos = view_get_cursor_pos(app, view);
    Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(pos));
    Rect_f32 r = view_padded_box_of_pos(app, view, cursor.line, pos);
    Vec2_f32 p{};
    p.y = r.y0;

    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    f32 *preferred_column = scope_attachment(app, scope, buffer_preferred_column, f32);
    p.x = *preferred_column;

    i64 new_pos = view_pos_at_relative_xy(app, view, cursor.line, p);
    view_set_cursor(app, view, seek_pos(new_pos));
}

static void clear_jump_buffer(JumpBufferCmd *jump_buffer)
{
    JumpBufferCmd old = *jump_buffer;

    switch (jump_buffer->type) {
    case JUMP_BUFFER_CMD_SYSTEM_PROC:
        heap_free(&global_heap, jump_buffer->system.cmd.str);
        heap_free(&global_heap, jump_buffer->system.path.str);
        jump_buffer->system.cmd.size = 0;
        jump_buffer->system.path.size = 0;
        jump_buffer->system.has_exit = false;
        jump_buffer->system.status = -2;
        break;
    case JUMP_BUFFER_CMD_GLOBAL_SEARCH:
        heap_free(&global_heap, jump_buffer->global_search.query.str);
        jump_buffer->global_search.query.size = 0;
        break;
    case JUMP_BUFFER_CMD_BUFFER_SEARCH:
        heap_free(&global_heap, jump_buffer->buffer_search.query.str);
        jump_buffer->buffer_search.query.size = 0;
        break;
    }

    *jump_buffer = {};
    jump_buffer->buffer_id = old.buffer_id;
    jump_buffer->sticky = old.sticky;
}

static JumpBufferCmd duplicate_jump_buffer(JumpBufferCmd *src)
{
    JumpBufferCmd result = *src;

    switch (src->type) {
    case JUMP_BUFFER_CMD_GLOBAL_SEARCH:
        result.global_search.query.str = (u8*)heap_allocate(&global_heap, src->global_search.query.size);
        block_copy(result.global_search.query.str, src->global_search.query.str, result.global_search.query.size);
        break;
    case JUMP_BUFFER_CMD_BUFFER_SEARCH:
        result.buffer_search.query.str = (u8*)heap_allocate(&global_heap, src->buffer_search.query.size);
        block_copy(result.buffer_search.query.str, src->buffer_search.query.str, result.buffer_search.query.size);
        break;
    }

    return result;
}

static i32 push_jump_buffer(JumpBufferCmdType type, i32 index)
{
    Buffer_ID last = -1;
    i32 jump_buffer = 0;

    while (g_jump_buffers[index].sticky && index < JUMP_BUFFER_COUNT) {        
        if (g_jump_buffers[index].type == type) {
            clear_jump_buffer(&g_jump_buffers[index]);
            g_jump_buffers[index].type = type;
            goto fin;
        }

        index++;
    }

    switch (g_jump_buffers[index].type) {
    case JUMP_BUFFER_CMD_BUFFER_SEARCH:
        if (g_jump_buffers[index].buffer_search.query.size == 0) {
            clear_jump_buffer(&g_jump_buffers[index]);
            g_jump_buffers[index].type = type;
            goto fin;
        }
        break;
    case JUMP_BUFFER_CMD_GLOBAL_SEARCH:
        if (g_jump_buffers[index].global_search.query.size == 0) {
            clear_jump_buffer(&g_jump_buffers[index]);
            g_jump_buffers[index].type = type;
            goto fin;
        }
        break;
    case JUMP_BUFFER_CMD_SYSTEM_PROC:
        if (g_jump_buffers[index].system.cmd.size == 0) {
            clear_jump_buffer(&g_jump_buffers[index]);
            g_jump_buffers[index].type = type;
            goto fin;
        }
        break;
    }

    for (i32 i = JUMP_BUFFER_COUNT-1; i >= 0; i--) {
        if (!g_jump_buffers[i].sticky) {
            last = g_jump_buffers[i].buffer_id;
            break;
        }
    }

    jump_buffer = JUMP_BUFFER_COUNT-1;
    while (jump_buffer >= index+1) {
        if (g_jump_buffers[jump_buffer].sticky) {
            jump_buffer--;
            continue;
        }

        i32 j = jump_buffer-1;
        for (; j >= index; j--) {
            if (!g_jump_buffers[j].sticky) break;
        }

        g_jump_buffers[jump_buffer] = g_jump_buffers[j];
        jump_buffer = j;
    }

    g_jump_buffers[index] = {};
    g_jump_buffers[index].buffer_id = last;
    g_jump_buffers[index].type = type;

fin:
    switch (type) {
    case JUMP_BUFFER_CMD_SYSTEM_PROC:
        g_jump_buffers[index].system.has_exit = false;
        g_jump_buffers[index].system.status = -2;
        g_jump_buffers[index].sticky = true;
        break;
    }
    return index;
}

static void set_active_jump_buffer(Application_Links *app, i32 index)
{
    JumpBufferCmd *jump_buffer = &g_jump_buffers[index];
    switch (jump_buffer->type) {
    case JUMP_BUFFER_CMD_SYSTEM_PROC:
        // TODO(jesper): check if existing child process exists, sigkill
        // TODO(jesper): re-issue system proc
        break;
    case JUMP_BUFFER_CMD_BUFFER_SEARCH:
    case JUMP_BUFFER_CMD_GLOBAL_SEARCH:
        break;
    }

    view_set_buffer(app, g_jump_view, jump_buffer->buffer_id, 0);
    lock_jump_buffer(app, jump_buffer->buffer_id);

    g_active_jump_buffer = index;
}

static bool is_boundary(char c)
{
    switch(c) {
    case '*':
    case '!':
    case '@':
    case '$':
    case '&':
    case '#':
    case '^':
    case '+':
    case '-':
    case '=':
    case '.':
    case ',':
    case ';':
    case ':':
    case '?':
    case '<': case '>':
    case '%':
    case '[': case ']':
    case '{': case '}':
    case '(': case ')':
    case '\'': case '\"': case '`':
    case '/':
    case '\\':
    case '|':
        return true;
    default:
        return false;
    }
}

static i64* custom_get_indentation_array(
    Application_Links *app,
    Arena *arena,
    Buffer_ID buffer,
    Range_i64 lines,
    Indent_Flag flags,
    i32 tab_width,
    i32 indent_width)
{
    i64 count = lines.end - lines.start + 1;
    i64 *indent_marks = push_array(arena, i64, count);
    block_fill_u64(indent_marks, count * sizeof *indent_marks, (u64)(-1));

    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    Token_Array *tokens = scope_attachment(app, scope, attachment_tokens, Token_Array);

    i64 anchor_line = clamp_bot(1, lines.first - 1);
    Token *anchor = find_anchor_token(app, buffer, tokens, anchor_line);
    while (anchor->kind == TokenBaseKind_Whitespace && anchor->kind != TokenBaseKind_EOF) anchor++;

    i64 *shifted_indent_marks = indent_marks - lines.start;

    Token_Iterator_Array iter = token_iterator(0, tokens, anchor);
    Token *tok = anchor;
    while (tok->kind == TokenBaseKind_Whitespace && tok->kind != TokenBaseKind_EOF) tok++;

    Token *tok_end = iter.tokens + iter.count;

    i64 line_number = get_line_number_from_pos(app, buffer, iter.ptr->pos);
    line_number = Min(line_number, lines.start);

    i64 actual_anchor_line = get_line_number_from_pos(app, buffer, anchor->pos);
    i64 anchor_line_start = get_line_start_pos(app, buffer, actual_anchor_line);
    Indent_Info anchor_indent = get_indent_info_line_number_and_start(app, buffer, actual_anchor_line, anchor_line_start, tab_width);

    // NOTE(jesper): indentation state
    i64 cur_indent = anchor_indent.indent_pos;
    i64 next_indent = anchor_indent.indent_pos;

    if (tok->sub_kind == TokenCppKind_BraceOp || tok->sub_kind == TokenCppKind_BrackOp) {
        tok--;
    }

    bool is_in_define = false;
    i32 brace_level = 0;

    // NOTE(jesper): 32 nested switches ought to be enough for anybody?
    i64 switch_indent_stack[32];
    i64 switch_brace_stack[32];
    i32 switch_depth = 0;

    // NOTE(jesper): 64 nested parens ought to be enough for anybody?
    i64 paren_anchor_stack[64];
    i32 paren_depth = 0;
    i32 ternary_depth = 0;

    struct PrevLine {
        Token *first = nullptr;
        Token *last = nullptr;
    };

    PrevLine prev_line = {};

    struct {
        i64 indent = 0;
        i64 next_indent = 0;
        PrevLine prev_line = {};
    } before_define = {};

    while (tok < tok_end && line_number <= lines.end) {

        if (is_in_define) {
            if (((tok->flags & TokenBaseFlag_PreprocessorBody) == 0 ||
                 tok->kind == TokenBaseKind_Preprocessor))
            {
                is_in_define = false;
                cur_indent = before_define.indent;
                next_indent = before_define.next_indent;
                prev_line = before_define.prev_line;
            } else {
                cur_indent += tab_width;
            }
        }

        bool statement_complete = false;
        if (prev_line.last && prev_line.first) {

            switch (prev_line.last->kind) {
            case TokenBaseKind_ScopeOpen:
            case TokenBaseKind_ScopeClose:
                statement_complete = true;
                break;
            case TokenBaseKind_LiteralString:
                statement_complete = 
                    (prev_line.last->flags & TokenBaseFlag_PreprocessorBody) != 0 &&
                    prev_line.last->kind != TokenBaseKind_Preprocessor;
                break;
            default:
                switch (prev_line.last->sub_kind) {
                case TokenCppKind_Semicolon:
                case TokenCppKind_Comma:
                    statement_complete = true;
                    break;
                case TokenCppKind_Colon:
                    if (prev_line.first->sub_kind == TokenCppKind_Case ||
                        ternary_depth == 0) 
                    {
                        statement_complete = true;
                    }
                    break;
                }
                break;
            }

            switch (prev_line.first->kind) {
            case TokenBaseKind_Keyword:
                switch (prev_line.first->sub_kind) {
                case TokenCppKind_Template:
                    statement_complete = true;
                    break;
                }
                break;
            case TokenBaseKind_Preprocessor:
                // NOTE(jesper): this will break if I start breaking up defines or include 
                // directives across multiple lines but like... let's jsut not?
                statement_complete = true;
                break;
            }

            if (!statement_complete && tok->kind != TokenBaseKind_ScopeClose) {
                cur_indent += tab_width;
            }
        }

        if (paren_depth > 0) cur_indent = paren_anchor_stack[paren_depth-1];

        i64 line_end_pos = get_line_end_pos(app, buffer, line_number);
        while (tok->pos > line_end_pos && line_number <= lines.end) {
            if (lines.start == lines.end && line_number == lines.start) {
                shifted_indent_marks[line_number] = cur_indent;
            }
            line_end_pos = get_line_end_pos(app, buffer, ++line_number);
        }

        Token *first = tok;
        Token *last = tok;

        switch (first->kind) {
        case TokenBaseKind_Preprocessor:
            if (first->sub_kind == TokenCppKind_PPDefine) {
                before_define.indent = cur_indent;
                before_define.next_indent = next_indent;
                before_define.prev_line = prev_line;

                next_indent = 0;
                is_in_define = true;
            }
            cur_indent = 0;
            break;
        case TokenBaseKind_ScopeOpen:
            if (!statement_complete && paren_depth == 0) {
                cur_indent -= tab_width;
            }
            break;
        case TokenBaseKind_ScopeClose:
            cur_indent -= tab_width;
            break;
        default:
            switch (first->sub_kind) {
            case TokenCppKind_Public:
            case TokenCppKind_Private:
            case TokenCppKind_Protected:
                cur_indent -= tab_width;
                break;
            case TokenCppKind_Switch:
                switch_indent_stack[switch_depth] = cur_indent;
                switch_brace_stack[switch_depth] = brace_level;

                if (switch_depth < ArrayCount(switch_indent_stack)) {
                    switch_depth++;
                }
                break;
            case TokenCppKind_Case:
            case TokenCppKind_Default:
                if (switch_depth > 0) {
                    cur_indent = switch_indent_stack[switch_depth-1];
                }
                next_indent = cur_indent + tab_width;
                break;
            default:
                // NOTE(jesper): this is in here cause I'm not sure what sub_kinds are
                // categories as TokenBaseKind_Identifier
                if (tok->kind == TokenBaseKind_Identifier &&
                    first+2 < tok_end &&
                    (first+1)->sub_kind == TokenCppKind_Colon &&
                    (first+2)->sub_kind != TokenCppKind_Colon)
                {
                    // NOTE(jesper): assume goto label;
                    cur_indent = 0;
                }
                break;
            }
            break;
        }

        i64 next_line_start_pos = get_line_start_pos(app, buffer, line_number + 1);
        do {
            switch (tok->kind) {
            case TokenBaseKind_ScopeClose:
                brace_level--;
                next_indent -= tab_width;

                if (switch_depth > 0 &&
                    switch_brace_stack[switch_depth-1] == brace_level)
                {
                    switch_depth--;
                }

                if (paren_depth > 0) {
                    paren_anchor_stack[paren_depth-1] -= tab_width;
                }
                break;
            case TokenBaseKind_ScopeOpen:
                next_indent += tab_width;
                if (paren_depth > 0) {
                    paren_anchor_stack[paren_depth-1] += tab_width;
                }
                brace_level++;
                break;
            case TokenBaseKind_ParentheticalOpen:
                if (paren_depth < ArrayCount(paren_anchor_stack)) {
                    // TODO(jesper): I know this isn't accurate when there are spaces just
                    // after the paranthetical before the first token
                    paren_anchor_stack[paren_depth++] = tok->pos - first->pos + cur_indent + 1;
                }
                break;
            case TokenBaseKind_ParentheticalClose:
                paren_depth = Max(0, paren_depth - 1);
                break;
            }

            switch (tok->sub_kind) {
            case TokenCppKind_Ternary:
                ternary_depth = 1;
                break;
            case TokenCppKind_Semicolon:
                ternary_depth = 0;
                break;
            }

            last = tok++;
            while (tok->kind == TokenBaseKind_Whitespace && tok->kind != TokenBaseKind_EOF) tok++;
        } while (tok < tok_end && tok->pos < next_line_start_pos);

        if (last->kind != TokenBaseKind_Comment &&
            first->kind != TokenBaseKind_Comment)
        {
            prev_line.first = first;
            prev_line.last = last;
        }

        if (last->kind == TokenBaseKind_ParentheticalOpen) {
            paren_anchor_stack[paren_depth-1] = cur_indent + tab_width;
        }

        next_indent = Max(0, next_indent);
        cur_indent = Max(0, cur_indent);

        if (line_number >= lines.first) shifted_indent_marks[line_number] = cur_indent;

        line_number++;
        cur_indent = next_indent;
    }

    return indent_marks;
}

static b32 custom_auto_indent_buffer(
    Application_Links *app,
    Buffer_ID buffer,
    Range_i64 pos,
    Indent_Flag flags,
    i32 tab_width,
    i32 indent_width)
{
    ProfileScope(app, "auto indent buffer");
    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    Token_Array *tokens = scope_attachment(app, scope, attachment_tokens, Token_Array);

    b32 result = false;
    if (tokens != 0 && tokens->tokens != 0){
        result = true;

        Scratch_Block scratch(app);
        Range_i64 line_numbers = {};
        if (HasFlag(flags, Indent_FullTokens)){
            i32 safety_counter = 0;
            for (;;){
                Range_i64 expanded = enclose_tokens(app, buffer, pos);
                expanded = enclose_whole_lines(app, buffer, expanded);
                if (expanded == pos){
                    break;
                }
                pos = expanded;
                safety_counter += 1;
                if (safety_counter == 20){
                    pos = buffer_range(app, buffer);
                    break;
                }
            }
        }
        line_numbers = get_line_range_from_pos_range(app, buffer, pos);

        i64 *indentations = custom_get_indentation_array(app, scratch, buffer, line_numbers, flags, tab_width, indent_width);
        set_line_indents(app, scratch, buffer, line_numbers, indentations, flags, tab_width);
    }

    return(result);
}

CUSTOM_COMMAND_SIG(sort_lines)
CUSTOM_DOC("sort the lines in selection in alphabetical order")
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    if (buffer == 0) return;

    i64 cursor_pos = view_get_cursor_pos(app, view);
    i64 mark_pos = view_get_mark_pos(app, view);

    Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(cursor_pos));
    Buffer_Cursor mark = view_compute_cursor(app, view, seek_pos(mark_pos));
    if (cursor.line == mark.line) return;

    Buffer_Cursor start = cursor.line > mark.line ? mark : cursor;
    Buffer_Cursor end = cursor.line > mark.line ? cursor : mark;

    i32 num_lines = (i32)(end.line - start.line + 1);
    Scratch_Block scratch(app);
    String_Const_u8 *lines = push_array(scratch, String_Const_u8, num_lines);

    for (i32 i = 0; i < num_lines; i++) {
        String_Const_u8 line = push_buffer_line(app, scratch, buffer, start.line+i);

        if (line.size > 0) {
            if (line.str[line.size-1] == '\r') line.size--;
            if (line.str[line.size-1] == '\n') line.size--;
        }

        lines[i] = line;
    }

    quicksort_asc(lines, 0, num_lines-1);

    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    Line_Ending_Kind *eol_setting = scope_attachment(app, scope, buffer_eol_setting, Line_Ending_Kind);

    i32 eol_size = 0;
    String_Const_u8 eol_str = SCu8("\n");
    switch (*eol_setting) {
    case LineEndingKind_LF: 
        eol_size = 1; 
        eol_str = SCu8("\n");
        break;
    case LineEndingKind_CRLF: 
        eol_size = 2; 
        eol_str = SCu8("\r\n");
        break;
    }

    History_Group group = history_group_begin(app, buffer);

    i64 start_pos = get_line_start_pos(app, buffer, start.line);
    i64 end_pos = get_line_end_pos(app, buffer, end.line);

    buffer_replace_range(app, buffer, Ii64(start_pos, end_pos), string_u8_empty);

    i64 p = start_pos;
    for (i32 i = 0; i < num_lines-1; i++) {
        buffer_replace_range(app, buffer, Ii64(p, p), lines[i]);
        p += lines[i].size;

        buffer_replace_range(app, buffer, Ii64(p, p), eol_str);
        p += eol_size;
    }
    buffer_replace_range(app, buffer, Ii64(p, p), lines[num_lines-1]);


    history_group_end(group);
    reset_preferred_column(app);
}

CUSTOM_COMMAND_SIG(custom_auto_indent_range)
    CUSTOM_DOC("Auto-indents the range between the cursor and the mark.")
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    Range_i64 range = get_view_range(app, view);

    Indent_Flag flags = 0;

    i32 indent_width = (i32)def_get_config_u64(app, vars_save_string_lit("indent_width"));
    i32 tab_width = (i32)def_get_config_u64(app, vars_save_string_lit("default_tab_width"));
    AddFlag(flags, Indent_FullTokens);

    // NOTE(jesper): I'm not sure about this. I think I want this to come
    // from a project configuration. For my own projects I wouldn't mind
    // auto indent to clean things up, but not always feasible for work?
    AddFlag(flags, Indent_ClearLine);

    b32 indent_with_tabs = def_get_config_b32(vars_save_string_lit("indent_with_tabs"));
    if (indent_with_tabs) {
        AddFlag(flags, Indent_UseTab);
    }

    custom_auto_indent_buffer(app, buffer, range, flags, indent_width, tab_width);
    move_past_lead_whitespace(app, view, buffer);
}

CUSTOM_COMMAND_SIG(custom_auto_indent_whole_file)
    CUSTOM_DOC("Audo-indents the entire current buffer.")
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    i64 buffer_size = buffer_get_size(app, buffer);

    Indent_Flag flags = 0;
    i32 indent_width = (i32)def_get_config_u64(app, vars_save_string_lit("indent_width"));
    i32 tab_width = (i32)def_get_config_u64(app, vars_save_string_lit("default_tab_width"));

    AddFlag(flags, Indent_FullTokens);

    // NOTE(jesper): I'm not sure about this. I think I want this to come
    // from a project configuration. For my own projects I wouldn't mind
    // auto indent to clean things up, but not always feasible for work?
    AddFlag(flags, Indent_ClearLine);
    b32 indent_with_tabs = def_get_config_b32(vars_save_string_lit("indent_with_tabs"));
    if (indent_with_tabs) {
        AddFlag(flags, Indent_UseTab);
    }

    custom_auto_indent_buffer(app, buffer, Ii64(0, buffer_size), flags, indent_width, tab_width);
}

CUSTOM_COMMAND_SIG(custom_write_and_auto_tab)
{
    write_text_input(app);

    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);

    i64 cursor_pos = view_get_cursor_pos(app, view);
    i32 indent_width = (i32)def_get_config_u64(app, vars_save_string_lit("indent_width"));
    i32 tab_width = (i32)def_get_config_u64(app, vars_save_string_lit("default_tab_width"));

    Indent_Flag flags = 0;
    AddFlag(flags, Indent_FullTokens);

    b32 indent_with_tabs = def_get_config_b32(vars_save_string_lit("indent_with_tabs"));
    if (indent_with_tabs) {
        AddFlag(flags, Indent_UseTab);
    }

    custom_auto_indent_buffer(app, buffer, Ii64(cursor_pos, cursor_pos), flags, indent_width, tab_width);
    move_past_lead_whitespace(app, view, buffer);
}

CUSTOM_COMMAND_SIG(custom_newline)
{
    Scratch_Block scratch(app);

    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);

    String_Const_u8 filename = push_buffer_file_name(app, scratch, buffer);
    String_Const_u8 ext = string_file_extension(filename);

    i64 cursor_pos = view_get_cursor_pos(app, view);
    i64 line_start = get_line_side_pos_from_pos(app, buffer, cursor_pos, Side_Min);
    line_start = get_pos_past_lead_whitespace(app, buffer, line_start);

    i32 indent_width = (i32)def_get_config_u64(app, vars_save_string_lit("indent_width"));
    i32 tab_width = (i32)def_get_config_u64(app, vars_save_string_lit("default_tab_width"));

    Indent_Flag flags = 0;
    AddFlag(flags, Indent_FullTokens);

    b32 indent_with_tabs = def_get_config_b32(vars_save_string_lit("indent_with_tabs"));
    if (indent_with_tabs) AddFlag(flags, Indent_UseTab);

    if (c_line_comment_starts_at_position(app, buffer, line_start) &&
        (string_match(ext, string_u8_litexpr("c")) ||
         string_match(ext, string_u8_litexpr("cpp")) ||
         string_match(ext, string_u8_litexpr("h")) ||
         string_match(ext, string_u8_litexpr("hpp"))))
    {
        write_text(app, string_u8_litexpr("\n"));

        cursor_pos = view_get_cursor_pos(app, view);
        custom_auto_indent_buffer(app, buffer, Ii64(cursor_pos, cursor_pos), flags, indent_width, tab_width);
        move_past_lead_whitespace(app, view, buffer);

        // TODO(jesper): this should really read the text between the last / and beginning of text and duplicate that
        // to also get any indentation and other alignment, but this works and is simple
        write_text(app, string_u8_litexpr("// "));
    } else {
        write_text(app, string_u8_litexpr("\n"));
        cursor_pos = view_get_cursor_pos(app, view);
        custom_auto_indent_buffer(app, buffer, Ii64(cursor_pos, cursor_pos), flags, indent_width, tab_width);
        move_past_lead_whitespace(app, view, buffer);
    }
}

static Child_Process_ID custom_compile_project(
    Application_Links *app,
    JumpBufferCmd *jump_buffer,
    String_Const_u8 path,
    String_Const_u8 cmd_string)
{
    if (has_dirty_buffers(app)) {
        ListerDirtyBuffersChoice choice = lister_handle_dirty_buffers(app);

        switch (choice) {
        case CHOICE_NULL:
        case CHOICE_OPEN_BUFFER:
        case CHOICE_CANCEL:
            return 0;
        case CHOICE_SAVE_ALL:
            save_all_dirty_buffers(app);
            break;
        case CHOICE_DISCARD:
            break;
        }
    }

    Scratch_Block scratch(app);
    String_Const_u8 command = push_u8_stringf(
        scratch, "\"%.*s/%.*s\"",
        string_expand(path),
        string_expand(cmd_string));

    Child_Process_ID child_process = create_child_process(app, path, command);
    if (child_process == 0) return 0;

    u32 flags = CLI_OverlapWithConflict | CLI_SendEndSignal;
    if (!set_buffer_system_command(app, child_process, jump_buffer->buffer_id, flags)) return 0;

    set_fancy_compilation_buffer_font(app);
    block_zero_struct(&prev_location);

    jump_buffer->system.status = -1;

    return child_process;
}

static Range_i64 get_mark_cursor_lines_range(Application_Links *app, View_ID view, Buffer_ID buffer)
{
    i64 cursor = view_get_cursor_pos(app, view);
    i64 mark = view_get_mark_pos(app, view);

    i64 cursor_line = get_line_number_from_pos(app, buffer, cursor);
    i64 mark_line = get_line_number_from_pos(app, buffer, mark);

    i64 first_line = cursor_line > mark_line ? mark_line : cursor_line;
    i64 last_line = cursor_line > mark_line ? cursor_line: mark_line;

    Range_i64 range;
    range.start = get_line_start_pos(app, buffer, first_line);
    range.end = get_line_end_pos(app, buffer, last_line);

    i64 buffer_size = buffer_get_size(app, buffer);
    char last_c = buffer_get_char(app, buffer, range.end);
    range.end = Min(buffer_size, range.end+1);

    if (range.start == range.end || (last_c != '\n' && last_c != '\r')) {
        range.start = Max(0, range.start-1);
    }

    return range;
}

static void set_map_id(Application_Links *app, Command_Map_ID map_id)
{
    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);

    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    Command_Map_ID *map_id_ptr = scope_attachment(app, scope, buffer_map_id, Command_Map_ID);
    *map_id_ptr = map_id;
}

static void set_modal_mode(Application_Links *app, ModalMode mode)
{
    if (g_mode != mode) {
        g_mode = mode;

        switch (mode) {
        case MODAL_MODE_INSERT:
            set_mark(app);
            set_map_id(app, vars_save_string_lit("keys_insert"));
            break;
        case MODAL_MODE_EDIT:
            set_map_id(app, vars_save_string_lit("keys_edit"));
            break;
        }
    }
}

static i64 seek_next_word(Application_Links *app, View_ID view, i64 pos)
{
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
    i64 buffer_size = buffer_get_size(app, buffer);

    // TODO(jesper): I think 4coder has a more efficient search API now, though it's
    // not really relevant for this code-path it might be worth investigating
    u8 data[128];

    Range_i64 range = {};
    range.start = Max(0, pos);
    range.end = Min(range.start + (i64)sizeof data, buffer_size);

    if (!buffer_read_range(app, buffer, range, data)) return pos;

    char *p = (char*)(&data[0] - range.start);
    char start_c = p[pos++];
    i32 start_is_whitespace = character_is_whitespace(start_c);

    bool start_is_boundary = !start_is_whitespace && is_boundary(start_c);
    bool in_whitespace = start_is_whitespace;
    bool was_cr = start_c == '\r';

    do {
        for (; pos < range.end; pos++) {
            char c = p[pos];
            b32 whitespace = character_is_whitespace(c);
            bool boundary = !whitespace && is_boundary(c);

            if (c == '\r') {
                was_cr = true;
                boundary = true;
            } else {
                if (!was_cr && c == '\n') boundary = true;
                was_cr = false;
            }

            if (start_is_boundary && !whitespace) {
                goto done;
            }

            if (!start_is_whitespace) {
                if (boundary || (in_whitespace && !whitespace)) {
                    goto done;
                } else if (whitespace) {
                    in_whitespace = true;
                }
            } else if (!whitespace || boundary) {
                goto done;
            }
        }

        range.start = range.end;
        range.end = Min(range.start + (i64)sizeof data, buffer_size);
        p = (char*)(&data[0] - range.start);
    } while (pos < buffer_size && buffer_read_range(app, buffer, range, data));

done:
    return clamp(0, pos, buffer_size);
}

static i64 seek_prev_word(Application_Links *app, View_ID view, i64 pos)
{
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
    i64 buffer_size = buffer_get_size(app, buffer);

    // TODO(jesper): I think 4coder has a more efficient search API now, though it's
    // not really relevant for this code-path it might be worth investigating
    u8 data[128];

    Range_i64 range = {};
    range.start = Max(0, pos - 1);
    range.end = Min(range.start + (i64)sizeof data, buffer_size);

    if (!buffer_read_range(app, buffer, range, data)) return pos;

    i64 start_pos = pos;
    char *p = (char*)(&data[0] - range.start);

    char start_c = p[pos--];
    i32 start_is_whitespace = character_is_whitespace(start_c);
    bool start_is_boundary = !start_is_whitespace && is_boundary(start_c);
    bool start_is_normal = !start_is_boundary && !start_is_whitespace;

    bool was_whitespace = start_is_whitespace;
    bool has_whitespace = false;
    bool was_ln = start_c == '\n';


    do {
        for (; pos >= range.start; pos--) {
            char c = p[pos];
            b32 whitespace = character_is_whitespace(c);
            bool boundary = !whitespace && is_boundary(c);

            if (c == '\n') {
                was_ln = true;
                boundary = true;
            } else {
                if (was_ln && c == '\r') boundary = true;
                was_ln = false;
            }

            has_whitespace = has_whitespace || whitespace;
            if (whitespace && !was_whitespace) {
                if (pos + 1 != start_pos) {
                    return clamp(0, pos+1, buffer_size);
                }
            }

            if (start_is_boundary && !whitespace && boundary && (pos + 1 != start_pos)) {
                if (was_whitespace) {
                    return clamp(0, pos, buffer_size);
                } else {
                    return clamp(0, pos+1, buffer_size);
                }
            }

            if (boundary && pos != start_pos) {
                if ((has_whitespace || start_is_normal) && !was_whitespace && pos+1 != start_pos) {
                    return clamp(0, pos+1, buffer_size);
                } else {
                    return clamp(0, pos, buffer_size);
                }
            }

            was_whitespace = whitespace;
        }

        range.start = Max(0, range.start - (i64)sizeof data);
        range.end = Min(range.start + (i64)sizeof data, buffer_size);
        p = (char*)(&data[0] - range.start);
    } while (pos > 0 && buffer_read_range(app, buffer, range, data));

    return clamp(0, pos, buffer_size);
}

static i64 seek_matching_scope(Application_Links *app, View_ID view, i64 pos)
{
    // TODO(jesper): I think 4coder has a more efficient search API now. This
    // code path could potentially end up seeking very large portions of the
    // buffer, so it's probably worth investigating
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
    i64 buffer_size = buffer_get_size(app, buffer);

    u8 data[512];

    Range_i64 range = {};
    range.start = pos;
    range.end = Min(range.start + (i64)sizeof data, buffer_size);

    if (!buffer_read_range(app, buffer, range, data)) return pos;

    char *p = (char*)(&data[0] - range.start);
    i64 final_pos = pos;
    i32 brace_level = 0;

    char start_c = p[pos];
    char opening_c = ' ';
    char closing_c = ' ';
    bool forward = true;

init_chars:
    if (start_c == '{') {
        opening_c = '{';
        closing_c = '}';
        forward = true;
    } else if (start_c == '}') {
        opening_c = '{';
        closing_c = '}';
        forward = false;
    } else if (start_c == '<') {
        opening_c = '<';
        closing_c = '>';
        forward = true;
    } else if (start_c == '>') {
        opening_c = '<';
        closing_c = '>';
        forward = false;
    } else if (start_c == '[') {
        opening_c = '[';
        closing_c = ']';
        forward = true;
    } else if (start_c == ']') {
        opening_c = '[';
        closing_c = ']';
        forward = false;
    } else if (start_c == '(') {
        opening_c = '(';
        closing_c = ')';
        forward = true;
    } else if (start_c == ')') {
        opening_c = '(';
        closing_c = ')';
        forward = false;
    } else if (start_c == '#') {

    } else {
        i64 line = get_line_number_from_pos(app, buffer, pos);
        i64 line_start = get_line_start_pos(app, buffer, line);
        i64 line_end = get_line_end_pos(app, buffer, line);

        auto closer_forward = [](
            Application_Links *app, 
            Buffer_ID buffer, 
            i64 pos, i64 max, 
            String_Const_u8 str, 
            i64 *dist, i64 *o_pos)
        {
            i64 forward;
            seek_string_forward(app, buffer, pos, max, str, &forward);

            if (forward != -1 && forward <= max && forward > pos && forward - pos < *dist) {
                *dist = forward - pos;
                *o_pos = forward;
                return true;
            }
            return false;
        };

        auto closer_backward = [](
            Application_Links *app, 
            Buffer_ID buffer, 
            i64 pos, i64 min, 
            String_Const_u8 str, 
            i64 *dist, i64 *o_pos)
        {
            i64 backward;
            seek_string_backward(app, buffer, pos, min, str, &backward);

            if (backward != -1 && backward >= min && backward < pos && pos - backward < *dist) {
                *dist = pos - backward;
                *o_pos = backward;
                return true;
            }
            return false;
        };


        i64 closest_dist = I64_MAX;
        i64 closest_pos = pos;

        if (line_end != pos) {
            if (closer_forward(app, buffer, pos, line_end, string_u8_litexpr("{"), &closest_dist, &closest_pos)) start_c = '{';
            if (closer_forward(app, buffer, pos, line_end, string_u8_litexpr("}"), &closest_dist, &closest_pos)) start_c = '}';
            if (closer_forward(app, buffer, pos, line_end, string_u8_litexpr("["), &closest_dist, &closest_pos)) start_c = '[';
            if (closer_forward(app, buffer, pos, line_end, string_u8_litexpr("]"), &closest_dist, &closest_pos)) start_c = ']';
            if (closer_forward(app, buffer, pos, line_end, string_u8_litexpr("("), &closest_dist, &closest_pos)) start_c = '(';
            if (closer_forward(app, buffer, pos, line_end, string_u8_litexpr(")"), &closest_dist, &closest_pos)) start_c = ')';
        }

        if (line_start != pos) {
            if (closer_backward(app, buffer, pos, line_start, string_u8_litexpr("{"), &closest_dist, &closest_pos)) start_c = '{';
            if (closer_backward(app, buffer, pos, line_start, string_u8_litexpr("}"), &closest_dist, &closest_pos)) start_c = '}';
            if (closer_backward(app, buffer, pos, line_start, string_u8_litexpr("["), &closest_dist, &closest_pos)) start_c = '[';
            if (closer_backward(app, buffer, pos, line_start, string_u8_litexpr("]"), &closest_dist, &closest_pos)) start_c = ']';
            if (closer_backward(app, buffer, pos, line_start, string_u8_litexpr("("), &closest_dist, &closest_pos)) start_c = '(';
            if (closer_backward(app, buffer, pos, line_start, string_u8_litexpr(")"), &closest_dist, &closest_pos)) start_c = ')';
        }

        if (closest_dist < I64_MAX) {
            pos = closest_pos;
            goto init_chars;
        }

        if (closer_forward(app, buffer, pos, buffer_size, string_u8_litexpr("{"), &closest_dist, &closest_pos)) start_c = '{';
        if (closer_forward(app, buffer, pos, buffer_size, string_u8_litexpr("}"), &closest_dist, &closest_pos)) start_c = '}';
        if (closer_forward(app, buffer, pos, buffer_size, string_u8_litexpr("["), &closest_dist, &closest_pos)) start_c = '[';
        if (closer_forward(app, buffer, pos, buffer_size, string_u8_litexpr("]"), &closest_dist, &closest_pos)) start_c = ']';
        if (closer_forward(app, buffer, pos, buffer_size, string_u8_litexpr("("), &closest_dist, &closest_pos)) start_c = '(';
        if (closer_forward(app, buffer, pos, buffer_size, string_u8_litexpr(")"), &closest_dist, &closest_pos)) start_c = ')';

        if (closer_backward(app, buffer, pos, 0, string_u8_litexpr("{"), &closest_dist, &closest_pos)) start_c = '{';
        if (closer_backward(app, buffer, pos, 0, string_u8_litexpr("}"), &closest_dist, &closest_pos)) start_c = '}';
        if (closer_backward(app, buffer, pos, 0, string_u8_litexpr("["), &closest_dist, &closest_pos)) start_c = '[';
        if (closer_backward(app, buffer, pos, 0, string_u8_litexpr("]"), &closest_dist, &closest_pos)) start_c = ']';
        if (closer_backward(app, buffer, pos, 0, string_u8_litexpr("("), &closest_dist, &closest_pos)) start_c = '(';
        if (closer_backward(app, buffer, pos, 0, string_u8_litexpr(")"), &closest_dist, &closest_pos)) start_c = ')';

        if (closest_dist < I64_MAX) {
            pos = closest_pos;
            goto init_chars;
        }

        return pos;
    }

    while (pos >= range.end) {
        range.start = range.end;
        range.end = Min(buffer_size, range.start + (i64)sizeof data);
        if (!buffer_read_range(app, buffer, range, data)) return pos;
        p = (char*)(&data[0] - range.start);
    }

    while (pos < range.start) {
        range.end = range.start;
        range.start = Max(0, range.start - (i64)sizeof data);
        if (!buffer_read_range(app, buffer, range, data)) return pos;
        p = (char*)(&data[0] - range.start);
    }

    // TODO(jesper): we need to figure out what to do about comments here
    // TODO(jesper): and scope tokens in string/character literals
    if (forward) {
        do {
            for (; pos < range.end; pos++) {
                char c = p[pos];

                if (c == opening_c) brace_level++;
                if (c == closing_c) {
                    if (--brace_level == 0) {
                        final_pos = pos;
                        goto pos_found;
                    }

                }
            }

            range.start = range.end;
            range.end = Min(buffer_size, range.start + (i64)sizeof data);
            p = (char*)(&data[0] - range.start);
        } while (pos < buffer_size && buffer_read_range(app, buffer, range, data));
    } else {
        do {
            for (; pos >= range.start; pos--) {
                char c = p[pos];
                if (c == opening_c) {
                    if (++brace_level == 0) {
                        final_pos = pos;
                        goto pos_found;
                    }
                }
                if (c == closing_c) {
                    brace_level--;

                }
            }

            range.end = range.start;
            range.start = Max(0, range.end - (i64)sizeof data);
            p = (char*)(&data[0] - range.start);
        } while (pos >= 0 && buffer_read_range(app, buffer, range, data));
    }

pos_found:
    return pos;
}

String_Const_u8 shorten_path(String_Const_u8 path, String_Const_u8 root)
{
    u8 *p_ptr = path.str;
    u8 *p_end = path.str+path.size;

    u8 *r_ptr = root.str;
    u8 *r_end = root.str+root.size;

    while (p_ptr < p_end && r_ptr < r_end &&
           (*p_ptr == *r_ptr || (*p_ptr == '/' && *r_ptr == '\\') || (*p_ptr == '\\' && *r_ptr == '/')))
    {
        p_ptr++;
        r_ptr++;
    }

    String_Const_u8 result;
    result.str = p_ptr;
    result.size = (u64)(p_end-p_ptr);
    return result;
}

static void custom_isearch(
    Application_Links *app,
    JumpBufferCmd *jb,
    View_ID view,
    Buffer_ID active_buffer,
    String_Const_u8 initial_query)
{
    Scratch_Block scratch(app);
    Arena *arena = scratch.arena;

    if (!buffer_exists(app, active_buffer)) return;
    i64 buffer_size = buffer_get_size(app, active_buffer);

    Query_Bar_Group group(app);
    Query_Bar bar = {};
    if (start_query_bar(app, &bar, 0) == 0) return;

    i64 cursor_pos = view_get_cursor_pos(app, view);

    u8 string_mem[256];
    bar.string = SCu8(string_mem, Min(sizeof string_mem, initial_query.size));
    block_copy(bar.string.str, initial_query.str, bar.string.size);
    b32 string_changed = bar.string.size != 0;

    String_Const_u8 prompt_str = string_u8_litexpr("I-Search: ");
    bar.prompt = prompt_str;

    clear_buffer(app, jb->buffer_id);
    i64 jb_size = buffer_get_size(app, jb->buffer_id);

    String_Const_u8 buffer_file_name = push_buffer_file_name(app, scratch, active_buffer);
    if (buffer_file_name.size == 0) {
        buffer_file_name = push_buffer_unique_name(app, scratch, active_buffer);
    }

    i64 closest_location = -1;
    i64 closest_pos = max_i64;

    struct JumpLocation {
        i64 pos;
        i64 line;
        i64 col;
        i64 line_start;
        i64 line_end;
    };

    i32 location_cap = 1024;
    i32 location_count = 0;
    JumpLocation *locations = heap_alloc_arr(&global_heap, JumpLocation, location_cap);

    User_Input in = {};
    for (;;) {
        if (!string_changed) {
            in = get_next_input(
                app,
                EventPropertyGroup_AnyKeyboardEvent,
                EventProperty_Escape|EventProperty_ViewActivation);

            if (in.abort) break;
        }

        String_Const_u8 str = to_writable(&in);
        if (match_key_code(&in, KeyCode_P) && has_modifier(&in, KeyCode_Control)) {
            str = push_clipboard_index(scratch, 0, 0);
        } else if (match_key_code(&in, KeyCode_Return)) {
            lock_jump_buffer(app, jb->buffer_id);
            jb->buffer_search.query.str = (u8*)heap_realloc(
                &global_heap, 
                jb->buffer_search.query.str, 
                jb->buffer_search.query.size, 
                bar.string.size);
            block_copy(jb->buffer_search.query.str, bar.string.str, bar.string.size);
            jb->buffer_search.query.size = bar.string.size;
            jb->buffer_search.buffer = active_buffer;
            break;
        } else if (match_key_code(&in, KeyCode_Backspace)) {
            if (is_unmodified_key(&in.event)) {
                u64 old_bar_string_size = bar.string.size;
                bar.string = backspace_utf8(bar.string);
                jb->label_size = (i32)bar.string.size;
                string_changed = bar.string.size < old_bar_string_size;
            }
            
            goto handle_string_changed;
        }
        
        if (str.str != 0 && str.size > 0) {
            String_u8 bar_string = Su8(bar.string, sizeof string_mem);
            string_append(&bar_string, str);
            bar.string = bar_string.string;

            jb->label_size = (i32)Min(sizeof jb->label, bar.string.size);
            block_copy(jb->label, bar.string.str, jb->label_size);

            if (location_count > 0) {
                ProfileScope(app, "re-check existing locations");

                Temp_Memory tmp_mem = begin_temp(arena);
                String_u8 haystack = Su8(push_array(scratch, u8, bar_string.size), bar_string.size);

                for (i32 i = 0; i < location_count; i++) {
                    JumpLocation location = locations[i];

                    b32 matches = false;

                    if (location.pos > buffer_size) goto remove_location;
                    if (location.pos + (i64)bar_string.size > buffer_size) goto remove_location;
                    if (!buffer_read_range(app, active_buffer, Ii64(location.pos, location.pos + haystack.size), haystack.str)) goto remove_location;

                    matches = string_match_insensitive(SCu8(haystack), SCu8(bar_string));

                    if (!matches) {
remove_location:
                        for (i32 j = i; j < location_count-1; j++) {
                            locations[j] = locations[j+1];
                        }
                        location_count--;
                        i--;
                    }
                }

                end_temp(tmp_mem);
            } else {
                string_changed = true;
            }

        } else {
            leave_current_input_unhandled(app);
        }

handle_string_changed:
        if (string_changed) {
            ProfileScope(app, "buffer seeking");

            location_count = 0;
            i64 pos = 0;
            while (pos < buffer_size) {
                seek_string_insensitive_forward(app, active_buffer, pos, 0, bar.string, &pos);

                if (pos < buffer_size) {
                    if (location_count == location_cap) {
                        i32 new_cap = location_cap * 3 / 2;
                        JumpLocation *new_locs = heap_alloc_arr(&global_heap, JumpLocation, new_cap);
                        memcpy(new_locs, locations, location_cap * sizeof *locations);
                        heap_free(&global_heap, locations);
                        locations = new_locs;
                        location_cap = new_cap;
                    }

                    Buffer_Cursor full_cursor = view_compute_cursor(app, view, seek_pos(pos));

                    JumpLocation location;
                    location.pos = pos;
                    location.line = full_cursor.line;
                    location.col = full_cursor.col;

                    location.line_start = get_line_start_pos(app, active_buffer, location.line);
                    location.line_end = get_line_end_pos(app, active_buffer, location.line);

                    locations[location_count++] = location;
                }
            }
        }

        clear_buffer(app, jb->buffer_id);
        jb_size = buffer_get_size(app, jb->buffer_id);

        closest_location = -1;
        closest_pos = max_i64;

        i32 size = 1024;
        i32 written = 0;
        char *buffer = push_array(scratch, char, size);


        for (i32 i = 0; i < location_count; i++) {
            Scratch_Block scratch_inner(app);
            ProfileScope(app, "output jump location");

            JumpLocation location = locations[i];

            i64 line_start = location.line_start;
            i64 line_end = location.line_end;

            String_Const_u8 line_str = push_buffer_range(app, scratch_inner, active_buffer,Ii64(line_start, line_end));
            String_Const_u8 chopped_line = string_skip_whitespace(line_str);

            if (llabs(cursor_pos - location.pos) < llabs(cursor_pos - closest_pos) ||
                (location.pos > cursor_pos && closest_pos < cursor_pos))
            {
                closest_location = i;
                closest_pos = location.pos;
            }

            String_Const_u8 isearch_line = push_stringf(
                scratch_inner,
                "%.*s:%d:%d: %.*s\n",
                buffer_file_name.size, buffer_file_name.str,
                location.line,
                location.col,
                chopped_line.size, chopped_line.str);

            i32 bytes_to_write = (i32)isearch_line.size;
            i32 bytes_written = 0;

            while (bytes_written < bytes_to_write) {
                i32 available = Min(size - written, bytes_to_write - bytes_written);
                memcpy(buffer+written, isearch_line.str + bytes_written, available);
                written += available;
                bytes_written += available;

                if (written == size) {
                    buffer_replace_range(
                        app,
                        jb->buffer_id,
                        Ii64(jb_size),
                        SCu8(buffer, written));
                    jb_size = buffer_get_size(app, jb->buffer_id);
                    written = 0;
                }
            }
        }

        if (written > 0) {
            buffer_replace_range(
                app,
                jb->buffer_id,
                Ii64(jb_size),
                SCu8(buffer, written));
            jb_size = buffer_get_size(app, jb->buffer_id);
            written = 0;
        }

        if (closest_location != -1) {
            view_set_cursor(app, view, seek_pos(closest_pos));
            view_set_cursor(app, g_jump_view, seek_line_col(closest_location+1, 0));
            reset_preferred_column(app);
        }

        string_changed = false;
    }

    heap_free(&global_heap, locations);
}

static void custom_startup(Application_Links *app)
{
    ProfileScope(app, "default startup");
    Scratch_Block scratch(app);

    fzy_init_table();

    User_Input input = get_current_input(app);
    if (!match_core_code(&input, CoreCode_Startup)) return;

    String_Const_u8_Array file_names = input.event.core.file_names;

    String_Const_u8 filename{};
    if (file_names.count > 0) filename = file_names.vals[0];

#ifdef _WIN32
    if (filename.size == 0) {
        // NOTE(jesper): there's some bug with 4coder not having a filename despite launched with a command line
        // argument with a path to a file. I think this has to do with read-only files in P4-land, but I'm not
        // completely sure. Might also have to do with p4v adding spaces before start of the argument
        char *cmdline = GetCommandLineA();

        //print_message(app, S8Lit("command line: "));
        //print_message(app, SCu8(cmdline, strlen(cmdline)));
        //print_message(app, S8Lit("\n"));

        char *args = nullptr;
        char *p = cmdline;

        bool in_string = false;
        while (*p) {
            if (*p == '"') in_string = !in_string;
            else if (!in_string && *p == ' ') {
                args = p+1;
                while (*args && *args == ' ') args++;
                break;
            }
            p++;

        }

        if (args && *args) {
            p = args;
            in_string = false;
            while (*p) {
                if (*p == '"') in_string = !in_string;
                else if (!in_string && *p == ' ') {
                    filename = SCu8((u8*)args, (u64)(p-args));
                }
                p++;
            }

            if (filename.size == 0) {
                filename = SCu8((u8*)args, (u64)(p-args));
            }

            if (filename.size > 0) {
                if (filename.str[0] == '"') {
                    filename.str += 1;
                    filename.size -= 1;
                }

                if (filename.str[filename.size-1] == '"') {
                    filename.size -= 1;
                }
            }
        }
    }
#endif

    load_themes_default_folder(app);
    load_config_and_apply(app, &global_config_arena, 0, 0);

    // open command line files
    String_Const_u8 hot_directory = push_hot_directory(app, scratch);
    for (i32 i = 0; i < file_names.count; i += 1){
        Temp_Memory_Block temp(scratch);
        String_Const_u8 input_name = file_names.vals[i];
        String_Const_u8 full_name = push_u8_stringf(
            scratch, "%.*s/%.*s",
            string_expand(hot_directory),
            string_expand(input_name));
        Buffer_ID new_buffer = create_buffer(app, full_name, BufferCreate_NeverNew|BufferCreate_MustAttachToFile);

        if (new_buffer == 0){
            create_buffer(app, input_name, 0);
        }
    }

    custom_setup_fonts(app);
    custom_setup_necessary_bindings(&framework_mapping);
    custom_setup_default_bindings(&framework_mapping);

    View_ID main_view = get_active_view(app, Access_Always);
    Face_ID face_id = get_face_id(app, view_get_buffer(app, main_view, Access_Always));
    Face_Metrics metrics = get_face_metrics(app, face_id);

    View_ID bottom = open_view(app, main_view, ViewSplit_Bottom);
    view_set_passive(app, bottom, true);
    view_set_setting(app, bottom, ViewSetting_ShowFileBar, 0);

    view_set_split_pixel_size(app, bottom, (i32)(metrics.line_height*3.0f + 5.0f));

    u8 mem[256];
    String_u8 buffer_name = Su8(mem, 0, sizeof mem);
    for (i32 i = 0; i < JUMP_BUFFER_COUNT; i++) {
        string_append(&buffer_name, string_u8_litexpr("*jump_buffer_"));
        string_append_character(&buffer_name, (u8)('0' + i));
        string_append_character(&buffer_name, (u8)'*');

        Buffer_ID buffer = create_buffer(app, SCu8(buffer_name), BufferCreate_AlwaysNew);
        //buffer_set_face(app, buffer, global_small_code_face);
        buffer_set_setting(app, buffer, BufferSetting_Unimportant, true);
        buffer_set_setting(app, buffer, BufferSetting_ReadOnly, true);

        JumpBufferCmd cmd = {};
        cmd.buffer_id = buffer;
        g_jump_buffers[i] = cmd;
        buffer_name.size = 0;
    }

    g_jump_view = bottom;

    b32 auto_load_project = def_get_config_b32(vars_save_string_lit("automatically_load_project"));

    bool project_loaded = false;
    if (filename.size > 0) {

        Buffer_ID buffer = buffer_identifier_to_id(app, buffer_identifier(filename));
        if (buffer == 0) {
            buffer = create_buffer(app, filename, BufferCreate_NeverNew);
            if (buffer == 0) {
                String_Const_u8 hot_dir = push_hot_directory(app, scratch);
                String_Const_u8 path = push_file_search_up_path(app, scratch, hot_dir, filename);
                buffer = create_buffer(app, path, BufferCreate_NeverNew);

                if (buffer == 0) {
                    String_Const_u8 full_path = push_u8_stringf(scratch, "%.*s/%.*s", string_expand(hot_dir), string_expand(filename));
                    buffer = create_buffer(app, full_path, BufferCreate_NeverNew);
                }
            }
        }
        view_set_buffer(app, main_view, buffer, 0);

#if defined(__linux__)
        // TODO(jesper): what I really need here is to check if file_names.vals[0] is relative or absolute, and if it's relative then
        // resolve it to an absolute path using the hot_dir.

        String_Const_u8 hot_dir = push_hot_directory(app, scratch);
        String_Const_u8 path = push_file_search_up_path(app, scratch, hot_dir, file_names.vals[0]);

        if (path.size > 0) {
            if (path.size > 1 && 
                path.str[0] == '/' && path.str[1] == '/') 
            {
                path.str++;
                path.size--;
            }
            set_hot_directory(app, path);
        }
#else
        set_hot_directory(app, filename);
#endif
        String_Const_u8 project_file = S8Lit("project.4coder");
        if (auto_load_project && filename.size >= project_file.size) {
            i64 start = filename.size - project_file.size;
            i64 end = start + project_file.size;

            String_Const_u8 sub = string_substring(filename, Ii64(start, end));
            if (string_match(sub, project_file)) {
                load_project_file(app, filename);
                project_loaded = true;
            }
        }

    } else if (auto_load_project) {
        custom_load_project(app);
        project_loaded = true;
    }

    if (!project_loaded) {
        set_window_title(app, filename);
    } else {
        String_Const_u8 proj_name = def_get_config_string(scratch, vars_save_string_lit("project_name"));
        set_window_title(app, proj_name);
    }

    view_set_active(app, main_view);
}

static void custom_draw_cursor(
    Application_Links *app,
    View_ID view_id,
    b32 is_active_view,
    Buffer_ID buffer,
    Text_Layout_ID text_layout_id,
    f32 outline_thickness)
{
    b32 has_highlight_range = draw_highlight_range(app, view_id, buffer, text_layout_id, 0.0f);

    if (!has_highlight_range) {
        i64 cursor_pos = view_get_cursor_pos(app, view_id);
        i64 mark_pos = view_get_mark_pos(app, view_id);

        ARGB_Color cursor_color = fcolor_resolve(fcolor_id(defcolor_cursor));
        ARGB_Color cursor_background = fcolor_resolve(fcolor_id(defcolor_cursor_background));
        ARGB_Color mark_color = fcolor_resolve(fcolor_id(defcolor_mark));
        ARGB_Color at_cursor_color = fcolor_resolve(fcolor_id(defcolor_at_cursor));

        switch (g_mode) {
        case MODAL_MODE_INSERT:
            cursor_color = fcolor_resolve(fcolor_id(defcolor_cursor_insert));
            break;
        case MODAL_MODE_EDIT:
            break;
        }

        if (is_active_view) {
            f32 thickness = 1.0f;

            Rect_f32 cursor_rect, cursor_top, cursor_bottom, cursor_left;
            Rect_f32 mark_rect, mark_top, mark_bottom, mark_left;

            cursor_rect = text_layout_character_on_screen(app, text_layout_id, cursor_pos);
            cursor_top = cursor_bottom = cursor_left = cursor_rect;

            cursor_bottom.y0 = cursor_bottom.y1 - thickness;
            cursor_top.y1 = cursor_top.y0 + thickness;
            cursor_left.x1 = cursor_left.x0 + thickness;


            mark_rect = text_layout_character_on_screen(app, text_layout_id, mark_pos);
            mark_top = mark_bottom = mark_left = mark_rect;

            mark_bottom.y0 = mark_bottom.y1 - thickness;
            mark_bottom.x1 = mark_bottom.x0 + (mark_bottom.x1 - mark_bottom.x0) *0.5f;

            mark_top.y1 = mark_top.y0 + thickness;
            mark_top.x1 = mark_top.x0 + (mark_top.x1 - mark_top.x0) * 0.5f;

            mark_left.x1 = mark_left.x0 + thickness;

            switch (g_mode) {
            case MODAL_MODE_INSERT:
                draw_rectangle(app, cursor_left, 0.0f, cursor_color);
                break;
            case MODAL_MODE_EDIT:
                draw_rectangle(app, cursor_rect, 0.0f, cursor_background);

                draw_rectangle(app, cursor_top, 0.0f, cursor_color);
                draw_rectangle(app, cursor_bottom, 0.0f, cursor_color);
                draw_rectangle(app, mark_top, 0.0f, mark_color);
                draw_rectangle(app, mark_bottom, 0.0f, mark_color);

                if (cursor_pos > mark_pos) {
                    draw_rectangle(app, cursor_left, 0.0f, cursor_color);
                    draw_rectangle(app, mark_left, 0.0f, mark_color);
                } else if (cursor_pos < mark_pos) {
                    draw_rectangle(app, cursor_left, 0.0f, cursor_color);
                    draw_rectangle(app, mark_left, 0.0f, mark_color);
                } else {
                    draw_rectangle(app, cursor_left, 0.0f, cursor_color);
                    draw_rectangle(app, mark_left, 0.0f, mark_color);
                }

                paint_text_color_pos(app, text_layout_id, cursor_pos, at_cursor_color);
                break;
            }

        } else {
            draw_character_wire_frame(app, text_layout_id, mark_pos, 0.0f, outline_thickness, mark_color);
            draw_character_wire_frame(app, text_layout_id, cursor_pos, 0.0f, outline_thickness, cursor_color);
        }
    }
}

static void custom_query_replace(
    Application_Links *app,
    View_ID view,
    String_Const_u8 prompt,
    Buffer_ID buffer,
    Range_i64 range)
{
    Query_Bar_Group group(app);

    Query_Bar replace = {};
    u8 replace_space[1024];
    replace.prompt = prompt;
    replace.string = SCu8(replace_space, (u64)0);
    replace.string_capacity = sizeof(replace_space);
    if (!query_user_string(app, &replace)) return;

    Query_Bar with = {};
    u8 with_space[1024];
    with.prompt = string_u8_litexpr("With: ");
    with.string = SCu8(with_space, (u64)0);
    with.string_capacity = sizeof(with_space);
    if (!query_user_string(app, &with)) return;

    String_Const_u8 r = replace.string;
    String_Const_u8 w = with.string;

    Query_Bar bar = {};
    bar.prompt = string_u8_litexpr("Replace? (y)es, (n)ext, (esc)\n");
    start_query_bar(app, &bar, 0);

    i64 new_pos = 0;
    i64 pos = range.start;
    seek_string_forward(app, buffer, range.start - 1, 0, r, &new_pos);

    User_Input in = {};
    for (;new_pos < range.max;){
        Range_i64 match = Ii64(new_pos, new_pos + r.size);
        isearch__update_highlight(app, view, match);

        in = get_next_input(app, EventProperty_AnyKey, EventProperty_MouseButton);
        if (in.abort || match_key_code(&in, KeyCode_Escape) || !is_unmodified_key(&in.event)){
            break;
        }

        if (match_key_code(&in, KeyCode_Y) ||
            match_key_code(&in, KeyCode_Return) ||
            match_key_code(&in, KeyCode_Tab)){
            buffer_replace_range(app, buffer, match, w);
            pos = match.start + w.size;
        }
        else{
            pos = match.max;
        }

        seek_string_forward(app, buffer, pos, 0, r, &new_pos);
    }

    view_disable_highlight_range(app, view);

    if (in.abort){
        return;
    }

    view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
    reset_preferred_column(app);
}


static void custom_render_buffer(
    Application_Links *app,
    View_ID view_id,
    Buffer_ID buffer,
    Text_Layout_ID text_layout_id,
    Rect_f32 rect)
{
    ProfileScope(app, "render buffer");

    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = (active_view == view_id);
    Rect_f32 prev_clip = draw_set_clip(app, rect);

    // NOTE(allen): Token colorizing
    Token_Array token_array = get_token_array_from_buffer(app, buffer);
    if (token_array.tokens != 0){
        F4_SyntaxHighlight(app, text_layout_id, &token_array);

        // NOTE(allen): Scan for TODOs and NOTEs
        if (def_get_config_b32(vars_save_string_lit("use_comment_keywords"))) {
            Comment_Highlight_Pair pairs[] = {
                {string_u8_litexpr("NOTE"), finalize_color(defcolor_comment_note, 0)},
                {string_u8_litexpr("TODO"), finalize_color(defcolor_comment_todo, 0)},
            };
            draw_comment_highlights(app, buffer, text_layout_id, &token_array, pairs, ArrayCount(pairs));
        }
    } else {
        Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
        paint_text_color_fcolor(app, text_layout_id, visible_range, fcolor_id(defcolor_text_default));
    }

    i64 cursor_pos = view_correct_cursor(app, view_id);
    view_correct_mark(app, view_id);

    // NOTE(allen): Scope highlight
    if (def_get_config_b32(vars_save_string_lit("use_scope_highlight"))) {
        Color_Array colors = finalize_color_array(defcolor_back_cycle);
        draw_scope_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
    }

    // NOTE(allen): Color parens
    if (def_get_config_b32(vars_save_string_lit("use_paren_helper"))) {
        Color_Array colors = finalize_color_array(defcolor_text_cycle);
        draw_paren_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
    }

    // NOTE(rjf): Brace highlight
    {
        Color_Array colors = finalize_color_array(fleury_color_brace_highlight);
        if(colors.count >= 1 && F4_ARGBIsValid(colors.vals[0]))
        {
            F4_Brace_RenderHighlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
        }
    }


    // NOTE(allen): Line highlight
    if (def_get_config_b32(vars_save_string_lit("highlight_line_at_cursor")) && is_active_view) {
        i64 line_number = get_line_number_from_pos(app, buffer, cursor_pos);
        auto line_highlight_color = global_keyboard_macro_is_recording
            ? fcolor_id(defcolor_highlight_cursor_line_recording)
            : fcolor_id(defcolor_highlight_cursor_line);
        draw_line_highlight(app, text_layout_id, line_number, line_highlight_color);
    }

    // NOTE(rjf): Error annotations & highlights
    b32 use_error_highlight = def_get_config_b32(vars_save_string_lit("use_error_highlight"));
    for (i32 i = 0; i < JUMP_BUFFER_COUNT; i++) {
        if (g_jump_buffers[i].type == JUMP_BUFFER_CMD_SYSTEM_PROC && 
            g_jump_buffers[i].buffer_id != buffer) 
        {
            if (use_error_highlight) {
                draw_jump_highlights(
                    app, 
                    buffer, 
                    text_layout_id, 
                    g_jump_buffers[i].buffer_id, 
                    fcolor_id(defcolor_highlight_junk));
            }

            F4_RenderErrorAnnotations(
                app, 
                buffer, 
                text_layout_id, 
                g_jump_buffers[i].buffer_id);
        }
    }

    custom_draw_cursor(app, view_id, is_active_view, buffer, text_layout_id, 2.0f);

    // NOTE(allen): put the actual text on the actual screen
    draw_text_layout_default(app, text_layout_id);
    draw_set_clip(app, prev_clip);

    // NOTE(rjf): Draw inactive rectangle
    if(is_active_view == 0 && view_id != g_jump_view)
    {
        Rect_f32 view_rect = view_get_screen_rect(app, view_id);
        ARGB_Color color = fcolor_resolve(fcolor_id(fleury_color_inactive_pane_overlay));
        if(F4_ARGBIsValid(color))
        {
            draw_rectangle(app, view_rect, 0.f, color);
        }
    }
}

static void custom_view_change_buffer(
    Application_Links *app, 
    View_ID view_id,
    Buffer_ID old_buffer_id, 
    Buffer_ID new_buffer_id)
{
    default_view_change_buffer(app, view_id, old_buffer_id, new_buffer_id);

    Command_Map_ID map_id = 0;
    switch (g_mode) {
    case MODAL_MODE_INSERT: map_id = vars_save_string_lit("keys_insert"); break;
    case MODAL_MODE_EDIT:   map_id = vars_save_string_lit("keys_edit"); break;
    }
    
    b32 automatically_index_code = def_get_config_b32(vars_save_string_lit("automatically_index_code"), false);
    if (!automatically_index_code && should_treat_as_code(app, new_buffer_id)) {
        Managed_Scope scope = buffer_get_managed_scope(app, new_buffer_id);
        if (scope != 0) {
            Token_Array *tokens = scope_attachment(app, scope, attachment_tokens, Token_Array);
            
            if (tokens->count == 0) {
                Async_Task *lex_task_ptr = scope_attachment(app, scope, buffer_lex_task, Async_Task);
                *lex_task_ptr = async_task_no_dep(&global_async_system, F4_DoFullLex_ASYNC, make_data_struct(&new_buffer_id));

            }
        }
    }


    Managed_Scope scope = buffer_get_managed_scope(app, new_buffer_id);
    Command_Map_ID *map_id_ptr = scope_attachment(app, scope, buffer_map_id, Command_Map_ID);
    *map_id_ptr = map_id;
}


static void custom_render_caller(
    Application_Links *app,
    Frame_Info frame_info,
    View_ID view_id)
{
    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = (active_view == view_id);

    Rect_f32 region = draw_background_and_margin(app, view_id, is_active_view, 1.0f);
    Rect_f32 prev_clip = draw_set_clip(app, region);

    Buffer_ID buffer = view_get_buffer(app, view_id, Access_Always);
    Face_ID face_id = get_face_id(app, buffer);
    Face_Metrics face_metrics = get_face_metrics(app, face_id);
    f32 line_height = face_metrics.line_height;
    f32 digit_advance = face_metrics.decimal_digit_advance;

    // NOTE(allen): file bar
    b64 showing_file_bar = false;
    if (view_get_setting(app, view_id, ViewSetting_ShowFileBar, &showing_file_bar) && showing_file_bar){
        Rect_f32_Pair pair = layout_file_bar_on_top(region, line_height);
        draw_file_bar(app, view_id, buffer, face_id, pair.min);
        region = pair.max;
    }

    Buffer_Scroll scroll = view_get_buffer_scroll(app, view_id);
    Buffer_Point_Delta_Result delta = delta_apply(app, view_id, frame_info.animation_dt, scroll);
    if (!block_match_struct(&scroll.position, &delta.point)){
        block_copy_struct(&scroll.position, &delta.point);
        view_set_buffer_scroll(app, view_id, scroll, SetBufferScroll_NoCursorChange);
    }
    if (delta.still_animating){
        animate_in_n_milliseconds(app, 0);
    }

    region = default_draw_query_bars(app, region, view_id, face_id);

    // NOTE(allen): FPS hud
    if (show_fps_hud){
        Rect_f32_Pair pair = layout_fps_hud_on_bottom(region, line_height);
        draw_fps_hud(app, frame_info, face_id, pair.max);
        region = pair.min;
        animate_in_n_milliseconds(app, 1000);
    }

    // NOTE(allen): layout line numbers
    Rect_f32 line_number_rect = {};
    if (def_get_config_b32(vars_save_string_lit("show_line_number_margins"))) {
        Rect_f32_Pair pair = layout_line_number_margin(app, buffer, region, digit_advance);
        line_number_rect = pair.min;
        region = pair.max;
    }

    if (view_id == g_jump_view) {
        Face_ID jump_panel_face = global_styled_label_face;
        Face_Metrics metrics = get_face_metrics(app, jump_panel_face);

        f32 total_width = region.x1 - region.x0;
        f32 height = metrics.line_height;
        f32 height_pad = 2.0f;

        u8 mem[256];
        String_u8 key = Su8(mem, 0, sizeof mem);

        ARGB_Color bg = fcolor_resolve(fcolor_id(defcolor_jump_buffer_background));
        ARGB_Color fg = fcolor_resolve(fcolor_id(defcolor_jump_buffer_foreground));

        ARGB_Color bg_active = fcolor_resolve(fcolor_id(defcolor_jump_buffer_background_active));
        ARGB_Color fg_active = fcolor_resolve(fcolor_id(defcolor_jump_buffer_foreground_active));

        ARGB_Color bg_cmd_fail = fcolor_resolve(fcolor_id(defcolor_jump_buffer_background_cmd_fail));
        ARGB_Color bg_cmd_executing = fcolor_resolve(fcolor_id(defcolor_jump_buffer_background_cmd_executing));


        ARGB_Color fg_sticky = fcolor_resolve(fcolor_id(defcolor_jump_buffer_sticky));

        Rect_f32 jump_region = region;
        jump_region.y1 = jump_region.y0 + height + height_pad;
        jump_region.x1 = jump_region.x0;

        for (i32 i = 0; i < JUMP_BUFFER_COUNT; i++) {
            jump_region.x0 = jump_region.x1;
            jump_region.x1 = (i+1) * total_width / JUMP_BUFFER_COUNT;

            JumpBufferCmd *jb = &g_jump_buffers[i];

            ARGB_Color background = (i == g_active_jump_buffer) ? bg_active : bg;
            ARGB_Color foreground = (i == g_active_jump_buffer) ? fg_active : fg;

            key.size = 0;
            string_append(&key, string_u8_litexpr("F"));
            string_append_character(&key, (u8)('0' + i + 1));

            Vec2_f32 pos = {};
            pos.x = jump_region.x0;
            pos.y = jump_region.y0 + 1.0f;

            if (jb->type == JUMP_BUFFER_CMD_SYSTEM_PROC) {

                if (!jb->system.has_exit || jb->system.status == -1) {
                    Marker_List *jump_list = get_or_make_list_for_buffer(app, &global_heap, jb->buffer_id);

                    if (jump_list) {
                        for (i32 ji = 0; ji < jump_list->jump_count; ji++) {
                            i64 line_number = get_line_from_list(app, jump_list, ji);

                            Scratch_Block scratch(app);
                            String_Const_u8 line = push_buffer_line(app, scratch, jb->buffer_id, line_number);
                            if (string_find_first(line, string_u8_litexpr("error"), StringMatch_CaseInsensitive) != line.size) {
                                jb->system.status = 0;
                                goto done_error_check;
                            }
                        }

                        if (jb->system.has_exit) jb->system.status = 1;
                    }
                }

done_error_check:
                if (jb->system.status == 0) {
                    background = bg_cmd_fail;
                } else if (!jb->system.has_exit && jb->system.status == -1) {
                    background = bg_cmd_executing;
                }
            }

            draw_rectangle(app, jump_region, 0.0f, background);

            pos = draw_string_oriented(app, jump_panel_face, foreground, string_u8_litexpr("["), pos, 0, V2f32(1.0f, 0.0f));
            pos = draw_string_oriented(app, jump_panel_face, jb->sticky ? fg_sticky : foreground, SCu8(key), pos, 0, V2f32(1.0f, 0.0f));
            pos = draw_string_oriented(app, jump_panel_face, foreground, string_u8_litexpr("]"), pos, 0, V2f32(1.0f, 0.0f));

            pos = draw_string_oriented(app, jump_panel_face, foreground, SCu8(jb->label, jb->label_size), pos, 0, V2f32(1.0f, 0.0f));

        }

        region.y0 += height + height_pad + 2.0f;
    }

    // NOTE(allen): begin buffer render
    Buffer_Point buffer_point = scroll.position;
    Text_Layout_ID text_layout_id = text_layout_create(app, buffer, region, buffer_point);

    // NOTE(allen): draw line numbers
    if (def_get_config_b32(vars_save_string_lit("show_line_number_margins"))) {
        draw_line_number_margin(app, view_id, buffer, face_id, text_layout_id, line_number_rect);
    }

    custom_render_buffer(app, view_id, buffer, text_layout_id, region);

    u32 annotation_flags = vertical_scope_annotation_flag_top_to_bottom;
    vertical_scope_annotation_draw(app, view_id, buffer, text_layout_id, annotation_flags);

    text_layout_free(app, text_layout_id);
    draw_set_clip(app, prev_clip);
}

static void custom_tick(Application_Links *app, Frame_Info frame_info)
{
    linalloc_clear(&global_frame_arena);

    i32 exit_code_str_length = (i32)strlen("exited with code");
    for (i32 i = 0; i < JUMP_BUFFER_COUNT; i++) {
        JumpBufferCmd *jb = &g_jump_buffers[i];
        if (jb->type == JUMP_BUFFER_CMD_SYSTEM_PROC && !jb->system.has_exit) {
            i64 buffer_size = buffer_get_size(app, jb->buffer_id);
            if (buffer_size >= exit_code_str_length+2) {
                Scratch_Block scratch(app);

                Range_i64 range;
                range.start = buffer_size - (exit_code_str_length+2);
                range.end = buffer_size;

                u8 *data = push_array(scratch, u8, exit_code_str_length+1);
                if (buffer_read_range(app, jb->buffer_id, range, data)) {
                    if (strncmp("exited with code", (char*)data, exit_code_str_length) == 0) {
                        jb->system.has_exit = true;
                    }
                }
            }
        }
    }

    F4_TickColors(app, frame_info);
    F4_Index_Tick(app);

    default_tick(app, frame_info);
}

CUSTOM_COMMAND_SIG(move_word)
{
    View_ID view = get_active_view(app, Access_Always);
    i64 pos = seek_next_word(app, view, view_get_cursor_pos(app, view));
    view_set_cursor(app, view, seek_pos(pos));
    reset_preferred_column(app);
}

CUSTOM_COMMAND_SIG(move_word_back)
{
    View_ID view = get_active_view(app, Access_Always);
    i64 pos = seek_prev_word(app, view, view_get_cursor_pos(app, view));
    view_set_cursor(app, view, seek_pos(pos));
    reset_preferred_column(app);
}

CUSTOM_COMMAND_SIG(move_matching_scope)
{
    View_ID view = get_active_view(app, Access_Always);
    i64 pos = seek_matching_scope(app, view, view_get_cursor_pos(app, view));
    view_set_cursor(app, view, seek_pos(pos));
    reset_preferred_column(app);
}

CUSTOM_COMMAND_SIG(seek_whitespace_up)
{
    seek_blank_line(app, Scan_Backward, PositionWithinLine_Start);
    reset_preferred_column(app);
}

CUSTOM_COMMAND_SIG(seek_whitespace_down)
{
    seek_blank_line(app, Scan_Forward, PositionWithinLine_Start);
    reset_preferred_column(app);
}

CUSTOM_COMMAND_SIG(seek_char)
{
    Query_Bar_Group group(app);
    Query_Bar bar = {};
    if (start_query_bar(app, &bar, 0) == 0) return;

    bar.prompt = string_u8_litexpr("Seek to char: ");

    for (;;) {
        User_Input in = get_next_input(
            app,
            EventPropertyGroup_AnyKeyboardEvent,
            EventProperty_Escape | EventProperty_ViewActivation);

        if (in.abort) return;

        String_Const_u8 str = to_writable(&in);
        if (str.str != nullptr && str.size > 0) {

            View_ID view = get_active_view(app, Access_Always);
            Buffer_ID buffer = view_get_buffer(app, view, Access_Always);

            i64 cursor_pos = view_get_cursor_pos(app, view);
            i64 buffer_size = buffer_get_size(app, buffer);
            i64 result_pos = 0;
            seek_string_forward(app, buffer, cursor_pos, 0, str, &result_pos);

            if (result_pos < buffer_size) {
                view_set_cursor(app, view, seek_pos(result_pos));
                reset_preferred_column(app);
            }
            return;
        } else {
            leave_current_input_unhandled(app);
        }
    }
}

CUSTOM_COMMAND_SIG(custom_cut)
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    Range_i64 range = get_view_range(app, view);

    if (range.start == range.end) {
        range.end += 1;
    }

    if (clipboard_post_buffer_range(app, 0, buffer, range)){
        buffer_replace_range(app, buffer, range, string_u8_empty);
        reset_preferred_column(app);
    }
}

CUSTOM_COMMAND_SIG(custom_delete_range)
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    Range_i64 range = get_view_range(app, view);

    if (range.start == range.end) {
        range.end += 1;
    }

    buffer_replace_range(app, buffer, range, string_u8_empty);
    reset_preferred_column(app);
}

CUSTOM_COMMAND_SIG(delete_range_lines)
{
    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
    Range_i64 range = get_mark_cursor_lines_range(app, view, buffer);
    buffer_replace_range(app, buffer, range, string_u8_empty);
    reset_preferred_column(app);
}

CUSTOM_COMMAND_SIG(cut_range_lines)
{
    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
    Range_i64 range = get_mark_cursor_lines_range(app, view, buffer);

    if (clipboard_post_buffer_range(app, 0, buffer, range)) {
        buffer_replace_range(app, buffer, range, string_u8_empty);
        reset_preferred_column(app);
    }
}

CUSTOM_COMMAND_SIG(copy_range_lines)
{
    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
    Range_i64 range = get_mark_cursor_lines_range(app, view, buffer);

    clipboard_post_buffer_range(app, 0, buffer, range);
}

CUSTOM_COMMAND_SIG(custom_replace_range)
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);

    if (buffer != 0) {
        Scratch_Block scratch(app);
        Range_i64 range = get_view_range(app, view);
        custom_query_replace(app, view, string_u8_litexpr("Replace (range):"), buffer, range);
    }
}

CUSTOM_COMMAND_SIG(custom_replace_range_lines)
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);

    if (buffer != 0) {
        Scratch_Block scratch(app);
        Range_i64 range = get_mark_cursor_lines_range(app, view, buffer);
        custom_query_replace(app, view, string_u8_litexpr("Replace (range, lines):"), buffer, range);
    }
}

CUSTOM_COMMAND_SIG(custom_replace_file)
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);

    if (buffer != 0) {
        Scratch_Block scratch(app);
        Range_i64 range = Ii64(0, buffer_get_size(app, buffer));
        custom_query_replace(app, view, string_u8_litexpr("Replace (file):"), buffer, range);
    }
}

CUSTOM_UI_COMMAND_SIG(new_file)
    CUSTOM_DOC("Interactively creates a new file without setting hot dir")
{
    Scratch_Block scratch0(app);
    String_Const_u8 hot_dir = push_hot_directory(app, scratch0);

    for (;;) {
        Scratch_Block scratch(app);

        View_ID view = get_this_ctx_view(app, Access_Always);
        File_Name_Result result = get_file_name_from_user(app, scratch, "New:",
                                                          view);
        if (result.canceled) break;

        // NOTE(allen): file_name from the text field always
        // unless this is a folder or a mouse click.
        String_Const_u8 file_name = result.file_name_in_text_field;
        if (result.is_folder || result.clicked){
            file_name = result.file_name_activated;
        }
        if (file_name.size == 0) break;

        String_Const_u8 path = result.path_in_text_field;
        String_Const_u8 full_file_name =
            push_u8_stringf(scratch, "%.*s/%.*s",
                            string_expand(path), string_expand(file_name));

        if (result.is_folder){
            set_hot_directory(app, full_file_name);
            continue;
        }

        if (character_is_slash(file_name.str[file_name.size - 1])) {
            File_Attributes attribs = system_quick_file_attributes(scratch, full_file_name);
            if (HasFlag(attribs.flags, FileAttribute_IsDirectory)) {
                set_hot_directory(app, full_file_name);
                continue;
            }

            if (string_looks_like_drive_letter(file_name)){
                set_hot_directory(app, file_name);
                continue;
            }
            if (query_create_folder(app, file_name)){
                set_hot_directory(app, full_file_name);
                continue;
            }
            break;
        }

        Buffer_Create_Flag flags = BufferCreate_AlwaysNew;
        Buffer_ID buffer = create_buffer(app, full_file_name, flags);
        if (buffer != 0){
            view_set_buffer(app, view, buffer, 0);
        }

        break;
    }

    set_hot_directory(app, hot_dir);
}

CUSTOM_COMMAND_SIG(cut_to_end_of_line)
{
    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);

    i64 start = view_get_cursor_pos(app, view);
    i64 line = get_line_number_from_pos(app, buffer, start);
    i64 end = get_line_end_pos(app, buffer, line);

    Range_i64 range = { start, end };
    if (clipboard_post_buffer_range(app, 0, buffer, range)) {
        buffer_replace_range(app, buffer, range, string_u8_empty);
        reset_preferred_column(app);
    }
}

CUSTOM_COMMAND_SIG(delete_to_end_of_line)
{
    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);

    i64 start = view_get_cursor_pos(app, view);
    i64 line = get_line_number_from_pos(app, buffer, start);
    i64 end = get_line_end_pos(app, buffer, line);

    Range_i64 range = { start, end };
    buffer_replace_range(app, buffer, range, string_u8_empty);
    reset_preferred_column(app);
}


CUSTOM_COMMAND_SIG(combine_with_next_line)
{
    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);

    seek_end_of_line(app);
    i64 start = view_get_cursor_pos(app, view);
    move_down_textual(app);
    seek_beginning_of_line(app);
    move_past_lead_whitespace(app, view, buffer);
    i64 end = view_get_cursor_pos(app, view);

    u8 space[1] = {' '};
    buffer_replace_range(app, buffer, Ii64(start, end), SCu8(space, sizeof space));
    move_right(app);

    reset_preferred_column(app);
}

CUSTOM_COMMAND_SIG(move_beginning_of_line)
{
    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);

    History_Group group = history_group_begin(app, buffer);
    seek_beginning_of_line(app);
    move_past_lead_whitespace(app, view, buffer);
    history_group_end(group);

    reset_preferred_column(app);
}

CUSTOM_COMMAND_SIG(move_end_of_line)
{
    seek_end_of_line(app);
    reset_preferred_column(app);
}

CUSTOM_COMMAND_SIG(query_replace_in_all_buffers)
{
    global_history_edit_group_begin(app);

    Scratch_Block scratch(app);
    Query_Bar_Group group(app);
    String_Pair pair = query_user_replace_pair(app, scratch);

    Query_Bar bar = {};
    bar.prompt = string_u8_litexpr("Replace? (y)es, (n)ext, (esc)\n");
    start_query_bar(app, &bar, 0);

    View_ID view = get_active_view(app, Access_Always);

    for (Buffer_ID buffer = get_buffer_next(app, 0, Access_ReadWriteVisible);
         buffer != 0;
         buffer = get_buffer_next(app, buffer, Access_ReadWriteVisible))
    {
        query_replace_base(app, view, buffer, 0, pair.a, pair.b);
    }

    global_history_edit_group_end(app);
}

CUSTOM_COMMAND_SIG(custom_isearch_cmd)
{
    i32 ji = push_jump_buffer(JUMP_BUFFER_CMD_BUFFER_SEARCH, g_active_jump_buffer);
    set_active_jump_buffer(app, ji);
    JumpBufferCmd *jb = &g_jump_buffers[ji];

    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID active_buffer = view_get_buffer(app, view, Access_Always);

    custom_isearch(app, jb, view, active_buffer, SCu8());
}

CUSTOM_COMMAND_SIG(custom_search_all_buffers_cmd)
{
    i32 ji = push_jump_buffer(JUMP_BUFFER_CMD_GLOBAL_SEARCH, g_active_jump_buffer);
    set_active_jump_buffer(app, ji);
    JumpBufferCmd *jb = &g_jump_buffers[ji];

    Scratch_Block scratch(app);
    u8 *space = push_array(scratch, u8, KB(1));
    String_Const_u8 query = get_query_string(app, "search: ", space, KB(1));
    if (query.size != 0) {
        String_Const_u8_Array array = { &query, 1 };

        String_Match_Flag must_have_flags = 0;
        String_Match_Flag must_not_have_flags = 0;

        jb->global_search.query.str = (u8*)heap_realloc(
            &global_heap,
            jb->global_search.query.str, jb->global_search.query.size,
            query.size);
        jb->global_search.query.size = query.size;
        block_copy(jb->global_search.query.str, query.str, query.size);

        jb->label_size = Min((i32)sizeof jb->label, (i32)query.size);
        block_copy(jb->label, query.str, jb->label_size);
        print_all_matches_all_buffers(app, array, must_have_flags, must_not_have_flags, jb->buffer_id);
    }
}

CUSTOM_COMMAND_SIG(custom_compile_cmd)
CUSTOM_DOC("push a system command onto jump buffer")
{
    Scratch_Block scratch(app);
    View_ID active_view = get_active_view(app, Access_Always);

    i32 ji = push_jump_buffer(JUMP_BUFFER_CMD_SYSTEM_PROC, g_active_jump_buffer);
    JumpBufferCmd *jb = &g_jump_buffers[ji];
    clear_buffer(app, jb->buffer_id);
    set_active_jump_buffer(app, ji);

    if (jb->system.cmd.size == 0) {
        File_Name_Result result = query_file_path(app, scratch, SCu8("build script: "), active_view);
        if (result.canceled || result.is_folder) {
            jb->type = JUMP_BUFFER_CMD_NONE;
            return;
        }

        String_Const_u8 cmd = result.file_name_activated;
        String_Const_u8 path = result.path_in_text_field;

        if (cmd.size > jb->system.cmd.size) {
            jb->system.cmd.str = (u8*)heap_realloc(
                &global_heap,
                jb->system.cmd.str, 
                jb->system.cmd.size,
                cmd.size);
        }
        block_copy(jb->system.cmd.str, cmd.str, cmd.size);
        jb->system.cmd.size = cmd.size;

        if (path.size > jb->system.path.size) {
            jb->system.path.str = (u8*)heap_realloc(
                &global_heap,
                jb->system.path.str, 
                jb->system.path.size,
                path.size);
        }
        block_copy(jb->system.path.str, path.str, path.size);
        jb->system.path.size = path.size;

        jb->label_size = (i32)Min(sizeof jb->label, cmd.size);
        block_copy(jb->label, cmd.str, jb->label_size);
    }

    String_Const_u8 cmd = jb->system.cmd;
    String_Const_u8 path = jb->system.path;

    Process_State state = child_process_get_state(app, jb->system.process);
    jb->system.process = custom_compile_project(app, jb, path, cmd);
}

CUSTOM_COMMAND_SIG(toggle_sticky_jump_buffer)
{
    g_jump_buffers[g_active_jump_buffer].sticky = !g_jump_buffers[g_active_jump_buffer].sticky;
}

CUSTOM_COMMAND_SIG(custom_paste)
{
    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);

    History_Group group = history_group_begin(app, buffer);
    paste(app);
    custom_auto_indent_range(app);
    history_group_end(group);
}

CUSTOM_COMMAND_SIG(custom_paste_next)
{
    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);

    History_Group group = history_group_begin(app, buffer);
    paste_next(app);
    custom_auto_indent_range(app);
    history_group_end(group);
}

CUSTOM_COMMAND_SIG(custom_fuzzy_find_file)
{
    ProfileScope(app, "custom_fuzzy_find_file");
    Buffer_ID buffer = fuzzy_file_lister(app, SCu8((u8*)0, (u64)0));
    if (buffer != 0) {
        View_ID view = get_this_ctx_view(app, Access_Always);
        view_set_buffer(app, view, buffer, 0);
    }
}

CUSTOM_COMMAND_SIG(custom_fuzzy_find_buffer)
{
    ProfileScope(app, "custom_fuzzy_find_buffer");
    Buffer_ID buffer = fuzzy_buffer_lister(app, SCu8((u8*)0, (u64)0));
    if (buffer != 0) {
        View_ID view = get_this_ctx_view(app, Access_Always);
        view_set_buffer(app, view, buffer, 0);
    }
}


CUSTOM_COMMAND_SIG(find_corresponding_file)
CUSTOM_DOC("Find the corresponding header/source file")
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);

    Scratch_Block scratch(app);
    String_Const_u8 buffer_file_name = push_buffer_file_name(app, scratch, buffer);
    if (buffer_file_name.size > 0) {
        String_Const_u8 ext = string_file_extension(buffer_file_name);
        String_Const_u8 filename = string_file_without_extension(buffer_file_name);
        
        i64 slash_pos = string_find_last_slash(filename);
        if (slash_pos > 0) {
            filename.str += slash_pos + 1;
            filename.size -= slash_pos + 1;
        }

        String_Const_u8 corresponding{};

        String_Const_u8 h = string_u8_litexpr("h");
        String_Const_u8 hpp = string_u8_litexpr("hpp");
        String_Const_u8 c = string_u8_litexpr("c");
        String_Const_u8 cpp = string_u8_litexpr("cpp");

        if (string_match(cpp, ext) || string_match(c, ext)) {
            corresponding = push_u8_stringf(scratch, "%.*s.%.*s", string_expand(filename), string_expand(h));
        } else if (string_match(h, ext) || string_match(hpp, ext)) {
            corresponding = push_u8_stringf(scratch, "%.*s.%.*s", string_expand(filename), string_expand(c));
        }

        if (corresponding.size > 0) {
            Buffer_ID found = fuzzy_file_lister(app, corresponding);
            if (found != 0) {
                view_set_buffer(app, view, found, 0);
            }
        }
    }
}

CUSTOM_COMMAND_SIG(custom_fuzzy_command_lister)
{
    View_ID view = get_this_ctx_view(app, Access_Always);
    if (view == 0) return;

    auto fuzzy_lister_generate_commands = [](Application_Links *app, Lister *lister)
    {
        lister_begin_new_item_set(app, lister);
        Scratch_Block scratch(app, lister->arena);
        
        i32 *command_ids = nullptr;
        i32 command_id_count = 0;

        Command_Lister_Status_Rule status_rule = {};
        
        View_ID view = get_this_ctx_view(app, Access_Always);
        if (view == 0) return;

        Buffer_ID buffer = view_get_buffer(app, view, Access_Visible);
        Managed_Scope buffer_scope = buffer_get_managed_scope(app, buffer);
        Command_Map_ID *map_id_ptr = scope_attachment(app, buffer_scope, buffer_map_id, Command_Map_ID);

        if (map_id_ptr != 0) {
            status_rule = command_lister_status_bindings(&framework_mapping, *map_id_ptr);
        } else {
            status_rule = command_lister_status_descriptions();
        }

        if (command_ids == 0){
            command_id_count = command_one_past_last_id;
        }



        for (i32 i = 0; i < command_id_count; i += 1){
            i32 j = i;
            if (command_ids != 0){
                j = command_ids[i];
            }
            j = clamp(0, j, command_one_past_last_id);

            Custom_Command_Function *proc = fcoder_metacmd_table[j].proc;
            String_Const_u8 status = {};
            switch (status_rule.mode){
            case CommandLister_Descriptions:
                {
                    status = SCu8(fcoder_metacmd_table[j].description);
                }break;
            case CommandLister_Bindings:
                {
                    Command_Trigger_List triggers = map_get_triggers_recursive(
                        scratch, 
                        status_rule.mapping, 
                        status_rule.map_id, 
                        proc);

                    List_String_Const_u8 list = {};
                    for (Command_Trigger *node = triggers.first;
                         node != 0;
                         node = node->next){
                        command_trigger_stringize(scratch, &list, node);
                        if (node->next != 0){
                            string_list_push(scratch, &list, string_u8_litexpr(" "));
                        }
                    }

                    status = string_list_flatten(scratch, list);
                }break;
            }

            fuzzy_lister_add_node(app, lister, SCu8(fcoder_metacmd_table[j].name), status, (void*)proc);
        }
    };
    
    Lister_Handlers handlers = fuzzy_lister_handlers(fuzzy_lister_generate_commands);
    Lister_Result l_result = fuzzy_lister(app, &handlers, string_u8_litexpr("command:"), string_u8_litexpr(""));

    Custom_Command_Function *func = 0;
    if (!l_result.canceled){
        func = (Custom_Command_Function*)l_result.user_data;
    }

    if (func != 0){
        view_enqueue_command_function(app, view, func);
    }
}

CUSTOM_COMMAND_SIG(toggle_macro_record)
{
    if (global_keyboard_macro_is_recording) {
        keyboard_macro_finish_recording(app);
    } else {
        keyboard_macro_start_recording(app);
    }
}

CUSTOM_COMMAND_SIG(replay_macro)
CUSTOM_DOC("replay recorded keyboard macro")
{
    // NOTE(jesper): I'd like to treat this as one history group, but because the way the macros
    // in 4coder work with enqueing virtual input events this does not seem possible
    keyboard_macro_replay(app);
}

CUSTOM_COMMAND_SIG(toggle_large_jump_view)
CUSTOM_DOC("switch between a small and a large jump view")
{
    View_ID main_view = get_active_view(app, Access_Always);
    Face_ID face_id = get_face_id(app, view_get_buffer(app, main_view, Access_Always));
    Face_Metrics metrics = get_face_metrics(app, face_id);
    
    // TODO(jesper): should we maybe remember manual resize heights by hooking into the
    // view adjust hook?
    i32 small_height = (i32)(metrics.line_height*3.0f + 5.0f);
    i32 large_height = (i32)(metrics.line_height*20.0f + 5.0f);

    Rect_f32 view_rect = view_get_screen_rect(app, g_jump_view);
    if (view_rect.y1 - view_rect.y0 <= large_height*0.5f) {
        view_set_split_pixel_size(app, g_jump_view, large_height);
    } else {
        view_set_split_pixel_size(app, g_jump_view, small_height);
    }
}

CUSTOM_COMMAND_SIG(replay_macro_lines)
CUSTOM_DOC("replay the recorded macro on each line in the range given by mark and cursor")
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);

    if (buffer != 0) {
        if (global_keyboard_macro_is_recording ||
            get_current_input_is_virtual(app)){
            return;
        }
        
        i64 cursor_pos = view_get_cursor_pos(app, view);
        i64 mark_pos = view_get_mark_pos(app, view);

        Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(cursor_pos));
        Buffer_Cursor mark = view_compute_cursor(app, view, seek_pos(mark_pos));

        Buffer_Cursor start = cursor.line > mark.line ? mark : cursor;
        Buffer_Cursor end = cursor.line > mark.line ? cursor : mark;

        Buffer_ID macro_buffer = get_keyboard_log_buffer(app);
        Scratch_Block scratch(app);
        String_Const_u8 macro = push_buffer_range(app, scratch, macro_buffer, global_keyboard_macro_range);
        
        // NOTE(jesper): this seems super hacky because the 4coder macro system works be enqueing events parsed
        // from the keyboard log, which is fine. But it means in order to perform a macro across muliple lines 
        // correctly, we need to enqueue the event which moves cursor down
        
        view_set_cursor(app, view, seek_pos(start.pos));
        reset_preferred_column(app);
        keyboard_macro_play(app, macro);

        for (i64 line = start.line+1; line <= end.line; line++) {
            Input_Event down{};
            down.kind = InputEventKind_KeyStroke;
            down.key.code = KeyCode_J;
            enqueue_virtual_event(app, &down);
            
            keyboard_macro_play(app, macro);
        }

        reset_preferred_column(app);
    }
}

static void jump_buffer_cmd(Application_Links *app, i32 jump_buffer_index)
{
    if (g_active_jump_buffer == jump_buffer_index) {
        // TODO(jesper): do we check for ctrl in here to re-issue the
        // command in case of buffer search, or separate bind?
        JumpBufferCmd *jb = &g_jump_buffers[jump_buffer_index];
        
        switch (jb->type) {
        case JUMP_BUFFER_CMD_SYSTEM_PROC:
            // TODO(jesper): I'd really like to be able to terminate the process and re-issue the compile
            // at this point. With the current APIs, that might mean having to do my own CreateProcess and
            // figure out the thread safety requirements for accessing and writing into buffers from a custom
            // thread...
            if (jb->system.has_exit) {
                jb->system.has_exit = false;
                jb->system.process = custom_compile_project(app, jb, jb->system.path, jb->system.cmd);
            }
            break;
        case JUMP_BUFFER_CMD_BUFFER_SEARCH:
            {
                View_ID view = 0;
                Buffer_ID buffer = 0;
                
                User_Input input = get_current_input(app);
                if (has_modifier(&input, KeyCode_Control)) {
                    view = get_active_view(app, Access_Always);
                    buffer = view_get_buffer(app, view, Access_Always);
                    
                    JumpBufferCmd copy = duplicate_jump_buffer(jb);
                    i32 ji = push_jump_buffer(JUMP_BUFFER_CMD_BUFFER_SEARCH, jump_buffer_index);
                    jb = &g_jump_buffers[ji];
                    *jb = copy;
                    
                    jb->buffer_search.buffer = buffer;
                } else {
                    view = get_first_view_with_buffer(app, jb->buffer_search.buffer);
                    buffer = jb->buffer_search.buffer;
                    if (view_exists(app, view)) {
                        if (view != get_active_view(app, Access_Always)) {
                            // NOTE(jesper): in this case I think I might have wanted to switch
                            // the active buffer and continue with the inc search, but due to how
                            // 4coder works, input is specific to a view, so switching active
                            // view makes a subsequent query bar not receive any input.
                            view_set_active(app, view);
                            return;
                        }
                    } else {
                        
                        view = get_active_view(app, Access_Always);
                        view_set_buffer(app, view, jb->buffer_search.buffer, 0);
                    }
                }
                
                custom_isearch(app, jb, view, buffer, jb->buffer_search.query);
            } break;
        case JUMP_BUFFER_CMD_GLOBAL_SEARCH: 
            break;
        }
    } else {
        set_active_jump_buffer(app, jump_buffer_index);
    }
}

BUFFER_HOOK_SIG(custom_begin_buffer)
{
    ProfileScope(app, "begin buffer");
    
    Scratch_Block scratch(app);
    
    Command_Map_ID map_id = 0;
    switch (g_mode) {
    case MODAL_MODE_INSERT: map_id = vars_save_string_lit("keys_insert"); break;
    case MODAL_MODE_EDIT:   map_id = vars_save_string_lit("keys_edit"); break;
    }
        
    Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
    Command_Map_ID *map_id_ptr = scope_attachment(app, scope, buffer_map_id, Command_Map_ID);
    *map_id_ptr = map_id;
    
    Line_Ending_Kind setting = guess_line_ending_kind_from_buffer(app, buffer_id);
    Line_Ending_Kind *eol_setting = scope_attachment(app, scope, buffer_eol_setting, Line_Ending_Kind);
    *eol_setting = setting;
    
    bool treat_as_code = should_treat_as_code(app, buffer_id);
    
    // NOTE(allen): Decide buffer settings
    b32 wrap_lines = true;
    b32 use_lexer = treat_as_code;
    
    if (treat_as_code) {
        wrap_lines = def_get_config_b32(vars_save_string_lit("enable_code_wrapping"));
    }
    
    String_Const_u8 buffer_name = push_buffer_base_name(app, scratch, buffer_id);
    if (buffer_name.size > 0 && buffer_name.str[0] == '*' && buffer_name.str[buffer_name.size - 1] == '*'){
        wrap_lines = def_get_config_b32(vars_save_string_lit("enable_output_wrapping"));
    }
    
    b32 automatically_index_code = def_get_config_b32(vars_save_string_lit("automatically_index_code"), false);
    if (automatically_index_code && treat_as_code) {
        ProfileBlock(app, "begin buffer kick off lexer");
        Async_Task *lex_task_ptr = scope_attachment(app, scope, buffer_lex_task, Async_Task);
        *lex_task_ptr = async_task_no_dep(&global_async_system, F4_DoFullLex_ASYNC, make_data_struct(&buffer_id));
    }

    {
        b32 *wrap_lines_ptr = scope_attachment(app, scope, buffer_wrap_lines, b32);
        *wrap_lines_ptr = wrap_lines;
    }

    if (use_lexer) {
        buffer_set_layout(app, buffer_id, layout_virt_indent_index_generic);
    } else {
        if (treat_as_code) {
            buffer_set_layout(app, buffer_id, layout_virt_indent_literal_generic);
        } else {
            buffer_set_layout(app, buffer_id, layout_generic);
        }
    }

    // no meaning for return
    return(0);
}

DELTA_RULE_SIG(custom_delta_rule)
{
    if (def_get_config_b32(vars_save_string_lit("smooth_scrolling"))) {
        return fixed_time_cubic_delta(pending, is_new_target, dt, data);
    } else {
        return pending;
    }
}
global_const u64 custom_delta_rule_size = sizeof(f32);

void custom_layer_init(Application_Links *app)
{
    default_framework_init(app);
    
    global_frame_arena = make_arena(get_base_allocator_system());
    permanent_arena = make_arena(get_base_allocator_system());

    set_all_default_hooks(app);
    set_custom_hook(app, HookID_BeginBuffer, custom_begin_buffer);
    set_custom_hook(app, HookID_RenderCaller, custom_render_caller);
    set_custom_hook(app, HookID_Tick, custom_tick);
    set_custom_hook(app, HookID_ViewChangeBuffer, custom_view_change_buffer);
    
    set_custom_hook(app, HookID_DeltaRule, custom_delta_rule);
    set_custom_hook_memory_size(app, HookID_DeltaRule, delta_ctx_size(custom_delta_rule_size));

    Thread_Context *tctx = get_thread_context(app);
    mapping_init(tctx, &framework_mapping);
    
    custom_setup_necessary_bindings(&framework_mapping);

    F4_Index_Initialize();
    F4_RegisterLanguages();
    
    F4_RegisterLanguage(
        S8Lit("ino"),
        F4_CPP_IndexFile,
        lex_full_input_cpp_init,
        lex_full_input_cpp_breaks,
        F4_CPP_PosContext,
        F4_CPP_Highlight,
        Lex_State_Cpp);
}

CUSTOM_COMMAND_SIG(move_up_textual)
CUSTOM_DOC("Moves down to the next line of actual text, regardless of line wrapping.")
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    i64 pos = view_get_cursor_pos(app, view);
    Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(pos));
    i64 next_line = cursor.line - 1;
    view_set_cursor_and_preferred_x(app, view, seek_line_col(next_line, 1));
}

CUSTOM_COMMAND_SIG(custom_try_exit)
{
    User_Input input = get_current_input(app);
    if (match_core_code(&input, CoreCode_TryExit)) {
        b32 do_exit = true;
        
        if (has_dirty_buffers(app)) {
            ListerDirtyBuffersChoice choice = lister_handle_dirty_buffers(app);
            
            switch (choice) {
            case CHOICE_NULL:
            case CHOICE_OPEN_BUFFER:
            case CHOICE_CANCEL:
                do_exit = false;
                break;
            case CHOICE_SAVE_ALL:
                save_all_dirty_buffers(app);
            case CHOICE_DISCARD:
                do_exit = true;
                break;
            }
        }
        
        if (do_exit) hard_exit(app);
    }
}

static void custom_setup_fonts(Application_Links *app)
{
    Scratch_Block scratch(app);
    
    Face_ID default_font = get_face_id(app, 0);
    Face_Description default_font_desc = get_face_description(app, default_font);
    String_Const_u8 bin_path = system_get_path(scratch, SystemPath_Binary);

    // NOTE(rjf): Title font.
    {
        Face_Description desc{0};
        desc.font.file_name = push_u8_stringf(
            scratch, 
            "%.*sfonts/Ubuntu-Regular.ttf", 
            string_expand(bin_path));
        desc.parameters.pt_size = default_font_desc.parameters.pt_size + 4;

        if(IsFileReadable(desc.font.file_name)) {
            global_styled_title_face = try_create_new_face(app, &desc);
        } else {
            global_styled_title_face = default_font;
        }
    }

    // NOTE(rjf): Label font.
    {
        Face_Description desc{0};
        desc.font.file_name = push_u8_stringf(
            scratch, 
            "%.*sfonts/Ubuntu-Regular.ttf", 
            string_expand(bin_path));
        desc.parameters.pt_size = default_font_desc.parameters.pt_size - 4;

        if(IsFileReadable(desc.font.file_name)) {
            global_styled_label_face = try_create_new_face(app, &desc);
        } else {
            global_styled_label_face = default_font;
        }
    }

    // NOTE(rjf): Small code font.
    {
        Face_Description desc{0};
        desc.font.file_name = push_u8_stringf(
            scratch, 
            "%.*sfonts/UbuntuMono-R.ttf", 
            string_expand(bin_path));
        desc.parameters.pt_size = default_font_desc.parameters.pt_size - 4;

        if(IsFileReadable(desc.font.file_name)) {
            global_small_code_face = try_create_new_face(app, &desc);
        } else {
            global_small_code_face = default_font;
        }
    }
}

static void custom_setup_necessary_bindings(Mapping *mapping)
{
    String_ID global_map_id = vars_save_string_lit("keys_global");
    String_ID file_map_id = vars_save_string_lit("keys_file");
    String_ID insert_map_id = vars_save_string_lit("keys_insert");
    String_ID edit_map_id = vars_save_string_lit("keys_edit");


    MappingScope();
    SelectMapping(mapping);

    SelectMap(global_map_id);
    {
        BindCore(custom_startup, CoreCode_Startup);
        BindCore(custom_try_exit, CoreCode_TryExit);
        BindCore(clipboard_record_clip, CoreCode_NewClipboardContents);
        Bind(exit_4coder, KeyCode_F4, KeyCode_Alt);

        BindMouseWheel(CMD_L(mouse_wheel_scroll(app); set_cursor_column_to_preferred(app)));
        BindMouseWheel(mouse_wheel_change_face_size, KeyCode_Control);
    }
    
    SelectMap(file_map_id);
    {
        ParentMap(global_map_id);
        BindMouse(CMD_L(click_set_cursor_and_mark(app); reset_preferred_column(app)), MouseCode_Left);
        BindCore(CMD_L(click_set_cursor_and_mark(app); reset_preferred_column(app)), CoreCode_ClickActivateView);
        BindMouseRelease(CMD_L(click_set_cursor(app); reset_preferred_column(app)), MouseCode_Left);
        BindMouseMove(CMD_L(click_set_cursor_if_lbutton(app); reset_preferred_column(app)));
    }
    
    SelectMap(insert_map_id);
    {
        ParentMap(file_map_id);
        BindTextInput(custom_write_and_auto_tab);
    }

    SelectMap(edit_map_id);
    {
        ParentMap(file_map_id);
    }
}

static void custom_setup_default_bindings(Mapping *mapping)
{
    String_ID global_map_id = vars_save_string_lit("keys_global");
    String_ID file_map_id = vars_save_string_lit("keys_file");

    String_ID insert_map_id = vars_save_string_lit("keys_insert");
    String_ID edit_map_id = vars_save_string_lit("keys_edit");

    MappingScope();
    SelectMapping(&framework_mapping);

    SelectMap(global_map_id);
    {
        Bind(toggle_large_jump_view, KeyCode_Insert);
    }

    SelectMap(file_map_id);
    {
        BIND_MOTION(CMD_L(move_up(app); set_cursor_column_to_preferred(app)), KeyCode_Up);
        BIND_MOTION(CMD_L(move_down(app); set_cursor_column_to_preferred(app)), KeyCode_Down);
        BIND_MOTION(CMD_L(move_left(app); reset_preferred_column(app)), KeyCode_Left);
        BIND_MOTION(CMD_L(move_right(app); reset_preferred_column(app)), KeyCode_Right);
    }

    SelectMap(insert_map_id);
    {
        Bind(delete_char,            KeyCode_Delete);
        Bind(backspace_char,         KeyCode_Backspace);
        Bind(custom_newline,         KeyCode_Return);

        Bind(CMD_L(set_modal_mode(app, MODAL_MODE_EDIT)), KeyCode_Escape);
    }

    SelectMap(edit_map_id);
    {
        Bind(CMD_L(set_modal_mode(app, MODAL_MODE_INSERT)), KeyCode_I);

        // NOTE(jesper): motions
        BIND_MOTION(CMD_L(move_up(app); set_cursor_column_to_preferred(app)), KeyCode_K);
        BIND_MOTION(CMD_L(move_down(app); set_cursor_column_to_preferred(app)), KeyCode_J);
        BIND_MOTION(CMD_L(move_left(app); reset_preferred_column(app)), KeyCode_H);
        BIND_MOTION(CMD_L(move_right(app); reset_preferred_column(app)), KeyCode_L);
        
        BIND_MOTION(CMD_L(move_right(app); reset_preferred_column(app)), KeyCode_Space);
        BIND_MOTION(CMD_L(move_right(app); reset_preferred_column(app)), KeyCode_Tab);

        BIND_MOTION(move_word, KeyCode_W);
        BIND_MOTION(move_word_back, KeyCode_B);
        BIND_MOTION(seek_whitespace_up, KeyCode_LeftBracket);
        BIND_MOTION(seek_whitespace_down, KeyCode_RightBracket);
        BIND_MOTION(move_matching_scope, KeyCode_Tick);
        BIND_MOTION(move_beginning_of_line, KeyCode_Q);
        BIND_MOTION(move_end_of_line, KeyCode_E);
        BIND_MOTION(goto_beginning_of_file, KeyCode_Home);
        BIND_MOTION(goto_end_of_file, KeyCode_End);
        BIND_MOTION(seek_char, KeyCode_T);
        BIND_MOTION(page_up, KeyCode_PageUp);
        BIND_MOTION(page_down, KeyCode_PageDown);

        // NOTE(jesper): misc functions
        Bind(custom_delete_range, KeyCode_D);
        Bind(delete_range_lines, KeyCode_D, KeyCode_Control);
        Bind(custom_cut, KeyCode_X);
        Bind(cut_range_lines, KeyCode_X, KeyCode_Control);
        Bind(copy, KeyCode_Y);
        Bind(copy_range_lines, KeyCode_Y, KeyCode_Control);
        Bind(custom_replace_range, KeyCode_F);
        Bind(custom_replace_file, KeyCode_F, KeyCode_Alt);
        Bind(custom_replace_range_lines, KeyCode_F, KeyCode_Control);
        Bind(custom_fuzzy_find_buffer, KeyCode_O);
        Bind(custom_fuzzy_find_file, KeyCode_O, KeyCode_Control);
        Bind(custom_fuzzy_command_lister, KeyCode_Semicolon);
        Bind(change_active_panel, KeyCode_W, KeyCode_Control);
        Bind(custom_paste, KeyCode_P);
        Bind(custom_paste_next, KeyCode_P, KeyCode_Control);
        Bind(undo, KeyCode_U);
        Bind(redo, KeyCode_R);
        Bind(combine_with_next_line, KeyCode_J, KeyCode_Control);
        Bind(set_mark, KeyCode_M);
        Bind(save, KeyCode_S, KeyCode_Control);
        Bind(goto_line, KeyCode_G, KeyCode_Control);
        Bind(custom_auto_indent_range, KeyCode_Equal);
        Bind(replay_macro, KeyCode_V);
        Bind(toggle_macro_record, KeyCode_C);
        Bind(toggle_fullscreen, KeyCode_F11);

        Bind(CMD_L(jump_buffer_cmd(app, 0)), KeyCode_F1);
        Bind(CMD_L(jump_buffer_cmd(app, 1)), KeyCode_F2);
        Bind(CMD_L(jump_buffer_cmd(app, 2)), KeyCode_F3);
        Bind(CMD_L(jump_buffer_cmd(app, 3)), KeyCode_F4);
        Bind(CMD_L(jump_buffer_cmd(app, 4)), KeyCode_F5);
        Bind(CMD_L(jump_buffer_cmd(app, 5)), KeyCode_F6);
        Bind(CMD_L(jump_buffer_cmd(app, 6)), KeyCode_F7);
        Bind(CMD_L(jump_buffer_cmd(app, 7)), KeyCode_F8);

        // NOTE(jesper): explicitly binding control variants to the same
        // function ptr to signal that the function does different things
        // depending on modifier state
        Bind(CMD_L(jump_buffer_cmd(app, 0)), KeyCode_F1, KeyCode_Control);
        Bind(CMD_L(jump_buffer_cmd(app, 1)), KeyCode_F2, KeyCode_Control);
        Bind(CMD_L(jump_buffer_cmd(app, 2)), KeyCode_F3, KeyCode_Control);
        Bind(CMD_L(jump_buffer_cmd(app, 3)), KeyCode_F4, KeyCode_Control);
        Bind(CMD_L(jump_buffer_cmd(app, 4)), KeyCode_F5, KeyCode_Control);
        Bind(CMD_L(jump_buffer_cmd(app, 5)), KeyCode_F6, KeyCode_Control);
        Bind(CMD_L(jump_buffer_cmd(app, 6)), KeyCode_F7, KeyCode_Control);
        Bind(CMD_L(jump_buffer_cmd(app, 7)), KeyCode_F8, KeyCode_Control);

        Bind(CMD_L(goto_next_jump(app); set_mark(app)), KeyCode_N);
        Bind(CMD_L(goto_prev_jump(app); set_mark(app)), KeyCode_N, KeyCode_Shift);

        Bind(custom_isearch_cmd, KeyCode_ForwardSlash);
        Bind(custom_search_all_buffers_cmd, KeyCode_ForwardSlash, KeyCode_Control);
        Bind(toggle_sticky_jump_buffer, KeyCode_B, KeyCode_Control);
    }
}
        
