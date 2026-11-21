/* The way to the battle's camera (BT4's new select; the user's reference, Dragon Ball: Sparking! ZERO's presentation).
   Plain C with no game headers, so the same code runs in the game (view_a_d.c) and in the test that checks its framing
   on the PC (port/tests/cine_shot_test.c).

   What it fixes (the causes, found in view_a_d.c): the old camera was told distances as multiples of the body's height
   (1.05, 0.78, 0.5, 0.95 x h) chosen by eye for one fighter in one picture shape. The picture's field of view decides
   how much of a body fits at a distance, and the port keeps the VERTICAL field of view (tan(half) = 224 / 433 = 0.517
   from a 4:3 up to any wide shape; View_SetProjection / View_UpdateMatrices) and widens the sides, so:
     - a body of height h at 1.05 h is seen over 1.09 h of the picture: feet cut off, nothing around it;
     - at 0.78 h and 0.5 h the fighter fills the picture and is cut at the chest, as the user saw;
     - on a narrow picture the width, not the height, decides, and nothing looked at it.
   Here every shot is asked for as "this part of the body, with this much around it", and the distance is worked out
   from the lens of the view, both ways (height, width), the larger taken. The numbers of a shot are fractions of the
   fighter's own measured height, never a fixed size.

   Units: y is up NEGATIVE (the floor is 0, the head is -h), as the game's own orbit camera takes them. */
#ifndef CINE_SHOT_H
#define CINE_SHOT_H

#define CINE_PI 3.14159265f
#define CINE_CUT 55 /* the frame of the cut (under the speed lines): the row of the team goes, the opponent comes */
#define CINE_END 270

typedef struct CineLens {
    float tanV, tanH; /* tangents of half the vertical / horizontal angle of view */
} CineLens;

typedef struct CineCam {
    float tx, ty, tz;   /* what it looks at */
    float yaw, pitch;   /* the orbit camera's angles */
    float dist;         /* and its distance from the target */
} CineCam;

/* The lens of a view as the port sets it up (btl_cam.c): screenDist, and the factors aspectX / aspectY
   (View_BuildProjection: the screen is 512 x 448 and the half sizes are 256 and 224). */
static inline CineLens CineLens_Of(float screenDist, float aspectX, float aspectY) {
    CineLens l;

    /* a view whose projection was never built (or a NaN) would put zeros and infinities in every distance: the game's own numbers then */
    if (!(screenDist > 1.0f)) {
        screenDist = 433.0f;
    }
    if (!(aspectX > 0.05f)) {
        aspectX = 1.0f;
    }
    if (!(aspectY > 0.05f)) {
        aspectY = 1.0f;
    }

    l.tanH = 256.0f / (screenDist * aspectX);
    l.tanV = 224.0f / (screenDist * aspectY);
    return l;
}

/* The same from the picture's shape (width / height), by View_SetProjection's own rules: the pixel factor 1.1667, the
   shape's factor w = shape / (4/3), y = 1.1667 w; if y > 1 the sides are widened (aspectX = 1 / y, aspectY = 1), else
   the height is (aspectX = 1, aspectY = y). For the test; the game reads its own view. */
static inline CineLens CineLens_Shape(float shape) {
    float y = 1.16666667f * (shape / 1.33333333f);

    return y > 1.0f ? CineLens_Of(433.0f, 1.0f / y, 1.0f) : CineLens_Of(433.0f, 1.0f, y);
}

static inline float Cine_Clamp(float v) {
    return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}
static inline float Cine_Ease(float u) {
    u = Cine_Clamp(u);
    return u * u * (3.0f - 2.0f * u);
}
static inline float Cine_Mix(float a, float b, float e) {
    return a + (b - a) * e;
}

/* The distance at which a subject `height` tall, `width` wide and `deep` deep (its half, `halfDepth`, is nearer the camera
   than its middle, and what is nearer looks larger) fills 1 / margin of the picture, whichever way is the tighter (a
   tall shape's height, a narrow picture's width). */
static inline float Cine_Fit(const CineLens *l, float height, float width, float margin, float halfDepth) {
    float dv = 0.5f * height * margin / l->tanV + halfDepth;
    float dh = 0.5f * width * margin / l->tanH + halfDepth;

    return dv > dh ? dv : dh;
}

/* A shot: the part of the body from lo to hi (fractions of its height, floor 0) with the margin around it, `wide` the
   width and `deep` the depth of what is shown (fractions of the height). It gives what the camera looks at (y, up negative) and how far. */
static inline void Cine_Shot(const CineLens *l, float h, float lo, float hi, float wide, float deep, float margin, float *ty, float *dist) {
    *ty = -0.5f * (lo + hi) * h;
    *dist = Cine_Fit(l, (hi - lo) * h, wide * h, margin, 0.5f * deep * h);
}

