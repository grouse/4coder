enum ListerDirtyBuffersChoice {
    CHOICE_NULL = 0,
    CHOICE_CANCEL,
    CHOICE_DISCARD,
    CHOICE_SAVE_ALL,
    CHOICE_OPEN_BUFFER,
    CHOICE_BUFFER_ID_START,
};

typedef void lister_update_filtered_func(Application_Links*, struct Lister*);

// NOTE(jesper): this function is edited from the default run_lister in several ways:
//     1) lister_update_filtered_list function calls are replaced with calls to a callback function
// supplied to the function. This is used for my fuzzy lister. I don't exactly recall what for and 
// there may be better ways to do it
//     2) If the handlers return ListerActivation_ContinueAndRefresh, call the refresh handlers at
// the end of the iteration. This seems to be the only usage of ContinueAndRefresh in the custom layer,
// which makes me wonder what its intention was
//     3) Add lister->out.canceled as a break condition at the start of the loop. Used by my custom_try_exit
// binding which signals that the lister should close from the refresh function, and this is the only way I've
// found how.
function Lister_Result custom_run_lister(
    Application_Links *app, 
    Lister *lister,
    lister_update_filtered_func *update_filtered = lister_update_filtered_list)
{
    lister->filter_restore_point = begin_temp(lister->arena);
    update_filtered(app, lister);
    //lister_update_filtered_list(app, lister);

    View_ID view = get_this_ctx_view(app, Access_Always);
    View_Context ctx = view_current_context(app, view);
    ctx.render_caller = lister_render;
    ctx.hides_buffer = true;
    View_Context_Block ctx_block(app, view, &ctx);

    while (!lister->out.canceled) {
        User_Input in = get_next_input(app, EventPropertyGroup_Any, EventProperty_Escape);
        if (in.abort){
            block_zero_struct(&lister->out);
            lister->out.canceled = true;
            break;
        }

        Lister_Activation_Code result = ListerActivation_Continue;
        b32 handled = true;
        switch (in.event.kind){
        case InputEventKind_TextInsert:
            {
                if (lister->handlers.write_character != 0){
                    result = lister->handlers.write_character(app);
                }
            }break;

        case InputEventKind_KeyStroke:
            {
                switch (in.event.key.code){
                case KeyCode_Return:
                case KeyCode_Tab:
                    {
                        void *user_data = 0;
                        if (0 <= lister->raw_item_index &&
                            lister->raw_item_index < lister->options.count){
                            user_data = lister_get_user_data(lister, lister->raw_item_index);
                        }
                        lister_activate(app, lister, user_data, false);
                        result = ListerActivation_Finished;
                    }break;

                case KeyCode_Backspace:
                    {
                        if (lister->handlers.backspace != 0){
                            lister->handlers.backspace(app);
                        }
                        else if (lister->handlers.key_stroke != 0){
                            result = lister->handlers.key_stroke(app);
                        }
                        else{
                            handled = false;
                        }
                    }break;

                case KeyCode_Up:
                    {
                        if (lister->handlers.navigate != 0){
                            lister->handlers.navigate(app, view, lister, -1);
                        }
                        else if (lister->handlers.key_stroke != 0){
                            result = lister->handlers.key_stroke(app);
                        }
                        else{
                            handled = false;
                        }
                    }break;

                case KeyCode_Down:
                    {
                        if (lister->handlers.navigate != 0){
                            lister->handlers.navigate(app, view, lister, 1);
                        }
                        else if (lister->handlers.key_stroke != 0){
                            result = lister->handlers.key_stroke(app);
                        }
                        else{
                            handled = false;
                        }
                    }break;

                case KeyCode_PageUp:
                    {
                        if (lister->handlers.navigate != 0){
                            lister->handlers.navigate(app, view, lister,
                                                      -lister->visible_count);
                        }
                        else if (lister->handlers.key_stroke != 0){
                            result = lister->handlers.key_stroke(app);
                        }
                        else{
                            handled = false;
                        }
                    }break;

                case KeyCode_PageDown:
                    {
                        if (lister->handlers.navigate != 0){
                            lister->handlers.navigate(app, view, lister,
                                                      lister->visible_count);
                        }
                        else if (lister->handlers.key_stroke != 0){
                            result = lister->handlers.key_stroke(app);
                        }
                        else{
                            handled = false;
                        }
                    }break;

                default:
                    {
                        if (lister->handlers.key_stroke != 0){
                            result = lister->handlers.key_stroke(app);
                        }
                        else{
                            handled = false;
                        }
                    }break;
                }
            }break;

        case InputEventKind_MouseButton:
            {
                switch (in.event.mouse.code){
                case MouseCode_Left:
                    {
                        Vec2_f32 p = V2f32(in.event.mouse.p);
                        void *clicked = lister_user_data_at_p(app, view, lister, p);
                        lister->hot_user_data = clicked;
                    }break;

                default:
                    {
                        handled = false;
                    }break;
                }
            }break;

        case InputEventKind_MouseButtonRelease:
            {
                switch (in.event.mouse.code){
                case MouseCode_Left:
                    {
                        if (lister->hot_user_data != 0){
                            Vec2_f32 p = V2f32(in.event.mouse.p);
                            void *clicked = lister_user_data_at_p(app, view, lister, p);
                            if (lister->hot_user_data == clicked){
                                lister_activate(app, lister, clicked, true);
                                result = ListerActivation_Finished;
                            }
                        }
                        lister->hot_user_data = 0;
                    }break;

                default:
                    {
                        handled = false;
                    }break;
                }
            }break;

        case InputEventKind_MouseWheel:
            {
                Mouse_State mouse = get_mouse_state(app);
                lister->scroll.target.y += mouse.wheel;
                update_filtered(app, lister);
                //lister_update_filtered_list(app, lister);
            }break;

        case InputEventKind_MouseMove:
            {
                update_filtered(app, lister);
                //lister_update_filtered_list(app, lister);
            }break;

        case InputEventKind_Core:
            {
                switch (in.event.core.code){
                case CoreCode_Animate:
                    {
                        update_filtered(app, lister);
                        //lister_update_filtered_list(app, lister);
                    }break;

                default:
                    {
                        handled = false;
                    }break;
                }
            }break;

        default:
            {
                handled = false;
            }break;
        }

        if (result == ListerActivation_Finished){
            break;
        } else if (result == ListerActivation_ContinueAndRefresh) {
            lister_call_refresh_handler(app, lister);
        }

        if (!handled){
            Mapping *mapping = lister->mapping;
            Command_Map *map = lister->map;

            Fallback_Dispatch_Result disp_result =
                fallback_command_dispatch(app, mapping, map, &in);
            if (disp_result.code == FallbackDispatch_DelayedUICall){
                call_after_ctx_shutdown(app, view, disp_result.func);
                break;
            }
            if (disp_result.code == FallbackDispatch_Unhandled){
                leave_current_input_unhandled(app);
            }
            else{
                lister_call_refresh_handler(app, lister);
            }
        }
    }

    return(lister->out);
}


