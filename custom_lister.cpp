enum ListerDirtyBuffersChoice {
    CHOICE_NULL = 0,
    CHOICE_CANCEL,
    CHOICE_DISCARD,
    CHOICE_SAVE_ALL,
    CHOICE_OPEN_BUFFER,
    CHOICE_BUFFER_ID_START,
};

struct FuzzyListerNode {
    String_Const_u8 lower_string;
    fzy_score_t *match_bonus;
    void *user_data;
};

typedef void lister_update_filtered_func(Application_Links*, struct Lister*);

// NOTE(jesper): this procedure is identical to lister_render except for these changes:
//	1) Early break the lister node loop when it goes out of bounds. Essential for dealing with
// very large listers. E.g. listing every file in the Linux kernel source tree.
function void custom_lister_render(
    Application_Links *app, 
    Frame_Info frame_info, 
    View_ID view)
{
    ProfileScope(app, "custom_lister_render");
    Scratch_Block scratch(app);

    Lister *lister = view_get_lister(app, view);
    if (lister == 0){
        return;
    }

    Rect_f32 region = draw_background_and_margin(app, view);
    Rect_f32 prev_clip = draw_set_clip(app, region);

    f32 lister_padding = (f32)def_get_config_u64(app, vars_save_string_lit("lister_padding"));

    Face_ID face_id = get_face_id(app, 0);
    Face_Metrics metrics = get_face_metrics(app, face_id);

    f32 line_height = metrics.line_height;
    f32 block_height = lister_get_block_height(line_height);
    f32 text_field_height = lister_get_text_field_height(line_height);

    // NOTE(allen): file bar
    // TODO(allen): What's going on with 'showing_file_bar'? I found it like this.
    b64 showing_file_bar = false;
    b32 hide_file_bar_in_ui = def_get_config_b32(vars_save_string_lit("hide_file_bar_in_ui"));
    if (view_get_setting(app, view, ViewSetting_ShowFileBar, &showing_file_bar) &&
        showing_file_bar && !hide_file_bar_in_ui){
        Rect_f32_Pair pair = layout_file_bar_on_top(region, line_height);
        Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
        draw_file_bar(app, view, buffer, face_id, pair.min);
        region = pair.max;
    }

    Mouse_State mouse = get_mouse_state(app);
    Vec2_f32 m_p = V2f32(mouse.p);

    lister->visible_count = (i32)((rect_height(region)/block_height)) - 3;
    lister->visible_count = clamp_bot(1, lister->visible_count);

    Rect_f32 text_field_rect = {};
    Rect_f32 list_rect = {};
    {
        Rect_f32_Pair pair = lister_get_top_level_layout(region, text_field_height);
        text_field_rect = pair.min;
        list_rect = pair.max;
    }

    {
        Vec2_f32 p = V2f32(text_field_rect.x0 + 3.f, text_field_rect.y0);
        Fancy_Line text_field = {};
        push_fancy_string(scratch, &text_field, fcolor_id(defcolor_pop1),
                          lister->query.string);
        push_fancy_stringf(scratch, &text_field, " ");
        p = draw_fancy_line(app, face_id, fcolor_zero(), &text_field, p);

        // TODO(allen): This is a bit of a hack. Maybe an upgrade to fancy to focus
        // more on being good at this and less on overriding everything 10 ways to sunday
        // would be good.
        block_zero_struct(&text_field);
        push_fancy_string(scratch, &text_field, fcolor_id(defcolor_text_default),
                          lister->text_field.string);
        f32 width = get_fancy_line_width(app, face_id, &text_field);
        f32 cap_width = text_field_rect.x1 - p.x - 6.f;
        if (cap_width < width){
            Rect_f32 prect = draw_set_clip(app, Rf32(p.x, text_field_rect.y0, p.x + cap_width, text_field_rect.y1));
            p.x += cap_width - width;
            draw_fancy_line(app, face_id, fcolor_zero(), &text_field, p);
            draw_set_clip(app, prect);
        }
        else{
            draw_fancy_line(app, face_id, fcolor_zero(), &text_field, p);
        }
    }


    Range_f32 x = rect_range_x(list_rect);
    draw_set_clip(app, list_rect);

    // NOTE(allen): auto scroll to the item if the flag is set.
    f32 scroll_y = lister->scroll.position.y;

    if (lister->set_vertical_focus_to_item){
        lister->set_vertical_focus_to_item = false;
        Range_f32 item_y = If32_size(lister->item_index*block_height, block_height);
        f32 view_h = rect_height(list_rect);
        Range_f32 view_y = If32_size(scroll_y, view_h);
        if (view_y.min > item_y.min || item_y.max > view_y.max){
            f32 item_center = (item_y.min + item_y.max)*0.5f;
            f32 view_center = (view_y.min + view_y.max)*0.5f;
            f32 margin = view_h*.3f;
            margin = clamp_top(margin, block_height*3.f);
            if (item_center < view_center){
                lister->scroll.target.y = item_y.min - margin;
            }
            else{
                f32 target_bot = item_y.max + margin;
                lister->scroll.target.y = target_bot - view_h;
            }
        }
    }

    // NOTE(allen): clamp scroll target and position; smooth scroll rule
    i32 count = lister->filtered.count;
    Range_f32 scroll_range = If32(0.f, clamp_bot(0.f, count*block_height - block_height));
    lister->scroll.target.y = clamp_range(scroll_range, lister->scroll.target.y);
    lister->scroll.target.x = 0.f;

    Vec2_f32_Delta_Result delta = delta_apply(app, view,
                                              frame_info.animation_dt, lister->scroll);
    lister->scroll.position = delta.p;
    if (delta.still_animating){
        animate_in_n_milliseconds(app, 0);
    }

    lister->scroll.position.y = clamp_range(scroll_range, lister->scroll.position.y);
    lister->scroll.position.x = 0.f;

    scroll_y = lister->scroll.position.y;
    f32 y_pos = list_rect.y0 - scroll_y;

    i32 first_index = (i32)(scroll_y/block_height);
    
    y_pos += first_index*block_height;

    for (i32 i = first_index; i < count; i += 1){
        Lister_Node *node = lister->filtered.node_ptrs[i];

        Range_f32 y = If32(y_pos, y_pos + block_height);
        if (y.min > region.y1) break;
        
        y_pos = y.max;
        Rect_f32 item_rect = Rf32(x, y);
        Rect_f32 item_inner = rect_inner(item_rect, lister_padding);


        b32 hovered = rect_contains_point(item_rect, m_p);
        UI_Highlight_Level highlight = UIHighlight_None;
        if (node == lister->highlighted_node){
            highlight = UIHighlight_Active;
        }
        else if (node->user_data == lister->hot_user_data){
            if (hovered){
                highlight = UIHighlight_Active;
            }
            else{
                highlight = UIHighlight_Hover;
            }
        }
        else if (hovered){
            highlight = UIHighlight_Hover;
        }

        u64 lister_roundness_100 = def_get_config_u64(app, vars_save_string_lit("lister_roundness"));
        f32 roundness = block_height*lister_roundness_100*0.01f;
        draw_rectangle_fcolor(app, item_rect, roundness, get_item_margin_color(highlight));
        draw_rectangle_fcolor(app, item_inner, roundness, get_item_margin_color(highlight, 1));

        Fancy_Line line = {};
        push_fancy_string(scratch, &line, fcolor_id(defcolor_text_default), node->string);
        push_fancy_stringf(scratch, &line, " ");
        push_fancy_string(scratch, &line, fcolor_id(defcolor_pop2), node->status);

        Vec2_f32 p = item_inner.p0 + V2f32(lister_padding, (block_height - line_height)*0.5f);
        draw_fancy_line(app, face_id, fcolor_zero(), &line, p);
    }

    draw_set_clip(app, prev_clip);
}


