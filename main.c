#include <stdlib.h>

// write()
#include <unistd.h>

// printf()
#include <stdio.h>

// file operations: open() etc
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// strcp() 
#include <string.h>

// mmap()
#include <sys/mman.h>

// OpenCL
#include <CL/cl.h>

// gettimeofday()
#include <sys/time.h>



int save_to_file(double x0, double y0, double incr, int width, int height, int depth, char *buffer);

cl_int create_opencl_context(int pl, int dev, cl_context *context, cl_device_id *device);

cl_int create_opencl_program(cl_context context, const char *filename, cl_program *program);

// This is the device and platform that we are going to use.
#define PLATFORM 0
#define DEVICE 0

// The OpenCl program
#define PROGRAM "mandelbrot.cl"
#define PROCEDURE "render"

// We query the node for existing platforms.
#define MAX_PLATFORMS 16
#define MAX_DEVICES 16


int main(int argc, const char * argv[]) {

  cl_context  context;
  cl_device_id  device;
  cl_int err, err0, err1, err2, err3, err4 = 0;

  cl_command_queue cmd_queue;      
  cl_kernel  kernel;
  cl_program  program;    

  cl_mem global_buffer;

  int width = 1980;
  int height = 1080;
  int depth = 1024;
  double x0 = -2.0;
  double y0 =  1.0;
  double incr = 0.002;

  struct timeval t0,t1,t2,t3,t4;
  
  // The arguments to the program, specifies the image.
  if(argc < 7) {
    printf("usage: mandelbrot <width> <height> <depth> <x-upper> <y-left> <k-incr>\n");
    printf("will use deafult values\n");
  } else {
    width = atoi(argv[1]);
    height = atoi(argv[2]);    
    depth = atoi(argv[3]);
    x0 = atof(argv[4]);        
    y0 = atof(argv[5]);
    incr = atof(argv[6]);
  }

  
  // We're building a rgb image and need three bytes per pixel.
  size_t image_size = sizeof(char) * 3 * width * height;

  // We're now ready to set up the OpenCL device for the computation. 
  
  //printf("Creating context on platform %d, deviec %d.\n", PLATFORM, DEVICE);

  gettimeofday(&t0, NULL);
  
  // This will create a context attached to a device.
  err = create_opencl_context(PLATFORM, DEVICE, &context, &device);
  if(err != CL_SUCCESS) {
    printf("Failed to create context.\n");    
    return -1;
  }
  
  //printf("Creating queue for context.\n");
  
  // Create a queue attached to the context and device.
  cmd_queue = clCreateCommandQueueWithProperties(context, device, 0, &err);
  if(err != CL_SUCCESS) {
    printf("Failed to create queue.\n");
    return -1;
  }

  //printf("Creating local buffer in context.\n");  
  
  // This is the image buffer on the device.
  global_buffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY, image_size, NULL, &err);
  if(err != CL_SUCCESS) {
    printf("Failed to create buffer.\n");
    return -1;
  }

  //printf("Loading OpenCL source program\n");
  
  // Load the OpenCL source program.
  err = create_opencl_program(context, PROGRAM, &program);
  if(err != CL_SUCCESS) {
    printf("Failed to load program.\n");
    return -1;
  }

  //printf("Creating OpenCL kernel\n");
  
  kernel = clCreateKernel(program, PROCEDURE, &err);
  if(err != CL_SUCCESS) {
    printf("Failed to create kernel.\n");
    return -1;
  }

  //printf("Setting kernel arguments\n");  
  
  // Now set up the arguments to our kernel: a pointer to the global buffer and information about the image. 
  err0 = clSetKernelArg(kernel, 0, sizeof(cl_mem), &global_buffer);
  err1 = clSetKernelArg(kernel, 1, sizeof(int), &depth);
  err2 = clSetKernelArg(kernel, 2, sizeof(double), &x0);
  err3 = clSetKernelArg(kernel, 3, sizeof(double), &y0);
  err4 = clSetKernelArg(kernel, 4, sizeof(double), &incr);      
  if(err0 != CL_SUCCESS || err1 != CL_SUCCESS || err2 != CL_SUCCESS || err3 != CL_SUCCESS || err4 != CL_SUCCESS ) {
    printf("Failed to set arguments\n");
    return -1;
  }

  //printf("Enqueue of kernels\n");
  
  gettimeofday(&t1, NULL);
  
  size_t device_work_size[2] = {width, height};

  // This is where the OpenCL computations starts.
  err = clEnqueueNDRangeKernel(cmd_queue, kernel, 2, NULL, device_work_size, NULL, 0, NULL, NULL);
  if(err != CL_SUCCESS) {
    printf("Failed run program.\n");
    return -1;
  }

  // The host buffer to where we will copy the file
  char *host_buffer = malloc(image_size);
  
  // We now enqueue a read operation that will copy the content of the
  // global_buffer (on the device) to our host_buffer (in main ḿemory).

  // printf("Enqueue read global buffer to heap buffer\n");      
  err = clEnqueueReadBuffer(cmd_queue, global_buffer, CL_TRUE, 0, image_size, host_buffer, 0, NULL, NULL);
  if(err != CL_SUCCESS) {
    printf("Failed to read buffer.\n");
    return -1;
  }

  // printf("Waiting until done.\n");    
  clFinish(cmd_queue);

  gettimeofday(&t2, NULL);

  // printf("Saving image to file .\n");      
  save_to_file(x0, y0, incr, width, height, depth, host_buffer);

  
  // Now, eelease OpenCL objects.
  clReleaseMemObject(global_buffer);
  clReleaseCommandQueue(cmd_queue);
  clReleaseContext(context);

  gettimeofday(&t3, NULL);

  printf("Platform setup in %ld ms\n", (t1.tv_sec - t0.tv_sec)*1000 + (t1.tv_usec - t0.tv_usec)/1000);  
  printf("Image rendered in %ld ms\n", (t2.tv_sec - t1.tv_sec)*1000 + (t2.tv_usec - t1.tv_usec)/1000);
  printf("Saving file    in %ld ms\n", (t3.tv_sec - t2.tv_sec)*1000 + (t3.tv_usec - t2.tv_usec)/1000);
  printf("Total time        %ld ms\n", (t3.tv_sec - t0.tv_sec)*1000 + (t3.tv_usec - t0.tv_usec)/1000);    

  return 0;
}


