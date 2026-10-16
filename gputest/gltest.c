#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

static int fails;
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static void res(const char*n,int ok,const char*fmt,...){
    char b[200]; va_list a; __builtin_va_start(a,fmt); vsnprintf(b,sizeof b,fmt,a); __builtin_va_end(a);
    printf("%-10s %s  %s\n",n,ok?"PASS":"FAIL",b); if(!ok)fails++;
}
static int glerr(const char*w){GLenum e=glGetError();if(e){printf("GL error 0x%x at %s\n",e,w);return 1;}return 0;}
static GLuint prog(const char*vs,const char*fs){
    GLuint p=glCreateProgram(); const char*src[2]={vs,fs}; GLenum ty[2]={GL_VERTEX_SHADER,GL_FRAGMENT_SHADER};
    for(int i=0;i<2;i++){GLuint s=glCreateShader(ty[i]);glShaderSource(s,1,&src[i],0);glCompileShader(s);GLint ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
        if(!ok){char l[2048];glGetShaderInfoLog(s,sizeof l,0,l);printf("shader error: %s\n",l);exit(2);}glAttachShader(p,s);}
    glLinkProgram(p);GLint ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){char l[2048];glGetProgramInfoLog(p,sizeof l,0,l);printf("link error: %s\n",l);exit(2);}return p;
}

static void t_compute(void){
    const char*cs="#version 430\nlayout(local_size_x=64) in;\nlayout(std430,binding=0) buffer B{uint d[];};\n"
    "void main(){uint i=gl_GlobalInvocationID.x;d[i]=(i*i)%100003u;}";
    GLuint s=glCreateShader(GL_COMPUTE_SHADER);glShaderSource(s,1,&cs,0);glCompileShader(s);GLint ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){char l[2048];glGetShaderInfoLog(s,sizeof l,0,l);res("compute",0,"compile: %s",l);return;}
    GLuint p=glCreateProgram();glAttachShader(p,s);glLinkProgram(p);
    size_t n=4u<<20; GLuint b;glGenBuffers(1,&b);glBindBuffer(GL_SHADER_STORAGE_BUFFER,b);
    glBufferData(GL_SHADER_STORAGE_BUFFER,n*4,0,GL_DYNAMIC_READ);glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,b);
    glUseProgram(p); double best=1e9;
    for(int r=0;r<3;r++){double t=now();glDispatchCompute(n/64,1,1);glMemoryBarrier(GL_ALL_BARRIER_BITS);glFinish();t=now()-t;if(t<best)best=t;}
    uint32_t*m=glMapBufferRange(GL_SHADER_STORAGE_BUFFER,0,n*4,GL_MAP_READ_BIT);
    if(!m){res("compute",0,"map failed 0x%x",glGetError());return;}
    size_t bad=0;for(size_t i=0;i<n;i++)if(m[i]!=(uint32_t)((uint32_t)i*(uint32_t)i%100003u))bad++;
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    res("compute",!bad&&!glerr("compute"),"%.3f ms for %zu elems, mismatches=%zu",best*1e3,n,bad);
}

