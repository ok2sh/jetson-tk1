#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>

static cl_context ctx; static cl_command_queue q; static cl_device_id dev;
static int fails;
#define CK(e) do{cl_int _e=(e); if(_e!=CL_SUCCESS){printf("CL error %d at line %d: %s\n",_e,__LINE__,#e);fails++;return;}}while(0)
#define CKE(e,err) do{cl_int _e=(e); if(_e!=CL_SUCCESS){printf("CL error %d at line %d: %s\n",_e,__LINE__,#e);return err;}}while(0)

/* GK20A PTIMER ticks from the 12 MHz osc and nouveau does not rescale it, so event timestamps read 2.6x short */
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static cl_program build(const char*src){
    cl_int e; cl_program p=clCreateProgramWithSource(ctx,1,&src,0,&e);
    e=clBuildProgram(p,1,&dev,"",0,0);
    if(e){char log[8192]; clGetProgramBuildInfo(p,dev,CL_PROGRAM_BUILD_LOG,sizeof log,log,0); printf("build failed (%d): %s\n",e,log); return 0;}
    return p;
}
static void result(const char*n,int ok,const char*fmt,double v){
    char b[128]; snprintf(b,sizeof b,fmt,v);
    printf("%-10s %s  %s\n",n,ok?"PASS":"FAIL",b); if(!ok)fails++;
}

static const char *saxpy_src="__kernel void saxpy(float a,__global const float*x,__global const float*y,__global float*z){size_t i=get_global_id(0);z[i]=a*x[i]+y[i];}";
static void t_saxpy(void){
    size_t n=16u<<20; cl_int e; float *x=malloc(n*4),*y=malloc(n*4),*z=malloc(n*4);
    for(size_t i=0;i<n;i++){x[i]=(i%1000)*0.001f;y[i]=(i%777)*0.01f;}
    cl_mem bx=clCreateBuffer(ctx,CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,n*4,x,&e);CK(e);
    cl_mem by=clCreateBuffer(ctx,CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,n*4,y,&e);CK(e);
    cl_mem bz=clCreateBuffer(ctx,CL_MEM_WRITE_ONLY,n*4,0,&e);CK(e);
    cl_program p=build(saxpy_src); if(!p){fails++;return;}
    cl_kernel k=clCreateKernel(p,"saxpy",&e);CK(e); float a=2.5f;
    clSetKernelArg(k,0,4,&a);clSetKernelArg(k,1,sizeof bx,&bx);clSetKernelArg(k,2,sizeof by,&by);clSetKernelArg(k,3,sizeof bz,&bz);
    double best=1e9;
    for(int r=0;r<5;r++){double t0=now();cl_event ev;CK(clEnqueueNDRangeKernel(q,k,1,0,&n,0,0,0,&ev));clFinish(q);double t=now()-t0;if(t<best)best=t;clReleaseEvent(ev);}
    CK(clEnqueueReadBuffer(q,bz,CL_TRUE,0,n*4,z,0,0,0));
    size_t bad=0; for(size_t i=0;i<n;i++){float r=a*x[i]+y[i]; if(fabsf(z[i]-r)>1e-5f*fabsf(r)+1e-6f)bad++;}
    char b[160]; snprintf(b,sizeof b,"%.3f ms, %.2f GB/s (12 B/elem), mismatches=%zu",best*1e3,n*12/best/1e9,bad);
    printf("%-10s %s  %s\n","saxpy",bad?"FAIL":"PASS",b); if(bad)fails++;
    clReleaseMemObject(bx);clReleaseMemObject(by);clReleaseMemObject(bz);free(x);free(y);free(z);
}

