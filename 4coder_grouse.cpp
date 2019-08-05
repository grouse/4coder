#include "4coder_default_include.cpp"
#include <math.h>

// TODO(jesper): alternative cursor and mark graphics for better visualisation
// TODO(jesper): workaround/fix create_buffer count crash bug
// TODO(jesper): search project commands
// TODO(jesper): implement visual range sort
// TODO(jesper): visualise 80 and 100+ column

// TODO(jesper): adjust wrap width based on view width - currently on a per
// buffer setting instead of per view, which makes this impossible/very
// limmited/hacky

using i32 = int32_t;
using u32 = uint32_t;
using u8 = uint8_t;
using f32 = float;
using f64 = double;

#define I32_MAX INT32_MAX

#define ARRAY_COUNT(arr) (sizeof arr / sizeof arr[0])

#define HEAP_CHUNK_SIZE (1024 * 1024)

using fzy_score_t = f64;

#define MAX_CHORD_STACK 256
#define NUM_CHORD_COMMANDS 32

#define MODAL_CMD_FULL(insert_func, edit_func, chord_func, visual_func) []( Application_Links *app )\
    {\
        switch(g_mode) {\
        case MODAL_MODE_INSERT:\
            insert_func(app);\
            break;\
        case MODAL_MODE_EDIT:\
            edit_func(app);\
            break;\
        case MODAL_MODE_VISUAL:\
            visual_func(app);\
            break;\
        case MODAL_MODE_CHORD:\
            chord_func(app);\
            break;\
        }\
    }

#define MODAL_CMD_V(edit_func, visual_func) \
    MODAL_CMD_FULL(write_character, edit_func, push_chord_char, visual_func)

#define MODAL_CMD_I(edit_func, insert_func) \
    MODAL_CMD_FULL(insert_func, edit_func, push_chord_char, edit_func)

#define MODAL_CMD(edit_func)\
    MODAL_CMD_FULL(write_character, edit_func, push_chord_char, edit_func)

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

enum ModalMode {
    MODAL_MODE_INSERT,
    MODAL_MODE_EDIT,
    MODAL_MODE_CHORD,
    MODAL_MODE_VISUAL,
};

// TODO(jesper): visual block
enum VisualMode {
    VISUAL_MODE,
    VISUAL_MODE_LINE,
};

struct ChordInfo {
    Key_Code chord_start;
    
    i32 chord_end_count;
    Key_Code *chord_end_keys;
};

static ModalMode g_mode = MODAL_MODE_EDIT;
static VisualMode g_visual_mode = VISUAL_MODE;
static u32 g_chord_stack[MAX_CHORD_STACK];
static i32 g_chord_stack_count;

static String *g_project_files;
static i32 g_project_files_count;
static i32 g_project_files_cap;

static ChordInfo g_chord_infos[NUM_CHORD_COMMANDS];

enum JumpBuffer {
    JUMP_BUFFER_BUILD,
    JUMP_BUFFER_INC_SEARCH,
    JUMP_BUFFER_COUNT
};

static Buffer_ID g_active_jump_buffer = -1;
static Buffer_ID g_jump_buffers[JUMP_BUFFER_COUNT];
static View_Summary g_jump_view;

static fzy_score_t fzy_bonus_states[3][256];
static size_t fzy_bonus_index[256];


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
    fzy_bonus_states[1]['-'] = fzy_bonus_states[2]['-'] = FZY_SCORE_MATCH_WORD;
    fzy_bonus_states[1]['_'] = fzy_bonus_states[2]['_'] = FZY_SCORE_MATCH_WORD;
    fzy_bonus_states[1][' '] = fzy_bonus_states[2][' '] = FZY_SCORE_MATCH_WORD;
    fzy_bonus_states[1]['.'] = fzy_bonus_states[2]['.'] = FZY_SCORE_MATCH_DOT;
}

#define fzy_compute_bonus(last_ch, ch) \
    (fzy_bonus_states[fzy_bonus_index[(unsigned char)(ch)]][(unsigned char)(last_ch)])

static bool ext_is_c(String ext)
{
	return match(ext, "cpp") || match(ext, "h") || match(ext, "c") || match(ext, "hpp") || match(ext, "cc");
}

static bool ext_is_c_like(String ext)
{
	return ext_is_c(ext) || match(ext, "java") || match(ext, "cs") || match(ext, "rs");
}

static void quicksort(fzy_score_t *scores, UI_Item *items, i32 *indices, i32 l, i32 r)
{
    if (l >= r) {
        return;
    }
    
    fzy_score_t pivot = scores[r];
    
    i32 cnt = l;
    
    for (i32 i = l; i <= r; i++) {
        if (scores[i] >= pivot) {
            UI_Item tmp = items[cnt];
            items[cnt] = items[i];
            items[i] = tmp;
            
            i32 tmp_index = indices[cnt];
            indices[cnt] = indices[i];
            indices[i] = tmp_index;
            
            fzy_score_t tmp_score = scores[cnt];
            scores[cnt] = scores[i];
            scores[i] = tmp_score;
            
            cnt++;
        }
    }
    
    quicksort(scores, items, indices, l, cnt-2);
    quicksort(scores, items, indices, cnt, r);
}

static i32 max(i32 a, i32 b)
{
    return a > b ? a : b;
}

static i32 min(i32 a, i32 b)
{
    return a < b ? a : b;
}

static f64 max(f64 a, f64 b)
{
    return a > b ? a : b;
}

static i32 clamp(i32 val, i32 min, i32 max)
{
    if (val > max) return max;
    if (val < min) return min;
    return val;
}

static void* heap_alloc(Application_Links *app, Heap *heap, i32 size)
{
    void *mem = heap_allocate(heap, size);
    if (mem == nullptr) {
        i32 chunk_size = max(HEAP_CHUNK_SIZE, size + 128);
        heap_extend(heap, memory_allocate(app, chunk_size), chunk_size);
        mem = heap_allocate(heap, size);
    }
    
    return mem;
}

#define heap_alloc_t(app, Type, size) (Type*)heap_alloc(app, &global_heap, size * sizeof(Type))

#include "indent.cpp"
#include "additions.cpp"

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
    case '<':
    case '>':
    case '%':
    case '[':
    case ']':
    case '{':
    case '}':
    case '(':
    case ')':
    case '\'':
    case '\"':
    case '/':
    case '\\':
    case '|':
        return true;
    default:
        return false;
    }
}

static void buffer_append(
    Application_Links *app,
    Buffer_Summary *buffer,
    char* str, i32 length)
{
    buffer_replace_range(app, buffer, buffer->size, buffer->size - 1, str, length);
}

static i32 get_chord_number(i32 *i)
{
    i32 number = 0;
    while (*i < g_chord_stack_count && char_is_numeric((char)g_chord_stack[*i]))
    {
        number = number * 10 + g_chord_stack[(*i)++] - '0';
    }
    return max(1, number);
}

static i32 seek_next_word(Application_Links *app, i32 pos)
{
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    char chunk[1024];
    Stream_Chunk stream = {};
    
    if (!init_stream_chunk(&stream, app, &buffer, pos, chunk, sizeof chunk)) {
        return pos;
    }
    
    char start_c = stream.data[pos++];
    i32 start_is_whitespace = char_is_whitespace(start_c);
    
    bool start_is_boundary =
        !start_is_whitespace &&
        is_boundary(stream.data[pos-1]);
    
    bool in_whitespace = false;
    bool was_cr = false;
    
    do {
        for (; pos < stream.end; pos++) {
            char c = stream.data[pos];
            i32 whitespace = char_is_whitespace(c);
            bool boundary = !whitespace && is_boundary(stream.data[pos]);
            
            if (c == '\r') {
                was_cr = true;
                boundary = true;
            } else {
                if (!was_cr && c == '\n') {
                    boundary = true;
                }
                was_cr = false;
            }
            
            if (start_is_boundary && !whitespace) {
                return clamp(pos, 0, buffer.size);
            }
            
            if (!start_is_whitespace) {
                if (boundary || (in_whitespace && !whitespace)) {
                    return clamp(pos, 0, buffer.size);
                } else if (whitespace) {
                    in_whitespace = true;
                }
            } else if (!whitespace || boundary) {
                return clamp(pos, 0, buffer.size);
            }
        }
    } while (forward_stream_chunk(&stream));
    
    return pos;
}

static i32 seek_prev_word(Application_Links *app, i32 pos)
{
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    char chunk[1024];
    Stream_Chunk stream = {};
    
    // NOTE(jesper): the -1 here is because if we're at the end of line this
    // function returns false, and I haven't found an edge case where this
    // doesn't work, yet
    if (!init_stream_chunk(&stream, app, &buffer, pos - 1, chunk, sizeof chunk)) {
        return pos;
    }
    
    i32 start_pos = pos;
    
    char start_c = stream.data[pos--];
    i32 start_is_whitespace = char_is_whitespace(start_c);
    
    bool start_is_boundary =
        !start_is_whitespace &&
        is_boundary(stream.data[pos+1]);
    
    bool in_whitespace = false;
    bool was_cr = false;
    
    do {
        for (; pos >= stream.start; pos--) {
            char c = stream.data[pos];
            
            i32 whitespace = char_is_whitespace(c);
            bool boundary = !whitespace && is_boundary(stream.data[pos]);
            
            if (c == '\r') {
                was_cr = true;
                boundary = true;
            } else {
                if (!was_cr && c == '\n') {
                    boundary = true;
                }
                was_cr = false;
            }
            
            if (!whitespace) {
                in_whitespace = false;
            }
            
            if (whitespace && !in_whitespace) {
                if (pos + 1 != start_pos) {
                    return clamp(pos + 1, 0, buffer.size);
                }
                
                in_whitespace = true;
            }
            
            if (start_is_boundary && !whitespace && boundary && (pos + 1 != start_pos)) {
                return clamp(pos + 1, 0, buffer.size);
            }
            
            if (!in_whitespace && boundary && pos != start_pos) {
                return clamp(pos, 0, buffer.size);
            }
        }
    } while (backward_stream_chunk(&stream));
    
    return pos;
}

