#ifndef GS_CAMERA_INTERP_H
#define GS_CAMERA_INTERP_H
#include <math.h>
#include <string.h>

/* Perspective camera interpolation in pose space. Taking fractional matrix
 * powers in projected GS coordinates is ill-conditioned, particularly for
 * almost stationary cameras and large principal-point/depth coefficients. */
typedef struct GsCameraPose {
    double eye[3], q[4], fx, fy, cx, cy, zs, zo, scale;
} GsCameraPose;

static double GsCamera_Dot(const double *a,const double *b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
static int GsCamera_Extract(const double *m,GsCameraPose *p) {
    double r[3][3],x[3],y[3],z[3],cross[3],s,trace,k,coord[3];
    int i,j;
    for(i=0;i<16;i++)if(!isfinite(m[i]))return 0;
    for(i=0;i<3;i++)z[i]=m[i*4+3];
    s=sqrt(GsCamera_Dot(z,z));
    if(!(s>1e-12&&s<1e12))return 0; /* orthographic/degenerate: caller uses linear fallback */
    for(i=0;i<3;i++){r[2][i]=z[i]/s;x[i]=m[i*4]/s;y[i]=m[i*4+1]/s;}
    p->cx=GsCamera_Dot(x,r[2]);p->cy=GsCamera_Dot(y,r[2]);
    for(i=0;i<3;i++){x[i]-=p->cx*r[2][i];y[i]-=p->cy*r[2][i];}
    p->fx=sqrt(GsCamera_Dot(x,x));p->fy=sqrt(GsCamera_Dot(y,y));
    if(!(p->fx>1e-8&&p->fy>1e-8))return 0;
    for(i=0;i<3;i++){r[0][i]=x[i]/p->fx;r[1][i]=y[i]/p->fy;}
    if(fabs(GsCamera_Dot(r[0],r[1]))>1e-4)return 0;
    cross[0]=r[0][1]*r[1][2]-r[0][2]*r[1][1];
    cross[1]=r[0][2]*r[1][0]-r[0][0]*r[1][2];
    cross[2]=r[0][0]*r[1][1]-r[0][1]*r[1][0];
    k=GsCamera_Dot(cross,r[2]);
    if(fabs(fabs(k)-1)>1e-4)return 0;
    if(k<0){p->fy=-p->fy;for(i=0;i<3;i++)r[1][i]=-r[1][i];}
    coord[2]=-m[15]/s;
    coord[0]=-(m[12]/s-p->cx*m[15]/s)/p->fx;
    coord[1]=-(m[13]/s-p->cy*m[15]/s)/p->fy;
    for(i=0;i<3;i++){
        p->eye[i]=r[0][i]*coord[0]+r[1][i]*coord[1]+r[2][i]*coord[2];
        if(!isfinite(p->eye[i])||fabs(p->eye[i])>1e9)return 0;
        z[i]=m[i*4+2]/s;
    }
    p->zs=GsCamera_Dot(z,r[2]);p->zo=m[14]/s-p->zs*m[15]/s;p->scale=s;
    for(i=0;i<3;i++)if(fabs(z[i]-p->zs*r[2][i])>1e-5*(1+fabs(p->zs)))return 0;
    trace=r[0][0]+r[1][1]+r[2][2];
    if(trace>0){
        k=sqrt(trace+1)*2;p->q[3]=k*.25;
        p->q[0]=(r[2][1]-r[1][2])/k;p->q[1]=(r[0][2]-r[2][0])/k;p->q[2]=(r[1][0]-r[0][1])/k;
    }else{
        i=r[1][1]>r[0][0]?1:0;if(r[2][2]>r[i][i])i=2;
        j=(i+1)%3;int n=(i+2)%3;
        k=sqrt(fmax(0,1+r[i][i]-r[j][j]-r[n][n]))*2;
        if(k<1e-12)return 0;
        p->q[i]=k*.25;p->q[j]=(r[j][i]+r[i][j])/k;p->q[n]=(r[n][i]+r[i][n])/k;
        p->q[3]=(r[n][j]-r[j][n])/k;
    }
    return 1;
}
static void GsCamera_Matrix(const GsCameraPose *p,double *m) {
    double x=p->q[0],y=p->q[1],z=p->q[2],w=p->q[3];
    double r[3][3]={{1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)},
                    {2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)},
                    {2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)}};
    int i;
    for(i=0;i<3;i++){
        m[i*4]=(p->fx*r[0][i]+p->cx*r[2][i])*p->scale;
        m[i*4+1]=(p->fy*r[1][i]+p->cy*r[2][i])*p->scale;
        m[i*4+2]=p->zs*r[2][i]*p->scale;m[i*4+3]=r[2][i]*p->scale;
    }
    for(i=0;i<4;i++)m[12+i]=-(m[i]*p->eye[0]+m[4+i]*p->eye[1]+m[8+i]*p->eye[2]);
    m[14]+=p->zo*p->scale;
}
static int GsCamera_Blend(const double *previous,const double *current,double t,double *out) {
    GsCameraPose a,b,p;
    double dot=0,wa,wb,n=0;
    int i;
    if(!isfinite(t)||t<0||t>1)return 0;
    if(!GsCamera_Extract(previous,&a)||!GsCamera_Extract(current,&b)||a.fy*b.fy<=0)return 0;
    if(t==0){memcpy(out,previous,16*sizeof(double));return 1;}
    if(t==1||memcmp(previous,current,16*sizeof(double))==0){memcpy(out,current,16*sizeof(double));return 1;}
    for(i=0;i<4;i++)dot+=a.q[i]*b.q[i];
    if(dot<0){dot=-dot;for(i=0;i<4;i++)b.q[i]=-b.q[i];}
    dot=fmin(1,fmax(0,dot));
    wa=1-t;wb=t;
    if(dot<.9995){double angle=acos(dot),den=sin(angle);wa=sin((1-t)*angle)/den;wb=sin(t*angle)/den;}
    for(i=0;i<4;i++){p.q[i]=a.q[i]*wa+b.q[i]*wb;n+=p.q[i]*p.q[i];}
    if(!(n>1e-12))return 0;
    n=sqrt(n);for(i=0;i<4;i++)p.q[i]/=n;
    for(i=0;i<3;i++)p.eye[i]=a.eye[i]+(b.eye[i]-a.eye[i])*t;
#define GS_CAMERA_LERP(field) p.field=a.field+(b.field-a.field)*t
    GS_CAMERA_LERP(fx);GS_CAMERA_LERP(fy);GS_CAMERA_LERP(cx);GS_CAMERA_LERP(cy);
    GS_CAMERA_LERP(zs);GS_CAMERA_LERP(zo);GS_CAMERA_LERP(scale);
#undef GS_CAMERA_LERP
    GsCamera_Matrix(&p,out);
    for(i=0;i<16;i++)if(!isfinite(out[i]))return 0;
    return 1;
}
#endif
