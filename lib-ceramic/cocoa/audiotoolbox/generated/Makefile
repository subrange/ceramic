ifdef IOS_SDK_ROOT
all: generated.macosx.x86.32.crm generated.macosx.x86.64.crm generated.macosx.arm.crm
else
all: generated.macosx.x86.32.crm generated.macosx.x86.64.crm
endif

CERAMIC_BINDGEN_FLAGS = \
    -import libc \
    -import unix \
    -import cocoa.corefoundation \
    -import cocoa.coreaudio \
    -match /AudioToolbox

generated.macosx.x86.32.crm: generated_from.h
	ceramic-bindgen -arch i386 $(CERAMIC_BINDGEN_FLAGS) -o $@ $<
generated.macosx.x86.64.crm: generated_from.h
	ceramic-bindgen -arch x86_64 $(CERAMIC_BINDGEN_FLAGS) -o $@ $<

generated.macosx.arm.crm: generated_from.h
	ceramic-bindgen -arch armv6 -isysroot $(IOS_SDK_ROOT) $(CERAMIC_BINDGEN_FLAGS) -o $@ $<

