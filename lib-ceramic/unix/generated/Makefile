# Ceramic Makefile

default :
	@echo "use 'make macosx[32|64]' or 'make IOS_SDK_ROOT=<ROOT> ios' or 'make linux[32|64]' or 'make freebsd[32|64]'."

macosx : macosx64 macosx32

macosx64 :
	ceramic-bindgen -m64 generated_from_macosx.h -o generated.macosx.x86.64.crm

macosx32 :
	ceramic-bindgen -m32 generated_from_macosx.h -o generated.macosx.x86.32.crm

ios:
	ceramic-bindgen -isysroot $(IOS_SDK_ROOT) -arch armv6 generated_from_macosx.h -o generated.macosx.arm.crm

linux32 :
	ceramic-bindgen -D_FILE_OFFSET_BITS=64 generated_from_linux.h -o generated.linux.32.crm

linux64 :
	ceramic-bindgen generated_from_linux.h -o generated.linux.64.crm

freebsd32 :
	ceramic-bindgen generated_from_freebsd.h -o generated.freebsd.32.crm

freebsd64 :
	ceramic-bindgen generated_from_freebsd.h -o generated.freebsd.64.crm
