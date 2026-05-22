all: generated.macosx.x86.32.ceramic generated.macosx.x86.64.ceramic

CERAMIC_BINDGEN_FLAGS = -x objective-c \
    -import libc \
    -import cocoa.foundation \
    -import cocoa.appkit \
    -import cocoa.applicationservices \
    -import cocoa.quartzcore \
    -match /QTKit

generated.macosx.x86.32.ceramic: generated_from.h
	ceramic-bindgen -arch i386 $(CERAMIC_BINDGEN_FLAGS) -o $@ $<
generated.macosx.x86.64.ceramic: generated_from.h
	ceramic-bindgen -arch x86_64 $(CERAMIC_BINDGEN_FLAGS) -o $@ $<

