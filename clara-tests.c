#include "common.h"

void test_intersize() {
        int x,y;
#define Test(a,b,c,d, e,f, s) do {                                      \
                x = y = -2;                                             \
                g_assert_cmpint(intersize(a,b,c,d, &x, &y), ==, s);     \
                g_assert_cmpint(e, ==, x);                              \
                g_assert_cmpint(f, ==, y);                              \
                x = y = -2;                                             \
                g_assert_cmpint(intersize(c,d,a,b, &x,&y), ==, s);      \
                g_assert_cmpint(e, ==, x);                              \
                g_assert_cmpint(f, ==, y);                              \
        } while(0)
        
        
        Test(1,2,3,4, -2,-2, 0); // a-b c-d
        Test(1,2,2,4, 2,2, 1); // a-bc-d
        Test(1,3,2,4, 2,3, 2); // a-c=b-d
        Test(1,3,2,3, 2,3, 2); // a-c=bd
        Test(1,4,2,3, 2,3, 2); // a-b=d-c

        Test(2,2,3,4, -2,-2, 0); // ab c-d
        Test(2,2,2,4, 2,2, 1); // abc-d
        Test(2,3,2,4, 2,3, 2); // ac=b-d
        Test(2,3,2,3, 2,3, 2); // ac=bd
        Test(2,4,2,3, 2,3, 2); // ab=d-c

#undef Test        
}

int main(int argc, char** argv) {
        g_test_init(&argc, &argv, NULL);
        g_test_add_func("/clara.c/intersize", test_intersize);
        return g_test_run();
}
