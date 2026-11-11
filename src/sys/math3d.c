#include "common.h"
#include "sys/math3d.h"

extern float sqrtf(float x);
extern float fabsf(float x);
extern float sinf(float x);
extern float cosf(float x);
extern float atan2f(float y, float x);
extern float Mathf_Acos(float x); /* acosf with the argument clamped to [-1, 1] */
extern float Mathf_Asin(float x); /* asinf with the argument clamped to [-1, 1] */

extern void Vec4_Copy(void *dst, void *src);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern float Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Mtx_StoreIdentity(Mtx44 *m);

/* Cubic Hermite spline on xyz: p0/p1 are the end points, t0/t1 the tangents, t in [0, 1]. */
void Vec3_Hermite(Vec4 *out, Vec4 *p0, Vec4 *p1, Vec4 *t0, Vec4 *t1, float t) {
    float h0 = (t - 1.0f) * (t - 1.0f) * (2.0f * t + 1.0f);
    float h1 = t * t * (3.0f - 2.0f * t);
    float h2 = (1.0f - t) * (1.0f - t) * t;
    float h3 = (t - 1.0f) * t * t;

    out->x = h0 * p0->x + h1 * p1->x + h2 * t0->x + h3 * t1->x;
    out->y = h0 * p0->y + h1 * p1->y + h2 * t0->y + h3 * t1->y;
    out->z = h0 * p0->z + h1 * p1->z + h2 * t0->z + h3 * t1->z;
}

/* Sets q to (0, 0, 0, 1). */
void Quat_SetIdentity(Quat *q) {
    q->x = 0.0f;
    q->y = 0.0f;
    q->z = 0.0f;
    q->w = 1.0f;
}

/* Rotation of `angle` radians about the axis (x, y, z), which need not be normalised; identity for a null axis. */
void Quat_FromAxisAngle(Quat *out, float x, float y, float z, float angle) {
    float len = sqrtf(x * x + y * y + z * z);

    if (len > 0.000001f) {
        float half = angle * 0.5f;
        float s = sinf(half);
        float inv = 1.0f / len;

        out->x = x * inv * s;
        out->y = y * inv * s;
        out->z = z * inv * s;
        out->w = cosf(half);
    } else {
        out->x = 0.0f;
        out->y = 0.0f;
        out->z = 0.0f;
        out->w = 1.0f;
    }
}

/* Shortest rotation taking direction `from` to direction `to`, scaled by t (1 = the full rotation). */
void Quat_FromVectors(Quat *out, Vec4 *from, Vec4 *to, float t) {
    Vec4 a;
    Vec4 b;
    Vec4 axis;
    float s;
    float inv;

    Vec3_Normalize(&a, from);
    Vec3_Normalize(&b, to);
    Vec3_Cross(&axis, &a, &b);
    s = sqrtf((Vec3_Dot(&a, &b) + 1.0f) * 2.0f);
    inv = 1.0f / s;
    out->x = axis.x * inv;
    out->y = axis.y * inv;
    out->z = axis.z * inv;
    out->w = s * 0.5f;
    if (t != 1.0f) {
        Quat_SlerpIdentity(out, out, t);
    }
}

/* Squared norm of the four components. */
float Quat_LengthSq(Quat *q) {
    return q->x * q->x + q->y * q->y + q->z * q->z + q->w * q->w;
}

/* Norm of the four components. */
float Quat_Length(Quat *q) {
    return sqrtf(Quat_LengthSq(q));
}

/* Slerp from the identity to q: the same axis with the angle scaled by t. */
void Quat_SlerpIdentity(Quat *out, Quat *q, float t) {
    float w;
    float angle;
    float s;

    w = fabsf(q->w);
    if (w > 1.0f) {
        w = 1.0f;
    }
    angle = Mathf_Acos(w);
    s = sinf(angle);
    if (s < 0.000001f) {
        out->x = 0.0f;
        out->y = 0.0f;
        out->z = 0.0f;
        out->w = 1.0f;
    } else {
        float inv = 1.0f / s;
        float ka = sinf(angle * (1.0f - t)) * inv;
        float kb = sinf(angle * t) * inv;

        out->x = q->x * kb;
        out->y = q->y * kb;
        out->z = q->z * kb;
        out->w = ka + q->w * kb;
    }
}

