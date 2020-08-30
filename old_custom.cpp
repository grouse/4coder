#include "4coder_default_include.cpp"
#include <math.h>

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