static void fuzzy_update_list(
    Application_Links *app,
    Partition *scratch,
    View_Summary *view,
    Lister_State *state)
{
    bool32 is_theme_list = state->lister.data.theme_list;
    
    int32_t x0 = 0;
    int32_t x1 = view->view_region.x1 - view->view_region.x0;
    int32_t line_height = lister_get_line_height(view);
    int32_t block_height = lister_get_block_height(line_height, is_theme_list);
    int32_t text_field_height = lister_get_text_field_height(view);
    
    Temp_Memory full_temp = begin_temp_memory(scratch);
    
    int32_t mx = 0;
    int32_t my = 0;
    refresh_view(app, view);
    get_view_relative_mouse_positions(app, *view, &mx, &my, 0, 0);
    
    int32_t y_pos = text_field_height;
    
    state->raw_item_index = -1;
    
    int32_t node_count = state->lister.data.options.count;
    
    i32 key_size = state->lister.data.key_string.size;
    
    i32 item_count = 0;
    UI_Item *items = push_array(scratch, UI_Item, node_count);
    i32 *raw_indices = push_array(scratch, i32, node_count);
    
    if (key_size == 0) {
        for (Lister_Node *node = state->lister.data.options.first;
             node != nullptr;
             node = node->next)
        {
            UI_Item item = {};
            item.type = UIType_Option;
            item.option.string = node->string;
            item.option.status = node->status;
            item.activation_level = UIActivation_None;
            item.coordinates = UICoordinates_Scrolled;
            item.user_data = node->user_data;
            
            raw_indices[item_count] = node->raw_index;
            items[item_count++] = item;
        }
        
        goto finalise_list;
    }
    
    String needle = make_string((char*)partition_allocate(scratch, key_size), key_size);
    to_lower(&needle, state->lister.data.key_string);
    
    fzy_score_t *scores = push_array(scratch, fzy_score_t, node_count);
    
    i32 ni = 0;
    for (Lister_Node *node = state->lister.data.options.first;
         node != nullptr;
         node = node->next)
    {
        Temp_Memory temp = begin_temp_memory(scratch);
        
        String label = node->string;
        String haystack = make_string(partition_allocate(scratch, label.size), label.size);
        to_lower(&haystack, label);
        
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
                        
                        score = max(m_val + match_bonus[j], d_val + FZY_SCORE_MATCH_CONSECUTIVE);
                    }
                    
                    D[i * haystack.size + j] = score;
                    M[i * haystack.size + j] = prev_score = max(score, prev_score + gap_score);
                } else {
                    D[i * haystack.size + j] = FZY_SCORE_MIN;
                    M[i * haystack.size + j] = prev_score = prev_score + gap_score;
                }
            }
        }
        
        fzy_score_t match_score = M[(needle.size-1) * haystack.size + haystack.size - 1];
        
        if (match_score != FZY_SCORE_MIN) {
            UI_Item item = {};
            item.type = UIType_Option;
            item.option.string = node->string;
            item.option.status = node->status;
            item.activation_level = UIActivation_None;
            item.coordinates = UICoordinates_Scrolled;
            item.user_data = node->user_data;
            
            scores[item_count] = match_score;
            raw_indices[item_count] = node->raw_index;
            items[item_count++] = item;
        }
        
        ni++;
        
        end_temp_memory(temp);
    }
    
    quicksort(scores, items, raw_indices, 0, item_count-1);
    
finalise_list:
    UI_List list = {};
    UI_Item *highlighted_item = 0;
    UI_Item *hot_item = 0;
    UI_Item *hovered_item = 0;
    
    i32 item_index = 0;
    for (i32 i = 0; i < item_count; i++)
    {
        i32_Rect item_rect = {};
        item_rect.x0 = x0;
        item_rect.y0 = y_pos;
        item_rect.x1 = x1;
        item_rect.y1 = y_pos + block_height;
        y_pos = item_rect.y1;
        
        UI_Item item = items[i];
        item.rectangle = item_rect;
        
        UI_Item *item_ptr = ui_list_add_item(scratch, &list, item);
        if (item_rect.x0 <= mx && mx < item_rect.x1 &&
            item_rect.y0 <= my && my < item_rect.y1)
        {
            hovered_item = item_ptr;
        }
        
        if (state->item_index == item_index) {
            highlighted_item = item_ptr;
            state->raw_item_index = raw_indices[i];
        }
        
        item_index += 1;
        
        if (item.user_data == state->hot_user_data && hot_item != 0) {
            hot_item = item_ptr;
        }
    }
    
    state->item_count_after_filter = item_count;
    
    if (hovered_item != 0) {
        hovered_item->activation_level = UIActivation_Hover;
    }
    
    if (hot_item != 0) {
        if (hot_item == hovered_item){
            hot_item->activation_level = UIActivation_Active;
        } else {
            hot_item->activation_level = UIActivation_Hover;
        }
    }
    
    if (highlighted_item != 0) {
        highlighted_item->activation_level = UIActivation_Active;
    }
    
    if (state->set_view_vertical_focus_to_item){
        if (highlighted_item != 0) {
            view_set_vertical_focus(
                app, view,
                highlighted_item->rectangle.y0,
                highlighted_item->rectangle.y1);
        }
        
        state->set_view_vertical_focus_to_item = false;
    }
    
{
        i32_Rect item_rect = {};
        item_rect.x0 = x0;
        item_rect.y0 = 0;
        item_rect.x1 = x1;
        item_rect.y1 = item_rect.y0 + text_field_height;
        y_pos = item_rect.y1;
        
        UI_Item item = {};
        item.type = UIType_TextField;
        item.activation_level = UIActivation_Active;
        item.coordinates = UICoordinates_ViewRelative;
        item.text_field.query = state->lister.data.query;
        item.text_field.string = state->lister.data.text_field;
        item.user_data = 0;
        item.rectangle = item_rect;
        ui_list_add_item(scratch, &list, item);
    }
    
    UI_Control control = ui_list_to_ui_control(scratch, &list);
    view_set_ui(app, view, &control, lister_quit_function);
    
    end_temp_memory(full_temp);
}

static void fill_project_files_array(
    Application_Links *app,
    String path,
    Project_File_Pattern_Array whitelist,
    Project_File_Pattern_Array blacklist)
{
    File_List list = get_file_list(app, path.str, path.size);
    i32 dir_size = path.size;
    
    File_Info *info = list.infos;
    for (u32 i = 0; i < list.count; i++, info++)
    {
        if (info->folder) {
            if (match_in_pattern_array(info->filename, blacklist)) {
                continue;
            }
            if (info->filename[0] == '.') {
                continue;
            }
            
            // TODO(jesper): I really don't understand how the pattern arrays
            // are supposed to work....
            if (match_cc(info->filename, ".git")) {
                continue;
            }
            
            path.size = dir_size;
            String str = make_string(info->filename, info->filename_len);
            append(&path, str);
            append(&path, "/");
            
            fill_project_files_array(app, path, whitelist, blacklist);
        } else {
            if (!match_in_pattern_array(info->filename, whitelist)) {
                continue;
            }
            
            if (match_in_pattern_array(info->filename, blacklist)) {
                continue;
            }
            
            void *str_mem = heap_alloc(app, &global_heap, dir_size + info->filename_len);
            path.size = dir_size;
            
            String str = make_string_cap(str_mem, 0, dir_size + info->filename_len);
            append(&str, path);
            append(&str, make_string(info->filename, info->filename_len));
            
            if (g_project_files_count == g_project_files_cap) {
                i32 new_capacity = max(128, (g_project_files_count * 3) / 2);
                void *new_mem = heap_alloc(app, &global_heap, new_capacity * sizeof *g_project_files);
                memcpy(new_mem, g_project_files, g_project_files_cap * sizeof *g_project_files);
                heap_free(&global_heap, g_project_files);
                g_project_files = (String*)new_mem;
                g_project_files_cap = new_capacity;
            }
            
            g_project_files[g_project_files_count++] = str;
        }
    }
}