static void t_copy(void){
    size_t n=64u<<20; cl_int e; unsigned char*h=malloc(n),*o=malloc(n);
    for(size_t i=0;i<n;i++)h[i]=(i*31+7)&0xff;
    cl_mem a=clCreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,n,h,&e);CK(e);
    cl_mem b=clCreateBuffer(ctx,CL_MEM_READ_WRITE,n,0,&e);CK(e);
    double best=1e9;
    for(int r=0;r<5;r++){double t0=now();cl_event ev;CK(clEnqueueCopyBuffer(q,a,b,0,0,n,0,0,&ev));clFinish(q);double t=now()-t0;if(t<best)best=t;clReleaseEvent(ev);}
    CK(clEnqueueReadBuffer(q,b,CL_TRUE,0,n,o,0,0,0));
    int ok=!memcmp(h,o,n);
    char s[128]; snprintf(s,sizeof s,"%.3f ms, %.2f GB/s (r+w %.2f GB/s)",best*1e3,n/best/1e9,2.0*n/best/1e9);
    printf("%-10s %s  %s\n","copy64MB",ok?"PASS":"FAIL",s); if(!ok)fails++;
    clReleaseMemObject(a);clReleaseMemObject(b);free(h);free(o);
}

static const char *sgemm_src=
"#define T 16\n"
"__kernel void sgemm(int N,__global const float*A,__global const float*B,__global float*C){\n"
" __local float As[T][T],Bs[T][T]; int tx=get_local_id(0),ty=get_local_id(1);\n"
" int col=get_global_id(0),row=get_global_id(1); float s=0;\n"
" for(int t=0;t<N/T;t++){As[ty][tx]=A[row*N+t*T+tx];Bs[ty][tx]=B[(t*T+ty)*N+col];barrier(CLK_LOCAL_MEM_FENCE);\n"
"  for(int k=0;k<T;k++)s+=As[ty][k]*Bs[k][tx]; barrier(CLK_LOCAL_MEM_FENCE);}\n"
" C[row*N+col]=s;}";
static void t_sgemm(void){
    int N=1024; size_t n=(size_t)N*N; cl_int e; float*A=malloc(n*4),*B=malloc(n*4),*C=malloc(n*4);
    srand(1); for(size_t i=0;i<n;i++){A[i]=rand()/(float)RAND_MAX-0.5f;B[i]=rand()/(float)RAND_MAX-0.5f;}
    cl_mem ba=clCreateBuffer(ctx,CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,n*4,A,&e);CK(e);
    cl_mem bb=clCreateBuffer(ctx,CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,n*4,B,&e);CK(e);
    cl_mem bc=clCreateBuffer(ctx,CL_MEM_WRITE_ONLY,n*4,0,&e);CK(e);
    cl_program p=build(sgemm_src); if(!p){fails++;return;}
    cl_kernel k=clCreateKernel(p,"sgemm",&e);CK(e);
    clSetKernelArg(k,0,4,&N);clSetKernelArg(k,1,sizeof ba,&ba);clSetKernelArg(k,2,sizeof bb,&bb);clSetKernelArg(k,3,sizeof bc,&bc);
    size_t g[2]={N,N},l[2]={16,16}; double best=1e9;
    for(int r=0;r<5;r++){double t0=now();cl_event ev;CK(clEnqueueNDRangeKernel(q,k,2,0,g,l,0,0,&ev));clFinish(q);double t=now()-t0;if(t<best)best=t;clReleaseEvent(ev);}
    CK(clEnqueueReadBuffer(q,bc,CL_TRUE,0,n*4,C,0,0,0));
    double maxrel=0,maxabs=0; 
    for(int i=0;i<N;i+=1){ for(int j=0;j<N;j+=1){ if((i%8)&&(j%8))continue;
        double s=0; for(int kk=0;kk<N;kk++)s+=(double)A[(size_t)i*N+kk]*B[(size_t)kk*N+j];
        double d=fabs(C[(size_t)i*N+j]-s); if(d>maxabs)maxabs=d; } }
    maxrel=maxabs/ (sqrt((double)N)*0.0833*0.0833*0.5+1e-9);
    int ok=maxabs<1e-3*sqrt((double)N)*0.0833*0.0833*3+1e-4 && maxabs<1e-2;
    char b[200]; snprintf(b,sizeof b,"%.3f ms, %.2f GFLOPS, max abs err %.3g (rel to typical |C| %.2g)",best*1e3,2.0*N*N*N/best/1e9,maxabs,maxrel);
    printf("%-10s %s  %s\n","sgemm1024",ok?"PASS":"FAIL",b); if(!ok)fails++;
    clReleaseMemObject(ba);clReleaseMemObject(bb);clReleaseMemObject(bc);free(A);free(B);free(C);
}

