#include <stdlib.h>

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
 
// This is the device and platform that we are going to use.
#define PLATFORM 0
#define DEVICE 0

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

  // This is the kernel program and the procedure to call.
  const char *prgrm_file_name = "mandelbrot.cl";
  const char *prgrm_proc_name = "render";  

  int width = 1980;
  int height = 1080;
  int depth = 1024;
  double x0 = -2.0;
  double y0 =  1.0;
  double incr = 0.002;
  
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
  
  printf("Creating context on platform %d, device %d.\n", PLATFORM, DEVICE);
  
  // This will create a context attached to a device.
  err = create_opencl_context(PLATFORM, DEVICE, &context, &device);
  if(err != CL_SUCCESS) {
    printf("Failed to create context.\n");    
    return -1;
  }
  
  printf("Creating queue for context.\n");
  
  // Create a queue attached to the context and device.
  cmd_queue = clCreateCommandQueue(context, device, 0, &err);
  if(err != CL_SUCCESS) {
    printf("Failed to create queue.\n");
    return -1;
  }

  printf("Creating local buffer in context.\n");  
  
  // This is the image buffer on the device.
  global_buffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY, image_size, NULL, &err);
  if(err != CL_SUCCESS) {
    printf("Failed to create buffer.\n");
    return -1;
  }

  printf("Loading OpenCL source program\n");
  
  // Load the OpenCL source program.
  err = create_opencl_program(context, prgrm_file_name, &program);
  if(err != CL_SUCCESS) {
    printf("Failed to load program.\n");
    return -1;
  }

  printf("Creating OpenCL kernel\n");
  
  kernel = clCreateKernel(program, prgrm_proc_name, &err);
  if(err != CL_SUCCESS) {
    printf("Failed to create kernel.\n");
    return -1;
  }

  printf("Setting kernel arguments\n");  
  
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

  printf("Enqueue of kernels\n");    
  size_t device_work_size[2] = {width, height};
  size_t device_work_offset[2] = {0, 0};
  size_t offset = 0;
  cl_event done;

  // This is where the OpenCL computations starts.
  err = clEnqueueNDRangeKernel(cmd_queue, kernel, 2, device_work_offset, device_work_size, NULL, 0, NULL, &done);
  if(err != CL_SUCCESS) {
    printf("Failed run program.\n");
    return -1;
  }

  int fd;                 // The file image.ppm that we map to memory
  char *file_map;         // The map of the file, needed when sync:ing
  int total_length;       // The total size of the fiel map, needed when sync:ing
  char *host_buffer;      // The part of the map that is the image buffer. 
  
  int total_size = open_and_map_file(x0, y0, incr, width, height, depth, &fd, &file_map, &host_buffer);

  // We now enqueue a read operation that will copy the content of the
  // global_buffer (on the device) to our host_buffer (in main nḿemory).

  // hmm, we should make sure that all operations are done!
  printf("Enqueue read global buffer to heap buffer\n");      
  err = clEnqueueReadBuffer(cmd_queue, global_buffer, CL_FALSE, offset, image_size, host_buffer, 1, &done, NULL);
  if(err != CL_SUCCESS) {
    printf("Failed to read buffer.\n");
    return -1;
  }

  printf("Waiting until done.\n");    
  clFinish(cmd_queue);

  printf("Sync:ing and closing file .\n");      
  sync_and_close_file(fd, file_map, total_size);
  
  // Now, eelease OpenCL objects.
  clReleaseMemObject(global_buffer);
  clReleaseCommandQueue(cmd_queue);
  clReleaseContext(context);

  return 0;
}


int sync_and_close_file(int fd, char *file_map, int total_size) {

  // The image should now be in main memory and it's time to write it to the file.
  printf("Sync buffer to file .\n");      
  if (msync(file_map, total_size, MS_SYNC) == -1) {
    printf("Failed to sync the file to disk\n");
    return -1;
  }

  // Done, let's unmap the file. 
  printf("Unmap file from memory.\n");        
  if (munmap(file_map, total_size) == -1) {
    printf("Failed to  un-mmap the file\n");
    return -1;
  }
  // .. and close it.
  close(fd);
}

int open_and_map_file(double x0, double y0, double incr, int width, int height, int depth, int *file_descr, char **file_map, char **host_buffer) {

  // In the end everytghing will be in this file. 
  const char *filepath = "image.ppm";

  // Open the output file for reading and writing.
  int fd = open(filepath, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
    
  if (fd== -1) {
    printf("Failed to open file %s\n", filepath);
    return -1;
  }

  // The file should start with a .ppm header containing information abut the image. 
  
  // 256 bytes should be enough for the header.
  char header[256];

  // The header, including a comment (will be copied to .jpg image).
  int header_length = sprintf(&header, "P6\n# Mandelbrot image: x0 = %f y0 = %f k = %f width = %d height = %d depth = %d\n%d %d\n255\n",
			                                        x0,     y0,     incr,  width,     height,     depth,     width, height);

  printf("Header of .ppm file\n%s\n", header);

  // This is the tola size of the final file. 
  int total_size = header_length + width*height*3;

  // Extend the file to its full length.
  if (lseek(fd, total_size, SEEK_SET) == -1)  {
    close(fd);
    printf("Failed to extend file to size  %d\n", total_size);
    return -1;
  }

  // This is needed before we map the file. 
  if (write(fd, "", 1) == -1) {
    close(fd);
    printf("Failed writing last byte of the file");
    return -1;
  }

  // We now use mmap to map the whole file to a memory area. 
  char *map = mmap(0, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (map == MAP_FAILED)  {
      printf("Failed to map file\n");
      close(fd);
      return -1;
  }
  
  // We write the header to the beginning of the maped file.
  strcpy(map, header);

  // The file descriptor.   
  *file_descr = fd;

  // This is the whole map
  *file_map = map;
  
  // The host_buffer is where the image starts. 
  *host_buffer = &map[header_length];

  return total_size;
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

  printf("Creating OpenCL program.\n");
      
  *program = clCreateProgramWithSource(context, 1, (const char**)&source, NULL, &err);
  if(err != CL_SUCCESS) {
    printf("Failed to load program.\n");
    return err;
  }    

  printf("Building OpenCL program.\n");
  
  err = clBuildProgram(*program, 0, NULL, NULL, NULL, NULL);
  if(err != CL_SUCCESS) {
    printf("Failed to build program.\n");
    return err;
  }

  // The source is no longer needed
  free(source);
  
  return CL_SUCCESS;
}





