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
function Lister_Result fixed_run_lister(
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