static const char *red_src=
"__kernel void red(__global const uint*in,__global ulong*out,__local ulong*s){\n"
" int l=get_local_id(0); s[l]=in[get_global_id(0)]; barrier(CLK_LOCAL_MEM_FENCE);\n"
" for(int o=get_local_size(0)/2;o>0;o>>=1){if(l<o)s[l]+=s[l+o];barrier(CLK_LOCAL_MEM_FENCE);}\n"
" if(l==0)out[get_group_id(0)]=s[0];}";
static void t_reduce(void){
    size_t n=16u<<20,ls=256,ng=n/ls; cl_int e; uint32_t*h=malloc(n*4); for(size_t i=0;i<n;i++)h[i]=i;
    cl_mem bi=clCreateBuffer(ctx,CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,n*4,h,&e);CK(e);
    cl_mem bo=clCreateBuffer(ctx,CL_MEM_WRITE_ONLY,ng*8,0,&e);CK(e);
    cl_program p=build(red_src); if(!p){fails++;return;}
    cl_kernel k=clCreateKernel(p,"red",&e);CK(e);
    clSetKernelArg(k,0,sizeof bi,&bi);clSetKernelArg(k,1,sizeof bo,&bo);clSetKernelArg(k,2,ls*8,0);
    double best=1e9; uint64_t*part=malloc(ng*8);
    for(int r=0;r<5;r++){double t0=now();cl_event ev;CK(clEnqueueNDRangeKernel(q,k,1,0,&n,&ls,0,0,&ev));clFinish(q);double t=now()-t0;if(t<best)best=t;clReleaseEvent(ev);}
    CK(clEnqueueReadBuffer(q,bo,CL_TRUE,0,ng*8,part,0,0,0));
    uint64_t s=0; for(size_t i=0;i<ng;i++)s+=part[i]; uint64_t ref=(uint64_t)n*(n-1)/2;
    char b[200]; snprintf(b,sizeof b,"%.3f ms, %.2f GB/s, got %llu expected %llu",best*1e3,n*4/best/1e9,(unsigned long long)s,(unsigned long long)ref);
    printf("%-10s %s  %s\n","reduce",s==ref?"PASS":"FAIL",b); if(s!=ref)fails++;
    clReleaseMemObject(bi);clReleaseMemObject(bo);free(h);free(part);
}