static unsigned char *img;
static int near(unsigned char*p,int r,int g,int b){return abs(p[0]-r)<=2&&abs(p[1]-g)<=2&&abs(p[2]-b)<=2;}
static void t_render(void){
    const int W=1024;
    GLuint fbo,rb,dummy;glGenFramebuffers(1,&fbo);glGenRenderbuffers(1,&rb);
    glBindRenderbuffer(GL_RENDERBUFFER,rb);glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA8,W,W);
    glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,rb);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){res("render",0,"fbo incomplete");return;}
    glGenVertexArrays(1,&dummy);glBindVertexArray(dummy);
    glViewport(0,0,W,W);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    GLuint pc=prog("#version 430\nlayout(location=0) in vec2 p;layout(location=1) in vec3 c;out vec3 vc;void main(){vc=c;gl_Position=vec4(p,0,1);}",
                   "#version 430\nin vec3 vc;out vec4 o;void main(){o=vec4(vc,1);}");
    float tri[]={ -0.9f,-0.9f,1,0,0, -0.5f,-0.9f,1,0,0, -0.7f,-0.5f,1,0,0,
                   0.5f,-0.9f,0,1,0,  0.9f,-0.9f,0,1,0,  0.7f,-0.5f,0,1,0,
                  -0.2f, 0.5f,0,0,1,  0.2f, 0.5f,0,0,1,  0.0f, 0.9f,0,0,1 };
    GLuint vb;glGenBuffers(1,&vb);glBindBuffer(GL_ARRAY_BUFFER,vb);glBufferData(GL_ARRAY_BUFFER,sizeof tri,tri,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,0,20,(void*)0);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,3,GL_FLOAT,0,20,(void*)8);
    glUseProgram(pc);glDrawArrays(GL_TRIANGLES,0,9);
    GLuint tex;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);
    unsigned char t[16];for(int i=0;i<4;i++){int top=i<2;unsigned char*p=t+i*4;p[0]=(i%2)?255:0;p[1]=(i%2)?255:0;p[2]=top?0:255;p[3]=255;}
    unsigned char texd[4][4]={{255,255,0,255},{0,255,255,255},{255,0,255,255},{255,255,255,255}};
    (void)t;
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,texd);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    GLuint pt=prog("#version 430\nlayout(location=0) in vec2 p;layout(location=1) in vec2 u;out vec2 vu;void main(){vu=u;gl_Position=vec4(p,0,1);}",
                   "#version 430\nin vec2 vu;uniform sampler2D s;out vec4 o;void main(){o=texture(s,vu);}");
    float quad[]={ -0.4f,-0.4f,0,0, 0.4f,-0.4f,1,0, 0.4f,0.0f,1,1, -0.4f,-0.4f,0,0, 0.4f,0.0f,1,1, -0.4f,0.0f,0,1 };
    GLuint qb;glGenBuffers(1,&qb);glBindBuffer(GL_ARRAY_BUFFER,qb);glBufferData(GL_ARRAY_BUFFER,sizeof quad,quad,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,0,16,(void*)0);glVertexAttribPointer(1,2,GL_FLOAT,0,16,(void*)8);glDisableVertexAttribArray(1);glEnableVertexAttribArray(1);
    glUseProgram(pt);glDrawArrays(GL_TRIANGLES,0,6);
    img=malloc(W*W*4);glReadPixels(0,0,W,W,GL_RGBA,GL_UNSIGNED_BYTE,img);
    #define PX(x,y) (img+(((y)*W)+(x))*4)
    int ok=1,tot=0;
    struct{int x,y,r,g,b;const char*n;}chk[]={
        {153,118,255,0,0,"red tri"},{870,118,0,255,0,"green tri"},{512,836,0,0,255,"blue tri"},{10,10,0,0,0,"bg"},{900,900,0,0,0,"bg2"},
        {410,358,255,255,0,"tex00"},{614,358,0,255,255,"tex10"},{410,461,255,0,255,"tex01"},{614,461,255,255,255,"tex11"},
    };
    for(unsigned i=0;i<sizeof chk/sizeof*chk;i++){
        unsigned char*p=PX(chk[i].x,chk[i].y);
        int m=near(p,chk[i].r,chk[i].g,chk[i].b);tot++;
        if(!m){printf("           pixel '%s' (%d,%d) = %d,%d,%d expected %d,%d,%d\n",chk[i].n,chk[i].x,chk[i].y,p[0],p[1],p[2],chk[i].r,chk[i].g,chk[i].b);ok=0;}
    }
    int qok=1;
    FILE*f=fopen("render.ppm","wb");
    if(f){fprintf(f,"P6\n%d %d\n255\n",W,W);for(int y=W-1;y>=0;y--)for(int x=0;x<W;x++)fwrite(PX(x,y),1,3,f);fclose(f);}
    res("render",ok&&!glerr("render"),"%d tri/bg pixel checks, textured quad texels incl., wrote render.ppm",tot);

    GLuint pf=prog("#version 430\nlayout(location=0) in vec2 p;void main(){gl_Position=vec4(p,0,1);}",
                   "#version 430\nuniform float a;out vec4 o;void main(){o=vec4(a,0.5,0.25,1);}");
    float fq[]={-1,-1,1,-1,1,1,-1,-1,1,1,-1,1};
    glBufferData(GL_ARRAY_BUFFER,sizeof fq,fq,GL_STATIC_DRAW);glDisableVertexAttribArray(1);glVertexAttribPointer(0,2,GL_FLOAT,0,8,0);
    glUseProgram(pf);int NQ=200;double best=1e9;
    for(int r=0;r<4;r++){glFinish();double t0=now();for(int i=0;i<NQ;i++)glDrawArrays(GL_TRIANGLES,0,6);glFinish();double t=now()-t0;if(t<best)best=t;}
    res("fillrate",!glerr("fill"),"%.1f Mpix/s (%d fullscreen quads, %.1f ms)",(double)NQ*W*W/best/1e6,NQ,best*1e3);

    int G=512;size_t nv=(size_t)G*G*6;float*v=malloc(nv*8);float cs=2.0f/G;size_t k=0;
    for(int y=0;y<G;y++)for(int x=0;x<G;x++){float x0=-1+x*cs,y0=-1+y*cs,x1=x0+cs*0.5f,y1=y0+cs*0.5f;
        float c[12]={x0,y0,x1,y0,x1,y1,x0,y0,x1,y1,x0,y1};for(int i=0;i<12;i++)v[k++]=c[i];}
    glBufferData(GL_ARRAY_BUFFER,nv*8,v,GL_STATIC_DRAW);glVertexAttribPointer(0,2,GL_FLOAT,0,8,0);
    best=1e9;for(int r=0;r<4;r++){glFinish();double t0=now();for(int i=0;i<4;i++)glDrawArrays(GL_TRIANGLES,0,nv/1);glFinish();double t=now()-t0;if(t<best)best=t;}
    res("vertex",!glerr("vtx"),"%.2f Mtri/s (%zu tris x4, small triangles, %.1f ms)",(double)(nv/3)*4/best/1e6,nv/3,best*1e3);
    free(v);
}

