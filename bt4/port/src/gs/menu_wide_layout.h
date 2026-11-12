#ifndef BT4_MENU_WIDE_LAYOUT_H
#define BT4_MENU_WIDE_LAYOUT_H

static inline float Port_MenuWideScaleX(int aspect_milli) {
    return aspect_milli > 1333 ? 1333.333333f / (float)aspect_milli : 1.0f;
}

/* Scenery covers the viewport uniformly. Unlike UI artwork, its excess
 * height may be cropped, but its pixels must keep the original aspect. */
static inline float Port_MenuWideCoverY(float y, int aspect_milli) {
    float scale = Port_MenuWideScaleX(aspect_milli);
    return 224.0f + (y - 224.0f) / scale;
}

static inline float Port_MenuWideTransformX(float x, float pivot, float scale) {
    return pivot + (x - pivot) * scale;
}

#endif