static void generate_project_file_list(
    Application_Links *app,
    Lister *lister)
{
    Partition *scratch = &global_part;
    Temp_Memory temp = begin_temp_memory(scratch);
    
    i32 size = 32 << 10;
    char *mem = push_array(scratch, char, size);
    String project_root = make_string_cap(mem, 0, size);
    project_root.size = directory_get_hot(app, project_root.str, project_root.memory_size);
    
    if (project_root.size == 0 ||
        !char_is_slash(project_root.str[project_root.size-1]))
    {
        append(&project_root, '/');
    }
    
    int32_t memory_size = 0;
    memory_size += g_project_files_count*(sizeof(Lister_Node) + 3);
    for (i32 i = 0; i < g_project_files_count; i++) {
        memory_size += g_project_files[i].size;
    }
    
    lister_begin_new_item_set(app, lister, memory_size);
    for (i32 i = 0; i < g_project_files_count; i++) {
        String label = g_project_files[i];
        label.str += project_root.size;
        label.size -= project_root.size;
        label.memory_size -= project_root.size;
        
        lister_add_item(lister, label, make_lit_string(""), &g_project_files[i], 0);
    }
    
    end_temp_memory(temp);
}

static void generate_project_command_list(
    Application_Links *app,
    Lister *lister)
{
    // TODO(jesper): currently this lists all of the built in 4coder commands,
    // which is super nice but some of them are not particularly useful.
    Partition *scratch = &global_part;
    
    Temp_Memory temp = begin_temp_memory(scratch);
    
    int32_t memory_size = 0;
    memory_size += ARRAY_COUNT(fcoder_metacmd_table)*(sizeof(Lister_Node) + 3);
    for (i32 i = 0; i < ARRAY_COUNT(fcoder_metacmd_table); i++) {
        memory_size += fcoder_metacmd_table[i].name_len;
    }
    
    lister_begin_new_item_set(app, lister, memory_size);
    for (i32 i = 0; i < ARRAY_COUNT(fcoder_metacmd_table); i++) {
        String label = make_string(fcoder_metacmd_table[i].name, fcoder_metacmd_table[i].name_len);
        lister_add_item(lister, label, make_lit_string(""), fcoder_metacmd_table[i].proc, 0);
    }
    
    end_temp_memory(temp);
}

CUSTOM_COMMAND_SIG(fuzzy_list_write_character);

static void activate_switch_file(
    Application_Links *app,
    Partition *scratch,
    Heap *heap,
    View_Summary *view,
    Lister_State *state,
    String text_field,
    void *user_data,
    bool32 activated_by_mouse)
{
    if (user_data != nullptr) {
        String *path = (String*)user_data;
        Buffer_Summary buffer = create_buffer(app, path->str, path->size, BufferCreate_NeverNew);
        view_set_buffer(app, view, buffer.buffer_id, SetBuffer_KeepOriginalGUI);
    }
    lister_default(app, scratch, heap, view, state, ListerActivation_Finished);
}

static void create_fuzzy_lister(
    Application_Links *app,
    View_Summary view,
    String key_string)
{
    view_end_ui_mode(app, &view);
    
    Lister_Handlers handlers = lister_get_default_handlers();
    handlers.write_character = fuzzy_list_write_character;
    
    handlers.activate = activate_switch_file;
    handlers.refresh = generate_project_file_list;
    
    view_begin_ui_mode(app, &view);
    view_set_setting(app, &view, ViewSetting_UICommandMap, default_lister_ui_map);
    
    Lister_State *state = view_get_lister_state(&view);
    init_lister_state(app, state, &global_heap);
    lister_first_init(app, &state->lister, nullptr, 0, 0);
    
    lister_set_query_string(&state->lister.data, "open:");
    
    if (key_string.size > 0) {
        lister_set_key_string(&state->lister.data, key_string);
        lister_set_text_field_string(&state->lister.data, key_string);
    }
    
    state->lister.data.handlers = handlers;
    handlers.refresh(app, &state->lister);
    
    fuzzy_update_list(app, &global_part, &view, state);
}

static void update_modal_indicator(Application_Links *app)
{
    Theme_Color insert_colors[] =
    {
        { Stag_Cursor, 0x99FFFFFF },
        { Stag_At_Cursor, 0xFF282828 },
        { Stag_Mark, 0xFF808080 },
        { Stag_Highlight_Cursor_Line, 0xFF3C3836 },
    };
    
    Theme_Color edit_colors[] =
    {
        { Stag_Cursor, 0xFF8a523f },
        { Stag_At_Cursor, 0xFF282828 },
        { Stag_Mark, 0xFFFF6F1A },
        { Stag_Highlight_Cursor_Line, 0xFF3b2e29 },
    };

    switch (g_mode) {
    case MODAL_MODE_INSERT:
        set_theme_colors(app, insert_colors, ARRAY_COUNT(insert_colors));
        break;
    case MODAL_MODE_EDIT:
        set_theme_colors(app, edit_colors, ARRAY_COUNT(edit_colors));
        break;
    case MODAL_MODE_CHORD:
    case MODAL_MODE_VISUAL:
        break;
    }
    
    
}


CUSTOM_COMMAND_SIG(unused_func) {}

CUSTOM_COMMAND_SIG(write_indent)
{
    u8 character[4];
    u32 length;
    if (global_config.indent_with_tabs) {
        u32_to_utf8_unchecked('\t', character, &length);
        write_character_parameter(app, character, length);
    } else {
        u32_to_utf8_unchecked(' ', character, &length);
        write_character_parameter(app, character, length);
        write_character_parameter(app, character, length);
        write_character_parameter(app, character, length);
        write_character_parameter(app, character, length);
    }
}

CUSTOM_COMMAND_SIG(custom_auto_tab_range)
    CUSTOM_DOC("Auto-indents the range between the cursor and the mark.")
{
    uint32_t access = AccessOpen;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
    Range range = get_view_range(&view);
    
    custom_buffer_auto_indent(
        app,
        &global_part,
        &buffer,
        range.min, range.max,
        DEF_TAB_WIDTH,
        DEFAULT_INDENT_FLAGS | AutoIndent_FullTokens);
    move_past_lead_whitespace(app, &view, &buffer);
}

CUSTOM_COMMAND_SIG(custom_write_and_auto_tab)
{
    exec_command(app, write_character);
    
    uint32_t access = AccessOpen;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
    
    uint32_t flags = DEFAULT_INDENT_FLAGS;
    User_Input in = get_command_input(app);
    
    custom_buffer_auto_indent(
        app,
        &global_part,
        &buffer,
        view.cursor.pos,
        view.cursor.pos,
        DEF_TAB_WIDTH,
        flags);
    
    move_past_lead_whitespace(app, &view, &buffer);
}

CUSTOM_COMMAND_SIG(custom_newline)
{
    View_Summary view = get_active_view(app, AccessOpen);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen);
    
    int start = buffer_get_line_start(app, &buffer, view.cursor.line);
    int hard_start = get_start_of_line_at_cursor(app, &view, &buffer);

    String name = make_string(buffer.file_name, buffer.file_name_len);
	String ext = file_extension(name);

    if (ext_is_c_like(ext) &&
        c_line_comment_starts_at_position(app, &buffer, hard_start))
    {
        Hard_Start_Result after_comment = buffer_find_hard_start(app, &buffer, hard_start+2, DEF_TAB_WIDTH);
        hard_start = after_comment.char_pos;
        
        Partition *scratch = &global_part;
        Temp_Memory temp = begin_temp_memory(scratch);
        
        int size = hard_start - start;
        char *str = push_array(scratch, char, size);
        
        if(str != 0) {
            buffer_read_range(app, &buffer, start, hard_start, str);
            
            String prev_line_continuation = make_string(str, hard_start - start);
            
            write_string(app, make_lit_string("\n"));
            write_string(app, prev_line_continuation);
        }
        
        end_temp_memory(temp);
    }
    else
    {
        custom_write_and_auto_tab(app);
    }
}

CUSTOM_COMMAND_SIG(combine_with_next_line)
{
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    seek_end_of_line(app);
    refresh_view(app, &view);
    
    i32 start = view.cursor.pos;
    move_down(app);
    seek_beginning_of_line(app);
    move_past_lead_whitespace(app, &view, &buffer);
    refresh_view(app, &view);
    
    i32 end = view.cursor.pos;
    char space[1] = { ' ' };
    buffer_replace_range(app, &buffer, start, end, space, sizeof space);
    move_right(app);
}

CUSTOM_COMMAND_SIG(set_insert_mode)
{
    g_mode = MODAL_MODE_INSERT;
    g_chord_stack_count = 0;
    
    update_modal_indicator(app);
}

CUSTOM_COMMAND_SIG(set_insert_mode_beginning)
{
    seek_beginning_of_line(app);
    
    View_Summary view = get_active_view(app, AccessOpen);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen);
    
    move_past_lead_whitespace(app, &view, &buffer);
    set_insert_mode(app);
}

CUSTOM_COMMAND_SIG(set_insert_mode_end)
{
    seek_end_of_line(app);
    set_insert_mode(app);
}

CUSTOM_COMMAND_SIG(set_edit_mode)
{
    g_mode = MODAL_MODE_EDIT;
    g_chord_stack_count = 0;
    
    View_Summary view = get_active_view(app, AccessAll);
    view_set_mark(app, &view, seek_pos(-1));
    
    update_modal_indicator(app);
}

CUSTOM_COMMAND_SIG(set_visual_mode)
{
    g_mode = MODAL_MODE_VISUAL;
    g_visual_mode = VISUAL_MODE;
    g_chord_stack_count = 0;
    
    View_Summary view = get_active_view(app, AccessAll);
    view_set_mark(app, &view, seek_pos(view.cursor.pos));
    
    update_modal_indicator(app);
}

