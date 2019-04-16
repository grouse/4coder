// NOTE(jesper): these are mostly non-invasive changes to the standard custom
// layer that I'd like to be merged into upstream

#if defined(IS_WINDOWS)

static int32_t custom_standard_build_search(
    Application_Links *app,
    View_Summary *view,
    Buffer_Summary *active_buffer,
    Buffer_Identifier output_buffer_id,
    String *dir,
    String *command,
    bool32 perform_backup,
    bool32 use_path_in_command,
    String filename,
    String command_name)
{
    int32_t result = false;
    
    for(;;){
        int32_t old_size = dir->size;
        append_ss(dir, filename);
        
        if (file_exists(app, dir->str, dir->size)){
            dir->size = old_size;
            
            if (use_path_in_command){
                append(command, '"');
                append(command, *dir);
                append(command, command_name);
                append(command, '"');
            }
            else{
                append_ss(command, command_name);
            }
            
            char space[512];
            String message = make_fixed_width_string(space);
            append_ss(&message, make_lit_string("Building with: "));
            append_ss(&message, *command);
            append_s_char(&message, '\n');
            print_message(app, message.str, message.size);
            
            if (global_config.automatically_save_changes_on_build){
                save_all_dirty_buffers(app);
            }
            
            exec_system_command(
                app, view,
                output_buffer_id,
                dir->str, dir->size,
                command->str, command->size,
                CLI_OverlapWithConflict);
            result = true;
            break;
        }
        dir->size = old_size;
        
        if (directory_cd(app, dir->str, &dir->size, dir->memory_size, literal("..")) == 0){
            if (perform_backup){
                dir->size = directory_get_hot(app, dir->str, dir->memory_size);
                char backup_space[256];
                String backup_command = make_fixed_width_string(backup_space);
                append_ss(&backup_command, make_lit_string("echo could not find "));
                append_ss(&backup_command, filename);
                exec_system_command(
                    app, view,
                    output_buffer_id,
                    dir->str, dir->size,
                    backup_command.str, backup_command.size,
                    CLI_OverlapWithConflict);
            }
            break;
        }
    }
    
    return(result);
}


static int32_t custom_execute_standard_build_search(
    Application_Links *app,
    View_Summary *view,
    Buffer_Summary *active_buffer,
    Buffer_Identifier output_buffer_id,
    String *dir,
    String *command,
    int32_t perform_backup)
{
    int32_t result = custom_standard_build_search(
        app, view,
        active_buffer, output_buffer_id,
        dir,
        command,
        perform_backup,
        true,
        make_lit_string("build.bat"),
        make_lit_string("build"));
    return(result);
}

#elif defined(IS_LINUX) || defined(IS_MAC)

static int32_t
custom_execute_standard_build_search(
    Application_Links *app,
    View_Summary *view,
    Buffer_Summary *active_buffer,
    Buffer_Identifier output_buffer_id,
    String *dir,
    String *command,
    bool32 perform_backup)
{
    char dir_space[512];
    String dir_copy = make_fixed_width_string(dir_space);
    copy(&dir_copy, *dir);
    
    int32_t result = custom_standard_build_search(
        app, view,
        active_buffer, output_buffer_id,
        dir,
        command,
        0,
        1,
        make_lit_string("build.sh"),
        make_lit_string("build.sh"));
    
    if (!result) {
        result = custom_standard_build_search(
            app, view, active_buffer,
            &dir_copy,
            command,
            perform_backup,
            0,
            make_lit_string("Makefile"),
            make_lit_string("make"));
    }
    
    return(result);
}

#else
# error No build search rule for this platform.
#endif

static void custom_execute_standard_build(
    Application_Links *app,
    View_Summary *view,
    Buffer_Summary *active_buffer,
    Buffer_Identifier output_buffer_id)
{
    char dir_space[512];
    String dir = make_fixed_width_string(dir_space);
    
    char command_str_space[512];
    String command = make_fixed_width_string(command_str_space);
    
    int32_t build_dir_type = get_build_directory(app, active_buffer, &dir);
    
    if (build_dir_type == BuildDir_AtFile){
        if (!custom_execute_standard_build_search(app, view, active_buffer, output_buffer_id, &dir, &command, false)){
            dir.size = 0;
            command.size = 0;
            build_dir_type = get_build_directory(app, 0, &dir);
        }
    }
    
    if (build_dir_type == BuildDir_AtHot){
        custom_execute_standard_build_search(app, view, active_buffer, output_buffer_id, &dir, &command, true);
    }
}