// NOTE(jesper): this function is edited from the default run_lister in several ways:
//     1) lister_update_filtered_list function calls are entirely removed except for the first one, which
// is replaced with a call to the supplied function pointer. The removals are because they are entirely 
// unnecessary (and substantial in some cases) overhead, and the function pointer is to support custom
// filter functions, like my fuzzy one.
//         1b) Ideally, this would be replaced with a handler.update_filtered function pointer
// 
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

    View_ID view = get_this_ctx_view(app, Access_Always);
    View_Context ctx = view_current_context(app, view);
    ctx.render_caller = custom_lister_render;
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
            if (lister->handlers.write_character != 0){
                result = lister->handlers.write_character(app);
            }
            break;
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
                        if (lister->handlers.backspace != 0) {
                            lister->handlers.backspace(app);
                        } else if (lister->handlers.key_stroke != 0) {
                            result = lister->handlers.key_stroke(app);
                        } else {
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
            }break;

        case InputEventKind_MouseMove: break; 
        case InputEventKind_Core:
            {
                switch (in.event.core.code){
                case CoreCode_Animate:
                    {
                    }break;

                default:
                    {
                        handled = false;
                    }break;
                }
            }break;

        default:
            handled = false;
            break;
        }

        if (result == ListerActivation_Finished){
            break;
        } else if (result == ListerActivation_ContinueAndRefresh) {
            lister_call_refresh_handler(app, lister);
        }

        if (!handled) {
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
            } else{
                lister_call_refresh_handler(app, lister);
            }
        }
    }

    return(lister->out);
}

