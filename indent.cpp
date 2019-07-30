
String get_token_string(
    Application_Links *app,
    Buffer_Summary *buffer,
    Cpp_Token token,
    Partition *part)
{
    char chunk[32];
    Stream_Chunk stream = {};

    char *str_buffer = push_array(part, char, token.size);

    i32 pos = token.start;
    char *ptr = &str_buffer[0];
    char *end = ptr + token.size;

    if (init_stream_chunk(&stream, app, buffer, pos, chunk, sizeof chunk)) {
        do {
            for (; pos < stream.end && ptr < end; pos++) {
                *ptr++ = stream.data[pos];
            }
        } while (ptr < end && forward_stream_chunk(&stream));

        String str = make_string(str_buffer, token.size);
        return str;
    }

    return {};
}

static i32* custom_get_indentation_marks(
    Application_Links *app,
    Partition *arena,
    Buffer_Summary *buffer,
    Cpp_Token_Array tokens,
    int32_t first_line,
    int32_t one_past_last_line,
    bool32 exact_align,
    int32_t tab_width)
{
    i32 indent_marks_count = one_past_last_line - first_line;
    i32 *indent_marks = push_array(arena, i32, indent_marks_count);

    for (i32 i = 0; i < indent_marks_count; i++) {
        indent_marks[i] = 0;
    }

    indent_marks -= first_line;

    Indent_Anchor_Position anchor = find_anchor_token(app, buffer, tokens, first_line, tab_width);
    Cpp_Token *iter = anchor.token;
    Cpp_Token *iter_end = tokens.tokens + tokens.count;

    i32 line_number = buffer_get_line_number(app, buffer, iter->start);
    line_number = min(line_number, first_line);

    // NOTE(jesper): indentation state
    i32 cur_indent = anchor.indentation;
    i32 next_indent = anchor.indentation;

    if (iter == tokens.tokens) {
        cur_indent = 0;
        next_indent = 0;
    }

    if (iter->type == CPP_TOKEN_BRACE_OPEN || iter->type == CPP_TOKEN_BRACKET_OPEN) {
        iter--;
    }

    bool is_in_define = false;
    i32 indent_before_define = 0;

    i32 brace_level = 0;

    // NOTE(jesper): 16 nested switches ought to be enough for anybody?
    i32 switch_indent_stack[16];
    i32 switch_brace_stack[16];
    i32 switch_depth = 0;

    // NOTE(jesper): 32 nested parens ought to be enough for anybody?
    i32 paren_anchor_stack[32];
    i32 paren_depth = 0;

    Cpp_Token *prev_line_last = nullptr;
    while (iter < iter_end && line_number < one_past_last_line) {
        Temp_Memory tmp = begin_temp_memory(&global_part);

        if (is_in_define &&
            ((iter->flags & CPP_TFLAG_PP_BODY) == 0 ||
             iter->type == CPP_PP_DEFINE))
        {
            is_in_define = false;
            cur_indent = indent_before_define;
        }
        
        bool statement_complete = false;
        if (prev_line_last) {
            switch (prev_line_last->type) {
            case CPP_TOKEN_BRACE_OPEN:
            case CPP_TOKEN_BRACE_CLOSE:
            case CPP_TOKEN_SEMICOLON:
            case CPP_TOKEN_COLON:
            case CPP_TOKEN_COMMA:
                statement_complete = true;
                break;
            }

            if (!statement_complete) {
                cur_indent += tab_width;
            }
        }

        if (paren_depth > 0) {
            cur_indent = paren_anchor_stack[paren_depth-1];
        }

        i32 line_end_pos = buffer_get_line_end(app, buffer, line_number);
        while (iter->start > line_end_pos && line_number < one_past_last_line) {
            if (line_number >= first_line) {
                indent_marks[line_number] = cur_indent;
            }
            line_end_pos = buffer_get_line_end(app, buffer, ++line_number);
        }

        Cpp_Token *first = iter;
        Cpp_Token *last = iter;

        switch (first->type) {
        case CPP_PP_IF:
        case CPP_PP_IFDEF:
        case CPP_PP_IFNDEF:
        case CPP_PP_ELSE:
        case CPP_PP_ELIF:
        case CPP_PP_ENDIF:
        case CPP_PP_ERROR:
        case CPP_PP_INCLUDE:
            cur_indent = 0;
            break;
        case CPP_TOKEN_BRACE_OPEN:
            if (!statement_complete && paren_depth == 0) {
                cur_indent -= tab_width;
            }
            break;
        case CPP_TOKEN_BRACE_CLOSE:
            cur_indent -= tab_width;
            break;
        case CPP_TOKEN_KEY_ACCESS: {
                String str = get_token_string(app, buffer, *first, &global_part);
                if (match_cs("public", str) ||
                    match_cs("private", str) ||
                    match_cs("protected", str))
                {
                    cur_indent -= tab_width;
                }
            } break;
        case CPP_TOKEN_KEY_CONTROL_FLOW: {
                String str = get_token_string(app, buffer, *first, &global_part);
                if (match_cs("switch", str)) {
                    switch_indent_stack[switch_depth] = cur_indent;
                    switch_brace_stack[switch_depth] = brace_level;

                    if (switch_depth < ARRAY_COUNT(switch_indent_stack)) {
                        switch_depth++;
                    }
                } else if (match_cs("case", str) || match_cs("default", str)) {
                    if (switch_depth > 0) {
                        cur_indent = switch_indent_stack[switch_depth-1];
                    }
                    next_indent = cur_indent + tab_width;
                }
            } break;
        case CPP_TOKEN_IDENTIFIER:
            if (first+1 < iter_end &&
                (first+1)->type == CPP_TOKEN_COLON &&
                (first+2) < iter_end && (first+2)->type != CPP_TOKEN_COLON)
            {
                // NOTE(jesper): assume goto label
                cur_indent = 0;
            }
            break;
        }

        i32 next_line_start_pos = buffer_get_line_start(app, buffer, line_number + 1);
        do {
            switch (iter->type) {
            case CPP_TOKEN_BRACE_CLOSE:
                brace_level--;
                next_indent -= tab_width;

                if (switch_depth > 0 &&
                    switch_brace_stack[switch_depth-1] == brace_level)
                {
                    switch_depth--;
                }
                break;
            case CPP_TOKEN_BRACE_OPEN:
                next_indent += tab_width;
                if (paren_depth > 0) {
                    paren_anchor_stack[paren_depth-1] += tab_width;
                }
                brace_level++;
                break;
            case CPP_TOKEN_PARENTHESE_OPEN:
                if (paren_depth < ARRAY_COUNT(paren_anchor_stack)) {
                    paren_anchor_stack[paren_depth++] = iter->start - first->start + cur_indent + 1;
                }
                break;
            case CPP_TOKEN_PARENTHESE_CLOSE:
                paren_depth = max(0, paren_depth - 1);
                break;
            }

            last = iter++;
        } while (iter < iter_end && iter->start < next_line_start_pos);

        switch (last->type) {
        case CPP_TOKEN_PARENTHESE_OPEN:
            paren_anchor_stack[paren_depth-1] = cur_indent + tab_width;
            break;
        }

        if (!(last->flags & (CPP_TFLAG_PP_DIRECTIVE | CPP_TFLAG_PP_BODY)) &&
            last->type != CPP_TOKEN_COMMENT)
        {
            prev_line_last = last;
        }

        next_indent = max(0, next_indent);
        cur_indent = max(0, cur_indent);

        if (is_in_define) {
            cur_indent += tab_width;
        } else if (first->type == CPP_PP_DEFINE) {
            indent_before_define = cur_indent;
            cur_indent = 0;
            next_indent = 0;
            is_in_define = true;
        }

        if (line_number >= first_line) {
            indent_marks[line_number] = cur_indent;
        }
        line_number++;

        cur_indent = next_indent;
        end_temp_memory(tmp);
    }


    indent_marks += first_line;
    return indent_marks;
}

