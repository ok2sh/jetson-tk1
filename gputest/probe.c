#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static long avail(void){FILE*f=fopen("/proc/meminfo","r");char l[128];long v=-1;while(fgets(l,sizeof l,f))if(sscanf(l,"MemAvailable: %ld kB",&v)==1)break;fclose(f);return v>>10;}
#define S(m) printf("%-28s avail %ld MB\n",m,avail())
int main(int c,char**v){
  setvbuf(stdout,0,_IOLBF,0);setenv("RUSTICL_ENABLE","nouveau",0);
  size_t mb=c>1?atoi(v[1]):4; S("start");
  cl_platform_id ps[4];cl_uint np;clGetPlatformIDs(4,ps,&np);S("platforms");
  cl_device_id d=0;for(cl_uint i=0;i<np&&!d;i++){cl_uint n=0;if(clGetDeviceIDs(ps[i],CL_DEVICE_TYPE_GPU,1,&d,&n)||!n)d=0;}
  if(!d){puts("no gpu");return 2;} S("device");
  cl_int e;cl_context ctx=clCreateContext(0,1,&d,0,0,&e);S("context");
  cl_command_queue q=clCreateCommandQueue(ctx,d,0,&e);S("queue");
  const char*src="__kernel void k(__global uint*p){size_t i=get_global_id(0);p[i]=i*3u;}";
  cl_program p=clCreateProgramWithSource(ctx,1,&src,0,&e);e=clBuildProgram(p,1,&d,"",0,0);printf("build %d\n",e);S("build");
  cl_kernel k=clCreateKernel(p,"k",&e);S("kernel");
  size_t n=mb<<18;cl_mem b=clCreateBuffer(ctx,CL_MEM_READ_WRITE,n*4,0,&e);printf("buf %zu MB err %d\n",mb,e);S("buffer");
  clSetKernelArg(k,0,sizeof b,&b);e=clEnqueueNDRangeKernel(q,k,1,0,&n,0,0,0,0);printf("enqueue %d\n",e);
  e=clFinish(q);printf("finish %d\n",e);S("finish");
  unsigned*h=malloc(n*4);e=clEnqueueReadBuffer(q,b,CL_TRUE,0,n*4,h,0,0,0);
  size_t bad=0;for(size_t i=0;i<n;i++)if(h[i]!=(unsigned)(i*3u))bad++;printf("read %d mismatches %zu\n",e,bad);S("verify");
  clReleaseMemObject(b);clReleaseKernel(k);clReleaseProgram(p);clReleaseCommandQueue(q);clReleaseContext(ctx);S("released");
  return bad?1:0;}