void fuzzy_lister_add_node(
    Application_Links *app, 
    Lister *lister,
    String_Const_u8 string,
    String_Const_u8 status,
    void *user_data)
{
    FuzzyListerNode *data = push_array(lister->arena, FuzzyListerNode, 1);
    data->user_data = user_data;
    data->lower_string = string_const_u8_push(lister->arena, string.size);
    data->match_bonus = push_array(lister->arena, fzy_score_t, string.size);

    char prev = '/';
    for (i32 i = 0; i < string.size; i++) {
        char c = string.str[i];
        data->match_bonus[i] = fzy_compute_bonus(prev, c);
        
        char lower_c = character_to_lower(c);
        lower_c = lower_c == '\\' ? '/' : lower_c;
        data->lower_string.str[i] = lower_c;
        
        prev = c;
    }

    lister_add_item(lister, string, status, data, 0);
}

void fuzzy_lister_add_buffer(
    Application_Links *app, 
    Lister *lister,
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
    
    fuzzy_lister_add_node(app, lister, buffer_name, status, IntAsPtr(buffer));
}

void fuzzy_lister_generate_buffers(
    Application_Links *app, 
    Lister *lister)
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
            fuzzy_lister_add_buffer(app, lister, buffer);
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
            fuzzy_lister_add_buffer(app, lister, buffer);
        }
skip2:;
    }

    // Buffers That Are Open in Views
    for (i32 i = 0; i < viewed_buffer_count; i += 1){
        fuzzy_lister_add_buffer(app, lister, viewed_buffers[i]);
    }
}

void fuzzy_lister_generate_project_files(Application_Links *app, Lister *lister)
{
    ProfileScope(app, "lister_project_files_list");
    lister_begin_new_item_set(app, lister);
    
    Scratch_Block scratch(app, lister->arena);
    String_Const_u8 hot_dir = push_hot_directory(app, scratch);

    for (i32 i = 0; i < g_project_files.count; i++) {
        String_Const_u8 string = shorten_path(g_project_files.files[i], hot_dir);
        String_Const_u8 status = String_Const_u8{};
        fuzzy_lister_add_node(app, lister, string, status, IntAsPtr(i));
    }
}

