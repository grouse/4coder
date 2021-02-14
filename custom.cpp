// features
// TODO(jesper): jump location history to go back/forward
// TODO(jesper): improve start-up performance of large projects, and subsequent runtime memory usage
// TODO(jesper): implement/add good bindings/motions for finding/listing functions and identifiers
// TODO(jesper): command history
// TODO(jesper): switch between pascalCase, CamelCase, snake_case
// TODO(jesper): remove duplicate lines, remove unique lines
// TODO(jesper): figure out how most other editors handle going down lines and somehow remembering the column number
// I'm pretty sure this works by remembering the col number, setting cursor to min(col, max_col) for each line, until 
// cursor is moved left/right
// TODO(jesper): seek matching scope need to take into account scope characters inside strings and character literals
// fleury has a version of this function that uses the lexer tokens, but it doesn't seek to the nearest one like mine does
// if the cursor isn't currently on a brace. I do think the lexer token approach is the correct one, with a text-only fallback
// for non-code files
// TODO(jesper): grab the #if <stuff> and automatically append it to the corresponding #endif. Figure out how to handle else and elseif for that

// bugs

#include "4coder_default_include.cpp"

CUSTOM_ID(colors, defcolor_cursor_insert);
CUSTOM_ID(colors, defcolor_cursor_background);
CUSTOM_ID(colors, defcolor_highlight_cursor_line_recording);
CUSTOM_ID(colors, defcolor_jump_buffer_background);
CUSTOM_ID(colors, defcolor_jump_buffer_background_active);
CUSTOM_ID(colors, defcolor_jump_buffer_foreground);
CUSTOM_ID(colors, defcolor_jump_buffer_foreground_active);
CUSTOM_ID(colors, defcolor_jump_buffer_sticky);
CUSTOM_ID(colors, defcolor_comment_todo);
CUSTOM_ID(colors, defcolor_comment_note);
CUSTOM_ID(colors, defcolor_jump_buffer_background_cmd_executing);
CUSTOM_ID(colors, defcolor_jump_buffer_background_cmd_fail);

#define I64_MAX (i64)(0x7fffffffffffffff)

typedef char CHAR;
typedef CHAR *LPSTR;

extern "C" void * memmove ( void * destination, const void * source, size_t num );
extern "C" void* memset( void* dest, int ch, size_t count );
extern "C" void* memcpy( void* dest, const void* src, size_t count );
extern "C" int strncmp ( const char * str1, const char * str2, size_t num );
extern "C" size_t strlen ( const char * str );
extern "C" LPSTR GetCommandLineA();

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

#include "custom_fixes.cpp"
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

#define heap_alloc_arr(heap, Type, count) (Type*)heap_allocate(heap, count * sizeof(Type))

#define swap(a, b) do { auto glue(tmp_a_, __LINE__) = a; a = b; b = glue(tmp_a_, __LINE__); } while(0)

#define FZY_SCORE_MAX INFINITY
#define FZY_SCORE_MIN -INFINITY
#define FZY_SCORE_GAP_LEADING -0.005
#define FZY_SCORE_GAP_TRAILING -0.005
#define FZY_SCORE_GAP_INNER -0.01
#define FZY_SCORE_MATCH_CONSECUTIVE 1.0
#define FZY_SCORE_MATCH_SLASH 0.9
#define FZY_SCORE_MATCH_WORD 0.8
#define FZY_SCORE_MATCH_CAPITAL 0.7
#define FZY_SCORE_MATCH_DOT 0.6

#define FZY_SCORE_EPSILON 0.001

using fzy_score_t = f64;

static fzy_score_t fzy_bonus_states[3][256];
static size_t fzy_bonus_index[256];

static void custom_setup_necessary_bindings(Mapping *mapping);
static void custom_setup_default_bindings(Mapping *mapping);

static void fzy_init_table()
{
    memset(fzy_bonus_index, 0, sizeof fzy_bonus_index);
    memset(fzy_bonus_states, 0, sizeof fzy_bonus_states);

    for (i32 i = 'A'; i < 'Z'; i++) {
        fzy_bonus_index[i] = 2;
    }
    
    for (i32 i = 'a'; i < 'z'; i++) {
        fzy_bonus_index[i] = 1;
        fzy_bonus_states[2][i] = FZY_SCORE_MATCH_CAPITAL;
    }

    for (i32 i = '0'; i < '9'; i++) {
        fzy_bonus_index[i] = 1;
    }

    fzy_bonus_states[1]['/'] = fzy_bonus_states[2]['/'] = FZY_SCORE_MATCH_SLASH;
    fzy_bonus_states[1]['\\'] = fzy_bonus_states[2]['\\'] = FZY_SCORE_MATCH_SLASH;
    fzy_bonus_states[1]['-'] = fzy_bonus_states[2]['-'] = FZY_SCORE_MATCH_WORD;
    fzy_bonus_states[1]['_'] = fzy_bonus_states[2]['_'] = FZY_SCORE_MATCH_WORD;
    fzy_bonus_states[1][' '] = fzy_bonus_states[2][' '] = FZY_SCORE_MATCH_WORD;
    fzy_bonus_states[1]['.'] = fzy_bonus_states[2]['.'] = FZY_SCORE_MATCH_DOT;
}

