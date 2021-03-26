
enum {
    PROJECT_FILE_OPEN_RECURSIVE = 1 << 0,
    PROJECT_FILE_LOAD_BUFFER = 1 << 1,
};
typedef u32 ProjectFileFlags;

struct ProjectFiles {
    String_Const_u8 *files;
    i32 count;
    i32 capacity;
};

ProjectFiles g_project_files{};

static void custom_prj_open_files_pattern_filter(
    Application_Links *app, 
    String8 path, 
    Prj_Pattern_List whitelist, 
    Prj_Pattern_List blacklist, 
    ProjectFileFlags flags)
{
    ProfileScope(app, "ope_files_pattern_filter");

    Scratch_Block scratch(app);

    ProfileScopeNamed(app, "get file list", profile_get_file_list);
    File_List list = system_get_file_list(scratch, path);
    ProfileCloseNow(profile_get_file_list);

    File_Info **info = list.infos;
    for (u32 i = 0; i < list.count; ++i, ++info) {
        String8 file_name = (**info).file_name;
        if (HasFlag((**info).attributes.flags, FileAttribute_IsDirectory)){
            if ((flags & PROJECT_FILE_OPEN_RECURSIVE) == 0) continue;
            if (prj_match_in_pattern_list(file_name, blacklist)) continue;

            String8 new_path = push_u8_stringf(scratch, "%.*s%.*s" FILE_SEP, string_expand(path), string_expand(file_name));
            custom_prj_open_files_pattern_filter(app, new_path, whitelist, blacklist, flags);
        } else {
            if (!prj_match_in_pattern_list(file_name, whitelist)) continue;
            if (prj_match_in_pattern_list(file_name, blacklist)) continue;

            String8 full_path = push_u8_stringf(global_heap.arena, "%.*s%.*s", string_expand(path), string_expand(file_name));
            if (flags & PROJECT_FILE_LOAD_BUFFER) create_buffer(app, full_path, BufferCreate_MustAttachToFile);
            if (g_project_files.count == g_project_files.capacity) {
                g_project_files.capacity = Max(10, g_project_files.capacity * 3 / 2);
                g_project_files.files = heap_realloc_arr(
                    &global_heap, 
                    String_Const_u8, 
                    g_project_files.files,
                    g_project_files.count, 
                    g_project_files.capacity);
            }
            
            g_project_files.files[g_project_files.count++] = full_path;
        }
    }
}

function void prj_pattern_list_from_var(Prj_Pattern_List *result, Arena *arena, Variable_Handle var){
    for (Vars_Children(child_var, var)) {
        Variable_Handle child_whitelist = def_get_config_var(child_var.ptr->string);
        if (!vars_is_nil(child_whitelist)) {
            return prj_pattern_list_from_var(result, arena, child_whitelist);
        }
        
        Prj_Pattern_Node *node = push_array(arena, Prj_Pattern_Node, 1);
        sll_queue_push(result->first, result->last, node);
        result->count += 1;

        String8 str = vars_string_from_var(arena, child_var);
        node->pattern.absolutes = string_split_wildcards(arena, str);
    }
}