static void fuzzy_lister_update_filtered(
    Application_Links *app,
    Lister *lister)
{
    ProfileScope(app, "fuzzy_lister: update filtered");

    Arena *arena = lister->arena;
    Scratch_Block scratch(app, arena);

    String_Const_u8 needle = lister->key_string.string;;
    String_Const_u8 lower_needle{};
    
    i32 node_count = lister->options.count;

    Lister_Node **filtered = push_array(scratch, Lister_Node*, node_count);
    fzy_score_t *scores = nullptr;

    i32 filtered_count = 0;
    if (needle.size == 0) {
        for (Lister_Node *node = lister->options.first;
             node != nullptr;
             node = node->next)
        {
            filtered[filtered_count++] = node;
        }

        goto finalize_list;
    }

    scores = push_array(scratch, fzy_score_t, node_count);
    
    {
        ProfileScope(app, "fuzzy_lister: lower needle");
        lower_needle = string_const_u8_push(scratch, needle.size);
        for (i32 i = 0; i < needle.size; i++) {
            char lower_c = character_to_lower(needle.str[i]);
            lower_needle.str[i] = lower_c == '\\' ? '/' : lower_c;
        }
    }

    for (i32 ni = 0; ni < lister->filtered.count; ni++) {
        ProfileScope(app, "fuzzy_lister: compute scores");

        Scratch_Block scratch_inner(app);

        Lister_Node *node = lister->filtered.node_ptrs[ni];
        FuzzyListerNode *fzy_node = (FuzzyListerNode*)node->user_data;
        
        fzy_score_t match_score = 0;

        String_Const_u8 haystack = node->string;
        if (haystack.size <= 0) continue;

        fzy_score_t *D = push_array(scratch_inner, fzy_score_t, haystack.size * needle.size);
        fzy_score_t *M = push_array(scratch_inner, fzy_score_t, haystack.size * needle.size);

        {
            ProfileScope(app, "fuzzy_lister: zero matrices");
            memset(D, 0, sizeof *D * haystack.size * needle.size);
            memset(M, 0, sizeof *M * haystack.size * needle.size);
        }

        for (i32 i = 0; i < needle.size; i++) {
            fzy_score_t prev_score = FZY_SCORE_MIN;
            fzy_score_t gap_score = i == needle.size - 1 ? FZY_SCORE_GAP_TRAILING : FZY_SCORE_GAP_INNER;

            char lower_n = lower_needle.str[i];
            for (i32 j = 0; j < haystack.size; j++) {
                char lower_h = fzy_node->lower_string.str[j];

                if (lower_n == lower_h) {
                    fzy_score_t score = FZY_SCORE_MIN;
                    if (!i) {
                        score = (j * FZY_SCORE_GAP_LEADING) + fzy_node->match_bonus[j];
                    } else if (j) {
                        fzy_score_t d_val = D[(i-1) * haystack.size + j-1];
                        fzy_score_t m_val = M[(i-1) * haystack.size + j-1];

                        score = Max(m_val + fzy_node->match_bonus[j], d_val + FZY_SCORE_MATCH_CONSECUTIVE);
                    }

                    D[i * haystack.size + j] = score;
                    M[i * haystack.size + j] = prev_score = Max(score, prev_score + gap_score);
                } else {
                    D[i * haystack.size + j] = FZY_SCORE_MIN;
                    M[i * haystack.size + j] = prev_score = prev_score + gap_score;
                }
            }

            if (prev_score == FZY_SCORE_MIN) goto next_node;
        }

        match_score = M[(needle.size-1) * haystack.size + haystack.size - 1];
        if (match_score != FZY_SCORE_MIN) {
            filtered[filtered_count] = node;
            scores[filtered_count] = match_score;
            filtered_count++;
        }
next_node:;
    }

    {
        ProfileScope(app, "fuzzy_lister: quicksort");
        quicksort(scores, filtered, 0, filtered_count-1);
    }

finalize_list:
    {
        ProfileScope(app, "fuzzy_lister: finalize nodes");
        end_temp(lister->filter_restore_point);

        Lister_Node **node_ptrs = push_array(arena, Lister_Node*, filtered_count);
        lister->filtered.node_ptrs = node_ptrs;
        lister->filtered.count = filtered_count;

        for (i32 i = 0; i < filtered_count; i++) {
            lister->filtered.node_ptrs[i] = filtered[i];
        }

        lister_update_selection_values(lister);
    }
}