static bool32
custom_buffer_auto_indent(
    Application_Links *app, Partition *part,
    Buffer_Summary *buffer,
    int32_t start,
    int32_t end,
    int32_t tab_width, Auto_Indent_Flag flags)
{
    if (!buffer->exists) return false;
    
    String filename = make_string(buffer->file_name, buffer->file_name_len);
    if (ext_is_c(file_extension(filename))) {
        if (!buffer->tokens_are_ready) return false;
        Temp_Memory temp = begin_temp_memory(part);

        // Stage 1: Read the tokens to be used for indentation.
        Cpp_Token_Array tokens;
        tokens.count = buffer_token_count(app, buffer);
        tokens.max_count = tokens.count;
        tokens.tokens = push_array(part, Cpp_Token, tokens.count);
        buffer_read_tokens(app, buffer, 0, tokens.count, tokens.tokens);

        // Stage 2: Decide where the first and last lines are.
        //  The lines in the range [line_start,line_end) will be indented.
        int32_t line_start = 0, line_end = 0;
        if (flags & AutoIndent_FullTokens){
            get_indent_lines_whole_tokens(app, buffer, tokens, start, end, &line_start, &line_end);
        }
        else{
            get_indent_lines_minimum(app, buffer, start, end, &line_start, &line_end);
        }

        // Stage 3: Decide Indent Amounts
        //  Get an array representing how much each line in
        //   the range [line_start,line_end) should be indented.
        int32_t *indent_marks = custom_get_indentation_marks(
            app,
            part,
            buffer,
            tokens,
            line_start,
            line_end,
            (flags & AutoIndent_ExactAlignBlock),
            tab_width);

        // Stage 4: Set the Line Indents
        Indent_Options opts = {};
        opts.empty_blank_lines = (flags & AutoIndent_ClearLine);
        opts.use_tabs = (flags & AutoIndent_UseTab);
        opts.tab_width = tab_width;

        set_line_indents(app, part, buffer, line_start, line_end, indent_marks, opts);

        end_temp_memory(temp);
        
        return true;
    } else {
        Temp_Memory temp = begin_temp_memory(part);
        
        i32 line_start = 0, line_end = 0;
        get_indent_lines_minimum(app, buffer, start, end, &line_start, &line_end);
        
        i32 indent_marks_count = line_end - line_start;
        i32 *indent_marks = push_array(part, i32, indent_marks_count);
        memset(indent_marks, 0, sizeof *indent_marks * indent_marks_count);
        
        i32 line = max(line_start-1, 0);
        i32 line_pos = buffer_get_line_start(app, buffer, line);
        
        Hard_Start_Result hard_start = buffer_find_hard_start(app, buffer, line_pos, tab_width);
        i32 cur_indent = hard_start.indent_pos;
        i32 next_indent = cur_indent;
        
        indent_marks -= line_start;
        
        bool was_cr = false;
        
        i32 pos = line_pos;
        Stream_Chunk stream = {};
        char data[1024];
        if (init_stream_chunk(&stream, app, buffer, pos, data, sizeof data)) {
            do {
                for (; pos < stream.end; pos++) {
                    char c = stream.data[pos];
                    
                    if (c == '\r') {
                        was_cr = true;
                        if (line >= line_start) indent_marks[line] = cur_indent;
                        line++;
                        cur_indent = next_indent;
                        if (line == line_end) goto finish;
                    } else if (c == '\n' && !was_cr) {
                        if (line >= line_start) indent_marks[line] = cur_indent;
                        line++;
                        cur_indent = next_indent;
                        if (line == line_end) goto finish;
                    } else {
                        was_cr = false;
                        switch (c) {
                        case '{':
                            next_indent += tab_width;
                            break;
                        case '}':
                            next_indent -= tab_width;
                            cur_indent -= tab_width;
                            break;
                        }
                    }
                    
                    cur_indent = max(0, cur_indent);
                    next_indent = max(0, next_indent);
                }
            } while (forward_stream_chunk(&stream));
        } else {
            pos = -1;
        }
                     
finish:
        indent_marks += line_start;
        
        Indent_Options opts = {};
        opts.empty_blank_lines = (flags & AutoIndent_ClearLine);
        opts.use_tabs = (flags & AutoIndent_UseTab);
        opts.tab_width = tab_width;
        set_line_indents(app, part, buffer, line_start, line_end, indent_marks, opts);
        
        end_temp_memory(temp);
        return true;
    }
}