/* The three shots of the way: the whole fighter (with room above the hair and under the feet), from the waist up, and
   the face with its shoulders. Fractions of the fighter's own height, the same for every size. */
#define CINE_FULL_LO -0.03f
#define CINE_FULL_HI 1.07f
#define CINE_FULL_W 0.52f
#define CINE_FULL_D 0.34f
#define CINE_FULL_M 1.10f
#define CINE_MED_LO 0.30f
#define CINE_MED_HI 1.07f
#define CINE_MED_W 0.46f
#define CINE_MED_D 0.34f
#define CINE_MED_M 1.10f
#define CINE_CLOSE_LO 0.80f
#define CINE_CLOSE_HI 1.07f
#define CINE_CLOSE_W 0.34f
#define CINE_CLOSE_D 0.18f
#define CINE_CLOSE_M 1.28f

extern float sinf(float x);
extern float cosf(float x);
extern float sqrtf(float x);
extern float atan2f(float y, float x);

typedef struct CineV {
    float x, y, z;
} CineV;

/* Where the orbit camera's eye is for a camera (orbit_cam.c: target - dist * (cos p sin y, -sin p, cos p cos y)). */
static inline CineV Cine_Eye(const CineCam *c) {
    float cp = cosf(c->pitch);
    CineV e;

    e.x = c->tx - c->dist * cp * sinf(c->yaw);
    e.y = c->ty + c->dist * sinf(c->pitch);
    e.z = c->tz - c->dist * cp * cosf(c->yaw);
    return e;
}

/* The camera that looks from `eye` at `tgt`: its angles and distance (the inverse of the above). */
static inline CineCam Cine_Look(CineV eye, CineV tgt) {
    CineCam c;
    float dx = tgt.x - eye.x, dy = tgt.y - eye.y, dz = tgt.z - eye.z;
    float flat = sqrtf(dx * dx + dz * dz);

    c.tx = tgt.x;
    c.ty = tgt.y;
    c.tz = tgt.z;
    c.yaw = atan2f(dx, dz);
    c.pitch = atan2f(-dy, flat);
    c.dist = sqrtf(dx * dx + dy * dy + dz * dz);
    if (!(c.dist > 0.5f)) {
        c.dist = 0.5f;
    }
    return c;
}

/* The opponent, standing in the map for the last shot, ahead of the chosen fighter: a little to one side of his line
   (CINE_OPP_SIDE of his height) and Cine_OppDepth in front, turned to him. view_a_d.c places him by these, the test checks the
   picture with them. */
#define CINE_OPP_SIDE 0.25f
/* How far ahead of the chosen fighter the opponent stands: by BOTH heights (a giant needs more room than a small one, and the other
   way round), so that two of one height are 2.6 of it apart, as before. */
static inline float Cine_OppDepth(float h, float hOpp) {
    return 1.6f * h + 1.0f * hOpp;
}
#ifndef CINE_SH_BACK
#define CINE_SH_BACK 0.65f /* how far behind him the eye is, in his heights */
#endif
#ifndef CINE_SH_ROOM
#define CINE_SH_ROOM 0.65f /* the share of the picture's half width his far shoulder may reach */
#endif

/* The last shot: over the chosen fighter's shoulder (the user's: after the turn the camera stays over his shoulder and shows the
   opponent ahead, not his back from the ground). The camera is behind him and to one side, at about the height of his head, looking
   a little down at the opponent's chest; the fighter's head and shoulder fill one side of the picture and the opponent stands whole
   beyond, in the middle. How far to the side the eye is depends on the lens: the fighter's far shoulder is kept inside the picture. */
