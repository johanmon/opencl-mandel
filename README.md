# An example of how to use OpenCL.

In order to learn some OpenCL programming I got hold of a AMD GPU,
W7100, and started to experiment with what should be the first thing
anyone should do: creating a mandelbrot image :-) 


## A short description of the code

The C program is in one large file and most is done in the `main()`
procedure. Not the most elegant solution but it makes it easy to
follow the code; C code in one file and OpenCL code in another.

The C program will load and build the OpenCL program in
`mandelbrot.cl`. This is the so called kernel program and is the thing
that will run on the GPU. If you take a quick look at it you will see
one procedure `render()` that calculates a depth of coordinate and
colors a pixel with a rgb-value. The C program will do all the boiler
plate to set up the computation and make sure that a kernel
computation is scheduled for each pixel in the image.

Let's go through the `main()` procedure from top to bottom (excluding
things that you will figure out why they are there if you know some C.

### create a context

The first thing we need to do is to create a `context`, we have
delegated this to a procedure since this a probably something that we
later want to reuse.

```C
  err = create_opencl_context(PLATFORM, DEVICE, &context, &device);
```

What we need to know is what `PLATFORM` and which `DEVICE` on the
platform that we want to create the connect on. If you (as me) only
have one GPU this simply means platform `0` and device `0` so we have
them declared in the program. It could be that your CPU also shows up
as a device and depending on you configuration it could be part of a
larger platform with the GPU or treated as a separate platform.

We also provide a reference to our `context` and `device` structures
that will be populated with the appropriate information; more on this
later. If we succeed we should now have a `context` and  `device`  prepared for us.

### create a queue

The next thing we do is to create a queue, `cmd_queue`. that we need
in order to schedule our kernels. We create a default queue, `0`, that will
schedule kernels in FIFO order, have a default size etc.

```C
cmd_queue = clCreateCommandQueueWithProperties(context, device, 0, &err);
``` 

### create a buffer

Next we allocate a buffer in the device, this is the buffer where the
Mandelbrot image will be generated. Note that we call it the
`global_buffer` but this area is only global to the context and it is
allocated on the device i.e. not in our main memory. We provide the
size of the buffer that is width*height*3 since we will produce a rgb
image. 

```C
  global_buffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY, image_size, NULL, &err);
```

The kernels, the programs that will execute on the GPU, will
only write to the buffer and we do not have a buffer in main memory
from where we want to initialize the content (`NULL`).

### build a kernel program

The strange thing when you're new to OpenCL programming is that the
OpenCL program is normally compiled and loaded from source when you
run your program. This is more practical since the compilation will
depend on the particular device that you want to use. If we had
several devices we would need to keep track of which binary to use for
which device (it is possible to do offline compilation).

```C
  create_opencl_program(context, PROGRAM, &program);
```

We will later take a look at the steps needed to do the compilation
but for now let's assume that we have compiled kernel `program`.

### create a kernel

Time to create the kernel. It consists of the compiled program and the
name of the procedure that we wan to call. 

```C
  kernel = clCreateKernel(program, PROCEDURE, &err);
```

We're almost there, but first we have to provide the arguments to the
kernel program. We provide a pointer to the `global_buffer` (that is
on the device), the `depth`, upper left corner, `x0` and `y0`, and the increment per pixel. 

```C
  clSetKernelArg(kernel, 0, sizeof(cl_mem), &global_buffer);
  clSetKernelArg(kernel, 1, sizeof(int), &depth);
  clSetKernelArg(kernel, 2, sizeof(double), &x0);
  clSetKernelArg(kernel, 3, sizeof(double), &y0);
  clSetKernelArg(kernel, 4, sizeof(double), &incr);      
```

Note that there is no argument here that will tell the kernel program
which pixel it should work on. All instances of this kernel are given
the same arguments.

### schedule the kernels

We are now ready to actually launch our program and we will do so by
add the kernel to the queue that we created before. An entry in the
queue is a description of what we want to do. We provide the queue and
the kernel (that must both be created with the same device in mind)
and the dimension of the computation `2`. We also provide the total
size of this 2D space `{width, height}` and this how the system knows
how many kernels it should spawn. 

```C
  size_t device_work_size[2] = {width, height};

  clEnqueueNDRangeKernel(cmd_queue, kernel, 2, NULL, device_work_size, NULL, 0, NULL, NULL);
```

We will later see that the kernel program can figure out which kernel
it is in the 2D space. When, for example, it knows that it is
responsible for pixel 230x150 it can determine the depth at this point
and then write the right color codes in the buffer.

The computation is asynchronous so all we know is that it has been
queued for execution. Instead of waiting for it to complete we can now
queue the next job.

### reading the buffer