CUSTOM_COMMAND_SIG(set_visual_mode_line)
{
    seek_beginning_of_line(app);
    set_visual_mode(app);
    g_visual_mode = VISUAL_MODE_LINE;
    
    update_modal_indicator(app);
}

CUSTOM_COMMAND_SIG(set_chord_mode)
{
    User_Input trigger = get_command_input(app);
    g_chord_stack_count = 0;
    g_chord_stack[g_chord_stack_count++] = trigger.key.character;
    g_mode = MODAL_MODE_CHORD;
    
    update_modal_indicator(app);
}

CUSTOM_COMMAND_SIG(exec_chord)
{
    // NOTE(jesper): this function does not always clear the chord stack because
    // it is possible to defer chord execution to multiple passes
    //
    // e.g 32dd would trigger exec_chord at first 'd', but 32d means nothing on
    // its own and needs a second pass to interpret as "delete 32 lines"
    
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    i32 i = 0;
    i32 saved_cursor = view.cursor.pos;
    
    i32 number = get_chord_number(&i);
    
    switch  (g_chord_stack[i++]) {
    case 'g':
        switch (g_chord_stack[i]) {
        case 'g':
            goto_beginning_of_file(app);
            goto cmd_done;
        }
        break;
    case 'y':
        number = number * get_chord_number(&i);
        for (; i < g_chord_stack_count; i++) {
            switch (g_chord_stack[i]) {
            case 'y':
                seek_beginning_of_line(app);
                set_mark(app);
                move_vertical(app, 1.0f * number);
                copy(app);
                view_set_cursor(app, &view, seek_pos(saved_cursor), true);
                goto cmd_done;
            case 'w':
                set_mark(app);
                while (number--) {
                    move_word(app);
                }
                copy(app);
                view_set_cursor(app, &view, seek_pos(saved_cursor), true);
                goto cmd_done;
            case 'b':
                set_mark(app);
                while (number--) {
                    move_word_back(app);
                }
                copy(app);
                view_set_cursor(app, &view, seek_pos(saved_cursor), true);
                goto cmd_done;
            }
        }
        break;
    case 'd':
        number = number * get_chord_number(&i);
        
        for (; i < g_chord_stack_count; i++) {
            switch (g_chord_stack[i]) {
            case 'd': {
                    Temp_Memory tmp = begin_temp_memory(&global_part);
                    
                    i32 start = buffer_get_line_start(app, &buffer, view.cursor.line);
                    i32 end = buffer_get_line_end(app, &buffer, view.cursor.line) + 1;
                    end = min(end, buffer.size);
                    
                    char last_char = buffer_get_char(app, &buffer, end - 1);
                    if (start == end || last_char != '\n') {
                        start = max(start - 1, 0);
                    }
                    
                    if (post_buffer_range_to_clipboard(app, &global_part, 0, &buffer, start, end)) {
                        buffer_replace_range(app, &buffer, start, end, 0, 0);
                    }
                    seek_beginning_of_line(app);
                    
                    end_temp_memory(tmp);
                    goto cmd_done;
                } break;
            case 'w':
                set_mark(app);
                while (number--) {
                    move_word(app);
                }
                cut(app);
                goto cmd_done;
            case 'b':
                set_mark(app);
                while (number--) {
                    move_word_back(app);
                }
                cut(app);
                goto cmd_done;
            }
        }
        break;
    case 'w': {
            i32 pos = view.cursor.pos;
            while (number--) {
                pos = seek_next_word(app, pos);
            }
            view_set_cursor(app, &view, seek_pos(pos), true);
            goto cmd_done;
        }
    case 'b': {
            i32 pos = view.cursor.pos;
            while (number--) {
                pos = seek_prev_word(app, pos);
            }
            view_set_cursor(app, &view, seek_pos(pos), true);
            goto cmd_done;
        }
    case 'j':
        // NOTE(jesper): clamping to avoid float precision problems
        move_vertical(app, 1.0f * (f32)min(number, 655345));
        goto cmd_done;
    case 'k':
        // NOTE(jesper): clamping to avoid float precision problems
        move_vertical(app, -1.0f * (f32)min(number, 655345));
        goto cmd_done;
    default:
        goto cmd_done;
    }
    
    return;
cmd_done:
    g_chord_stack_count = 0;
    g_mode = MODAL_MODE_EDIT;
    update_modal_indicator(app);
}

CUSTOM_COMMAND_SIG(push_chord_char)
{
    User_Input trigger = get_command_input(app);
    g_chord_stack[g_chord_stack_count++] = trigger.key.character;
    
    if (g_chord_stack_count == MAX_CHORD_STACK) {
        exec_chord(app);
        
        // NOTE(jesper): exec_chord doesn't always reset these as it's possible
        // to still queue up chords for further execution
        //
        // In this case, our chord stack is full, so even if it did not execute
        // we need to clear it and return to edit keymap
        //
        // TODO(jesper): error reporting
        g_chord_stack_count = 0;
        g_mode = MODAL_MODE_EDIT;
        update_modal_indicator(app);
        return;
    }
    
    for (i32 i = 0; i < ARRAY_COUNT(g_chord_infos); i++) {
        if (g_chord_infos[i].chord_start == g_chord_stack[0] ) {
            for (i32 j = 0; j < g_chord_infos[i].chord_end_count; j++) {
                if (g_chord_infos[i].chord_end_keys[j] == trigger.key.character) {
                    exec_chord(app);
                    return;
                }
            }
            return;
        }
    }
}

CUSTOM_COMMAND_SIG(move_word)
    CUSTOM_DOC("Move cursor one word forward")
{
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    i32 pos = seek_next_word(app, view.cursor.pos);
    view_set_cursor(app, &view, seek_pos(pos), true);
}

CUSTOM_COMMAND_SIG(move_word_back)
    CUSTOM_DOC("Move cursor one word back")
{
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    i32 pos = seek_prev_word(app, view.cursor.pos);
    view_set_cursor(app, &view, seek_pos(pos), true);
}

CUSTOM_COMMAND_SIG(fuzzy_list_write_character)
{
    Partition *scratch = &global_part;
    View_Summary view = get_active_view(app, AccessAll);
    Lister_State *state = view_get_lister_state(&view);
    if (state->initialized){
        User_Input in = get_command_input(app);
        uint8_t character[4];
        uint32_t length = to_writable_character(in, character);
        if (length > 0){
            append(&state->lister.data.text_field, make_string(character, length));
            append(&state->lister.data.key_string, make_string(character, length));
            state->item_index = 0;
            view_zero_scroll(app, &view);
            fuzzy_update_list(app, scratch, &view, state);
        }
    }
}

static void command_list_execute(
    Application_Links *app,
    Partition *scratch,
    Heap *heap,
    View_Summary *view,
    Lister_State *state,
    String text_field,
    void *user_data,
    bool32 activated_by_mouse)
{
    lister_default(app, scratch, heap, view, state, ListerActivation_Finished);
    if (user_data != nullptr) {
        Custom_Command_Function *func = (Custom_Command_Function*)user_data;
        func(app);
    }
}

CUSTOM_COMMAND_SIG(custom_load_project)
    CUSTOM_DOC("custom load project")
{
    save_all_dirty_buffers(app);
    
    Partition *scratch = &global_part;
    
    Project_Parse_Result parse_result = parse_project__nearest_file(app, scratch);
    Project *project = parse_result.project;
    
    if (project == nullptr && current_project.loaded == false) {
        CString_Array extensions = get_code_extensions(&global_config.code_exts);
        Temp_Memory temp = begin_temp_memory(scratch);
        
        i32 size = 32 << 10;
        char *mem = push_array(scratch, char, size);
        String space = make_string_cap(mem, 0, size);
        space.size = directory_get_hot(app, space.str, space.memory_size);
        
        if (space.size == 0 || !char_is_slash(space.str[space.size-1])) {
            append(&space, '/');
        }
        
        // TODO(jesper): this doesn't seem to work correctly for folders,
        // haven't figured out why yet
        char *str_arr[] = {
            ".*",
            ".git/",
            ".git"
            };
        
        CString_Array black_array = {};
        black_array.strings = str_arr;
        black_array.count = ARRAY_COUNT(str_arr);
        Project_File_Pattern_Array blacklist = get_pattern_array_from_cstring_array(scratch, black_array);
        Project_File_Pattern_Array whitelist = get_pattern_array_from_cstring_array(scratch, extensions);
        
        fill_project_files_array(app, space, whitelist, blacklist);
        end_temp_memory(temp);
        return;
    }
    
    if (project) {
        Temp_Memory temp = begin_temp_memory(scratch);
        
        if (current_project_arena.base == 0){
            int32_t project_mem_size = (1 << 20);
            void *project_mem = memory_allocate(app, project_mem_size);
            current_project_arena = make_part(project_mem, project_mem_size);
        }
        
        // Copy project to current_project
        current_project_arena.pos = 0;
        Project new_project = project_deep_copy(&current_project_arena, project);
        
        if (new_project.loaded) {
            current_project = new_project;
            
            g_project_files_count = 0;
            
            i32 size = 32 << 10;
            char *space_mem = push_array(scratch, char, size);
            String space = make_string_cap(space_mem, 0, size);
            
            String project_dir = current_project.dir;
            Project_File_Pattern_Array blacklist = current_project.blacklist_pattern_array;
            Project_File_Pattern_Array whitelist = current_project.pattern_array;
            for (i32 i = 0; i < current_project.load_path_array.count; i++) {
                Project_File_Load_Path *load_path = &current_project.load_path_array.paths[i];
                
                space.size = 0;
                append(&space, load_path->path);
                
                if (load_path->relative) {
                    space.size = 0;
                    append(&space, project_dir);
                    
                    if (space.size == 0 || !char_is_slash(space.str[space.size-1])) {
                        append(&space, '/');
                    }
                    
                    append(&space, load_path->path);
                    if (space.size == 0 || !char_is_slash(space.str[space.size-1])) {
                        append(&space, '/');
                    }
                }
                
                // TODO(jesper): support recursive flag from paths array
                fill_project_files_array(app, space, whitelist, blacklist);
            }
            
            // Set window title
            if (project->name.size > 0){
                char buffer[1024];
                String builder = make_fixed_width_string(buffer);
                append(&builder, "4coder project: ");
                append(&builder, project->name);
                terminate_with_null(&builder);
                set_window_title(app, builder.str);
            }
        }
        
        end_temp_memory(temp);
    }
}

