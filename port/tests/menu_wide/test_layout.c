#include <assert.h>
#include <math.h>
#include "../../src/gs/menu_wide_layout.h"

static int close_to(float a, float b) {
    return fabsf(a - b) < 0.001f;
}

int main(void) {
    assert(close_to(Port_MenuWideScaleX(1333), 1.0f));
    assert(close_to(Port_MenuWideScaleX(1600), 0.833333f));
    assert(close_to(Port_MenuWideScaleX(1777), 0.75f));
    assert(close_to(Port_MenuWideScaleX(2333), 0.571429f));
    assert(close_to(Port_MenuWideScaleX(2667), 0.5f));

    {
        int aspects[] = {1333,1600,1778,2333,2667};
        unsigned i;
        for (i=0;i<sizeof(aspects)/sizeof(aspects[0]);i++) {
            float s=Port_MenuWideScaleX(aspects[i]);
            /* Text, portraits and masks use the same parent transform. */
            assert(close_to(Port_MenuWideTransformX(0,0,s),0));
            assert(close_to(Port_MenuWideTransformX(512,512,s),512));
            assert(close_to(Port_MenuWideTransformX(256,256,s),256));
            /* A background covers by the inverse scale vertically, keeping
             * its physical aspect instead of flattening its artwork. */
            assert(close_to(Port_MenuWideCoverY(224,aspects[i]),224));
            assert(close_to((Port_MenuWideCoverY(324,aspects[i])-224)*s,100));
            assert(Port_MenuWideCoverY(0,aspects[i])<=0);
            assert(Port_MenuWideCoverY(448,aspects[i])>=448);
        }
    }

    {
        /* 16:10 is exact in integer milli-aspect units. 16:9 is rounded to
         * 1777/1778, so expecting exact 64-pixel margins there is incorrect. */
        float scale = Port_MenuWideScaleX(1600);
        float left = Port_MenuWideTransformX(0.0f, 256.0f, scale);
        float center = Port_MenuWideTransformX(256.0f, 256.0f, scale);
        float right = Port_MenuWideTransformX(512.0f, 256.0f, scale);
        assert(close_to(left, 42.666667f));
        assert(close_to(center, 256.0f));
        assert(close_to(right, 469.333333f));
        assert(close_to(right - left, 426.666667f));
    }

    return 0;
}
