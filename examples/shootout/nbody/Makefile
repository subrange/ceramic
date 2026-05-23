
all : ceramic_nbody.exe c_nbody.exe cpp_nbody.exe

ceramic_nbody.exe : nbody.crm
	ceramic -no-exceptions -o ceramic_nbody.exe nbody.crm -lm

c_nbody.exe : nbody.c
	clang -O3 -o c_nbody.exe nbody.c

cpp_nbody.exe : nbody.cpp
	clang++ -O3 -o cpp_nbody.exe nbody.cpp


clean :
	rm -f ceramic_nbody.exe
	rm -f ceramic_nbody_simd.exe
	rm -f c_nbody.exe
	rm -f cpp_nbody.exe