When all kernels have executed the mandelbrot image will be in the
buffer on the device. We of course want in on a file so we need to
read this buffer to our main memory. We do not have direct access to
the buffer but can en-queue a task that will copy the buffer to a
buffer that we have prepared in main memory. Once we have the image in
memory we can write it to a file.

There are several way to write a buffer to a file and we will now do
it the fun way. Instead of writing a buffer, byte by byte, to a file
we will open a file and map it to memory using `mmap()`. The trick is
to first map the file (that is empty) to a buffer in memory and then
queue an event that copies the `global_buffer` to our mapped file
directly. I don't know if this is faster but it's more fun :-)


```C
  int total_size = open_and_map_file(x0, y0, incr, width, height, depth, &fd, &file_map, &host_buffer);
```

For now we hide the details of how this mapping is done. In the end we
will have a file descriptor, `fd`, a pointer to the the map-ed region,
`file_map` and a pointer to where the buffer starts, `host_buffer`
(the file has a header in the beginning so the buffer starts a bit
later).

The interesting thing is the pointer to the buffer area since this is
what we need when we en-queue our read command. The `CL_TRUE` flag
specifies that we want the command to be blocking i.e. we will not
proceed unless the command has finished. 


```C
  clEnqueueReadBuffer(cmd_queue, global_buffer, CL_TRUE, 0, image_size, host_buffer, 0, NULL, NULL);
```

### sync the file

We should now have the image in our `host_buffer` which is of course
the memory map of our file. The only thing we need to do now is to
synchronize the file and close it; file operations that we hide in a procedure. 

```C
  sync_and_close_file(fd, file_map, total_size);
```

### done

The only thing left is to clean up (and I guess that this will done
anyway since we terminate the process but it looks professional).

```C
  clReleaseMemObject(global_buffer);
  clReleaseCommandQueue(cmd_queue);
  clReleaseContext(context);
```

## The OpenCL-C code

The code described so far is only the boiler plate code that is needed
on the host side to create the computation. The actual computation of
the Mandelbrot set is done in a kernel process and is described in
`mandelbrot.cl`. The program is written in OpenCL-C which is a more
well defined, and a bit limited, version of C. We don't have to go
through the whole program but the interesting part is in how the kernel
program knows its position in the computation.


```C
__kernel void render(__global char *out, int max, double x0, double y0, double k) {
```

The `__kernel` directive specifies that this procedure can be an entry
point to the program. We never return anything so our procedure is
declared as `void`. The arguments to `render()` are the arguments that
we provided when we constructed the kernel but there is noting there
that tells us which pixel we are responsible for.

The magic is in the three rows that follows: we read the `x` and `y`
position using the function call `get_global_id()`. Since this is a 2D
computation we should have two values there. We also read the total
size of the width dimension since we need this to find the right
position in the `global_buffer`.

```C
      int x = get_global_id(0);
      int y = get_global_id(1);
      size_t width = get_global_size(0);
```

The rest of the code in `mandelbrot.cl` is (almost) plain C and there
is no magic there.


## The things we left out

OK, we left some parts out so let's have a quick look at them.

### create the context

In order to create a context we first have to find the `platform` that
we should use and then find the right device. If we have several
platforms or more than one device on a platform, and want to choose
the best device this could be tricky. In our case it's quite straight
forward and is done in a sequence of queries.

First we find all platforms of our host:

```C
  clGetPlatformIDs(MAX_PLATFORMS, &platform_id, &num_platforms);
```

Then we find the devices on the platform of our choice (the only one):

```C
  clGetDeviceIDs(platform_id[pl], CL_DEVICE_TYPE_DEFAULT, MAX_DEVICES, &device_id, &num_devices);
```

In the end we create the context on the device we have chosen:

```C
  *device = device_id[dev];

  *context = clCreateContext(NULL, 1, device, NULL, NULL, &err);
```


### building the program

The only tricky thing with the building of the program is that we
first have to read the file and place it in a `source` memory
buffer. We can provide an array of program sources and there are some
options on how these are stored but in our case it's straight forward.

```C
  *program = clCreateProgramWithSource(context, 1, (const char**)&source, NULL, &err);
```

Once we have a `program` we can do the compilation of the
program. There are number of parameters here, we could for example
provide compiler directives, but in our case we can do with the
default.

```C
  clBuildProgram(*program, 0, NULL, NULL, NULL, NULL);
```

### the image file

We did some tricks in order to store the image in a file. We open a
file and then extend it in length so that our image will fit. When we
have forced the file to be of the right size we `mmap()` the file. The
only tricky part here is that the length of the header can vary in
size, we need to know where the image is supposed to start.

The code would be simpler if we simply opened a file and wrote to it
but this might be quicker since the operating system can synchronize
who pages at a time (don't know if it makes a difference).


