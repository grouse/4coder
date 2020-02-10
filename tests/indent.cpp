#define simple_define foo
#define multi_line_define\
	several();\
	lines()
#define define_func(a, b)\
	while(a &&\
          b)\
    {\
        foobar(a, b);\
    }
struct simple_struct { int a; int b; };
struct normal_struct {
    int a;
    int b;
};

void foo()
{
    switch (foo) {
    case case0:
        {
            foo;
        } break;
    case case3: {
            foo;
        } break;
    case case2:
        break;
    case case4:
        {
        } break;
    case case5: {
            foobar;
        } break;
    }

    f32 vertices[] = {
        123.0f, 32.0, 0.4f,
        123.0f, 123.0f,
        123.0f, 2313.0f,
    };

    f32 vertices2[] = {
        123.0f, 32.0, 0.4f,
        123.0f, 123.0f,
        123.0f, 2313.0f
    };

    int result = calling_some_function(
        aiowjefoi,
        awefjim,
        awefioj);

    int result = calling_some_func_with_struct_literal(
        vector3{ 0.0f, 2.3f },
        foobar);
    
    int result = calling_another_function_with_struct_literal(
        vector3{
            0.0f,
            3.0f,
            3.0f
        },
        foobar);
    
    // NOTE(jesper): tbh I'm not sure what I'd want here. It's not
    // something I think I've ever done, so I don't think I care.
    int result = and_again(
        vector3{ 123,
            123,
            123
        },
        foobar);
    

    if (foobar &&
        iowajfi)
    {
    }

    foobar();
#define inside_func
    foobar();
#define inside_func_multi\
    several();\
    lines()
}
