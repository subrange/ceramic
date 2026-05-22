# Ceramic Makefile

default :
	@echo "use 'make macosx[32|64]' or 'make IOS_SDK_ROOT=<ROOT> ios' or 'make linux[32|64]' or 'make freebsd[32|64]' or 'make windows' or 'make windows64'."

macosx : macosx64 macosx32

macosx64 :
	ceramic-bindgen -builtins -m64 generated_from_unix.h -o generated.macosx.x86.64.ceramic

macosx32 :
	ceramic-bindgen -builtins -m32 generated_from_unix.h -o generated.macosx.x86.32.ceramic

IOS_GCC=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/gcc

ios:
	ceramic-bindgen -builtins -isysroot $(IOS_SDK_ROOT) -arch armv6 \
	    generated_from_unix.h -o generated.macosx.arm.ceramic

linux32 :
	ceramic-bindgen -builtins -D_FILE_OFFSET_BITS=64 generated_from_unix.h -o generated.linux.32.ceramic

linux64 :
	ceramic-bindgen -builtins generated_from_unix.h -o generated.linux.64.ceramic

freebsd32 :
	ceramic-bindgen -builtins -D_FILE_OFFSET_BITS=64 generated_from_unix.h -o generated.freebsd.32.ceramic

freebsd64 :
	ceramic-bindgen -builtins generated_from_unix.h -o generated.freebsd.64.ceramic

windows : windows32

windows32 :
	ceramic-bindgen -builtins generated_from_win32.h -o generated.windows.32.ceramic

windows64 :
	ceramic-bindgen -builtins generated_from_win32.h -o generated.windows.64.ceramic
