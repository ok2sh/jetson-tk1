#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(void){
  setenv("RUSTICL_ENABLE","nouveau",0);
  cl_platform_id ps[4];cl_uint np;clGetPlatformIDs(4,ps,&np);cl_device_id d=0;
  for(cl_uint i=0;i<np&&!d;i++){cl_uint n=0;if(clGetDeviceIDs(ps[i],CL_DEVICE_TYPE_GPU,1,&d,&n)||!n)d=0;}
  cl_int e;cl_context c=clCreateContext(0,1,&d,0,0,&e);cl_command_queue q=clCreateCommandQueueWithProperties(c,d,(cl_queue_properties[]){CL_QUEUE_PROPERTIES,CL_QUEUE_PROFILING_ENABLE,0},&e);
  const char*s="__kernel void k(float a,__global const float*x,__global const float*y,__global float*z){size_t i=get_global_id(0);z[i]=a*x[i]+y[i];}";
  cl_program p=clCreateProgramWithSource(c,1,&s,0,&e);clBuildProgram(p,1,&d,"",0,0);cl_kernel k=clCreateKernel(p,"k",&e);
  size_t n=(getenv("N")?atoi(getenv("N")):16)<<20;cl_mem x=clCreateBuffer(c,CL_MEM_READ_WRITE,n*4,0,&e),y=clCreateBuffer(c,CL_MEM_READ_WRITE,n*4,0,&e),z=clCreateBuffer(c,CL_MEM_READ_WRITE,n*4,0,&e);
  float a=2;clSetKernelArg(k,0,4,&a);clSetKernelArg(k,1,sizeof x,&x);clSetKernelArg(k,2,sizeof y,&y);clSetKernelArg(k,3,sizeof z,&z);
  for(int w=0;w<5;w++)clEnqueueNDRangeKernel(q,k,1,0,&n,0,0,0,0);clFinish(q);
  int R=50;cl_event ev[50];double t0=now();for(int r=0;r<R;r++)clEnqueueNDRangeKernel(q,k,1,0,&n,0,0,0,&ev[r]);clFinish(q);double wall=(now()-t0)/R;
  double prof=0;for(int r=0;r<R;r++){cl_ulong a0,b0;clGetEventProfilingInfo(ev[r],CL_PROFILING_COMMAND_START,8,&a0,0);clGetEventProfilingInfo(ev[r],CL_PROFILING_COMMAND_END,8,&b0,0);prof+=(b0-a0)*1e-9;}prof/=R;
  {cl_ulong s0,e1,q0,q1;clGetEventProfilingInfo(ev[0],CL_PROFILING_COMMAND_START,8,&s0,0);clGetEventProfilingInfo(ev[R-1],CL_PROFILING_COMMAND_END,8,&e1,0);clGetEventProfilingInfo(ev[0],CL_PROFILING_COMMAND_QUEUED,8,&q0,0);clGetEventProfilingInfo(ev[R-1],CL_PROFILING_COMMAND_QUEUED,8,&q1,0);printf("span gpu %.1f ms vs wall %.1f ms; queued-ts span %.1f ms\n",(e1-s0)*1e-6,wall*R*1e3,(q1-q0)*1e-6);}
  printf("saxpy 16M: wall %.3f ms (%.2f GB/s), profiling %.3f ms (%.2f GB/s), ratio %.2f\n",wall*1e3,n*12/wall/1e9,prof*1e3,n*12/prof/1e9,wall/prof);
  return 0;}