void custom_generate_all_buffers_list__output_buffer(
    Application_Links *app, Lister *lister,
    Buffer_ID buffer)
{
    // NOTE(jesper): this is identical to generate_all_buffers_list__output_buffer except it uses the full
    // filename instead of the unique buffer name
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

void custom_generate_all_buffer_list(Application_Links *app, Lister *lister)
{
    // NOTE(jesper): this is identical to generate_all_buffer_list except it calls custom_generate_all_buffers_list__output_buffer
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

    // NOTE(jesper): this is done by the default one, but it doesn't really seem like we need to?
    // does this mean the default lister is calling it twice with all the performance implications
    // of that?
    //fuzzy_lister_update_filtered(app, lister);

    return result;
}

static Buffer_ID fuzzy_file_lister(Application_Links *app, String_Const_u8 query)
{
    Scratch_Block scratch(app);

    Buffer_ID buffer = 0;

    Lister_Handlers handlers = lister_get_default_handlers();
    handlers.refresh = custom_generate_all_buffer_list;
    handlers.write_character = fuzzy_lister_write_string;

    Lister_Result result = {};
    if (handlers.refresh) {
        Lister_Block lister(app, scratch);
        lister_set_query(lister, string_u8_litexpr(""));
        lister_set_key(lister, query);
        lister_set_text_field(lister, query);

        lister_set_handlers(lister, &handlers);

        handlers.refresh(app, lister);
        result = custom_run_lister(app, lister, fuzzy_lister_update_filtered);
    } else {
        result.canceled = true;
    }

    if (!result.canceled){
        buffer = (Buffer_ID)(PtrAsInt(result.user_data));
    }

    return buffer;
}

ListerDirtyBuffersChoice lister_handle_dirty_buffers(Application_Links *app)
{
    Scratch_Block scratch(app);
    Lister_Block lister(app, scratch);

    lister_set_query(lister, "There are buffers with unsaved changes");

    Lister_Handlers handlers = lister_get_default_handlers();
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
            ListerDirtyBuffersChoice choice = (ListerDirtyBuffersChoice)(i64)lister->highlighted_node->user_data;

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
    Lister_Result result = custom_run_lister(app, lister);
    ListerDirtyBuffersChoice choice = (ListerDirtyBuffersChoice)(i64)result.user_data;

    if (choice >= CHOICE_BUFFER_ID_START) {
        Buffer_ID buffer = (Buffer_ID)(choice - CHOICE_BUFFER_ID_START);
        view_set_buffer(app, get_active_view(app, Access_Always), buffer, 0);
        return CHOICE_OPEN_BUFFER;
    }

    switch (choice) {
    case CHOICE_NULL:
    case CHOICE_CANCEL:
    case CHOICE_DISCARD:
    case CHOICE_SAVE_ALL:
        return choice;
    case CHOICE_OPEN_BUFFER:
        break;
    }

    return CHOICE_DISCARD;
}