CUSTOM_COMMAND_SIG(fuzzy_find_file)
    CUSTOM_DOC("Fuzzy find and open a file in the project")
{
    View_Summary view = get_active_view(app, AccessAll);
    create_fuzzy_lister(app, view, {});
}

CUSTOM_COMMAND_SIG(find_corresponding_file)
    CUSTOM_DOC("Find the corresponding header/source file")
{
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    String filename = make_string(buffer.buffer_name, buffer.buffer_name_len);
    String ext = file_extension(filename);
    
    String corresponding = {};
    
    if (match_cs("cpp", ext) || match_cs("c", ext)) {
        corresponding = make_string((char*)partition_allocate(&global_part, filename.size), filename.size);
        copy_ss(&corresponding, make_string(filename.str, filename.size - ext.size));
        append(&corresponding, "h");
    } else if (match_cs("h", ext) || match_cs("hpp", ext)) {
        corresponding = make_string((char*)partition_allocate(&global_part, filename.size), filename.size);
        copy_ss(&corresponding, make_string(filename.str, filename.size - ext.size));
        append(&corresponding, "c");
    }
    
    if (corresponding.size > 0) {
        create_fuzzy_lister(app, view, corresponding);
    }
}

CUSTOM_COMMAND_SIG(fuzzy_exec_command)
{
    View_Summary view = get_active_view(app, AccessAll);
    view_end_ui_mode(app, &view);
    
    Lister_Handlers handlers = lister_get_default_handlers();
    handlers.write_character = fuzzy_list_write_character;
    
    handlers.activate = command_list_execute;
    handlers.refresh = generate_project_command_list;
    
    view_begin_ui_mode(app, &view);
    view_set_setting(app, &view, ViewSetting_UICommandMap, default_lister_ui_map);
    
    Lister_State *state = view_get_lister_state(&view);
    init_lister_state(app, state, &global_heap);
    lister_first_init(app, &state->lister, nullptr, 0, 0);
    
    lister_set_query_string(&state->lister.data, ":");
    state->lister.data.handlers = handlers;
    handlers.refresh(app, &state->lister);
    
    fuzzy_update_list(app, &global_part, &view, state);
}

CUSTOM_COMMAND_SIG(fuzzy_list_repaint)
{
    Partition *scratch = &global_part;
    View_Summary view = get_active_view(app, AccessAll);
    Lister_State *state = view_get_lister_state(&view);
    
    if (state->initialized) {
        fuzzy_update_list(app, scratch, &view, state);
    }
}

CUSTOM_COMMAND_SIG(visual_cut_range)
{
    if (g_visual_mode == VISUAL_MODE_LINE) {
        View_Summary view = get_active_view(app, AccessOpen);
        Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen);
        
        i32 start = view.cursor.pos;
        i32 end = view.mark.pos;
        if (start > end) {
            i32 tmp = start;
            start = end;
            end = tmp;
        }
        
        start = seek_line_beginning(app, &buffer, start);
        end = seek_line_end(app, &buffer, end);
        
        if (post_buffer_range_to_clipboard(app, &global_part, 0, &buffer, start, end)) {
            buffer_replace_range(app, &buffer, start, end, 0, 0);
        }
    } else {
        cut(app);
    }
    
    set_edit_mode(app);
}

CUSTOM_COMMAND_SIG(visual_substitute)
{
    replace_in_range(app);
    set_edit_mode(app);
}

CUSTOM_COMMAND_SIG(substitute)
{
    View_Summary view = get_active_view(app, AccessOpen);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen);
    if (!buffer.exists){
        return;
    }
    
    Query_Bar replace = {};
    char replace_space[1024];
    replace.prompt = make_lit_string("Replace: ");
    replace.string = make_fixed_width_string(replace_space);
    
    if (!query_user_string(app, &replace)){
        return;
    }
    if (replace.string.size == 0){
        return;
    }
    
    query_replace_parameter(app, replace.string, 0, false);
}

CUSTOM_COMMAND_SIG(visual_copy_range)
{
    if (g_visual_mode == VISUAL_MODE_LINE) {
        View_Summary view = get_active_view(app, AccessOpen);
        Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen);
        
        i32 start = view.cursor.pos;
        i32 end = view.mark.pos;
        
        if (start > end) {
            i32 tmp = start;
            start = end;
            end = tmp;
        }
        
        start = seek_line_beginning(app, &buffer, start);
        end = seek_line_end(app, &buffer, end) + 1;
        end = min(end, buffer.size);
        
        post_buffer_range_to_clipboard(app, &global_part, 0, &buffer, start, end);
    } else {
        copy(app);
    }
    
    set_edit_mode(app);
}

CUSTOM_COMMAND_SIG(cut_to_end_of_line)
{
    View_Summary view = get_active_view(app, AccessOpen);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen);
    
    i32 start = view.cursor.pos;
    i32 end = seek_line_end(app, &buffer, start);
    
    if (post_buffer_range_to_clipboard(app, &global_part, 0, &buffer, start, end)) {
        buffer_replace_range(app, &buffer, start, end, 0, 0);
    }
}

CUSTOM_COMMAND_SIG(custom_seek_beginning_of_line)
{
    seek_beginning_of_line(app);
    View_Summary view = get_active_view(app, AccessOpen);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen);
    move_past_lead_whitespace(app, &view, &buffer);
}

CUSTOM_COMMAND_SIG(custom_next_jump)
{
}

CUSTOM_COMMAND_SIG(custom_prev_jump)
{
}