int main(void){
    PFNEGLQUERYDEVICESEXTPROC qd=(void*)eglGetProcAddress("eglQueryDevicesEXT");
    PFNEGLQUERYDEVICESTRINGEXTPROC qds=(void*)eglGetProcAddress("eglQueryDeviceStringEXT");
    PFNEGLGETPLATFORMDISPLAYEXTPROC gpd=(void*)eglGetProcAddress("eglGetPlatformDisplayEXT");
    EGLDisplay dpy=EGL_NO_DISPLAY;
    if(qd&&qds&&gpd){
        EGLDeviceEXT devs[16];EGLint n=0;qd(16,devs,&n);
        for(int i=0;i<n;i++){
            const char*df=qds(devs[i],EGL_DRM_DEVICE_FILE_EXT);const char*rn=qds(devs[i],EGL_DRM_RENDER_NODE_FILE_EXT);
            printf("egl device %d: drm=%s render=%s\n",i,df?df:"-",rn?rn:"-");
            if(dpy==EGL_NO_DISPLAY&&((rn&&strstr(rn,"renderD129"))||(df&&strstr(df,"card1"))))dpy=gpd(EGL_PLATFORM_DEVICE_EXT,devs[i],0);
        }
    }
    if(dpy==EGL_NO_DISPLAY){printf("device platform not found, falling back to surfaceless\n");dpy=eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,0);}
    EGLint maj,min;if(!eglInitialize(dpy,&maj,&min)){printf("FATAL eglInitialize 0x%x\n",eglGetError());return 2;}
    eglBindAPI(EGL_OPENGL_API);
    EGLint ca[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_NONE};EGLConfig cfg;EGLint nc;
    if(!eglChooseConfig(dpy,ca,&cfg,1,&nc)||!nc){EGLint ca2[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_NONE};eglChooseConfig(dpy,ca2,&cfg,1,&nc);}
    if(!nc){printf("FATAL no EGL config\n");return 2;}
    EGLint cx[]={EGL_CONTEXT_MAJOR_VERSION,4,EGL_CONTEXT_MINOR_VERSION,3,EGL_CONTEXT_OPENGL_PROFILE_MASK,EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,EGL_NONE};
    EGLContext c=eglCreateContext(dpy,cfg,EGL_NO_CONTEXT,cx);if(c==EGL_NO_CONTEXT){printf("FATAL eglCreateContext 0x%x\n",eglGetError());return 2;}
    if(!eglMakeCurrent(dpy,EGL_NO_SURFACE,EGL_NO_SURFACE,c)){printf("FATAL MakeCurrent 0x%x\n",eglGetError());return 2;}
    const char*rd=(const char*)glGetString(GL_RENDERER);
    printf("GL_RENDERER=%s\nGL_VERSION=%s\n",rd,glGetString(GL_VERSION));
    if(strcasestr(rd,"llvmpipe")||strcasestr(rd,"softpipe")||strcasestr(rd,"swrast")){printf("FATAL software renderer\n");return 2;}
    t_compute();t_render();
    printf("%s (%d failures)\n",fails?"OVERALL FAIL":"OVERALL PASS",fails);return fails?1:0;
}
