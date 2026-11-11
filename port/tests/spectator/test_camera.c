#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../src/gs/spectator_camera.h"

static const float eye[4]={0,-20,-100,1},rotation[4]={0,0,0,1};
static void containment(SpecCamera *s,const float *targets,int count,float aspect) {
    float right[3],up[3],forward[3];int i,k;
    spec_basis(s->yaw,s->pitch,right,up,forward);
    for(i=0;i<count;i++) {
        float d[3],z,r=targets[i*4+3];for(k=0;k<3;k++)d[k]=targets[i*4+k]-s->eye[k];
        z=spec_dot(d,forward);assert(z>r);
        assert((fabsf(spec_dot(d,right))+r)/(.517f*aspect*100/s->zoom)<z*.80f);
        assert((fabsf(spec_dot(d,up))+r)/(.517f*100/s->zoom)<z*.80f);
    }
}
int main(void) {
    SpecCamera s={0},fast={0},timed={0};SpecInput input={0};int i,k;
    float targets[40]={-20,-10,0,10,20,-10,0,10},savedEye[4],savedRotation[4];
    memcpy(savedEye,eye,16);memcpy(savedRotation,rotation,16);
    assert(!spec_step(&s,0,0,100,100,.016f,eye,rotation,targets,2,input,.517f,1.777f));
    input.forward=1;
    for(i=0;i<60;i++)assert(spec_step(&s,1,0,100,100,1.f/60,eye,rotation,targets,2,input,.517f,1.777f));
    assert(fabsf(s.eye[2])<.001f);
    for(i=0;i<120;i++)spec_step(&timed,1,0,100,100,1.f/120,eye,rotation,targets,2,input,.517f,1.777f);
    assert(fabsf(timed.eye[2]-s.eye[2])<.001f);
    for(i=0;i<60;i++)spec_step(&fast,1,0,400,100,1.f/60,eye,rotation,targets,2,input,.517f,1.777f);
    assert(fabsf(fast.eye[2]-300)<.001f);
    memset(&input,0,sizeof(input));input.zoom=1;
    spec_step(&s,1,0,100,299,.05f,eye,rotation,targets,2,input,.517f,1.777f);assert(s.zoom==300);
    input.zoom=-1;spec_step(&s,1,0,100,100,.05f,eye,rotation,targets,2,input,.517f,1.777f);assert(s.zoom==100);
    memset(&input,0,sizeof(input));memset(&s,0,sizeof(s));
    spec_step(&s,2,1,100,100,.016f,eye,rotation,targets,2,input,.517f,1.777f);
    assert(s.target==1 && fabsf(s.focus[0]-20)<.001f);
    targets[4]+=100;
    for(i=0;i<120;i++)spec_step(&s,2,1,100,100,1.f/60,eye,rotation,targets,2,input,.517f,1.777f);
    assert(fabsf(s.focus[0]-120)<.001f);
    spec_step(&s,2,0,100,100,.016f,eye,rotation,targets,2,input,.517f,1.777f);assert(s.focus[0]==targets[0]);
    for(i=0;i<10;i++){targets[i*4]=i*80.f;targets[i*4+1]=-i*20.f;targets[i*4+2]=(i%2)*100.f;targets[i*4+3]=10+i;}
    for(k=0;k<3;k++) {
        float aspect=k==0?1.333f:k==1?1.777f:2.333f;memset(&s,0,sizeof(s));
        for(i=0;i<240;i++) {
            int actor;for(actor=0;actor<10;actor++)targets[actor*4]+=sinf((float)i)*5;
            spec_step(&s,3,0,100,150,1.f/60,eye,rotation,targets,10,input,.517f,aspect);
            containment(&s,targets,10,aspect);
            assert(isfinite(s.eye[0]) && isfinite(s.eye[1]) && isfinite(s.eye[2]));
        }
    }
    {
        float a[3]={-1,-1,0},b[3]={1,-1,0},c[3]={0,1,0},from[3]={0,0,-3},to[3]={0,0,3},fraction=1;
        assert(spec_triangle(from,to,a,b,c,&fraction));assert(fabsf(fraction-.5f)<.001f);
        fraction=1;assert(spec_triangle(to,from,a,b,c,&fraction));
        from[0]=to[0]=10;fraction=1;assert(!spec_triangle(from,to,a,b,c,&fraction));
    }
    assert(!memcmp(eye,savedEye,16) && !memcmp(rotation,savedRotation,16));
    puts("PASS: four modes; frame-rate independent movement; 4x speed; zoom limits; target tracking/switching; 10-fighter framing at 4:3, 16:9 and 21:9; two-sided collision rays; unchanged native pose.");
    return 0;
}
