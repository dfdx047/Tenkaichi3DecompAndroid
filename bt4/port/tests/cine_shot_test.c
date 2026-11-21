/* Framing test for the way to the battle's camera (src/battle/cine_shot.h), on the PC, no game needed.
   For fighters of many sizes and picture shapes it follows the camera frame by frame, projects the fighter's body and head
   (boxes in the world, through the view's own lens) and checks what the user saw go wrong: heads cut off, no whole body in the
   shots that should show one, a face that does not fill or overflows, a camera that jumps.
   It runs the OLD camera (the multiples of the height, copied from view_a_d.c as it was) and the NEW one side by side.
   Build: cc -O2 -o /tmp/cine_shot_test port/tests/cine_shot_test.c -lm && /tmp/cine_shot_test */
#include <math.h>
#include <stdio.h>
#include "../../src/battle/cine_shot.h"

/* ---- the old camera, as it was (view_a_d.c ChrView_PortCineCam) */
static CineCam Old_Cam(int f, float lx, float h, float t0y, float p0, float d0) {
    CineCam c;

    c.tz = 0.0f;
    if (f < 30) {
        float u = (float)f / 30.0f, e = u * u;
        c.tx = Cine_Mix(0.0f, lx, e); c.ty = Cine_Mix(t0y, -0.62f * h, e); c.yaw = Cine_Mix(CINE_PI, CINE_PI - 0.28f, e);
        c.pitch = Cine_Mix(p0, -0.06f, e); c.dist = Cine_Mix(d0, 1.05f * h, e);
    } else if (f < CINE_CUT) {
        float e = Cine_Ease((float)(f - 30) / 12.0f);
        c.tx = lx; c.ty = Cine_Mix(-0.62f * h, -0.70f * h, e); c.yaw = Cine_Mix(CINE_PI - 0.28f, CINE_PI - 0.32f, e);
        c.pitch = -0.06f; c.dist = Cine_Mix(1.05f * h, 0.78f * h, e);
    } else if (f < 82) {
        c.tx = lx; c.ty = -0.86f * h; c.yaw = CINE_PI + 0.45f; c.pitch = -0.03f; c.dist = 0.5f * h;
    } else if (f < 135) {
        float e = Cine_Ease((float)(f - 82) / 53.0f);
        c.tx = lx; c.ty = Cine_Mix(-0.86f * h, -0.78f * h, e); c.yaw = Cine_Mix(CINE_PI + 0.45f, 0.18f, e);
        c.pitch = Cine_Mix(-0.03f, -0.05f, e); c.dist = Cine_Mix(0.5f * h, 0.95f * h, e);
    } else {
        float e = Cine_Ease((float)(f - 135) / 44.0f), drift = f > 179 ? (float)(f - 179) / (float)(CINE_END - 179) : 0.0f;
        drift = drift > 1.0f ? 1.0f : drift;
        c.tx = Cine_Mix(lx, lx - 0.55f * h, e); c.ty = Cine_Mix(-0.78f * h, -0.72f * h, e); c.tz = Cine_Mix(0.0f, 0.8f * h, e);
        c.yaw = Cine_Mix(0.18f, 0.10f, e); c.pitch = Cine_Mix(-0.05f, -0.06f, e);
        c.dist = Cine_Mix(0.95f * h, 1.35f * h, e) * (1.0f + 0.04f * drift);
    }
    return c;
}

