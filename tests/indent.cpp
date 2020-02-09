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
}
foobar();
#define inside_func
foobar();
#define inside_func_multi\
    several();\
    lines()


}