/* Rotation angle of q in radians, 2 * acos(w). */
float Quat_GetAngle(Quat *q) {
    float w = q->w;

    if (w < -1.0f) {
        w = -1.0f;
    }
    if (w > 1.0f) {
        w = 1.0f;
    }
    return Mathf_Acos(w) * 2.0f;
}

/* Conjugate: negated vector part. */
void Quat_Conjugate(Quat *out, Quat *q) {
    out->x = -q->x;
    out->y = -q->y;
    out->z = -q->z;
    out->w = q->w;
}

/* Inverse: conjugate divided by the squared norm. */
void Quat_Inverse(Quat *out, Quat *q) {
    float inv = 1.0f / Quat_LengthSq(q);

    out->x = -q->x * inv;
    out->y = -q->y * inv;
    out->z = -q->z * inv;
    out->w = q->w * inv;
}

/* Hamilton product out = a * b; out may alias either operand. */
void Quat_Mul(Quat *out, Quat *a, Quat *b) {
    Quat unused; /* never referenced, but it is what gives the function its 0x10 frame */
    float x, y, z, w;

    w = a->w * b->w - a->x * b->x - a->y * b->y - a->z * b->z;
    x = a->w * b->x + a->x * b->w + a->y * b->z - a->z * b->y;
    y = a->w * b->y - a->x * b->z + a->y * b->w + a->z * b->x;
    z = a->w * b->z + a->x * b->y - a->y * b->x + a->z * b->w;
    out->x = x;
    out->y = y;
    out->z = z;
    out->w = w;
}

/* Four-component dot product. */
static inline float Quat_Dot(Quat *a, Quat *b) {
    return a->x * b->x + a->y * b->y + a->z * b->z + a->w * b->w;
}

/* Spherical interpolation from a (t = 0) to b (t = 1) along the shorter arc. */
void Quat_Slerp(Quat *out, Quat *a, Quat *b, float t) {
    Quat qa;
    Quat qb;
    float dot;
    float c;
    float angle;
    float s;

    Vec4_Copy(&qa, a);
    Vec4_Copy(&qb, b);
    dot = Quat_Dot(&qa, &qb);
    if (dot < 0.0f) {
        qb.x = -qb.x;
        qb.y = -qb.y;
        qb.z = -qb.z;
        qb.w = -qb.w;
        c = Quat_Dot(&qa, &qb);
    } else {
        c = dot;
    }
    if (c > 1.0f) {
        c = 1.0f;
    }
    angle = Mathf_Acos(c);
    s = sinf(angle);
    if (s < 0.000001f) {
        out->x = qa.x;
        out->y = qa.y;
        out->z = qa.z;
        out->w = qa.w;
    } else {
        float inv = 1.0f / s;
        float ka = sinf(angle * (1.0f - t)) * inv;
        float kb = sinf(angle * t) * inv;

        out->x = qa.x * ka + qb.x * kb;
        out->y = qa.y * ka + qb.y * kb;
        out->z = qa.z * ka + qb.z * kb;
        out->w = qa.w * ka + qb.w * kb;
    }
}

/* out = r * q * r^-1: q expressed in the frame rotated by r. */
void Quat_ConjugateBy(Quat *out, Quat *q, Quat *r) {
    Quat inv;
    Quat tmp;

    Quat_Inverse(&inv, r);
    Quat_Mul(&tmp, q, &inv);
    Quat_Mul(&tmp, r, &tmp);
    out->x = tmp.x;
    out->y = tmp.y;
    out->z = tmp.z;
    out->w = tmp.w;
}