CUSTOM_COMMAND_SIG(custom_isearch)
{
    if (g_active_jump_buffer != g_jump_buffers[JUMP_BUFFER_INC_SEARCH]) {
        g_active_jump_buffer = g_jump_buffers[JUMP_BUFFER_INC_SEARCH];
        view_set_buffer(app, &g_jump_view, g_active_jump_buffer, SetBuffer_KeepOriginalGUI);
    }
    
    
    Partition *scratch = &global_part;
    
    // TODO(jesper): replace this stuff with casey's new gap buffer
    // incremental search when it's available
    
    View_Summary view = get_active_view(app, AccessProtected);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessProtected);
    String buffer_name = make_string(buffer.buffer_name, buffer.buffer_name_len);
    
    if (!buffer.exists) return;
    
    Buffer_Summary jump_buffer = get_buffer(app, g_active_jump_buffer, AccessAll);
    View_Summary jump_view = get_first_view_with_buffer(app, g_active_jump_buffer);
    i32 size = jump_buffer.size;
    
    char prompt_buffer[256];
    Query_Bar bar = {};
    bar.prompt = make_lit_string("I-Search: ");
    bar.string = make_fixed_width_string(prompt_buffer);
    
    if (start_query_bar(app, &bar, 0) == 0)
        return;
    
    i32 saved_view_pos = view.cursor.pos;
    (void)saved_view_pos;
    
    char chunk[1024];
    Stream_Chunk stream = {};
    
    char read_buffer[512];
    String read_str = make_fixed_width_string(read_buffer);
    
    struct JumpLocation {
        i32 absolute;
        i32 line;
        i32 column;
    };
    
    i32 locations_count = 0;
    i32 locations_cap = 1024;
    JumpLocation *locations = heap_alloc_t(app, JumpLocation, locations_cap);
    
    User_Input in = {};
    while (true) {
        in = get_user_input(app, EventOnAnyKey, EventOnEsc);
        
        if (in.abort) {
            view_set_cursor(app, &view, seek_pos(saved_view_pos), true);
            view_set_cursor(app, &jump_view, seek_pos(0), true);
            break;
        }
        
        u8 character[4];
        u32 length = to_writable_character(in, character);
        
        if (in.key.keycode == '\n') {
            lock_jump_buffer(jump_buffer);
            break;
        } else if (in.key.keycode == key_back) {
            backspace_utf8(&bar.string);
            locations_count = 0;
            view.cursor.pos = saved_view_pos;
            view_set_cursor(app, &jump_view, seek_pos(0), true);
        } else if (length != 0) {
            i32 old_size = bar.string.size;
            append(&bar.string, make_string(character, length));
            view.cursor.pos = saved_view_pos;
            view_set_cursor(app, &jump_view, seek_pos(0), true);
            
            if (old_size > 0) {
                char last_char = char_to_upper(bar.string.str[bar.string.size-1]);
                for (i32 i = 0; i < locations_count; i++) {
                    i32 pos = locations[i].absolute + old_size;
                    
                    if (!init_stream_chunk(&stream, app, &buffer, pos, chunk, sizeof chunk))
                        continue;
                    
                    if (last_char == char_to_upper(stream.data[pos]))
                        continue;
                    
                    // TODO(jesper): memmove?
                    for (i32 j = i; j < locations_count-1; j++) {
                        locations[j] = locations[j+1];
                    }
                    i--;
                    locations_count--;
                }
                
                goto print_jump_buffer;
            }
        }
        
        if (bar.string.size > 0) {
            read_str.size = bar.string.size;
            
            char first_char = char_to_upper(bar.string.str[0]);
            
            i32 pos = 0;
            if (!init_stream_chunk(&stream, app, &buffer, pos, chunk, sizeof chunk))
                break;
            
            do {
                for (; pos < stream.end; pos++) {
                    if (first_char == char_to_upper(stream.data[pos])) {
                        buffer_read_range(app, &buffer, pos, pos + bar.string.size, read_buffer);
                        if (match_insensitive_ss(bar.string, read_str)) {
                            Partial_Cursor cursor = {};
                            if (buffer_compute_cursor(app, &buffer, seek_pos(pos), &cursor)) {
                                JumpLocation location = {};
                                location.absolute = pos;
                                location.line = cursor.line;
                                location.column = cursor.character;
                                
                                if (locations_count == locations_cap) {
                                    i32 new_cap = locations_cap * 3 / 2;
                                    auto new_locs = heap_alloc_t(app, JumpLocation, new_cap);
                                    memcpy(new_locs,
                                           locations,
                                           locations_cap * sizeof *locations);
                                    heap_free(&global_heap, locations);
                                    locations = new_locs;
                                    locations_cap = new_cap;
                                }
                                
                                locations[locations_count++] = location;
                            }
                        }
                    }
                }
            } while (forward_stream_chunk(&stream));
            
print_jump_buffer:
            i32 closest_location_i = -1;
            i32 closest_pos = I32_MAX;
            Temp_Memory temp = begin_temp_memory(scratch);
            
            i32 str_size = 0;
            char *str = (char*)partition_current(scratch);
            
            buffer_replace_range(app, &jump_buffer, 0, size, 0, 0);
            size = 0;
            
            for (i32 i = 0; i < locations_count; i++) {
                pos = locations[i].absolute;
                
                if (abs(pos - saved_view_pos) < closest_pos ||
                    (pos >= saved_view_pos &&
                    closest_pos < saved_view_pos))
                {
                    closest_location_i = i;
                    closest_pos = pos;
                }
                
                i32 line_number_len = int_to_str_size(locations[i].line);
                i32 column_len = int_to_str_size(locations[i].column);
                
                i32 line_start = buffer_get_line_start(app, &buffer, locations[i].line);
                i32 line_end = buffer_get_line_end(app, &buffer, locations[i].line);
                
                i32 message_start = max(pos - 10, line_start);
                i32 message_end = line_end;
                i32 message_len = message_end - message_start;
                
                i32 append_len = buffer_name.size + 1 + line_number_len + 1 + column_len + 2 + message_len + 1;
                
                char *space = push_array(scratch, char, append_len);
                if (space == 0) {
                    buffer_replace_range(app, &jump_buffer, size, size, str, str_size);
                    size += str_size;
                    
                    end_temp_memory(temp);
                    temp = begin_temp_memory(scratch);
                    
                    str_size = 0;
                    space = push_array(scratch, char, append_len);
                }
                
                str_size += append_len;
                String out = make_string(space, 0, append_len);
                append_ss(&out, buffer_name);
                append_s_char(&out, ':');
                
                append_int_to_str(&out, locations[i].line);
                append_s_char(&out, ':');
                
                append_int_to_str(&out, locations[i].column);
                append_s_char(&out, ':');
                append_s_char(&out, ' ');
                
                
                pos = message_start;
                if (init_stream_chunk(&stream, app, &buffer, pos, chunk, sizeof chunk)) {
                    do {
                        for (; pos < stream.end && pos < message_end; pos++) {
                            append_s_char(&out, stream.data[pos]);
                        }
                    } while (forward_stream_chunk(&stream) && pos < message_end);
                }
                
                append_s_char(&out, '\n');
            }
            
            buffer_replace_range(app, &jump_buffer, size, size, str, str_size);
            size += str_size;
            str_size = 0;
            end_temp_memory(temp);
            
            if (closest_location_i != -1) {
                JumpLocation location = locations[closest_location_i];
                view_set_cursor(app, &view, seek_pos(location.absolute), true);
                view_set_cursor(app, &jump_view, seek_line_char(closest_location_i+1, 0), true);
            }
        }
    }
    
    heap_free(&global_heap, locations);
}

CUSTOM_COMMAND_SIG(custom_paste_and_indent)
{
    paste(app);
    custom_auto_tab_range(app);
}

CUSTOM_COMMAND_SIG(custom_paste_next_and_indent)
{
    paste_next(app);
    custom_auto_tab_range(app);
}

CUSTOM_COMMAND_SIG(jump_buffer_1)
{
    if (g_active_jump_buffer != g_jump_buffers[0]) {
        g_active_jump_buffer = g_jump_buffers[0];
        view_set_buffer(app, &g_jump_view, g_active_jump_buffer, SetBuffer_KeepOriginalGUI);
        
        Buffer_Summary jump_buffer = get_buffer(app, g_active_jump_buffer, AccessAll);
        lock_jump_buffer(jump_buffer);
        return;
    }
    
    // NOTE(jesper): re-do the command for jump_buffer_1 if it's already the active buffer
    
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    Buffer_Summary jump_buffer = get_buffer(app, g_active_jump_buffer, AccessAll);
    View_Summary jump_view = get_first_view_with_buffer(app, g_active_jump_buffer);
    
    custom_execute_standard_build(app, &jump_view, &buffer, buffer_identifier(g_active_jump_buffer));
    set_fancy_compilation_buffer_font(app);
    
    memset(&prev_location, 0, sizeof(prev_location));
    lock_jump_buffer(jump_buffer);
}

CUSTOM_COMMAND_SIG(jump_buffer_2)
{
    if (g_active_jump_buffer != g_jump_buffers[1]) {
        g_active_jump_buffer = g_jump_buffers[1];
        view_set_buffer(app, &g_jump_view, g_active_jump_buffer, SetBuffer_KeepOriginalGUI);
        
        Buffer_Summary jump_buffer = get_buffer(app, g_active_jump_buffer, AccessAll);
        lock_jump_buffer(jump_buffer);
        return;
    }
    
    // TODO(jesper): this should re-do the command used for jump_buffer_2 if it's already the active buffer
}

CUSTOM_COMMAND_SIG(seek_matching_scope)
{
    View_Summary view = get_active_view(app, AccessAll);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessAll);
    
    char chunk[1024];
    Stream_Chunk stream = {};
    
    if (!init_stream_chunk(&stream, app, &buffer, view.cursor.pos, chunk, sizeof chunk)) {
        return;
    }
    
    i32 pos = view.cursor.pos;
    i32 final_pos = view.cursor.pos;
    i32 brace_level = 0;
    
    char start_c = stream.data[view.cursor.pos];
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
        i32 line_start = buffer_get_line_start(app, &buffer, view.cursor.line);
        i32 line_end = buffer_get_line_end(app, &buffer, view.cursor.line);
        
        i32 backward_dist = -1;
        i32 forward_dist = -1;
        char forward_c = ' ';
        char backward_c = ' ';
        
        for (i32 i = pos; i < line_end; i++) {
            while (i >= stream.end) {
                forward_stream_chunk(&stream);
            }
            
            if (stream.data[i] == '{' ||
                stream.data[i] == '}' ||
                stream.data[i] == '[' ||
                stream.data[i] == ']' ||
                stream.data[i] == '(' ||
                stream.data[i] == ')')
            {
                forward_c = stream.data[i];
                forward_dist = i - pos;
                break;
            }
        }
        
        for (i32 i = pos; i >= line_start; i--) {
            while (i < stream.start) {
                backward_stream_chunk(&stream);
            }
            
            if (stream.data[i] == '{' ||
                stream.data[i] == '}' ||
                stream.data[i] == '[' ||
                stream.data[i] == ']' ||
                stream.data[i] == '(' ||
                stream.data[i] == ')')
            {
                backward_c = stream.data[i];
                backward_dist = pos - i;
                break;
            }
        }
        
        if (forward_dist == -1) {
            start_c = backward_c;
            pos -= backward_dist;
            goto init_chars;
        } else if (backward_dist == -1) {
            start_c = forward_c;
            pos += forward_dist;
            goto init_chars;
        } else if (forward_dist < backward_dist) {
            start_c = forward_c;
            pos += forward_dist;
            goto init_chars;
        } else if (backward_dist < forward_dist) {
            start_c = backward_c;
            pos -= backward_dist;
            goto init_chars;
        }
        
        goto pos_found;
    }
    
    while (pos >= stream.end) {
        forward_stream_chunk(&stream);
    }
    
    while (pos < stream.start) {
        backward_stream_chunk(&stream);
    }
    
    // TODO(jesper): we need to figure out what to do about comments here
    if (forward) {
        do {
            for (; pos < stream.end; pos++) {
                char c = stream.data[pos];
                
                if (c == opening_c) brace_level++;
                if (c == closing_c) {
                    if (--brace_level == 0) {
                        final_pos = pos;
                        goto pos_found;
                    }
                    
                }
            }
        } while (forward_stream_chunk(&stream));
    } else {
        do {
            for (; pos >= stream.start; pos--) {
                char c = stream.data[pos];
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
        } while (backward_stream_chunk(&stream));
    }
    
pos_found:
    view_set_cursor(app, &view, seek_pos(final_pos), true);
}

