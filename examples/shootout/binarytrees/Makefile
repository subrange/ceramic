
all : ceramic_binarytrees.exe c_binarytrees.exe cpp_binarytrees.exe

ceramic_binarytrees.exe : binarytrees.crm
	ceramic -no-exceptions -o ceramic_binarytrees.exe binarytrees.crm

c_binarytrees.exe : binarytrees.c
	clang -O3 -I/opt/local/include/apr-1 -o c_binarytrees.exe binarytrees.c -L/opt/local/include -lapr-1

cpp_binarytrees.exe : binarytrees.cpp
	clang++ -O3 -I/opt/local/include -o cpp_binarytrees.exe binarytrees.cpp


clean :
	rm -f ceramic_binarytrees.exe
	rm -f c_binarytrees.exe
	rm -f cpp_binarytrees.exe
