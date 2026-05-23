ifdef IOS_SDK_ROOT
all: generated.macosx.x86.32.crm generated.macosx.x86.64.crm generated.macosx.arm.crm
else
all: generated.macosx.x86.32.crm generated.macosx.x86.64.crm
endif

CERAMIC_BINDGEN_FLAGS = \
    -import libc \
    -import unix \
    -import cocoa.corefoundation
CERAMIC_BINDGEN_FLAGS_X86 = \
    -match /CoreServices
CERAMIC_BINDGEN_FLAGS_ARM = \
    -match /MobileCoreServices

generated.macosx.x86.32.crm: generated_from_x86.h
	ceramic-bindgen -arch i386 $(CERAMIC_BINDGEN_FLAGS_X86) -o $@ $<
generated.macosx.x86.64.crm: generated_from_x86.h
	ceramic-bindgen -arch x86_64 $(CERAMIC_BINDGEN_FLAGS_X86) -o $@ $<

generated.macosx.arm.crm: generated_from_arm.h
	ceramic-bindgen -arch armv6 -isysroot $(IOS_SDK_ROOT) $(CERAMIC_BINDGEN_FLAGS_ARM) -o $@ $<

