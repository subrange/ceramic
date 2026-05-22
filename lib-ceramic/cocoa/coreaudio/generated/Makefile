ifdef IOS_SDK_ROOT
all: generated.macosx.x86.32.ceramic generated.macosx.x86.64.ceramic generated.macosx.arm.ceramic
else
all: generated.macosx.x86.32.ceramic generated.macosx.x86.64.ceramic
endif

CERAMIC_BINDGEN_FLAGS = \
    -import libc \
    -import unix \
    -import cocoa.corefoundation \
    -match /CoreAudio

generated.macosx.x86.32.ceramic: generated_from_x86.h
	ceramic-bindgen -arch i386 $(CERAMIC_BINDGEN_FLAGS) -o $@ $<
generated.macosx.x86.64.ceramic: generated_from_x86.h
	ceramic-bindgen -arch x86_64 $(CERAMIC_BINDGEN_FLAGS) -o $@ $<

generated.macosx.arm.ceramic: generated_from_arm.h
	ceramic-bindgen -arch armv6 -isysroot $(IOS_SDK_ROOT) $(CERAMIC_BINDGEN_FLAGS) -o $@ $<

