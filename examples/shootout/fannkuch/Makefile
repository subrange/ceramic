
all : ceramic_fannkuch.exe c_fannkuch.exe cpp_fannkuch.exe

ceramic_fannkuch.exe : fannkuch.crm
	ceramic -no-exceptions -o ceramic_fannkuch.exe fannkuch.crm

c_fannkuch.exe : fannkuch.c
	clang -O3 -o c_fannkuch.exe fannkuch.c

cpp_fannkuch.exe : fannkuch.cpp
	clang++ -O3 -o cpp_fannkuch.exe fannkuch.cpp


clean :
	rm -f ceramic_fannkuch.exe
	rm -f c_fannkuch.exe
	rm -f cpp_fannkuch.exe
