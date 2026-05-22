ifdef IOS_SDK_ROOT
all: generated.macosx.x86.32.ceramic generated.macosx.x86.64.ceramic generated.macosx.arm.ceramic
else
all: generated.macosx.x86.32.ceramic generated.macosx.x86.64.ceramic
endif

CERAMIC_BINDGEN_FLAGS = \
    -import libc \
    -import unix \
    -import cocoa.corefoundation \
    -import cocoa.coreservices

CERAMIC_BINDGEN_FLAGS_X86 = \
    -match /ApplicationServices
CERAMIC_BINDGEN_FLAGS_ARM = \
    -match /CoreGraphics \
    -match /CoreText

generated.macosx.x86.32.ceramic: generated_from_x86.h
	ceramic-bindgen -arch i386 $(CERAMIC_BINDGEN_FLAGS_X86) -o $@ $<
generated.macosx.x86.64.ceramic: generated_from_x86.h
	ceramic-bindgen -arch x86_64 $(CERAMIC_BINDGEN_FLAGS_X86) -o $@ $<

generated.macosx.arm.ceramic: generated_from_arm.h
	ceramic-bindgen -arch armv6 -isysroot $(IOS_SDK_ROOT) $(CERAMIC_BINDGEN_FLAGS_ARM) -o $@ $<