OPEN_FILE_HOOK_SIG(custom_save_file_hook)
{
    Buffer_Summary buffer = get_buffer(app, buffer_id, AccessAll);
    Assert(buffer.exists);
    
#if 0
    u32 flags = DEFAULT_INDENT_FLAGS | AutoIndent_FullTokens | AutoIndent_ClearLine;
    if (global_config.automatically_indent_text_on_save) {
        custom_buffer_auto_indent(
            app,
            &global_part,
            &buffer,
            0, buffer.size,
            DEF_TAB_WIDTH,
            flags);
    }
#endif
    
    clean_all_lines(app);
    return 0;
}

OPEN_FILE_HOOK_SIG(custom_open_file_hook)
{
    Buffer_Summary buffer = get_buffer(app, buffer_id, AccessAll);
    Assert(buffer.exists);
    
    bool32 treat_as_code = false;
    bool32 treat_as_todo = false;
    bool32 lex_without_strings = false;
    
    CString_Array extensions = get_code_extensions(&global_config.code_exts);
    
    Parse_Context_ID parse_context_id = 0;
    
    if (buffer.file_name != 0 && buffer.size < (16 << 20)){
        String name = make_string(buffer.file_name, buffer.file_name_len);
        String ext = file_extension(name);
        for (int32_t i = 0; i < extensions.count; ++i){
            if (match(ext, extensions.strings[i])){
                treat_as_code = true;
                
                if (match(ext, "cs")){
                    if (parse_context_language_cs == 0){
                        init_language_cs(app);
                    }
                    parse_context_id = parse_context_language_cs;
                }
                
                if (match(ext, "java")){
                    if (parse_context_language_java == 0){
                        init_language_java(app);
                    }
                    parse_context_id = parse_context_language_java;
                }
                
                if (match(ext, "rs")){
                    if (parse_context_language_rust == 0){
                        init_language_rust(app);
                    }
                    parse_context_id = parse_context_language_rust;
                    lex_without_strings = true;
                }
                
                if (match(ext, "cpp") ||
                    match(ext, "h") ||
                    match(ext, "c") ||
                    match(ext, "hpp") ||
                    match(ext, "cc"))
                {
                    if (parse_context_language_cpp == 0){
                        init_language_cpp(app);
                    }
                    parse_context_id = parse_context_language_cpp;
                }
                
                if (match(ext, "glsl")){
                    if (parse_context_language_cpp == 0){
                        init_language_cpp(app);
                    }
                    parse_context_id = parse_context_language_cpp;
                }
                
                if (match(ext, "m")){
                    if (parse_context_language_cpp == 0){
                        init_language_cpp(app);
                    }
                    parse_context_id = parse_context_language_cpp;
                }
                
                break;
            }
        }
        
        if (!treat_as_code){
            treat_as_todo = match_insensitive(front_of_directory(name), "todo.txt");
        }
    }
    
    int32_t map_id = (treat_as_code)?((int32_t)default_code_map):((int32_t)mapid_file);
    int32_t map_id_query = 0;
    
    buffer_set_setting(app, &buffer, BufferSetting_MapID, default_lister_ui_map);
    buffer_get_setting(app, &buffer, BufferSetting_MapID, &map_id_query);
    Assert(map_id_query == default_lister_ui_map);
    
    buffer_set_setting(app, &buffer, BufferSetting_WrapPosition, global_config.default_wrap_width);
    buffer_set_setting(app, &buffer, BufferSetting_MinimumBaseWrapPosition, global_config.default_min_base_width);
    buffer_set_setting(app, &buffer, BufferSetting_MapID, map_id);
    buffer_get_setting(app, &buffer, BufferSetting_MapID, &map_id_query);
    Assert(map_id_query == map_id);
    buffer_set_setting(app, &buffer, BufferSetting_ParserContext, parse_context_id);
    
    // NOTE(allen): Decide buffer settings
    bool32 wrap_lines = true;
    bool32 use_lexer = false;
    
    if (treat_as_todo) {
        lex_without_strings = true;
        wrap_lines = true;
        use_lexer = true;
    } else if (treat_as_code) {
        wrap_lines = global_config.enable_code_wrapping;
        use_lexer = true;
    }
    
    if (match(make_string(buffer.buffer_name, buffer.buffer_name_len), "*compilation*")){
        wrap_lines = false;
    }
    
    if (buffer.size >= (128 << 10)) {
        wrap_lines = false;
    }
    
    buffer_set_setting(app, &buffer, BufferSetting_LexWithoutStrings, lex_without_strings);
    buffer_set_setting(app, &buffer, BufferSetting_WrapLine, wrap_lines);
    buffer_set_setting(app, &buffer, BufferSetting_Lex, use_lexer);
    
    return 0;
}


START_HOOK_SIG(custom_init)
{
    fzy_init_table();
    
    default_4coder_initialize(app);
    hide_scrollbar(app);
    custom_load_project(app);
    
    change_theme(app, literal("Grouse"));
    do_matching_enclosure_highlight = false;
    set_global_face_by_name(app, literal("Droid Sans Mono"), true);
    g_mode = MODAL_MODE_EDIT;
    update_modal_indicator(app);
    
    
    View_Summary view = get_active_view(app, AccessAll);
    View_Summary bottom = open_view(app, &view, ViewSplit_Bottom);
    view_set_split_proportion(app, &bottom, 0.2f);
    view_set_passive(app, &bottom, true);
    view_set_setting(app, &bottom, ViewSetting_ShowFileBar, 0);
    
    char mem[256];
    for (i32 i = 0; i < JUMP_BUFFER_COUNT; i++) {
        String buffer_name = make_fixed_width_string(mem);
        append(&buffer_name, "*jump_");
        append_int_to_str(&buffer_name, i);
        append(&buffer_name, "*");
        
        Buffer_Summary buffer = create_buffer(
            app,
            buffer_name.str, buffer_name.size,
            BufferCreate_AlwaysNew );
        
        buffer_set_setting(app, &buffer, BufferSetting_Unimportant, true);
        buffer_set_setting(app, &buffer, BufferSetting_ReadOnly, true);
        buffer_set_setting(app, &buffer, BufferSetting_WrapLine, false);
        
        g_jump_buffers[i] = buffer.buffer_id;
    }
    
    g_jump_view = bottom;
    
    set_active_view(app, &view);
    
    // NOTE(jesper): init chord binds
    i32 chord_count = 0;
{
        static Key_Code triggers[] = { 'd', 'w', 'b' };
        g_chord_infos[chord_count++] = { 'd', ARRAY_COUNT(triggers), triggers };
    }
    
{
        static Key_Code triggers[] = { 'y', 'w', 'b' };
        g_chord_infos[chord_count++] = { 'y', ARRAY_COUNT(triggers), triggers };
    }
    
{
        static Key_Code triggers[] = { 'g' };
        g_chord_infos[chord_count++] = { 'g', ARRAY_COUNT(triggers), triggers };
    }
    
{
        static Key_Code triggers[] = { 'd', 'w', 'b', 'j', 'k' };
        g_chord_infos[chord_count++] = { '0', ARRAY_COUNT(triggers), triggers };
        g_chord_infos[chord_count++] = { '1', ARRAY_COUNT(triggers), triggers };
        g_chord_infos[chord_count++] = { '2', ARRAY_COUNT(triggers), triggers };
        g_chord_infos[chord_count++] = { '3', ARRAY_COUNT(triggers), triggers };
        g_chord_infos[chord_count++] = { '4', ARRAY_COUNT(triggers), triggers };
        g_chord_infos[chord_count++] = { '5', ARRAY_COUNT(triggers), triggers };
        g_chord_infos[chord_count++] = { '6', ARRAY_COUNT(triggers), triggers };
        g_chord_infos[chord_count++] = { '7', ARRAY_COUNT(triggers), triggers };
        g_chord_infos[chord_count++] = { '8', ARRAY_COUNT(triggers), triggers };
        g_chord_infos[chord_count++] = { '9', ARRAY_COUNT(triggers), triggers };
    }
    
    return 0;
}