/* Copies q, but rebuilds it with rotation angle maxAngle when it turns further than that. */
void Quat_LimitAngle(Quat *out, Quat *q, float maxAngle) {
    if (fabsf(maxAngle) >= 3.14159265f) {
        Vec4_Copy(out, q);
        return;
    }
    if (fabsf(q->w) > 0.999999f) {
        Vec4_Copy(out, q);
        return;
    }
    if (cosf(maxAngle * 0.5f) < q->w) {
        Vec4_Copy(out, q);
        return;
    }
    Quat_FromAxisAngle(out, q->x, q->y, q->z, maxAngle);
}

/* out = q scaled by 1 / |out| (sic: the length is taken from out, so this is only right in place). */
void Quat_Normalize(Quat *out, Quat *q) {
    Quat tmp;
    float inv = 1.0f / Quat_Length(out);

    tmp.x = q->x * inv;
    tmp.y = q->y * inv;
    tmp.z = q->z * inv;
    tmp.w = q->w * inv;
    Vec4_Copy(out, &tmp);
}

/* "Smallest three" compression: drops the largest component (index in bits 60..63, made positive)
   and stores the other three as 20-bit fields. */
u64 Quat_Pack(Quat *q) {
    float v[4];
    s32 largest;
    s32 i;
    float best;
    u64 packed;

    Vec4_Copy(v, q);
    largest = 0;
    best = fabsf(v[0]);
    for (i = 1; i < 4; i++) {
        if (best < fabsf(v[i])) {
            best = fabsf(v[i]);
            largest = i;
        }
    }
    if (v[largest] < 0.0f) {
        v[0] = -v[0];
        v[1] = -v[1];
        v[2] = -v[2];
        v[3] = -v[3];
    }
    for (i = largest; i < 3; i++) {
        v[i] = v[i + 1];
    }
    packed = 0;
    for (i = 0; i < 3; i++) {
        s32 n = (v[i] * 0.70710677f + 0.5f) * 1048575.0f;

        if (n < 0) {
            n = 0;
        }
        if (n > 0xFFFFF) {
            n = 0xFFFFF;
        }
        packed |= (u64)n << (i * 20);
    }
    return packed | ((u64)largest << 60);
}

/* Inverse of Quat_Pack: the dropped component is rebuilt as sqrt(1 - sum of squares). */
void Quat_Unpack(Quat *out, u64 packed) {
    float v[4];
    s32 largest;
    s32 i;
    float sum;
    float w;

    largest = packed >> 60;
#ifdef PORT
    if (largest > 3) {
        extern int printf(const char *, ...);
        printf("bt4: invalid packed quaternion %08x%08x\n", (u32)(packed >> 32), (u32)packed);
        __builtin_trap();
    }
#endif
    for (i = 0; i < 3; i++) {
        sum = (u32)(packed >> (i * 20)) & 0xFFFFF;
        v[i] = (sum * (1.0f / 1048575.0f) - 0.5f) * 1.41421356f;
    }
    sum = 0.0f;
    for (i = 0; i < 3; i++) {
        sum += v[i] * v[i];
    }
    w = sqrtf(1.0f - sum);
    for (i = 3; i > largest; i--) {
        v[i] = v[i - 1];
    }
    v[largest] = w;
    Vec4_Copy(out, v);
}

/* Rotation matrix of a unit quaternion (identity elsewhere). */
void Quat_ToMtx(Mtx44 *out, Quat *q) {
    float x = q->x;
    float y = q->y;
    float z = q->z;
    float w = q->w;
    float xx = x * x;
    float yy = y * y;

    Mtx_StoreIdentity(out);
    out->m[0][0] = 1.0f - 2.0f * yy - 2.0f * (z * z);
    out->m[0][1] = 2.0f * x * y + 2.0f * w * z;
    out->m[0][2] = 2.0f * x * z - 2.0f * w * y;
    out->m[1][0] = 2.0f * x * y - 2.0f * w * z;
    out->m[1][1] = 1.0f - 2.0f * xx - 2.0f * (z * z);
    out->m[1][2] = 2.0f * y * z + 2.0f * w * x;
    out->m[2][0] = 2.0f * x * z + 2.0f * w * y;
    out->m[2][1] = 2.0f * y * z - 2.0f * w * x;
    out->m[2][2] = 1.0f - 2.0f * xx - 2.0f * yy;
}