#define fzy_compute_bonus(last_ch, ch) \
    (fzy_bonus_states[fzy_bonus_index[(unsigned char)(ch)]][(unsigned char)(last_ch)])


void* heap_realloc(Heap *heap, void *ptr, u64 old_size, u64 new_size)
{
    void *nptr = heap_allocate(heap, new_size);
    memcpy(nptr, ptr, old_size);
    heap_free(heap, ptr);
    return nptr;
}

static void string_mod_lower(String_Const_u8 dst, String_Const_u8 src)
{
    for (u64 i = 0; i < src.size; i += 1){
        dst.str[i] = character_to_lower(src.str[i]);
    }
}

static void quicksort(fzy_score_t *scores, Lister_Node **nodes, i32 l, i32 r)
{
    if (l >= r) {
        return;
    }

    fzy_score_t pivot = scores[r];

    i32 cnt = l;

    for (i32 i = l; i <= r; i++) {
        if (scores[i] >= pivot) {
            swap(nodes[cnt], nodes[i]);
            swap(scores[cnt], scores[i]);
            cnt++;
        }
    }

    quicksort(scores, nodes, l, cnt-2);
    quicksort(scores, nodes, cnt, r);
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
    
    i32 i = JUMP_BUFFER_COUNT-1;
    while (i >= index+1) {
        if (g_jump_buffers[i].sticky) {
            i--;
            continue;
        }
        
        i32 j = i-1;
        for (; j >= index; j--) {
            if (!g_jump_buffers[j].sticky) break;
        }
        
        g_jump_buffers[i] = g_jump_buffers[j];
        i = j;
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
                // TODO(jesper): this will break if I start breaking up defines or include 
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
    i64 text_size = 0;
    for (i32 i = 0; i < num_lines; i++) {
        String_Const_u8 line = push_buffer_line(app, scratch, buffer, start.line+i);
        
        // NOTE(jesper): this API is broken and for crlf files will return strings one byte too long
        // We do not want strings with line ending characters in this, so fix it up in post
        if (line.str[line.size-1] == '\r') line.size--;
        
        text_size += line.size;
        lines[i] = line;
    }
    
    
    quicksort_asc(lines, 0, num_lines-1);

    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    Line_Ending_Kind *eol_setting = scope_attachment(app, scope, buffer_eol_setting, Line_Ending_Kind);
    
    i32 eol_size = 0;
    switch (*eol_setting) {
    case LineEndingKind_LF: eol_size = 1; break;
    case LineEndingKind_CRLF: eol_size = 2; break;
    }
    
    History_Group group = history_group_begin(app, buffer);
    i64 p = start.pos;
    for (i32 i = 0; i < num_lines; i++) {
        buffer_replace_range(app, buffer, Ii64(p, p+lines[i].size), lines[i]);
        p += lines[i].size + eol_size;
    }
    history_group_end(group);
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
                if (has_whitespace && pos+1 != start_pos) {
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
        if (match_key_code(&in, KeyCode_Return)) {
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
        } else if (str.str != 0 && str.size > 0) {
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
            ProfileScope(app, "output jump location");
            
            JumpLocation location = locations[i];

            i64 line_start = location.line_start;
            i64 line_end = location.line_end;

            i64 line_length = line_end - line_start;
            String_u8 line_str = Su8(push_array(scratch, u8, line_length), line_length);
            if (!buffer_read_range(app, active_buffer, Ii64(line_start, line_end), line_str.str)) continue;

            String_Const_u8 chopped_line = string_skip_whitespace(SCu8(line_str));

            if (llabs(cursor_pos - location.pos) < llabs(cursor_pos - closest_pos) ||
                (location.pos > cursor_pos && closest_pos < cursor_pos))
            {
                closest_location = i;
                closest_pos = location.pos;
            }

            String_Const_u8 isearch_line = push_stringf(
                arena,
                "%.*s:%d:%d: %.*s\n",
                buffer_file_name.size, buffer_file_name.str,
                location.line,
                location.col,
                chopped_line.size, chopped_line.str);
            
            i32 bytes_to_write = (i32)isearch_line.size;
            i32 bytes_written = 0;
            
            while (bytes_written < bytes_to_write) {
                i32 available = Min(size - written, bytes_to_write);
                memcpy(buffer+written, isearch_line.str + bytes_written, available);
                written += available;
                bytes_written += available;
                
                if (written == size) {
                    buffer_replace_range(
                        app,
                        jb->buffer_id,
                        Ii64(jb_size, jb_size),
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
                Ii64(jb_size, jb_size),
                SCu8(buffer, written));
            jb_size = buffer_get_size(app, jb->buffer_id);
            written = 0;
        }

        if (closest_location != -1) {
            view_set_cursor(app, view, seek_pos(closest_pos));
            view_set_cursor(app, g_jump_view, seek_line_col(closest_location+1, 0));
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
#ifdef _WIN32
    char *cmdline = GetCommandLineA();
    char *args = nullptr;
    char *p = cmdline;
    
    bool in_string = false;
    while (*p) {
        if (*p == '"') in_string = !in_string;
        else if (!in_string && *p == ' ') {
            args = p+1;
            break;
        }
        p++;
    }
    
    String_Const_u8 filename{};
    if (args && *args) {
        p = args;
        in_string = false;
        while (*p) {
            if (*p == '"') in_string = !in_string;
            else if (!in_string && *p == ' ') {
                filename = SCu8(args, (i32)(p-args));
            }
            p++;
        }
        
        if (filename.size == 0) {
            filename = SCu8(args, (i32)(p-args));
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
#else
    String_Const_u8 filename{};
    if (file_names.count > 0) filename = file_names.vals[0];
#endif
    
    load_themes_default_folder(app);
    default_4coder_initialize(app, file_names);
    
    custom_setup_necessary_bindings(&framework_mapping);
    // TODO(jesper): make my modal thing work with the new dynamic binding system
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
        buffer_set_setting(app, buffer, BufferSetting_Unimportant, true);
        buffer_set_setting(app, buffer, BufferSetting_ReadOnly, true);
        
        JumpBufferCmd cmd = {};
        cmd.buffer_id = buffer;
        g_jump_buffers[i] = cmd;
        buffer_name.size = 0;
    }
    
    g_jump_view = bottom;
    
    b32 auto_load_project = def_get_config_b32(vars_save_string_lit("automatically_load_project"));
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
        if (auto_load_project) {
            String_Const_u8 project_file = SCu8("project.4coder");
            if (filename.size >= project_file.size) {
                i64 start = filename.size - project_file.size;
                i64 end = start + project_file.size;

                String_Const_u8 sub = string_substring(filename, Ii64(start, end));
                if (string_match(sub, project_file)) {
                    load_project(app);
                }
            }
        }
        
    } else if (auto_load_project) {
        load_project(app);
    }
    
    view_set_active(app, main_view);
    
    //~ NOTE(rjf): Initialize stylish fonts.
    {
        Face_Description normal_code_desc = get_face_description(app, get_face_id(app, 0));
        String_Const_u8 bin_path = system_get_path(scratch, SystemPath_Binary);

        // NOTE(rjf): Fallback font.
        Face_ID face_that_should_totally_be_there = get_face_id(app, 0);

        // NOTE(rjf): Title font.
        {
            Face_Description desc = {0};
            desc.font.file_name = push_u8_stringf(
                scratch, 
                "%.*sfonts/Ubuntu-Regular.ttf", 
                string_expand(bin_path));
            desc.parameters.pt_size = normal_code_desc.parameters.pt_size + 4;

            if(IsFileReadable(desc.font.file_name))
            {
                global_styled_title_face = try_create_new_face(app, &desc);
            }
            else
            {
                global_styled_title_face = face_that_should_totally_be_there;
            }
        }

        // NOTE(rjf): Label font.
        {
            Face_Description desc = {0};
            desc.font.file_name = push_u8_stringf(
                scratch, 
                "%.*sfonts/Ubuntu-Regular.ttf", 
                string_expand(bin_path));
            desc.parameters.pt_size = normal_code_desc.parameters.pt_size - 4;

            if(IsFileReadable(desc.font.file_name))
            {
                global_styled_label_face = try_create_new_face(app, &desc);
            }
            else
            {
                global_styled_label_face = face_that_should_totally_be_there;
            }
        }
        
        // NOTE(rjf): Small code font.
        {
            Face_Description desc = {0};
            desc.font.file_name = push_u8_stringf(
                scratch, 
                "%.*sfonts/UbuntuMono-R.ttf", 
                string_expand(bin_path));
            desc.parameters.pt_size = normal_code_desc.parameters.pt_size - 4;

            if(IsFileReadable(desc.font.file_name))
            {
                global_small_code_face = try_create_new_face(app, &desc);
            }
            else
            {
                global_small_code_face = face_that_should_totally_be_there;
            }
        }
    }
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
}


static void custom_render_buffer(
    Application_Links *app,
    View_ID view_id,
    Face_ID face_id,
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
    
    Buffer_Point_Delta_Result delta = delta_apply(app, view_id,
                                                  frame_info.animation_dt, scroll);
    if (!block_match_struct(&scroll.position, &delta.point)){
        block_copy_struct(&scroll.position, &delta.point);
        view_set_buffer_scroll(app, view_id, scroll, SetBufferScroll_NoCursorChange);
    }
    if (delta.still_animating){
        animate_in_n_milliseconds(app, 0);
    }
    
    // NOTE(allen): query bars
    {
        Query_Bar *space[32];
        Query_Bar_Ptr_Array query_bars = {};
        query_bars.ptrs = space;
        if (get_active_query_bars(app, view_id, ArrayCount(space), &query_bars)){
            for (i32 i = 0; i < query_bars.count; i += 1){
                Rect_f32_Pair pair = layout_query_bar_on_top(region, line_height, 1);
                draw_query_bar(app, query_bars.ptrs[i], face_id, pair.min);
                region = pair.max;
            }
        }
    }
    
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
    
    custom_render_buffer(app, view_id, face_id, buffer, text_layout_id, region);
    
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
}

CUSTOM_COMMAND_SIG(move_word_back)
{
    View_ID view = get_active_view(app, Access_Always);
    i64 pos = seek_prev_word(app, view, view_get_cursor_pos(app, view));
    view_set_cursor(app, view, seek_pos(pos));
}

CUSTOM_COMMAND_SIG(move_matching_scope)
{
    View_ID view = get_active_view(app, Access_Always);
    i64 pos = seek_matching_scope(app, view, view_get_cursor_pos(app, view));
    view_set_cursor(app, view, seek_pos(pos));
}

CUSTOM_COMMAND_SIG(seek_whitespace_up)
{
    seek_blank_line(app, Scan_Backward, PositionWithinLine_Start);
}

CUSTOM_COMMAND_SIG(seek_whitespace_down)
{
    seek_blank_line(app, Scan_Forward, PositionWithinLine_Start);
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
}

CUSTOM_COMMAND_SIG(delete_range_lines)
{
    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
    Range_i64 range = get_mark_cursor_lines_range(app, view, buffer);
    buffer_replace_range(app, buffer, range, string_u8_empty);
}

CUSTOM_COMMAND_SIG(cut_range_lines)
{
    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
    Range_i64 range = get_mark_cursor_lines_range(app, view, buffer);
    
    if (clipboard_post_buffer_range(app, 0, buffer, range)) {
        buffer_replace_range(app, buffer, range, string_u8_empty);
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
}


CUSTOM_COMMAND_SIG(combine_with_next_line)
{
    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
    
    seek_end_of_line(app);
    i64 start = view_get_cursor_pos(app, view);
    move_down(app);
    seek_beginning_of_line(app);
    move_past_lead_whitespace(app, view, buffer);
    i64 end = view_get_cursor_pos(app, view);
    
    u8 space[1] = {' '};
    buffer_replace_range(app, buffer, Ii64(start, end), SCu8(space, sizeof space));
    move_right(app);
}

CUSTOM_COMMAND_SIG(move_beginning_of_line)
{
    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
    
    History_Group group = history_group_begin(app, buffer);
    seek_beginning_of_line(app);
    move_past_lead_whitespace(app, view, buffer);
    history_group_end(group);
}

CUSTOM_COMMAND_SIG(move_end_of_line)
{
    seek_end_of_line(app);
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
        File_Name_Result result = get_file_name_from_user(app, scratch, SCu8("build script: "), active_view);
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

static void fuzzy_lister_update_filtered(
    Application_Links *app,
    Lister *lister)
{
    Arena *arena = lister->arena;
    Scratch_Block scratch(app, arena);
    
    String_Const_u8 key = lister->key_string.string;
    key = push_string_copy(arena, key);
    
    i32 node_count = lister->options.count;
    
    Lister_Node **filtered = push_array(scratch, Lister_Node*, node_count);
    i32 filtered_count = 0;
    
    String_Const_u8 needle;
    fzy_score_t *scores = nullptr;
    i32 ni = 0;
    if (key.size == 0) {
        for (Lister_Node *node = lister->options.first;
             node != nullptr;
             node = node->next)
        {
            filtered[filtered_count++] = node;
        }

        goto finalize_list;
    }
    
    needle = SCu8(push_array(scratch, u8, key.size), key.size);
    string_mod_lower(needle, key);

    scores = push_array(scratch, fzy_score_t, node_count);

    for (Lister_Node *node = lister->options.first;
         node != nullptr;
         node = node->next)
    {
        Temp_Memory_Block temp(scratch);

        String_Const_u8 label = node->string;
        String_Const_u8 haystack = SCu8(push_array(scratch, u8, label.size), label.size);
        string_mod_lower(haystack, label);
        
        if (label.size <= 0) continue;

        fzy_score_t *D = push_array(scratch, fzy_score_t, label.size * needle.size);
        fzy_score_t *M = push_array(scratch, fzy_score_t, label.size * needle.size);

        memset(D, 0, sizeof *D * label.size * needle.size);
        memset(M, 0, sizeof *M * label.size * needle.size);

        fzy_score_t *match_bonus = push_array(scratch, fzy_score_t, label.size);

        char prev = '/';
        for (i32 i = 0; i < haystack.size; i++) {
            char c = haystack.str[i];
            match_bonus[i] = fzy_compute_bonus(prev, c);
            prev = c;
        }

        for (i32 i = 0; i < needle.size; i++) {
            fzy_score_t prev_score = FZY_SCORE_MIN;
            fzy_score_t gap_score = i == needle.size - 1 ? FZY_SCORE_GAP_TRAILING : FZY_SCORE_GAP_INNER;

            for (i32 j = 0; j < haystack.size; j++) {
                if (needle.str[i] == haystack.str[j]) {
                    fzy_score_t score = FZY_SCORE_MIN;
                    if (!i) {
                        score = (j * FZY_SCORE_GAP_LEADING) + match_bonus[j];
                    } else if (j) {
                        fzy_score_t d_val = D[(i-1) * haystack.size + j-1];
                        fzy_score_t m_val = M[(i-1) * haystack.size + j-1];

                        score = Max(m_val + match_bonus[j], d_val + FZY_SCORE_MATCH_CONSECUTIVE);
                    }

                    D[i * haystack.size + j] = score;
                    M[i * haystack.size + j] = prev_score = Max(score, prev_score + gap_score);
                } else {
                    D[i * haystack.size + j] = FZY_SCORE_MIN;
                    M[i * haystack.size + j] = prev_score = prev_score + gap_score;
                }
            }
        }

        fzy_score_t match_score = M[(needle.size-1) * haystack.size + haystack.size - 1];

        if (match_score != FZY_SCORE_MIN) {
            filtered[filtered_count] = node;
            scores[filtered_count] = match_score;
            filtered_count++;
        }

        ni++;
    }
    
    quicksort(scores, filtered, 0, filtered_count-1);
    
finalize_list:
    end_temp(lister->filter_restore_point);
    
    Lister_Node **node_ptrs = push_array(arena, Lister_Node*, filtered_count);
    lister->filtered.node_ptrs = node_ptrs;
    lister->filtered.count = filtered_count;
    
    for (i32 i = 0; i < filtered_count; i++) {
        Lister_Node *node = filtered[i];
        node_ptrs[i] = node;
    }
    
    lister_update_selection_values(lister);
}

static Lister_Activation_Code fuzzy_lister_write_string(
    Application_Links *app)
{
    Lister_Activation_Code result = ListerActivation_Continue;
    
    View_ID view = get_active_view(app, Access_Always);
    Lister *lister = view_get_lister(app, view);
    if (!lister) return result;
    
    User_Input in = get_current_input(app);
    String_Const_u8 string = to_writable(&in);
    if (!string.str || string.size <= 0) return result;
    
    lister_append_text_field(lister, string);
    lister_append_key(lister, string);
    lister->item_index = 0;
    lister_zero_scroll(lister);
    
    fuzzy_lister_update_filtered(app, lister);

    return result;
}

void custom_generate_all_buffers_list__output_buffer(
    Application_Links *app, Lister *lister,
    Buffer_ID buffer)
{
    Dirty_State dirty = buffer_get_dirty_state(app, buffer);
    String_Const_u8 status = {};
    switch (dirty) {
    case DirtyState_UnsavedChanges:  
        status = string_u8_litexpr("*"); 
        break;
    case DirtyState_UnloadedChanges: 
        status = string_u8_litexpr("!"); 
        break;
    case DirtyState_UnsavedChangesAndUnloadedChanges: 
        status = string_u8_litexpr("*!"); 
        break;
    }
    
    Scratch_Block scratch(app, lister->arena);
    
    String_Const_u8 buffer_name = push_buffer_file_name(app, scratch, buffer);
    if (buffer_name.size > 0) {
        String_Const_u8 hot_dir = push_hot_directory(app, scratch);
        buffer_name = shorten_path(buffer_name, hot_dir);
    } else {
        buffer_name = push_buffer_unique_name(app, scratch, buffer);
    }
    lister_add_item(lister, buffer_name, status, IntAsPtr(buffer), 0);
}

void custom_generate_all_buffers_list(
    Application_Links *app, 
    Lister *lister)
{
    lister_begin_new_item_set(app, lister);

    Buffer_ID viewed_buffers[16];
    i32 viewed_buffer_count = 0;

    // List currently viewed buffers
    for (View_ID view = get_view_next(app, 0, Access_Always);
         view != 0;
         view = get_view_next(app, view, Access_Always)){
        Buffer_ID new_buffer_id = view_get_buffer(app, view, Access_Always);
        for (i32 i = 0; i < viewed_buffer_count; i += 1){
            if (new_buffer_id == viewed_buffers[i]){
                goto skip0;
            }
        }
        viewed_buffers[viewed_buffer_count++] = new_buffer_id;
skip0:;
    }

    // Regular Buffers
    for (Buffer_ID buffer = get_buffer_next(app, 0, Access_Always);
         buffer != 0;
         buffer = get_buffer_next(app, buffer, Access_Always)){
        for (i32 i = 0; i < viewed_buffer_count; i += 1){
            if (buffer == viewed_buffers[i]){
                goto skip1;
            }
        }
        if (!buffer_has_name_with_star(app, buffer)){
            custom_generate_all_buffers_list__output_buffer(app, lister, buffer);
        }
skip1:;
    }

    // Buffers Starting with *
    for (Buffer_ID buffer = get_buffer_next(app, 0, Access_Always);
         buffer != 0;
         buffer = get_buffer_next(app, buffer, Access_Always)){
        for (i32 i = 0; i < viewed_buffer_count; i += 1){
            if (buffer == viewed_buffers[i]){
                goto skip2;
            }
        }
        if (buffer_has_name_with_star(app, buffer)){
            custom_generate_all_buffers_list__output_buffer(app, lister, buffer);
        }
skip2:;
    }

    // Buffers That Are Open in Views
    for (i32 i = 0; i < viewed_buffer_count; i += 1){
        custom_generate_all_buffers_list__output_buffer(app, lister, viewed_buffers[i]);
    }
}

static Buffer_ID fuzzy_file_lister(Application_Links *app, String_Const_u8 query)
{
    Scratch_Block scratch(app);

    Buffer_ID buffer = 0;

    Lister_Handlers handlers = lister_get_default_handlers();
    handlers.refresh = custom_generate_all_buffers_list;
    handlers.write_character = fuzzy_lister_write_string;

    Lister_Result result = {};
    if (handlers.refresh) {
        Lister_Block lister(app, scratch);
        lister_set_query(lister, string_u8_litexpr(""));
        lister_set_key(lister, query);
        lister_set_text_field(lister, query);

        lister_set_handlers(lister, &handlers);

        handlers.refresh(app, lister);
        result = fixed_run_lister(app, lister, fuzzy_lister_update_filtered);
    } else {
        result.canceled = true;
    }

    if (!result.canceled){
        buffer = (Buffer_ID)(PtrAsInt(result.user_data));
    }

    return buffer;
}

CUSTOM_COMMAND_SIG(custom_fuzzy_find_file)
{
    Buffer_ID buffer = fuzzy_file_lister(app, SCu8((u8*)0, (u64)0));
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

    i32 *command_ids = nullptr;
    i32 command_id_count = 0;
    String_Const_u8 query = SCu8("Command: ");

    Command_Lister_Status_Rule status_rule = {};
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

    Scratch_Block scratch(app);
    Lister_Block lister(app, scratch);
    lister_set_query(lister, query);

    Lister_Handlers handlers = lister_get_default_handlers();
    handlers.write_character = fuzzy_lister_write_string;
    lister_set_handlers(lister, &handlers);

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

        lister_add_item(lister, SCu8(fcoder_metacmd_table[j].name), status, (void*)proc, 0);
    }

    Lister_Result l_result = fixed_run_lister(app, lister, fuzzy_lister_update_filtered);

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
        keyboard_macro_play(app, macro);

        for (i64 line = start.line+1; line <= end.line; line++) {
            Input_Event down{};
            down.kind = InputEventKind_KeyStroke;
            down.key.code = KeyCode_J;
            enqueue_virtual_event(app, &down);
            
            keyboard_macro_play(app, macro);
        }
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


BUFFER_HOOK_SIG(custom_begin_buffer)
{
    ProfileScope(app, "begin buffer");
    
    Scratch_Block scratch(app);
    
    b32 treat_as_code = false;
    String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer_id);
    if (file_name.size > 0){
        String_Const_u8 treat_as_code_string = def_get_config_string(scratch, vars_save_string_lit("treat_as_code"));
        String_Const_u8_Array extensions = parse_extension_line_to_extension_list(app, scratch, treat_as_code_string);
        String_Const_u8 ext = string_file_extension(file_name);
        for (i32 i = 0; i < extensions.count; ++i){
            if (string_match(ext, extensions.strings[i])){
                
                if (string_match(ext, string_u8_litexpr("cpp")) ||
                    string_match(ext, string_u8_litexpr("h")) ||
                    string_match(ext, string_u8_litexpr("c")) ||
                    string_match(ext, string_u8_litexpr("hpp")) ||
                    string_match(ext, string_u8_litexpr("cc"))){
                    treat_as_code = true;
                }
                
                break;
            }
        }
    }
    
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
    
    // NOTE(allen): Decide buffer settings
    b32 wrap_lines = true;
    b32 use_lexer = false;
    if (treat_as_code){
        wrap_lines = def_get_config_b32(vars_save_string_lit("enable_code_wrapping"));
        use_lexer = true;
    }
    
    String_Const_u8 buffer_name = push_buffer_base_name(app, scratch, buffer_id);
    if (buffer_name.size > 0 && buffer_name.str[0] == '*' && buffer_name.str[buffer_name.size - 1] == '*'){
        wrap_lines = def_get_config_b32(vars_save_string_lit("enable_output_wrapping"));
    }
    
    if(treat_as_code == false)
    {
        F4_Language *language = F4_LanguageFromBuffer(app, buffer_id);
        if(language)
        {
            treat_as_code = true;
        }
    }

    if (use_lexer){
        ProfileBlock(app, "begin buffer kick off lexer");
        Async_Task *lex_task_ptr = scope_attachment(app, scope, buffer_lex_task, Async_Task);
        *lex_task_ptr = async_task_no_dep(&global_async_system, F4_DoFullLex_ASYNC, make_data_struct(&buffer_id));
    }

    {
        b32 *wrap_lines_ptr = scope_attachment(app, scope, buffer_wrap_lines, b32);
        *wrap_lines_ptr = wrap_lines;
    }

    if (use_lexer){
        buffer_set_layout(app, buffer_id, layout_virt_indent_index_generic);
    }
    else{
        if (treat_as_code){
            buffer_set_layout(app, buffer_id, layout_virt_indent_literal_generic);
        }
        else{
            buffer_set_layout(app, buffer_id, layout_generic);
        }
    }

    // no meaning for return
    return(0);
}

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
    
    Thread_Context *tctx = get_thread_context(app);
    mapping_init(tctx, &framework_mapping);
    
    custom_setup_necessary_bindings(&framework_mapping);

    F4_Index_Initialize();
    F4_RegisterLanguages();
}

CUSTOM_COMMAND_SIG(custom_try_exit)
{
    User_Input input = get_current_input(app);
    if (match_core_code(&input, CoreCode_TryExit)) {
        b32 do_exit = true;
        
        if (!allow_immediate_close_without_checking_for_changes) {
            b32 has_unsaved_changes = false;
            
            for (Buffer_ID buffer = get_buffer_next(app, 0, Access_Always);
                 buffer != 0;
                 buffer = get_buffer_next(app, buffer, Access_Always))
            {
                Dirty_State dirty = buffer_get_dirty_state(app, buffer);
                if (HasFlag(dirty, DirtyState_UnsavedChanges)){
                    has_unsaved_changes = true;
                    break;
                }
            }
            
            if (has_unsaved_changes){
                Scratch_Block scratch(app);
                Lister_Block lister(app, scratch);
                
                enum ListerChoice {
                    CHOICE_NULL = 0,
                    CHOICE_CANCEL,
                    CHOICE_DISCARD,
                    CHOICE_SAVE_ALL,
                    CHOICE_BUFFER_ID_START,
                };

                lister_set_query(lister, "There are buffers with unsaved changes");

                Lister_Handlers handlers = {};
                handlers.navigate = lister__navigate__default;
                handlers.refresh = [](Application_Links *app, Lister *lister)
                {
                    lister_begin_new_item_set(app, lister);
                    
                    Scratch_Block scratch(app, lister->arena);
                    
                    bool has_dirty = false;
                    for (Buffer_ID buffer = get_buffer_next(app, 0, Access_Always);
                         buffer != 0;
                         buffer = get_buffer_next(app, buffer, Access_Always))
                    {
                        Dirty_State dirty = buffer_get_dirty_state(app, buffer);
                        if (HasFlag(dirty, DirtyState_UnsavedChanges)){
                            String_Const_u8 filename = push_buffer_file_name(app, scratch, buffer);
                            String_Const_u8 label = push_u8_stringf(scratch, "[D][S] %.*s", string_expand(filename));
                            lister_add_item(lister, label, string_u8_litexpr(""), IntAsPtr(CHOICE_BUFFER_ID_START + buffer), 0);
                            has_dirty = true;
                        }
                    }
                    
                    lister_add_item(lister, string_u8_litexpr("[C]ancel"), string_u8_litexpr(""), IntAsPtr(CHOICE_CANCEL), 0);
                    lister_add_item(lister, string_u8_litexpr("[D]iscard changes"), string_u8_litexpr(""), IntAsPtr(CHOICE_DISCARD), 0);
                    lister_add_item(lister, string_u8_litexpr("[S]ave all"), string_u8_litexpr(""), IntAsPtr(CHOICE_SAVE_ALL), 0);

                    // NOTE(jesper): this is a bit dirty and relies on a change in how the lister is run in fixed_run_lister
                    // I do not know if there is a better way to signal from the refresh handler that the lister should
                    // be considered finsihed.
                    // I can't do a "cleaner" hack that calls lister_activate with e.g. DISCARD choice, because the way the 
                    // lister update loop is structured that won't be procesed until next input event
                    if (!has_dirty) {
                        lister->out.user_data = IntAsPtr(CHOICE_DISCARD);
                        lister->out.canceled = true;
                    }
                };
                
                handlers.key_stroke = [](Application_Links *app)
                {
                    Scratch_Block scratch(app);
                    
                    Lister_Activation_Code result = ListerActivation_Continue;

                    View_ID view = get_active_view(app, Access_Always);
                    Lister *lister = view_get_lister(app, view);
                    if (!lister) return result;
                    
                    User_Input in = get_current_input(app);
                    if (in.event.kind == InputEventKind_KeyStroke) {
                        ListerChoice choice = (ListerChoice)(i64)lister->highlighted_node->user_data;
                        
                        switch (in.event.key.code) {
                        case KeyCode_C:
                            lister_activate(app, lister, (void*)(CHOICE_CANCEL), false);
                            result = ListerActivation_Finished;
                            break;
                        case KeyCode_D:
                            if (choice >= CHOICE_BUFFER_ID_START) {
                                buffer_reopen(app, (Buffer_ID)(choice - CHOICE_BUFFER_ID_START), 0);
                                result = ListerActivation_ContinueAndRefresh;
                            } else {
                                lister_activate(app, lister, (void*)(CHOICE_DISCARD), false);
                                result = ListerActivation_Finished;
                            }
                            break;
                        case KeyCode_S:
                            if (choice >= CHOICE_BUFFER_ID_START) {
                                Buffer_ID buffer = (Buffer_ID)(choice - CHOICE_BUFFER_ID_START);
                                String_Const_u8 filename = push_buffer_file_name(app, scratch, buffer);

                                buffer_save(app, buffer, filename, 0);
                                result = ListerActivation_ContinueAndRefresh;
                            } else {
                                lister_activate(app, lister, (void*)(CHOICE_SAVE_ALL), false);
                                result = ListerActivation_Finished;
                            }
                            break;
                        }

                    }

                    return result;
                };
                lister_set_handlers(lister, &handlers);
                lister_call_refresh_handler(app, lister);
                
                // NOTE(jesper): this lister relies on small edits in the run lister
                //   a) execute the refresh handler if any of its handlers return ListerActivation_ContinueAndRefresh
                //   b) terminate its outer loop if lister->out.canceled is true, which is set by the refresh handler 
                // above to signal that the refresh handler is trying to tell the lister to stop
                Lister_Result result = fixed_run_lister(app, lister);
                ListerChoice choice = (ListerChoice)(i64)result.user_data;
                switch (choice) {
                case CHOICE_NULL:
                case CHOICE_CANCEL:
                    do_exit = false;
                    break;
                case CHOICE_DISCARD:
                    do_exit = true;
                    break;
                case CHOICE_SAVE_ALL:
                    save_all_dirty_buffers(app);
                    do_exit = true;
                    break;
                default: {
                        Buffer_ID buffer = (Buffer_ID)(choice - CHOICE_BUFFER_ID_START);
                        view_set_buffer(app, get_active_view(app, Access_Always), buffer, 0);
                        do_exit = false;
                    } break;
                }
            }
        }
        
        if (do_exit){
            hard_exit(app);
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

        BindMouseWheel(mouse_wheel_scroll);
        BindMouseWheel(mouse_wheel_change_face_size, KeyCode_Control);
    }
    
    SelectMap(file_map_id);
    {
        ParentMap(global_map_id);
        BindMouse(click_set_cursor_and_mark, MouseCode_Left);
        BindCore(click_set_cursor_and_mark, CoreCode_ClickActivateView);
        BindMouseRelease(click_set_cursor, MouseCode_Left);
        BindMouseMove(click_set_cursor_if_lbutton);
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
        Bind(move_up,                KeyCode_Up);
        Bind(move_down,              KeyCode_Down);
        Bind(move_left,              KeyCode_Left);
        Bind(move_right,             KeyCode_Right);
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
        BIND_MOTION(move_down, KeyCode_J);
        BIND_MOTION(move_up, KeyCode_K);
        BIND_MOTION(move_left, KeyCode_H);
        BIND_MOTION(move_right, KeyCode_L);
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
        Bind(custom_fuzzy_find_file, KeyCode_O);
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
        