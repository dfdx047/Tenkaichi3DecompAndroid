/* Local camera mathematics. No game globals, input APIs or rollback state. */
#ifndef BT4_SPECTATOR_CAMERA_H
#define BT4_SPECTATOR_CAMERA_H
#ifdef BT4_SPECTATOR_GAME_MATH
/* The ptr32 translator defines PS2 math aliases before parsing game files.
 * glibc's SIMD declaration macros cannot be used under those aliases. */
extern float sinf(float),cosf(float),sqrtf(float),expf(float),fabsf(float);
extern float atan2f(float,float),fmaxf(float,float);
#else
#include <math.h>
#endif

typedef struct SpecInput { float right, forward, up, yaw, pitch, zoom, boost; } SpecInput;
typedef struct SpecCamera {
    float eye[4], rotation[4], focus[3], distance, zoom, yaw, pitch;
    int live, mode, target;
} SpecCamera;

static float spec_clamp(float n,float lo,float hi) { return !__builtin_isfinite(n)?lo:n<lo?lo:n>hi?hi:n; }
static float spec_angle(float n) { return atan2f(sinf(n),cosf(n)); }
static void spec_basis(float yaw,float pitch,float right[3],float up[3],float forward[3]) {
    float sy=sinf(yaw),cy=cosf(yaw),sp=sinf(pitch),cp=cosf(pitch);
    right[0]=cy;right[1]=0;right[2]=-sy;
    forward[0]=sy*cp;forward[1]=-sp;forward[2]=cy*cp;
    up[0]=-sy*sp;up[1]=-cp;up[2]=-cy*sp;
}
static float spec_dot(const float a[3],const float b[3]) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }

/* targets have xyz at their body centres and a conservative sphere radius in w. */
static int spec_step(SpecCamera *s,int mode,int target,int speed,float zoom,float dt,
                     const float native_eye[4],const float native_rot[4],
                     const float *targets,int count,SpecInput in,float tan_y,float aspect) {
    float right[3],up[3],forward[3],goal[3],focus[3]={0,0,0},required=35,rate;
    int i,k,reset;
    if(mode<1 || mode>3 || count<1 || count>10) { s->live=0;return 0; }
    dt=spec_clamp(dt,0,.05f);zoom=spec_clamp(zoom,100,300);
    target=target<0?0:target>=count?count-1:target;
    reset=!s->live || s->mode!=mode || (mode==2 && s->target!=target);
    if(reset) {
        *s=(SpecCamera){0};s->live=1;s->mode=mode;s->target=target;
        for(k=0;k<4;k++){s->eye[k]=native_eye[k];s->rotation[k]=native_rot[k];}
        s->yaw=mode==1?native_rot[1]:.65f;s->pitch=mode==1?native_rot[0]:-.18f;
        s->distance=70;s->zoom=zoom;
    }
    s->zoom=spec_clamp(zoom+spec_clamp(in.zoom,-1,1)*100*dt,100,300);
    s->yaw=spec_angle(s->yaw+spec_clamp(in.yaw+(mode>1?in.right:0),-1,1)*1.8f*dt);
    s->pitch=spec_clamp(s->pitch+spec_clamp(in.pitch-(mode>1?in.up:0),-1,1)*1.4f*dt,-1.45f,1.45f);
    spec_basis(s->yaw,s->pitch,right,up,forward);
    rate=spec_clamp((float)speed,25,400)/100;
    if(mode==1) {
        float movement[3],length;
        for(k=0;k<3;k++)movement[k]=right[k]*spec_clamp(in.right,-1,1)+forward[k]*spec_clamp(in.forward,-1,1)+up[k]*spec_clamp(in.up,-1,1);
        length=sqrtf(spec_dot(movement,movement));
        if(length>1)for(k=0;k<3;k++)movement[k]/=length;
        for(k=0;k<3;k++)s->eye[k]+=movement[k]*100*rate*(in.boost>0?3:1)*dt;
    } else {
        if(mode==2)for(k=0;k<3;k++)focus[k]=targets[target*4+k];
        else for(i=0;i<count;i++)for(k=0;k<3;k++)focus[k]+=targets[i*4+k]/count;
        if(reset)for(k=0;k<3;k++)s->focus[k]=focus[k];
        for(k=0;k<3;k++)s->focus[k]+=(focus[k]-s->focus[k])*(1-expf(-16*dt));
        if(mode==2) {
            s->distance=spec_clamp(s->distance-spec_clamp(in.forward,-1,1)*80*rate*dt,15,800);
            required=fmaxf(s->distance,targets[target*4+3]*3);
        } else {
            float ty=spec_clamp(tan_y,.1f,2)*100/s->zoom;
            float tx=ty*spec_clamp(aspect,.75f,4);
            for(i=0;i<count;i++) {
                float delta[3],depth,r=spec_clamp(targets[i*4+3],1,1000);
                for(k=0;k<3;k++)delta[k]=targets[i*4+k]-s->focus[k];
                depth=spec_dot(delta,forward);
                required=fmaxf(required,fmaxf((fabsf(spec_dot(delta,right))+r)/(tx*.75f)-depth,
                                             (fabsf(spec_dot(delta,up))+r)/(ty*.75f)-depth)+r);
            }
            /* Expand immediately to retain fighters; contract gradually. */
            s->distance=required>s->distance?required:s->distance+(required-s->distance)*(1-expf(-3*dt));
            required=s->distance;
        }
        for(k=0;k<3;k++)goal[k]=s->focus[k]-forward[k]*required;
        for(k=0;k<3;k++)s->eye[k]=goal[k];
    }
    s->eye[3]=1;s->rotation[0]=s->pitch;s->rotation[1]=s->yaw;s->rotation[2]=0;s->rotation[3]=1;
    return 1;
}

/* Two-sided finite segment / triangle intersection, with the nearest fraction. */
static int spec_triangle(const float *from,const float *to,const float *a,const float *b,const float *c,float *fraction) {
    float e1[3],e2[3],d[3],p[3],q[3],t[3],det,u,v,f;int k;
    for(k=0;k<3;k++){e1[k]=b[k]-a[k];e2[k]=c[k]-a[k];d[k]=to[k]-from[k];t[k]=from[k]-a[k];}
    p[0]=d[1]*e2[2]-d[2]*e2[1];p[1]=d[2]*e2[0]-d[0]*e2[2];p[2]=d[0]*e2[1]-d[1]*e2[0];
    det=spec_dot(e1,p);if(fabsf(det)<1e-6f)return 0;
    u=spec_dot(t,p)/det;if(u<0 || u>1)return 0;
    q[0]=t[1]*e1[2]-t[2]*e1[1];q[1]=t[2]*e1[0]-t[0]*e1[2];q[2]=t[0]*e1[1]-t[1]*e1[0];
    v=spec_dot(d,q)/det;if(v<0 || u+v>1)return 0;
    f=spec_dot(e2,q)/det;if(f<.0001f || f>1 || f>=*fraction)return 0;
    *fraction=f;return 1;
}
#endif