static void load_project_file(Application_Links *app, String_Const_u8 path)
{
    ProfileScope(app, "custom load project");
    Scratch_Block scratch(app);

    File_Name_Data project_file = dump_file(scratch, path);
    String8 project_root = string_remove_last_folder(project_file.file_name);

    if (project_file.data.str == 0){
        print_message(app, string_u8_litexpr("Did not find project.4coder.\n"));
    }

    // NOTE(allen): Parse config data out of project file
    Config *config_parse = 0;
    Variable_Handle prj_var = vars_get_nil();
    Variable_Handle root_var = vars_get_root();
    if (project_file.data.str != 0){
        Token_Array array = token_array_from_text(app, scratch, project_file.data);
        if (array.tokens != 0){
            config_parse = def_config_parse(app, scratch, project_file.file_name, project_file.data, array);
            if (config_parse != 0){
                i32 version = 0;
                if (config_parse->version != 0){
                    version = *config_parse->version;
                }

                switch (version){
                case 0:
                case 1:
                    {
                        prj_var = prj_v1_to_v2(app, project_root, config_parse);
                        root_var = prj_var;
                    }break;
                default:
                    {
                        prj_var = def_fill_var_from_config(app, vars_get_root(), vars_save_string_lit("prj_config"), config_parse);
                    }break;
                }

            }
        }
    }

    // NOTE(allen): Print Project
    if (!vars_is_nil(prj_var)){
        vars_print(app, prj_var);
        print_message(app, string_u8_litexpr("\n"));
    }

    // NOTE(allen): Print Errors
    if (config_parse != 0){
        String8 error_text = config_stringize_errors(app, scratch, config_parse);
        if (error_text.size > 0){
            print_message(app, string_u8_litexpr("Project errors:\n"));
            print_message(app, error_text);
            print_message(app, string_u8_litexpr("\n"));
        }
    }

    // NOTE(allen): Open All Project Files
    Variable_Handle load_paths_var = def_get_config_var(vars_save_string_lit("load_paths"));
    Variable_Handle load_paths_os_var = vars_read_key(load_paths_var, vars_save_string_lit(OS_NAME));


    Variable_Handle whitelist_var = def_get_config_var(vars_save_string_lit("patterns"));
    Variable_Handle blacklist_var = def_get_config_var(vars_save_string_lit("blacklist_patterns"));

    Prj_Pattern_List whitelist{};
    prj_pattern_list_from_var(&whitelist, scratch, whitelist_var);
    
    Prj_Pattern_List blacklist = prj_pattern_list_from_var(scratch, blacklist_var);
    
    ProjectFileFlags base_flags = 0;
    if (def_get_config_b32(vars_save_string_lit("automatically_load_project_buffers"), false))
        base_flags |= PROJECT_FILE_LOAD_BUFFER;

    g_project_files.count = 0;

    String_ID path_id = vars_save_string_lit("path");
    String_ID recursive_id = vars_save_string_lit("recursive");
    String_ID relative_id = vars_save_string_lit("relative");

    ProfileScope(app, "iterate load_paths");
    for (Variable_Handle load_path_var = vars_first_child(load_paths_os_var);
         !vars_is_nil(load_path_var);
         load_path_var = vars_next_sibling(load_path_var))
    {
        Variable_Handle path_var = vars_read_key(load_path_var, path_id);
        Variable_Handle recursive_var = vars_read_key(load_path_var, recursive_id);
        Variable_Handle relative_var = vars_read_key(load_path_var, relative_id);

        String8 path = vars_string_from_var(scratch, path_var);
        b32 recursive = vars_b32_from_var(recursive_var);
        b32 relative = vars_b32_from_var(relative_var);

        ProjectFileFlags flags = base_flags;
        if (recursive) flags |= PROJECT_FILE_OPEN_RECURSIVE;

        String8 file_dir = path;
        if (relative){
            String8 prj_dir = prj_path_from_project(scratch, prj_var);

            String8List file_dir_list = {};
            string_list_push(scratch, &file_dir_list, prj_dir);
            string_list_push_overlap(scratch, &file_dir_list, FILE_SEP_C, path);
            string_list_push_overlap(scratch, &file_dir_list, FILE_SEP_C, SCu8());
            file_dir = string_list_flatten(scratch, file_dir_list, StringFill_NullTerminate);
        }

        String8 directory = file_dir;
        if (!character_is_slash(string_get_character(directory, directory.size - 1))){
            directory = push_u8_stringf(scratch, "%.*s" FILE_SEP, string_expand(file_dir));
        }

        custom_prj_open_files_pattern_filter(app, directory, whitelist, blacklist, flags);
    }

    // NOTE(allen): Set Window Title
    Variable_Handle proj_name_var = vars_read_key(prj_var, vars_save_string_lit("project_name"));
    String_ID proj_name_id = vars_key_id_from_var(prj_var);
    if (proj_name_id != 0){
        String8 proj_name = vars_read_string(scratch, proj_name_id);
        String8 title = push_u8_stringf(scratch, "4coder project: %.*s", string_expand(proj_name));
        set_window_title(app, title);
    }
}

CUSTOM_COMMAND_SIG(custom_load_project)
CUSTOM_DOC("Looks for a project.4coder file in the current directory and tries to load it.  Looks in parent directories until a project file is found or there are no more parents.")
{
    save_all_dirty_buffers(app);
    Scratch_Block scratch(app);

    String8 project_path = push_hot_directory(app, scratch);
    String_Const_u8 full_path = push_file_search_up_path(app, scratch, project_path, string_u8_litexpr("project.4coder"));
    load_project_file(app, full_path);
}