static inline void Cine_ShoulderShot(const CineLens *l, float lx, float h, float hOpp, CineV *eye, CineV *tgt) {
    float back = CINE_SH_BACK * h;                      /* behind him: close, his head and shoulder big in the picture */
    float side = 0.40f * h;                             /* to one side of him */
    float room = CINE_SH_ROOM * back * l->tanH - 0.24f * h; /* what the picture's side leaves for his far shoulder */
    float ox, depth, dx, dz, flat, headTop, headChin, oppTop, oppFeet, top, bot, up;

    if (side > room) {
        side = room;
    }
    if (side < 0.12f * h) {
        side = 0.12f * h;
    }
    eye->x = lx + side;
    eye->y = -1.02f * h;
    eye->z = -back;
    ox = lx + CINE_OPP_SIDE * h;
    depth = Cine_OppDepth(h, hOpp);
    dx = ox - eye->x;
    dz = depth + back;
    flat = sqrtf(dx * dx + dz * dz);
    /* Where the fighter's head and the whole opponent fall up and down the picture if it looked straight ahead (1 = its edge, up
       positive); it is turned up or down to put the middle of what must be seen at its middle. A tall opponent beyond a small
       fighter (or the other way) needs it: aiming at the opponent's chest took the fighter's head out of the picture. */
    headTop = (1.07f * h - 1.02f * h) / (back * l->tanV);
    headChin = (0.86f * h - 1.02f * h) / (back * l->tanV);
    oppTop = (1.07f * hOpp - 1.02f * h) / (flat * l->tanV);
    oppFeet = (0.0f - 1.02f * h) / (flat * l->tanV);
    top = headTop > oppTop ? headTop : oppTop;
    bot = headChin < oppFeet ? headChin : oppFeet;
    {
        float c = 0.5f * (top + bot), lo = headTop - 0.80f, hi = headChin + 0.80f; /* the middle of what must be seen, and what keeps HIS head whole */

        if (lo > hi) {
            lo = hi = 0.5f * (headTop + headChin);
        }
        c = c < lo ? lo : c > hi ? hi : c; /* (a giant beyond a small fighter cannot be whole too: his head comes first) */
        up = atan2f(c * l->tanV, 1.0f);
    }
    up = up < -0.35f ? -0.35f : up > 0.35f ? 0.35f : up;
    tgt->x = ox;
    tgt->y = eye->y - (sinf(up) / cosf(up)) * flat;      /* (y is up negative) */
    tgt->z = depth;
}

/* The camera at frame f of the way (60 to a second), for a fighter `h` tall standing at world x = lx. (t0y, p0, d0): the
   select's own framing at frame 0, so the way starts from what was on the screen and does not cut to it. */
static inline CineCam Cine_Cam(int f, const CineLens *l, float lx, float h, float hOpp, float t0y, float p0, float d0) {
    CineCam c;
    float fullY, fullD, medY, medD, closeY, closeD;

    Cine_Shot(l, h, CINE_FULL_LO, CINE_FULL_HI, CINE_FULL_W, CINE_FULL_D, CINE_FULL_M, &fullY, &fullD);
    Cine_Shot(l, h, CINE_MED_LO, CINE_MED_HI, CINE_MED_W, CINE_MED_D, CINE_MED_M, &medY, &medD);
    Cine_Shot(l, h, CINE_CLOSE_LO, CINE_CLOSE_HI, CINE_CLOSE_W, CINE_CLOSE_D, CINE_CLOSE_M, &closeY, &closeD);
    c.tz = 0.0f;
    /* The way begins at once with the turn, from the select's own framing: no zoom in to a closer shot, no cut (the user: after the dust the
       camera turns round him at once, no hard cuts, no zoom). The framing the select shows (target height t0y, pitch p0, distance d0) is kept
       all through the turn; only the angle changes, from the front (the select looks at him from the front) round by his right to his back. */
    if (f < 100) {
        float e = Cine_Ease((float)f / 100.0f);

        c.tx = lx;
        c.ty = t0y;
        c.yaw = Cine_Mix(CINE_PI, 0.18f, e);
        c.pitch = p0;
        c.dist = d0;
    } else { /* the camera goes over his shoulder: it rises to the height of his head and moves in beside him, turning to the opponent, in the map ahead */
        float e = Cine_Ease((float)(f - 100) / 55.0f);
        float drift = f > 155 ? (float)(f - 155) / (float)(CINE_END - 155) : 0.0f;
        CineCam from;
        CineV e0, t0, e1, t1, eye, tgt;

        from.tx = lx; from.ty = t0y; from.tz = 0.0f; from.yaw = 0.18f; from.pitch = p0; from.dist = d0; /* where the turn ended */
        e0 = Cine_Eye(&from);
        t0.x = from.tx; t0.y = from.ty; t0.z = from.tz;
        Cine_ShoulderShot(l, lx, h, hOpp, &e1, &t1);
        drift = drift > 1.0f ? 1.0f : drift;
        tgt.x = Cine_Mix(t0.x, t1.x, e); tgt.y = Cine_Mix(t0.y, t1.y, e); tgt.z = Cine_Mix(t0.z, t1.z, e);
        eye.x = Cine_Mix(e0.x, e1.x, e); eye.y = Cine_Mix(e0.y, e1.y, e); eye.z = Cine_Mix(e0.z, e1.z, e);
        /* (a slow push in at the end: the eye goes along the view by a share of its distance BEHIND the fighter, never of its distance to
           the target, which is the opponent's and far: that took a small fighter's head out of the picture) */
        {
            float dx = tgt.x - eye.x, dy = tgt.y - eye.y, dz = tgt.z - eye.z, len = sqrtf(dx * dx + dy * dy + dz * dz);
            float step = 0.05f * CINE_SH_BACK * h * drift / (len > 0.001f ? len : 1.0f);

            eye.x += dx * step; eye.y += dy * step; eye.z += dz * step;
        }
        c = Cine_Look(eye, tgt);
    }
    return c;
}

#endif