static Lister_Activation_Code fuzzy_lister_write_string(
    Application_Links *app)
{
    ProfileScope(app, "fuzzy_lister: write string");
    Lister_Activation_Code result = ListerActivation_Continue;

    View_ID view = get_active_view(app, Access_Always);
    Lister *lister = view_get_lister(app, view);
    if (!lister) return result;

    User_Input in;
    {
        ProfileScope(app, "fuzzy_lister: wait for input");
        in = get_current_input(app);
    }
    
    String_Const_u8 string;
    {
        ProfileScope(app, "fuzzy_lister: input to writeable");
        string = to_writable(&in);
        if (!string.str || string.size <= 0) return result;
    }

    {
        ProfileScope(app, "fuzzy_lister: append input string");
        lister_append_text_field(lister, string);
        lister_append_key(lister, string);
        lister->item_index = 0;
    }
    
    lister_zero_scroll(lister);
    
    fuzzy_lister_update_filtered(app, lister);
    return result;
}

static void fuzzy_lister_backspace(
    Application_Links *app)
{
    ProfileScope(app, "fuzzy_lister_backspace");

    View_ID view = get_active_view(app, Access_Always);
    Lister *lister = view_get_lister(app, view);
    if (!lister) return;
    
    lister->text_field.string = backspace_utf8(lister->text_field.string);
    lister->key_string.string = backspace_utf8(lister->key_string.string);
    lister->item_index = 0;
    lister_zero_scroll(lister);
    
    // TODO(jesper): this is _super_ leaky. I need to fix this!
    lister->filtered.node_ptrs = push_array(lister->arena, Lister_Node*, lister->options.count);
    lister->filtered.count = 0;
    for (Lister_Node *node = lister->options.first;
         node != nullptr;
         node = node->next)
    {
        lister->filtered.node_ptrs[lister->filtered.count] = node;
        lister->filtered.count++;
    }

    fuzzy_lister_update_filtered(app, lister);
}

Lister_Handlers fuzzy_lister_handlers(Lister_Regenerate_List_Function_Type *refresh_handler)
{
    Lister_Handlers handlers = lister_get_default_handlers();
    if (refresh_handler) handlers.refresh = refresh_handler;
    handlers.write_character = fuzzy_lister_write_string;
    handlers.backspace = fuzzy_lister_backspace;
    return handlers;
}

Lister_Result fuzzy_lister(
    Application_Links *app, 
    Lister_Handlers *handlers, 
    String_Const_u8 query, 
    String_Const_u8 text)
{
    if (!handlers->refresh) {
        Lister_Result result{};
        result.canceled = true;
        return result;
    }
    
    Scratch_Block scratch(app);
    
    Lister_Block lister(app, scratch);
    lister_set_query(lister, query);
    lister_set_key(lister, text);
    lister_set_text_field(lister, text);
    lister_set_handlers(lister, handlers);
    
    handlers->refresh(app, lister);
    Lister_Result result = custom_run_lister(app, lister, fuzzy_lister_update_filtered);
    if (result.user_data) {
        FuzzyListerNode *fzy_node = (FuzzyListerNode*)result.user_data;
        result.user_data = fzy_node->user_data;
    }
    return result;
}

static Buffer_ID fuzzy_file_lister(Application_Links *app, String_Const_u8 start_text)
{
    Scratch_Block scratch(app);

    Lister_Handlers handlers = fuzzy_lister_handlers(fuzzy_lister_generate_project_files);
    Lister_Result result = fuzzy_lister(app, &handlers, string_u8_litexpr("file:"), start_text);

    Buffer_ID buffer = 0;
    if (!result.canceled) {
        String_Const_u8 file = g_project_files.files[PtrAsInt(result.user_data)];
        buffer = create_buffer(app, file, BufferCreate_NeverNew|BufferCreate_MustAttachToFile);
    }

    return buffer;
}

