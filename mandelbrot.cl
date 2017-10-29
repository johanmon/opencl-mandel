void set(__global char *out, int r, int g, int b) {
	 out[0] = r;
	 out[1] = g;
	 out[2] = b;
}

void color(int depth, int max,  __global char *out) {

  // depth is in the span [0 - (max-1)]

  // this will smoothen the depth (optional, try whithout)
  // depth = convert_int_rtz(log((float)(depth+1))*max/log((float)max));

  if(depth < 0 || depth > 256 ) 
    set(out, 255,0,0); // set to red to detect error
  
  int index = ((float)depth/max)*256*3;

  // index is in the span [0 - (3*256-1)]
  
  int x = index/256;	 // x is [0,1,2]
  
  int y = index%256;     // y is [0-255]
  
  switch(x) {
  case 0 :
    set(out, 0, 0, y);    // 
    break;
  case 1 :
    set(out, 0, y, 255);    //
    break;
  case 2 :
    set(out, y, 255, 255);    //
    break;
  } 
}




// probe will return a depth [0, max-1]

int probe(double r, double i, int max) {

  double zr = 0.0;   
  double zi = 0.0;    

  int depth = 0;

  while(depth < max) {
    double zr2 = zr*zr;
    double zi2 = zi*zi;
    double a2 = zr2 + zi2;
    if(a2 < 4.0) {
      depth++;
      zi = 2*zr*zi + i;
      zr = zr2 - zi2 + r;
    } else {
      return depth;
    }
  }
  return 0;
}


__kernel void render(__global char *out, int max, double x0, double y0, double k) {
  int x = get_global_id(0);
  int y = get_global_id(1);
  size_t width = get_global_size(0);

  // The (x0,y0) imaginary coordinate is the left upper corner, the
  // value k is the increment for each pixle. The max value is the
  // maximum depth that is probed.

  // Calculate the coordiantes for this kernel.
  double  xi = x0 + k*x;
  double  yi = y0 - k*y;

  // The position in the output bufffer.
  int indx =  3*width*y + x*3;

  // set(&out[indx], 0,0,0); // set to black incase we fail
  
  int depth = probe(xi, yi, max);

  color(depth, max,  &out[indx]);
}

