
all : ceramic_mandelbrot_non_simd.exe ceramic_mandelbrot.exe c_mandelbrot.exe cpp_mandelbrot.exe

ceramic_mandelbrot_non_simd.exe : mandelbrot_non_simd.crm
	ceramic -no-exceptions -o ceramic_mandelbrot_non_simd.exe mandelbrot_non_simd.crm -lm

ceramic_mandelbrot.exe : mandelbrot.crm
	ceramic -no-exceptions -o ceramic_mandelbrot.exe mandelbrot.crm -lm

c_mandelbrot.exe : mandelbrot.c
	clang -O3 -o c_mandelbrot.exe mandelbrot.c

cpp_mandelbrot.exe : mandelbrot.cpp
	clang++ -O3 -o cpp_mandelbrot.exe mandelbrot.cpp


clean :
	rm -f ceramic_mandelbrot_non_simd.exe
	rm -f ceramic_mandelbrot.exe
	rm -f c_mandelbrot.exe
	rm -f cpp_mandelbrot.exe