static Buffer_ID fuzzy_buffer_lister(Application_Links *app, String_Const_u8 start_text)
{
    Scratch_Block scratch(app);
    Lister_Handlers handlers = fuzzy_lister_handlers(fuzzy_lister_generate_buffers);
    Lister_Result result = fuzzy_lister(app, &handlers, string_u8_litexpr("buffer:"), start_text);

    Buffer_ID buffer = 0;
    if (!result.canceled) buffer = (Buffer_ID)PtrAsInt(result.user_data);

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

function Lister_Activation_Code custom_lister__write_character__file_path(
    Application_Links *app)
{
    Lister_Activation_Code result = ListerActivation_Continue;
    View_ID view = get_this_ctx_view(app, Access_Always);
    Lister *lister = view_get_lister(app, view);
    
    if (lister != 0) {
        User_Input in = get_current_input(app);
#if 0
        // TODO(jesper): this doesn't actually do anything because I can't override tab/return in the default lister....
        if (in.event.kind == InputEventKind_KeyStroke) {
            if (in.event.key.code == KeyCode_Return)  {
                lister->out.text_field = lister->text_field.string;
                return ListerActivation_Finished;
            } else if (in.event.key.code == KeyCode_Tab && 
                       lister->raw_item_index >= 0 &&
                       lister->raw_item_index < lister->options.count) 
            {
                String_Const_u8 selected = SCu8((u8*)lister_get_user_data(lister, lister->raw_item_index));
                if (selected.size > 0 && character_is_slash(selected.str[selected.size-1])) {
                    set_hot_directory(app, selected);
                    lister->handlers.refresh(app, lister);
                }
            }
        } 
#endif

        String_Const_u8 string = to_writable(&in);
        if (string.str != 0 && string.size > 0) {
            lister_append_text_field(lister, string);
            
            if (character_is_slash(string.str[0])) {
                set_hot_directory(app, lister->text_field.string);
                lister->handlers.refresh(app, lister);
            }
            
            String_Const_u8 front_name = string_front_of_path(lister->text_field.string);
            lister_set_key(lister, front_name);
            
            lister->item_index = 0;
            lister_zero_scroll(lister);
            lister_update_filtered_list(app, lister);
        }
    }
    return(result);
}

function File_Name_Result query_file_path(
    Application_Links *app, 
    Arena *arena, 
    String_Const_u8 query, 
    View_ID view)
{
    Scratch_Block scratch(app);
    
    String_Const_u8 hot_directory = push_hot_directory(app, scratch);
    
    Lister_Handlers handlers = lister_get_default_handlers();
    handlers.refresh = generate_hot_directory_file_list;
    handlers.write_character = custom_lister__write_character__file_path;
    handlers.backspace = lister__backspace_text_field__file_path;

    Lister_Result l_result = run_lister_with_refresh_handler(app, arena, query, handlers);
    set_hot_directory(app, hot_directory);

    File_Name_Result result = {};
    result.canceled = l_result.canceled;
    
    if (!l_result.canceled) {
        result.clicked = l_result.activated_by_click;
        if (l_result.user_data != 0){
            String_Const_u8 name = SCu8((u8*)l_result.user_data);
            result.file_name_activated = name;
            result.is_folder = character_is_slash(string_get_character(name, name.size - 1));
        }
        result.file_name_in_text_field = string_front_of_path(l_result.text_field);

        String_Const_u8 path = {};
        if (l_result.user_data == 0 && result.file_name_in_text_field.size == 0 && l_result.text_field.size > 0){
            result.file_name_in_text_field = string_front_folder_of_path(l_result.text_field);
            path = string_remove_front_folder_of_path(l_result.text_field);
        }
        else{
            path = string_remove_front_of_path(l_result.text_field);
        }
        if (character_is_slash(string_get_character(path, path.size - 1))){
            path = string_chop(path, 1);
        }
        result.path_in_text_field = path;
    }

    return(result);
}

