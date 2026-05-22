all: generated.macosx.arm.ceramic

CERAMIC_BINDGEN_FLAGS = -x objective-c \
    -import libc \
    -import cocoa.corefoundation \
    -import cocoa.applicationservices \
    -import cocoa.foundation \
    -import cocoa.quartzcore \
    -match /UIKit

generated.macosx.arm.ceramic: generated_from.h
	ceramic-bindgen -arch armv6 -isysroot $(IOS_SDK_ROOT) $(CERAMIC_BINDGEN_FLAGS) -o $@ $<