extern "C"
    GET_BINDING_DATA(get_bindings)
{
    Bind_Helper context = begin_bind_helper(data, size);
    Bind_Helper *ctx = &context;
    
    set_all_default_hooks(ctx);
    set_start_hook(ctx, &custom_init);
    set_save_file_hook(ctx, custom_save_file_hook);
    set_open_file_hook(ctx, custom_open_file_hook);
    
    begin_map(ctx, mapid_global);
{
        // MODAL_CMD_FULL(insert, edit, chord, visual)
        // MODAL_CMD_V(edit, visual) MODAL_CMD_FULL(write_char, edit, push_chord_char, visual)
        // MODAL_CMD_I(edit, insert) MODAL_CMD_FULL(insert, edit, push_chord_char, edit)
        // MODAL_CMD(edit) MODAL_CMD_FULL(write_char, edit, push_chord_char, edit)
        
        bind_vanilla_keys(ctx, MODAL_CMD(unused_func));
        bind(ctx, key_esc, MDFR_NONE, set_edit_mode);
        bind(ctx, '\n', MDFR_NONE, MODAL_CMD_I(unused_func, custom_newline));
        bind(ctx, '\t', MDFR_NONE, MODAL_CMD_I(unused_func, write_indent));
        bind(ctx, ' ', MDFR_SHIFT, MODAL_CMD(unused_func));
        bind(ctx, key_back, MDFR_NONE, MODAL_CMD_FULL(backspace_char, unused_func, unused_func, unused_func));
        bind(ctx, key_back, MDFR_SHIFT, MODAL_CMD_FULL(backspace_char, unused_func, unused_func, unused_func));
        
        bind(ctx, key_mouse_left, MDFR_NONE, click_set_cursor);
        bind(ctx, key_mouse_wheel, MDFR_NONE, mouse_wheel_scroll);
        
        bind(ctx, key_left, MDFR_NONE, MODAL_CMD_FULL(move_left, unused_func, unused_func, unused_func));
        bind(ctx, key_right, MDFR_NONE, MODAL_CMD_FULL(move_right, unused_func, unused_func, unused_func));
        bind(ctx, key_up, MDFR_NONE, MODAL_CMD_FULL(move_up, unused_func, unused_func, unused_func));
        bind(ctx, key_down, MDFR_NONE, MODAL_CMD_FULL(move_down, unused_func, unused_func, unused_func));
        
        bind(ctx, 'b', MDFR_NONE, MODAL_CMD(move_word_back));
        bind(ctx, 'd', MDFR_NONE, MODAL_CMD_V(set_chord_mode, visual_cut_range));
        bind(ctx, 'g', MDFR_NONE, MODAL_CMD(set_chord_mode));
        bind(ctx, 'h', MDFR_NONE, MODAL_CMD(move_left));
        bind(ctx, 'i', MDFR_NONE, MODAL_CMD(set_insert_mode));
        bind(ctx, 'j', MDFR_NONE, MODAL_CMD(move_down));
        bind(ctx, 'k', MDFR_NONE, MODAL_CMD(move_up));
        bind(ctx, 'l', MDFR_NONE, MODAL_CMD(move_right));
        bind(ctx, 'n', MDFR_NONE, MODAL_CMD(goto_next_jump_sticky));
        bind(ctx, 'o', MDFR_NONE, MODAL_CMD(fuzzy_find_file));
        bind(ctx, 'p', MDFR_NONE, MODAL_CMD(custom_paste_and_indent));
        bind(ctx, 'u', MDFR_NONE, MODAL_CMD(undo));
        bind(ctx, 'v', MDFR_NONE, MODAL_CMD(set_visual_mode));
        bind(ctx, 'w', MDFR_NONE, MODAL_CMD(move_word));
        bind(ctx, 'x', MDFR_NONE, MODAL_CMD(delete_char));
        bind(ctx, 'y', MDFR_NONE, MODAL_CMD_V(set_chord_mode, visual_copy_range));
        bind(ctx, 's', MDFR_NONE, MODAL_CMD_V(substitute, visual_substitute));
        bind(ctx, 'f', MDFR_NONE, MODAL_CMD_I(unused_func, custom_write_and_auto_tab));
        bind(ctx, 'e', MDFR_NONE, MODAL_CMD_I(unused_func, custom_write_and_auto_tab));
        
        
        bind(ctx, 'p', MDFR_CTRL, MODAL_CMD(custom_paste_next_and_indent));
        bind(ctx, 'r', MDFR_CTRL, MODAL_CMD(redo));
        bind(ctx, 'w', MDFR_CTRL, MODAL_CMD(change_active_panel));
        bind(ctx, 'x', MDFR_CTRL, MODAL_CMD_I(delete_char, delete_char));
        
        bind(ctx, 'A', MDFR_NONE, MODAL_CMD(set_insert_mode_end));
        bind(ctx, 'D', MDFR_NONE, MODAL_CMD(cut_to_end_of_line));
        bind(ctx, 'G', MDFR_NONE, MODAL_CMD(goto_end_of_file));
        bind(ctx, 'I', MDFR_NONE, MODAL_CMD(set_insert_mode_beginning));
        bind(ctx, 'J', MDFR_NONE, MODAL_CMD(combine_with_next_line));
        bind(ctx, 'N', MDFR_NONE, MODAL_CMD(goto_prev_jump_sticky));
        bind(ctx, 'O', MDFR_NONE, MODAL_CMD(open_all_code_recursive));
        bind(ctx, 'V', MDFR_NONE, MODAL_CMD(set_visual_mode_line));
        
        bind(ctx, '0', MDFR_NONE, MODAL_CMD(set_chord_mode));
        bind(ctx, '1', MDFR_NONE, MODAL_CMD(set_chord_mode));
        bind(ctx, '2', MDFR_NONE, MODAL_CMD(set_chord_mode));
        bind(ctx, '3', MDFR_NONE, MODAL_CMD(set_chord_mode));
        bind(ctx, '4', MDFR_NONE, MODAL_CMD(set_chord_mode));
        bind(ctx, '5', MDFR_NONE, MODAL_CMD(set_chord_mode));
        bind(ctx, '6', MDFR_NONE, MODAL_CMD(set_chord_mode));
        bind(ctx, '7', MDFR_NONE, MODAL_CMD(set_chord_mode));
        bind(ctx, '8', MDFR_NONE, MODAL_CMD(set_chord_mode));
        bind(ctx, '9', MDFR_NONE, MODAL_CMD(set_chord_mode));
        
        bind(ctx, key_f1, MDFR_NONE, MODAL_CMD(jump_buffer_1));
        bind(ctx, key_f2, MDFR_NONE, MODAL_CMD(jump_buffer_2));
        
        bind(ctx, '#', MDFR_NONE, MODAL_CMD_I(unused_func, custom_write_and_auto_tab));
        bind(ctx, '$', MDFR_NONE, MODAL_CMD(seek_end_of_line));
        bind(ctx, '%', MDFR_NONE, MODAL_CMD(seek_matching_scope));
        bind(ctx, '/', MDFR_NONE, MODAL_CMD(custom_isearch));
        bind(ctx, ':', MDFR_NONE, MODAL_CMD_I(fuzzy_exec_command, custom_write_and_auto_tab));
        bind(ctx, ';', MDFR_NONE, MODAL_CMD_I(fuzzy_exec_command, custom_write_and_auto_tab));
        bind(ctx, '[', MDFR_NONE, MODAL_CMD(seek_whitespace_up));
        bind(ctx, ']', MDFR_NONE, MODAL_CMD(seek_whitespace_down));
        bind(ctx, '^', MDFR_NONE, MODAL_CMD(custom_seek_beginning_of_line));
        bind(ctx, '{', MDFR_NONE, MODAL_CMD_I(unused_func, custom_write_and_auto_tab));
        bind(ctx, '}', MDFR_NONE, MODAL_CMD_I(unused_func, custom_write_and_auto_tab));
    }
    end_map(ctx);
    
    begin_map(ctx, mapid_file);
{
        inherit_map(ctx, mapid_global);
    }
    end_map(ctx);
    
    begin_map(ctx, default_code_map);
{
        inherit_map(ctx, mapid_file);
    }
    end_map(ctx);
    
    begin_map(ctx, default_lister_ui_map);
{
        bind_vanilla_keys(ctx, lister__write_character);
        bind(ctx, key_animate, MDFR_NONE, fuzzy_list_repaint);
        bind(ctx, key_esc, MDFR_NONE, lister__quit);
        bind(ctx, 'j', MDFR_CTRL, lister__move_down);
        bind(ctx, 'k', MDFR_CTRL, lister__move_up);
        bind(ctx, '\n', MDFR_NONE, lister__activate);
        bind(ctx, '\t', MDFR_NONE, lister__activate);
        bind(ctx, key_back, MDFR_NONE, lister__backspace_text_field);
        bind(ctx, key_up, MDFR_NONE, lister__move_up);
        bind(ctx, key_page_up, MDFR_NONE, lister__move_up);
        bind(ctx, key_down, MDFR_NONE, lister__move_down);
        bind(ctx, key_page_down, MDFR_NONE, lister__move_down);
        bind(ctx, key_mouse_wheel, MDFR_NONE, lister__wheel_scroll);
        bind(ctx, key_mouse_left, MDFR_NONE, lister__mouse_press);
        bind(ctx, key_mouse_left_release, MDFR_NONE, lister__mouse_release);
        bind(ctx, key_mouse_move, MDFR_NONE, lister__repaint);
    }
    end_map(ctx);
    
    i32 result = end_bind_helper(ctx);
    return result;
}

