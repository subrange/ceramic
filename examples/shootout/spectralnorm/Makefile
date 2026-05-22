
all : ceramic_spectralnorm_non_simd.exe ceramic_spectralnorm.exe c_spectralnorm.exe cpp_spectralnorm.exe

ceramic_spectralnorm_non_simd.exe : spectralnorm_non_simd.crm
	ceramic -no-exceptions -o ceramic_spectralnorm_non_simd.exe spectralnorm_non_simd.crm -lm

ceramic_spectralnorm.exe : spectralnorm.crm
	ceramic -no-exceptions -o ceramic_spectralnorm.exe spectralnorm.crm -lm

c_spectralnorm.exe : spectralnorm.c
	clang -O3 -o c_spectralnorm.exe spectralnorm.c

cpp_spectralnorm.exe : spectralnorm.cpp
	clang++ -O3 -o cpp_spectralnorm.exe spectralnorm.cpp


clean :
	rm -f ceramic_spectralnorm_non_simd.exe
	rm -f ceramic_spectralnorm.exe
	rm -f c_spectralnorm.exe
	rm -f cpp_spectralnorm.exe