cl_int create_opencl_context(int pl, int dev, cl_context *context, cl_device_id *device){


  cl_platform_id platform_id[MAX_PLATFORMS];
  cl_device_id device_id[MAX_DEVICES];

  cl_uint num_platforms = 0;
  cl_uint num_devices = 0;

  cl_int err = 0;  

  err = clGetPlatformIDs(MAX_PLATFORMS, &platform_id, &num_platforms);

  if(err != CL_SUCCESS) {
    printf("Failed to get platform id.\n");
    return err;
  }
  if((num_platforms -1) < pl) {
    printf("Platform %d not available.\n", pl);
    return CL_INVALID_PLATFORM;
  }

  err = clGetDeviceIDs(platform_id[pl], CL_DEVICE_TYPE_DEFAULT, MAX_DEVICES, &device_id, &num_devices);
  if(err != CL_SUCCESS) {
    printf("Failed to get device id.\n");
    return err;
  }

  if((num_devices -1) < dev) {
    printf("Device %d on platform %d not available.\n", dev, pl);
    return CL_INVALID_DEVICE;
  }  

  *device = device_id[dev];

  /*
  {
    // For the fun of it, let's see what we found
    cl_char vendor_name[1024] = {0};
    cl_char device_name[1024] = {0};

    if(CL_SUCCESS == clGetDeviceInfo(*device, CL_DEVICE_VENDOR, sizeof(vendor_name), vendor_name, NULL)) {
      printf("\t\tdevice vendor: %s\n", vendor_name);
    }

    if(CL_SUCCESS == clGetDeviceInfo(*device, CL_DEVICE_NAME, sizeof(device_name), device_name, NULL)) {
      printf("\t\tdevice name: %s\n", device_name);
    }
  }
  */
  
  /* Create a context with the selected device */
  *context = clCreateContext(NULL, 1, device, NULL, NULL, &err);
  if(err != CL_SUCCESS) {
    printf("Failed to create context.\n");
    return err;
  }

  return CL_SUCCESS;
}




cl_int create_opencl_program(cl_context context, const char *filename, cl_program *program) {

  int err;
  struct stat statbuf;
  FILE *fh;
  char *source;

  fh = fopen(filename, "r");
  if (fh == 0) {
    printf("program not found %s\n", filename);
    return CL_INVALID_PROGRAM;
  }
  
  stat(filename, &statbuf);
  source = (char *) malloc(statbuf.st_size + 1);
  fread(source, statbuf.st_size, 1, fh);
  source[statbuf.st_size] = '\0';

  // printf("Creating OpenCL program.\n");
      
  *program = clCreateProgramWithSource(context, 1, (const char**)&source, NULL, &err);
  if(err != CL_SUCCESS) {
    printf("Failed to load program.\n");
    return err;
  }    

  // printf("Building OpenCL program.\n");
  
  err = clBuildProgram(*program, NULL, 0, NULL, NULL, NULL);
  if(err != CL_SUCCESS) {
    printf("Failed to build program.\n");
    return err;
  }

  // The source is no longer needed
  free(source);
  
  return CL_SUCCESS;
}



int save_to_file(double x0, double y0, double incr, int width, int height, int depth, char *buffer) {

  // In the end everytghing will be in this file. 
  const char *filepath = "image.ppm";
  
  // Open the output file for reading and writing.
  int fd = open(filepath, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
    
  if (fd== -1) {
    printf("Failed to open file %s\n", filepath);
    return -1;
  }

  dprintf(fd, "P6\n# Mandelbrot image: x0 = %f y0 = %f k = %f width = %d height = %d depth = %d\n%d %d\n255\n",
	                               x0,     y0,     incr,  width,     height,     depth,     width, height);  

  write(fd, buffer, width*height*3);
  close(fd);
}




  