/* Quaternion from Euler angles (radians about x, y, z): qy * (qx * qz). */
void Quat_FromEuler(Quat *out, Vec4 *angles) {
    Quat qx;
    Quat qy;
    Quat qz;

    Quat_FromAxisAngle(&qx, 1.0f, 0.0f, 0.0f, angles->x);
    Quat_FromAxisAngle(&qy, 0.0f, 1.0f, 0.0f, angles->y);
    Quat_FromAxisAngle(&qz, 0.0f, 0.0f, 1.0f, angles->z);
    Quat_Mul(out, &qx, &qz);
    Quat_Mul(out, &qy, out);
}

/* Euler angles of a rotation matrix (the inverse of Quat_FromEuler + Quat_ToMtx), with the two gimbal-lock cases. */
void Mtx_ToEuler(Vec4 *out, Mtx44 *m) {
    float s = -m->m[2][1];

    if (s < -1.0f) {
        s = -1.0f;
    }
    if (s > 1.0f) {
        s = 1.0f;
    }
    out->x = Mathf_Asin(s);
    if (m->m[2][1] < 0.9999f) {
        if (m->m[2][1] > -0.9999f) {
            out->y = atan2f(m->m[2][0], m->m[2][2]);
            out->z = atan2f(m->m[0][1], m->m[1][1]);
        } else {
            out->y = atan2f(m->m[1][0], m->m[0][0]);
            out->z = 0.0f;
        }
    } else {
        out->y = -atan2f(m->m[1][0], m->m[0][0]);
        out->z = 0.0f;
    }
    out->w = 0.0f;
}

/* Euler angles of a quaternion, through the matrix. */
void Quat_ToEuler(Vec4 *out, Quat *q) {
    Mtx44 m;

    Quat_ToMtx(&m, q);
    Mtx_ToEuler(out, &m);
}

/* Quaternion of a rotation matrix (trace method, falling back to the largest diagonal element). */
void Quat_FromMtx(Quat *out, Mtx44 *m) {
    float tr = m->m[0][0] + 1.0f + m->m[1][1] + m->m[2][2];
    float s;

    if (tr > 0.0001f) {
        s = 0.5f / sqrtf(tr);
        out->w = 0.25f / s;
        out->x = (m->m[1][2] - m->m[2][1]) * s;
        out->y = (m->m[2][0] - m->m[0][2]) * s;
        out->z = (m->m[0][1] - m->m[1][0]) * s;
    } else if (m->m[0][0] > m->m[1][1] && m->m[0][0] > m->m[2][2]) {
        s = sqrtf(m->m[0][0] + 1.0f - m->m[1][1] - m->m[2][2]) * 2.0f;
        out->x = s * 0.25f;
        out->y = (m->m[1][0] + m->m[0][1]) / s;
        out->z = (m->m[2][0] + m->m[0][2]) / s;
        out->w = (m->m[1][2] - m->m[2][1]) / s;
    } else if (m->m[1][1] > m->m[2][2]) {
        s = sqrtf(m->m[1][1] + 1.0f - m->m[0][0] - m->m[2][2]) * 2.0f;
        out->x = (m->m[1][0] + m->m[0][1]) / s;
        out->y = s * 0.25f;
        out->z = (m->m[2][1] + m->m[1][2]) / s;
        out->w = (m->m[2][0] - m->m[0][2]) / s;
    } else {
        s = sqrtf(m->m[2][2] + 1.0f - m->m[0][0] - m->m[1][1]) * 2.0f;
        out->x = (m->m[2][0] + m->m[0][2]) / s;
        out->y = (m->m[2][1] + m->m[1][2]) / s;
        out->z = s * 0.25f;
        out->w = (m->m[0][1] - m->m[1][0]) / s;
    }
}
