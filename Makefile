C = gcc
CFLAGS = -w
OPENCL = -L/opt/amdgpu-pro/lib/x86_64-linux-gnu/ -lOpenCL


all: image.jpg

image.jpg: image.ppm
	convert image.ppm image.jpg

image.ppm: mandelbrot.cl mandelbrot
	./mandelbrot 1920 1080 1024 -0.14  0.8422  0.0000035


mandelbrot: main.c 
	$(C) $(CFLAGS) -o mandelbrot main.c  $(OPENCL)


%.o: %.c %.h
	$(C) $(CFLAGS) -c $<

clean:
	rm -f mandelbrot output.bmp *.o
