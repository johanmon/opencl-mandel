module Mandelbrot

using Images
using ImageView
using Colors
using FileIO


function probe(p, d) 
    z = 0 + 0im
    for n = 0:(d-1)
        if abs(z) < 2.0 
            z = z^2 + p
        else
            return n
        end
    end
    return 0
end

function color(n, d)
    i = Int(trunc(d/3))
    x = div(n,i) ## x is [0,1,2]
    if x == 0 
        return RGB24(0,0,n/255)
    elseif x == 1
        y = (n -i)/255
        return RGB24(0,y,1-y )
    else
        y = (n -2i)/255
        return RGB24(y,1-y,0)
    end
end


function mandel(w, h, z, k, d)

    image = Array{RGB24,2}(h, w)
    
    for i = 1:h, j = 1:w
        pix = z + ((j-1)*k) +  ((i-1)*-k)im
        image[i,j] = color(probe(pix, d), d)
    end
    image
end


function render(x, y, k, depth, width, height) 
    println("plot $x $y step $k")
    image = mandel(width, height, (x + (y)im), k, depth)
    view(image)
end

function demo()
    (x,y,xn) = mandel()
    (w,h,k) = small(x, xn)
    render(x, y, k, 100, w, h)    
end

function demo_waves()
    (x,y,xn) = waves()
    (w,h,k) = huge(x, xn)
    render(x, y, k, 768, w, h)    
end




function small(x, xn)
    width = 960
    height = 540
    (width, height, (xn-x)/width)
end

function large(x, xn)
    width = 1920
    height = 1080
    (width, height, (xn-x)/width)
end
    
function huge(x, xn)
    width = 3840
    height = 2160
    (width, height, (xn-x)/width)
end

function mandel()
    (-2.6,1.2,1.6)
end

function waves()
    (-0.14,0.85,-0.13)
end

function forest()
    (-0.136,0.85,-0.134)
end




end
