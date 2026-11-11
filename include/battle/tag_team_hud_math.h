/* GPL-3.0-or-later. Geometry shared by the native HUD and analytic tests.
 * Matrices use the engine's row-vector convention. No renderer or game state. */
#ifndef TAG_TEAM_HUD_MATH_H
#define TAG_TEAM_HUD_MATH_H
extern float sqrtf(float);
static float ThAbs(float x) { return x<0?-x:x; }
static float ThRadarRange(float current,float distance) {
    while(distance>current*0.85f && current<2400)current*=2;
    while(distance<current*0.32f && current>75)current*=0.5f;
    return current;
}
static int ThBasis(const float *v,float *b) {
    float n=sqrtf(v[2]*v[2]+v[10]*v[10]),fx,fz;
    if(n>=0.0001f){b[2]=v[2]/n;b[3]=v[10]/n;b[0]=b[3];b[1]=-b[2];return 1;}
    n=sqrtf(v[0]*v[0]+v[8]*v[8]);
    if(n<0.0001f)return 0;
    b[0]=v[0]/n;b[1]=v[8]/n;
    fx=-b[1];fz=b[0];
    if(fx*v[2]+fz*v[10]<0){fx=-fx;fz=-fz;}
    b[2]=fx;b[3]=fz;return 1;
}
static int ThRadar(const float *own,const float *other,const float *b,float range,float radius,float *out) {
    float dx=other[0]-own[0],dz=other[2]-own[2],n;
    out[0]=(dx*b[0]+dz*b[1])*radius/range;
    out[1]=-(dx*b[2]+dz*b[3])*radius/range;
    out[2]=sqrtf(dx*dx+dz*dz);out[3]=own[1]-other[1]; /* positive = above */
    n=sqrtf(out[0]*out[0]+out[1]*out[1]);
    if(n>radius){out[0]*=radius/n;out[1]*=radius/n;return 1;}
    return 0;
}
static int ThProject(const float *m,const float *p,float nearZ,float *out) {
    float c[4];int j;
    for(j=0;j<4;++j)c[j]=p[0]*m[j]+p[1]*m[4+j]+p[2]*m[8+j]+m[12+j];
    out[2]=c[3];
    out[0]=256;out[1]=224;
    if(c[3]<=nearZ)return 0;
    out[0]=c[0]/c[3]-1792;out[1]=c[1]/c[3]-1824;
    return 1;
}
static void ThRear(const float *view,const float *p,float *out) {
    /* Behind the eye, perspective division reverses left/right. Use azimuth
     * about the CAMERA origin instead; radar bearings use the PLAYER origin. */
    out[0]=p[0]*view[0]+p[1]*view[4]+p[2]*view[8]+view[12];
    out[1]=-(p[0]*view[2]+p[1]*view[6]+p[2]*view[10]+view[14]);
    if(ThAbs(out[0])+ThAbs(out[1])<0.0001f){
        out[0]=0;out[1]=p[0]*view[1]+p[1]*view[5]+p[2]*view[9]+view[13];
        if(ThAbs(out[1])<0.0001f)out[1]=1;
    }
}
static void ThRay(float cx,float cy,float dx,float dy,float x0,float y0,float x1,float y1,float *p) {
    float t=1.0e20f,q;
    if(ThAbs(dx)+ThAbs(dy)<0.0001f){dx=0;dy=1;}
    if(dx>0){q=(x1-cx)/dx;if(q<t)t=q;}else if(dx<0){q=(x0-cx)/dx;if(q<t)t=q;}
    if(dy>0){q=(y1-cy)/dy;if(q<t)t=q;}else if(dy<0){q=(y0-cy)/dy;if(q<t)t=q;}
    p[0]=cx+dx*t;p[1]=cy+dy*t;
}
/* Trim a centre-to-edge ray at its entry into an occupied HUD rectangle.
 * It stays on the same bearing instead of sliding the marker sideways. */
static void ThAvoid(float cx,float cy,float x0,float y0,float x1,float y1,float *p) {
    float dx=p[0]-cx,dy=p[1]-cy,lo=0,hi=1,a,b,tmp,n;
    if(ThAbs(dx)<0.0001f){if(cx<x0||cx>x1)return;}
    else {a=(x0-cx)/dx;b=(x1-cx)/dx;if(a>b){tmp=a;a=b;b=tmp;}if(a>lo)lo=a;if(b<hi)hi=b;}
    if(ThAbs(dy)<0.0001f){if(cy<y0||cy>y1)return;}
    else {a=(y0-cy)/dy;b=(y1-cy)/dy;if(a>b){tmp=a;a=b;b=tmp;}if(a>lo)lo=a;if(b<hi)hi=b;}
    if(hi<lo || hi<0 || lo>1 || lo<=0)return;
    n=sqrtf(dx*dx+dy*dy);lo-=2/(n>0.001f?n:1);if(lo<0)lo=0;
    p[0]=cx+dx*lo;p[1]=cy+dy*lo;
}
#endif