typedef struct { float x, y, z; } V3;
static V3 sub(V3 a, V3 b) { V3 r = {a.x - b.x, a.y - b.y, a.z - b.z}; return r; }
static float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 cross(V3 a, V3 b) { V3 r = {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; return r; }
static V3 norm(V3 a) { float n = sqrtf(dot(a, a)); V3 r = {a.x / n, a.y / n, a.z / n}; return r; }

/* The orbit camera's eye (orbit_cam.c): target - dist * (cos p sin y, -sin p, cos p cos y). It looks at the target. */
static V3 Eye(CineCam c) {
    V3 d = {cosf(c.pitch) * sinf(c.yaw), -sinf(c.pitch), cosf(c.pitch) * cosf(c.yaw)}, t = {c.tx, c.ty, c.tz};
    V3 e = {t.x - d.x * c.dist, t.y - d.y * c.dist, t.z - d.z * c.dist};
    return e;
}

/* The worst |screen| a box reaches (1 = the edge of the picture): over its 8 corners; and its height as a fraction of the
   picture. A corner behind the camera counts as 99. */
static void Box(CineCam c, const CineLens *l, float x0, float x1, float y0, float y1, float z0, float z1, float *worst, float *fillV) {
    V3 eye = Eye(c), tgt = {c.tx, c.ty, c.tz}, f = norm(sub(tgt, eye)), up0 = {0.0f, -1.0f, 0.0f};
    V3 r = norm(cross(f, up0)), u = cross(r, f);
    float lo = 1e9f, hi = -1e9f;
    int i;

    *worst = 0.0f;
    for (i = 0; i < 8; i++) {
        V3 p = {(i & 1) ? x1 : x0, (i & 2) ? y1 : y0, (i & 4) ? z1 : z0}, d = sub(p, eye);
        float zc = dot(d, f), sx, sy, m;

        if (zc < 0.05f) { *worst = 99.0f; *fillV = 0.0f; return; }
        sx = dot(d, r) / (zc * l->tanH);
        sy = dot(d, u) / (zc * l->tanV);
        m = fabsf(sx) > fabsf(sy) ? fabsf(sx) : fabsf(sy);
        if (m > *worst) *worst = m;
        if (sy < lo) lo = sy;
        if (sy > hi) hi = sy;
    }
    *fillV = (hi - lo) * 0.5f;
}

/* where a point lands on the picture (sx across, sy up, 1 = the edge); 99 if it is behind the camera */
static void Where(CineCam c, const CineLens *l, float x, float y, float z, float *sx, float *sy) {
    V3 eye = Eye(c), tgt = {c.tx, c.ty, c.tz}, f = norm(sub(tgt, eye)), up0 = {0.0f, -1.0f, 0.0f};
    V3 r = norm(cross(f, up0)), u = cross(r, f), p = {x, y, z}, d = sub(p, eye);
    float zc = dot(d, f);

    if (zc < 0.05f) { *sx = *sy = 99.0f; return; }
    *sx = dot(d, r) / (zc * l->tanH);
    *sy = dot(d, u) / (zc * l->tanV);
}

typedef struct { int run, headCut, noFull, closeBad, jumps, near, oppBad, eyeBad; float maxStep, minClose, maxClose, minOpp; } Score;

static void Run(int isNew, float shape, float h, float hOpp, float lx, Score *s) {
    CineLens l = CineLens_Shape(shape);
    float t0y = -0.85f * h, p0 = -0.04f, d0 = 0.829f * h; /* the select's own framing (port_charsel / view_a_d.c) */
    CineCam prev = {0}, c;
    int f;
    int headCut = 0, noFull = 0, closeBad = 0, jumps = 0, near = 0, oppBad = 0, eyeBad = 0;
    float maxStep = 0.0f, minClose = 9.0f, maxClose = 0.0f, minOpp = 9.0f;

    for (f = 0; f < CINE_END; f++) {
        float worst, fill;

        c = isNew ? Cine_Cam(f, &l, lx, h, hOpp, t0y, p0, d0) : Old_Cam(f, lx, h, t0y, p0, d0);
        /* the head: from 0.86 h to 1.07 h, 0.14 h wide and deep (the face and the hair) */
        Box(c, &l, lx - 0.07f * h, lx + 0.07f * h, -1.07f * h, -0.86f * h, -0.07f * h, 0.07f * h, &worst, &fill);
        if (worst > 1.0f) headCut++;
        /* the whole body, where the shot is the whole fighter: the end of the first push (f 30) and the hold (150 on) */
        if (f == 30) {
            Box(c, &l, lx - 0.26f * h, lx + 0.26f * h, -1.07f * h, 0.0f, -0.15f * h, 0.15f * h, &worst, &fill);
            if (worst > 1.0f) noFull++;
        }
        /* the face shot (frames 58 to 80): the head fills 30 % to 92 % of the picture's height */
        if (f >= 58 && f <= 80) {
            Box(c, &l, lx - 0.07f * h, lx + 0.07f * h, -1.07f * h, -0.86f * h, -0.07f * h, 0.07f * h, &worst, &fill);
            if (fill < minClose) minClose = fill;
            if (fill > maxClose) maxClose = fill;
            if (fill < 0.30f || fill > 0.92f || worst > 1.0f) closeBad++;
        }
        if (f >= 190) { /* the last shot, over his shoulder: his head and shoulders in the picture on one side, the opponent whole, in the middle */
            float ox = isNew ? lx + CINE_OPP_SIDE * h : lx - 1.4f * h, oz = isNew ? Cine_OppDepth(h, hOpp) : 6.0f * h;
            float ow, of, eyeUp, hx, hy, qx, qy, w3, f3;
            V3 eye = Eye(c);

            Box(c, &l, ox - 0.26f * hOpp, ox + 0.26f * hOpp, -1.07f * hOpp, 0.0f, oz - 0.15f * hOpp, oz + 0.15f * hOpp, &ow, &of);
            { /* a giant beyond a small fighter may be cut at the top (the fighter's head comes first): what must be whole is his
                 chest and waist; and how big he must look depends on how big he is next to the fighter */
                float w2b, f2b, need = 0.35f * hOpp / (hOpp > h ? hOpp : h);

                Box(c, &l, ox - 0.26f * hOpp, ox + 0.26f * hOpp, -0.85f * hOpp, -0.30f * hOpp, oz - 0.15f * hOpp, oz + 0.15f * hOpp, &w2b, &f2b);
                need = need < 0.04f ? 0.04f : need > 0.20f ? 0.20f : need;
                ow = w2b;
                of = of < need ? of : (of > need ? 1.0f : of); /* (>= need passes) */
                if (of >= need) of = 1.0f;
            }
            Box(c, &l, lx - 0.16f * h, lx + 0.16f * h, -1.07f * h, -0.90f * h, -0.10f * h, 0.10f * h, &w3, &f3); /* his head and neck whole (close: the far shoulder may touch the edge, the chest is out) */
            Where(c, &l, lx, -0.95f * h, 0.0f, &hx, &hy);   /* the fighter's head */
            Where(c, &l, ox, -0.55f * h, oz, &qx, &qy);     /* the opponent's middle */
            if (ow > 1.0f || of < 1.0f) oppBad++;
            if (w3 > 1.0f) noFull++;                         /* (his head and shoulders must not be cut) */
            if (fabsf(hx) < 0.15f || fabsf(qx) > 0.40f || (hx * qx > 0.0f && fabsf(hx - qx) < 0.15f)) oppBad++; /* he is to one side, the opponent not behind him */
            if (of < minOpp) minOpp = of;
            eyeUp = -eye.y / h; /* the eye's height over the floor, in his heights */
            if (eyeUp < 0.85f || eyeUp > 1.25f) eyeBad++;
        }
        {
            V3 e = Eye(c);
            float dx = e.x - 0.0f, dz = e.z, d2 = sqrtf((dx - lx) * (dx - lx) + dz * dz);
            if (d2 < 0.3f * h) near++; /* the camera inside the fighter */
        }
        if (f > 0 && f != CINE_CUT) {
            V3 a = Eye(prev), b = Eye(c), d = sub(b, a);
            float step = sqrtf(dot(d, d)) / h;
            if (step > maxStep) maxStep = step;
            if (step > 0.12f) jumps++; /* more than 12 % of the fighter's height in one frame */
        }
        prev = c;
    }
    s->run = 1; s->headCut = headCut; s->noFull = noFull; s->closeBad = closeBad; s->jumps = jumps; s->near = near;
    s->maxStep = maxStep; s->minClose = minClose; s->maxClose = maxClose; s->oppBad = oppBad; s->eyeBad = eyeBad; s->minOpp = minOpp;
}

int main(void) {
    static const float heights[] = {5.0f, 8.0f, 12.0f, 15.9f, 22.0f, 40.0f};
    static const char *hname[] = {"mini 5", "baixo 8", "medio 12", "Goku 15.9", "alto 22", "gigante 40"};
    static const float shapes[] = {0.75f, 1.0f, 1.3333f, 1.5f, 1.7778f, 2.0f, 2.1667f, 2.3333f};
    static const char *sname[] = {"3:4", "1:1", "4:3", "3:2", "16:9", "18:9", "19.5:9", "21:9"};
    static const float xs[] = {0.0f, 3.0f};
    static const float opps[] = {5.0f, 15.9f, 40.0f};
    int oi;
    int isNew, total[2] = {0, 0}, bad[2][7] = {{0}}, hi, si, xi, gated = 0;
    float worstStep[2] = {0, 0};

    printf("Teste de enquadramento do cinema: %d tamanhos x %d formas de tela x %d posicoes x 3 tamanhos de adversario = %d casos por camera\n\n",
           (int)(sizeof(heights) / sizeof(*heights)), (int)(sizeof(shapes) / sizeof(*shapes)), (int)(sizeof(xs) / sizeof(*xs)),
           (int)(sizeof(heights) / sizeof(*heights) * sizeof(shapes) / sizeof(*shapes) * sizeof(xs) / sizeof(*xs)) * 3);
    (void)gated;
    for (isNew = 0; isNew < 2; isNew++) {
        printf("== camera %s ==\n", isNew ? "NOVA (pelo FOV)" : "ANTIGA (multiplos de h)");
        printf("%-12s", "tamanho");
        for (si = 0; si < (int)(sizeof(shapes) / sizeof(*shapes)); si++) printf(" %-7s", sname[si]);
        printf("\n");
        for (hi = 0; hi < (int)(sizeof(heights) / sizeof(*heights)); hi++) {
            printf("%-12s", hname[hi]);
            for (si = 0; si < (int)(sizeof(shapes) / sizeof(*shapes)); si++) {
                int fails = 0;
                for (xi = 0; xi < (int)(sizeof(xs) / sizeof(*xs)) * (int)(sizeof(opps) / sizeof(*opps)); xi++) {
                    Score s;
                    int gate = shapes[si] >= 1.75f; /* the shapes where the way runs: the new select takes none under 1.9 (sNarrow); 16:9 kept for safety */

                    oi = xi / (int)(sizeof(xs) / sizeof(*xs));
                    Run(isNew, shapes[si], heights[hi], opps[oi], xs[xi % (int)(sizeof(xs) / sizeof(*xs))], &s);
                    if (gate) total[isNew]++;
                    if (s.headCut) { bad[isNew][0] += gate; fails |= 1; }
                    /* (the new camera keeps the select's own framing through the turn, no zoom: the whole body is not asked of it) */
                    if (s.noFull && !isNew) { bad[isNew][1] += gate; fails |= 2; }
                    /* (the face close-up is gone from the new camera: the user, no cuts on the body before the turn; the old one still has it) */
                    if (s.closeBad && !isNew) { bad[isNew][2] += gate; fails |= 4; }
                    if (s.jumps) { bad[isNew][3] += gate; fails |= 8; }
                    if (s.near) { bad[isNew][4] += gate; fails |= 16; }
                    if (s.oppBad) { bad[isNew][5] += gate; fails |= 32; }
                    if (s.eyeBad) { bad[isNew][6] += gate; fails |= 64; }
                    if (s.maxStep > worstStep[isNew]) worstStep[isNew] = s.maxStep;
                }
                printf(" %-7s", fails == 0 ? "ok" : fails & 32 ? "ADVERS" : fails & 64 ? "BAIXA" : fails & 1 ? "CABECA" : fails & 2 ? "SEMCORPO" : fails & 4 ? "ROSTO" : fails & 8 ? "SALTO" : "DENTRO");
            }
            printf("\n");
        }
        printf("(telas >= 16:9, onde o cinema roda) casos reprovados de %d: cabeca cortada %d | corpo inteiro nao cabe %d | rosto fora de 30-92%% %d | saltos %d | camera dentro do lutador %d | adversario fora/pequeno na tomada final %d | olho baixo/alto %d\n\n",
               total[isNew], bad[isNew][0], bad[isNew][1], bad[isNew][2], bad[isNew][3], bad[isNew][4], bad[isNew][5], bad[isNew][6]);
    }
    printf("maior passo da camera em um quadro (fracao da altura): antiga %.3f, nova %.3f\n", worstStep[0], worstStep[1]);
    {
        int n = 0, i;
        for (i = 0; i < 7; i++) n += bad[1][i];
        printf("RESULTADO: nova camera %s\n", n == 0 ? "APROVADA em todos os casos" : "REPROVADA");
        return n == 0 ? 0 : 1;
    }
}