static const char *alloc_src=
"__kernel void fill(__global uint*p,uint salt){size_t i=get_global_id(0);p[i]=(uint)i*2654435761u^salt;}\n"
"__kernel void chk(__global const uint*p,uint salt,__global uint*err){size_t i=get_global_id(0);if(p[i]!=((uint)i*2654435761u^salt))atomic_inc(err);}";
static long memavail_mb(void){
    FILE*f=fopen("/proc/meminfo","r");char l[128];long v=-1;if(!f)return -1;
    while(fgets(l,sizeof l,f))if(sscanf(l,"MemAvailable: %ld kB",&v)==1){v>>=10;break;}
    fclose(f);return v;
}
static void t_alloc(void){
    size_t step=256u<<20; int maxs=6; cl_mem bufs[6]; int got=0; cl_int e;
    cl_program p=build(alloc_src); if(!p){fails++;return;}
    cl_kernel kf=clCreateKernel(p,"fill",&e),kc=clCreateKernel(p,"chk",&e);
    cl_mem be=clCreateBuffer(ctx,CL_MEM_READ_WRITE,4,0,&e);CK(e);
    cl_ulong maxa; clGetDeviceInfo(dev,CL_DEVICE_MAX_MEM_ALLOC_SIZE,8,&maxa,0);
    cl_ulong gm; clGetDeviceInfo(dev,CL_DEVICE_GLOBAL_MEM_SIZE,8,&gm,0);
    printf("           device global mem %llu MB, max alloc %llu MB\n",(unsigned long long)(gm>>20),(unsigned long long)(maxa>>20));
    size_t n=step/4; int bad=0; char why[128]="";
    for(int s=0;s<maxs;s++){
        long av=memavail_mb();
        if(av>=0&&av<(long)(step>>20)+300){snprintf(why,sizeof why,"guard: MemAvailable %ld MB too low for step %d",av,s+1);break;}
        bufs[s]=clCreateBuffer(ctx,CL_MEM_READ_WRITE,step,0,&e);
        if(e){snprintf(why,sizeof why,"alloc step %d err %d",s+1,e);break;}
        cl_uint salt=0x1234+s; clSetKernelArg(kf,0,sizeof(cl_mem),&bufs[s]);clSetKernelArg(kf,1,4,&salt);
        e=clEnqueueNDRangeKernel(q,kf,1,0,&n,0,0,0,0); if(!e)e=clFinish(q);
        if(e){snprintf(why,sizeof why,"fill step %d err %d",s+1,e);clReleaseMemObject(bufs[s]);break;}
        got++;
    }
    cl_uint zero=0;
    for(int s=0;s<got;s++){
        clEnqueueWriteBuffer(q,be,CL_TRUE,0,4,&zero,0,0,0); cl_uint salt=0x1234+s;
        clSetKernelArg(kc,0,sizeof(cl_mem),&bufs[s]);clSetKernelArg(kc,1,4,&salt);clSetKernelArg(kc,2,sizeof be,&be);
        clEnqueueNDRangeKernel(q,kc,1,0,&n,0,0,0,0);clFinish(q);
        cl_uint er; clEnqueueReadBuffer(q,be,CL_TRUE,0,4,&er,0,0,0); if(er){bad++;printf("           buffer %d: %u mismatches\n",s,er);}
    }
    int ok=!bad&&got>0;
    printf("%-10s %s  max verified allocation %d MB (%d x 256MB)%s%s\n","alloc",ok?"PASS":"FAIL",got*256,got,*why?" stopped: ":"",why);
    if(!ok)fails++;
    for(int s=0;s<got;s++)clReleaseMemObject(bufs[s]);
}

int main(void){
    setvbuf(stdout,0,_IOLBF,0);setenv("RUSTICL_ENABLE","nouveau",0);
    cl_uint np; clGetPlatformIDs(0,0,&np); cl_platform_id*ps=malloc(np*sizeof *ps); clGetPlatformIDs(np,ps,0);
    int found=0;
    for(cl_uint i=0;i<np&&!found;i++){
        cl_uint nd=0; clGetDeviceIDs(ps[i],CL_DEVICE_TYPE_ALL,0,0,&nd); if(!nd)continue;
        cl_device_id*ds=malloc(nd*sizeof *ds); clGetDeviceIDs(ps[i],CL_DEVICE_TYPE_ALL,nd,ds,0);
        for(cl_uint j=0;j<nd;j++){char nm[256]; clGetDeviceInfo(ds[j],CL_DEVICE_NAME,sizeof nm,nm,0);
            printf("device: %s\n",nm); if(strstr(nm,"NVEA")){dev=ds[j];found=1;break;}}
    }
    if(!found){printf("FATAL: no NVEA device found\n");return 2;}
    char nm[256],ver[256]; clGetDeviceInfo(dev,CL_DEVICE_NAME,sizeof nm,nm,0);clGetDeviceInfo(dev,CL_DEVICE_VERSION,sizeof ver,ver,0);
    printf("using %s (%s)\n",nm,ver);
    cl_int e; ctx=clCreateContext(0,1,&dev,0,0,&e); if(e){printf("ctx err %d\n",e);return 2;}
    q=clCreateCommandQueue(ctx,dev,CL_QUEUE_PROFILING_ENABLE,&e); if(e){printf("queue err %d\n",e);return 2;}
    t_saxpy();t_copy();t_sgemm();t_reduce();t_alloc();
    printf("%s (%d failures)\n",fails?"OVERALL FAIL":"OVERALL PASS",fails); return fails?1:0;
}
