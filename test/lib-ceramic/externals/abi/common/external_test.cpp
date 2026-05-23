#include "../external_test.hpp"

struct Struct1 {
    uint64_t a;
};

struct Struct2 {
    uint32_t a;
};

struct Struct3 {
    uint32_t a;
    uint32_t b;
};

struct Struct4 {
    uint32_t a;
    uint32_t b;
    uint32_t c;
};

struct Struct5 {
    uint32_t a;
    uint32_t b;
    uint64_t c;
};

struct Struct6 {
    uint32_t a;
    uint64_t b;
};

struct Struct7 {
    uint32_t a;
    uint64_t b;
    uint32_t c;
};

struct Struct8 {
    uint64_t a;
    uint64_t b;
    uint64_t c;
};

struct Struct9 {
    uint32_t a;
    double b;
};

struct Struct10 {
    uint32_t a;
    float b;
};

struct Struct11 {
    double a;
    uint32_t b;
};

struct Struct12 {
    float a;
    uint32_t b;
};

struct Struct13 {
    float a;
};

struct Struct14 {
    float a;
    float b;
};

struct Struct15 {
    float a;
    float b;
    float c;
};

struct Struct16 {
    float a;
    float b;
    float c;
    float d;
    float e;
};

struct Struct17 {
    double a;
};

struct Struct18 {
    double a;
    double b;
};

struct Struct19 {
    double a;
    double b;
    double c;
};

struct Struct20 {
    double a;
    double b;
    double c;
    double d;
};

struct Struct21 {
    double a;
    double b;
    double c;
    double d;
    double e;
};

union Union22 {
    uint32_t a;
    float b;
};

union Union23 {
    uint32_t a;
    double b;
};

struct Struct26 {
    uint8_t a;
};

struct Struct27 {
    uint16_t a;
};

struct Struct28 {
    uint8_t a;
    uint8_t b;
};

struct Struct29 {
    uint16_t a;
    uint16_t b;
};

struct Struct30 {
    uint8_t a;
    uint8_t b;
    uint8_t c;
};

//
// arguments
//

extern "C" {

void c_scalar(uint32_t x, bool y, float z, double w) {
    printf("%x %x %.6a %.13a\n", x, y, z, w);
    fflush(stdout);
}

void c_1(struct Struct1 x) {
    printf("%" PRIx64 "\n", x.a);
    fflush(stdout);
}

void c_2(struct Struct2 x) {
    printf("%x\n", x.a);
    fflush(stdout);
}

void c_3(struct Struct3 x) {
    printf("%x %x\n", x.a, x.b);
    fflush(stdout);
}

void c_4(struct Struct4 x) {
    printf("%x %x %x\n", x.a, x.b, x.c);
    fflush(stdout);
}

void c_5(struct Struct5 x) {
    printf("%x %x %" PRIx64 "\n", x.a, x.b, x.c);
    fflush(stdout);
}

void c_6(struct Struct6 x) {
    printf("%x %" PRIx64 "\n", x.a, x.b);
    fflush(stdout);
}

void c_7(struct Struct7 x) {
    printf("%x %" PRIx64 " %x\n", x.a, x.b, x.c);
    fflush(stdout);
}

void c_8(struct Struct8 x) {
    printf("%" PRIx64 " %" PRIx64 " %" PRIx64 "\n", x.a, x.b, x.c);
    fflush(stdout);
}

void c_9(struct Struct9 x) {
    printf("%x %.13a\n", x.a, x.b);
    fflush(stdout);
}

void c_10(struct Struct10 x) {
    printf("%x %.6a\n", x.a, x.b);
    fflush(stdout);
}

void c_11(struct Struct11 x) {
    printf("%.13a %x\n", x.a, x.b);
    fflush(stdout);
}

void c_12(struct Struct12 x) {
    printf("%.6a %x\n", x.a, x.b);
    fflush(stdout);
}

void c_13(struct Struct13 x) {
    printf("%.6a\n", x.a);
    fflush(stdout);
}

void c_14(struct Struct14 x) {
    printf("%.6a %.6a\n", x.a, x.b);
    fflush(stdout);
}

void c_15(struct Struct15 x) {
    printf("%.6a %.6a %.6a\n", x.a, x.b, x.c);
    fflush(stdout);
}

void c_16(struct Struct16 x) {
    printf("%.6a %.6a %.6a %.6a %.6a\n", x.a, x.b, x.c, x.d, x.e);
    fflush(stdout);
}

void c_17(struct Struct17 x) {
    printf("%.13a\n", x.a);
    fflush(stdout);
}

void c_18(struct Struct18 x) {
    printf("%.13a %.13a\n", x.a, x.b);
    fflush(stdout);
}

void c_19(struct Struct19 x) {
    printf("%.13a %.13a %.13a\n", x.a, x.b, x.c);
    fflush(stdout);
}

void c_20(struct Struct20 x) {
    printf("%.13a %.13a %.13a %.13a\n", x.a, x.b, x.c, x.d);
    fflush(stdout);
}

void c_21(struct Struct21 x) {
    printf("%.13a %.13a %.13a %.13a %.13a\n", x.a, x.b, x.c, x.d, x.e);
    fflush(stdout);
}

void c_22(union Union22 x, int tag) {
    switch (tag) {
    case 0:
        printf("0: %x\n", x.a);
        break;
    case 1:
        printf("1: %.6a\n", x.b);
        break;
    }
    fflush(stdout);
}

void c_23(union Union23 x, int tag) {
    switch (tag) {
    case 0:
        printf("0: %x\n", x.a);
        break;
    case 1:
        printf("1: %.13a\n", x.b);
        break;
    }
    fflush(stdout);
}

void c_26(struct Struct26 x) {
    printf("%x\n", x.a);
    fflush(stdout);
}

void c_27(struct Struct27 x) {
    printf("%x\n", x.a);
    fflush(stdout);
}

void c_28(struct Struct28 x) {
    printf("%x %x\n", x.a, x.b);
    fflush(stdout);
}

void c_29(struct Struct29 x) {
    printf("%x %x\n", x.a, x.b);
    fflush(stdout);
}

void c_30(struct Struct30 x) {
    printf("%x %x %x\n", x.a, x.b, x.c);
    fflush(stdout);
}

//
// return
//

uint32_t c_return_int(void) {
    return 0xC1A4C1A4U;
}

bool c_return_bool(void) {
    return true;
}

float c_return_float(void) {
    HEX_FLOAT_VARIABLE(r, C1A4C0, 123);
    return r;
}

double c_return_double(void) {
    HEX_DOUBLE_VARIABLE(r, C1A4C1A4C1A4C, 123);
    return r;
}

struct Struct1 c_return_1(void) {
    struct Struct1 r = { 0xC1A4C1A4C1A4C1A4ULL };
    return r;
}

struct Struct2 c_return_2(void) {
    struct Struct2 r = { 0xC1A4C1A4 };
    return r;
}

struct Struct3 c_return_3(void) {
    struct Struct3 r = { 0xC1A4C1A4, 0x12345678 };
    return r;
}

struct Struct4 c_return_4(void) {
    struct Struct4 r = { 0xC1A4C1A4, 0x12345678, 0xABCDABCD };
    return r;
}

struct Struct5 c_return_5(void) {
    struct Struct5 r = { 0xC1A4C1A4, 0x12345678, 0xABCDABCDABCDABCDULL };
    return r;
}

struct Struct6 c_return_6(void) {
    struct Struct6 r = { 0xC1A4C1A4, 0xABCDABCDABCDABCDULL };
    return r;
}

struct Struct7 c_return_7(void) {
    struct Struct7 r = { 0xC1A4C1A4, 0xABCDABCDABCDABCDULL, 0x12345678 };
    return r;
}

struct Struct8 c_return_8(void) {
    struct Struct8 r = { 0xC1A4C1A4C1A4C1A4ULL, 0xABCDABCDABCDABCDULL, 0x1234567812345678ULL };
    return r;
}

struct Struct9 c_return_9(void) {
    HEX_DOUBLE_VARIABLE(f, C1A4C1A4C1A4C, 123);
    struct Struct9 r = { 0xC1A4C1A4, f };
    return r;
}

struct Struct10 c_return_10(void) {
    HEX_FLOAT_VARIABLE(f, C1A4C0, 123);
    struct Struct10 r = { 0xC1A4C1A4, f };
    return r;
}

struct Struct11 c_return_11(void) {
    HEX_DOUBLE_VARIABLE(f, C1A4C1A4C1A4C, 123);
    struct Struct11 r = { f, 0xC1A4C1A4 };
    return r;
}

struct Struct12 c_return_12(void) {
    HEX_FLOAT_VARIABLE(f, C1A4C0, 123);
    struct Struct12 r = { f, 0xC1A4C1A4 };
    return r;
}

struct Struct13 c_return_13(void) {
    HEX_FLOAT_VARIABLE(f, C1A4C0, 123);
    struct Struct13 r = { f };
    return r;
}

struct Struct14 c_return_14(void) {
    HEX_FLOAT_VARIABLE(f, C1A4C0, 123);
    HEX_FLOAT_VARIABLE(g, ABCDEE, 99);
    struct Struct14 r = { f, g };
    return r;
}

struct Struct15 c_return_15(void) {
    HEX_FLOAT_VARIABLE(f, C1A4C0, 123);
    HEX_FLOAT_VARIABLE(g, ABCDEE, 99);
    HEX_FLOAT_VARIABLE(h, 010102, 10);
    struct Struct15 r = { f, g, h };
    return r;
}

struct Struct16 c_return_16(void) {
    HEX_FLOAT_VARIABLE(f, C1A4C0, 123);
    HEX_FLOAT_VARIABLE(g, ABCDEE, 99);
    HEX_FLOAT_VARIABLE(h, 010102, 10);
    HEX_FLOAT_VARIABLE(i, 020202, 20);
    HEX_FLOAT_VARIABLE(j, 030302, 30);
    struct Struct16 r = { f, g, h, i, j };
    return r;
}

struct Struct17 c_return_17(void) {
    HEX_DOUBLE_VARIABLE(f, C1A4C1A4C1A4C, 123);
    struct Struct17 r = { f };
    return r;
}

struct Struct18 c_return_18(void) {
    HEX_DOUBLE_VARIABLE(f, C1A4C1A4C1A4C, 123);
    HEX_DOUBLE_VARIABLE(g, ABCDEFABCDEFA, 99);
    struct Struct18 r = { f, g };
    return r;
}

struct Struct19 c_return_19(void) {
    HEX_DOUBLE_VARIABLE(f, C1A4C1A4C1A4C, 123);
    HEX_DOUBLE_VARIABLE(g, ABCDEFABCDEFA, 99);
    HEX_DOUBLE_VARIABLE(h, 0101010101010, 10);
    struct Struct19 r = { f, g, h };
    return r;
}

struct Struct20 c_return_20(void) {
    HEX_DOUBLE_VARIABLE(f, C1A4C1A4C1A4C, 123);
    HEX_DOUBLE_VARIABLE(g, ABCDEFABCDEFA, 99);
    HEX_DOUBLE_VARIABLE(h, 0101010101010, 10);
    HEX_DOUBLE_VARIABLE(i, 0202020202020, 20);
    struct Struct20 r = { f, g, h, i };
    return r;
}

struct Struct21 c_return_21(void) {
    HEX_DOUBLE_VARIABLE(f, C1A4C1A4C1A4C, 123);
    HEX_DOUBLE_VARIABLE(g, ABCDEFABCDEFA, 99);
    HEX_DOUBLE_VARIABLE(h, 0101010101010, 10);
    HEX_DOUBLE_VARIABLE(i, 0202020202020, 20);
    HEX_DOUBLE_VARIABLE(j, 0303030303030, 30);
    struct Struct21 r = { f, g, h, i, j };
    return r;
}

union Union22 c_return_22(int tag) {
    union Union22 r;
    HEX_FLOAT_VARIABLE(f, C1A4C0, 123);
    switch (tag) {
    case 0:
        r.a = 0xC1A4C1A4;
        break;
    case 1:
        r.b = f;
        break;
    default:
        abort();
    }
    return r;
}

union Union23 c_return_23(int tag) {
    union Union23 r;
    HEX_DOUBLE_VARIABLE(f, C1A4C1A4C1A4C, 123);
    switch (tag) {
    case 0:
        r.a = 0xC1A4C1A4;
        break;
    case 1:
        r.b = f;
        break;
    default:
        abort();
    }
    return r;
}

struct Struct26 c_return_26(void) {
    struct Struct26 r = { 0xC1 };
    return r;
}

struct Struct27 c_return_27(void) {
    struct Struct27 r = { 0xC1A4 };
    return r;
}

struct Struct28 c_return_28(void) {
    struct Struct28 r = { 0xC1, 0xA4 };
    return r;
}

struct Struct29 c_return_29(void) {
    struct Struct29 r = { 0xC1A4, 0xABCD };
    return r;
}

struct Struct30 c_return_30(void) {
    struct Struct30 r = { 0xC1, 0xA4, 0xAB };
    return r;
}

//
// ceramic entry points
//

void ceramic_scalar(uint32_t x, bool y, float z, double w);

void ceramic_1(struct Struct1 x);

void ceramic_2(struct Struct2 x);

void ceramic_3(struct Struct3 x);

void ceramic_4(struct Struct4 x);

void ceramic_5(struct Struct5 x);

void ceramic_6(struct Struct6 x);

void ceramic_7(struct Struct7 x);

void ceramic_8(struct Struct8 x);

void ceramic_9(struct Struct9 x);

void ceramic_10(struct Struct10 x);

void ceramic_11(struct Struct11 x);

void ceramic_12(struct Struct12 x);

void ceramic_13(struct Struct13 x);

void ceramic_14(struct Struct14 x);

void ceramic_15(struct Struct15 x);

void ceramic_16(struct Struct16 x);

void ceramic_17(struct Struct17 x);

void ceramic_18(struct Struct18 x);

void ceramic_19(struct Struct19 x);

void ceramic_20(struct Struct20 x);

void ceramic_21(struct Struct21 x);

void ceramic_22(union Union22 x, int tag);

void ceramic_23(union Union23 x, int tag);

void ceramic_26(struct Struct26 x);

void ceramic_27(struct Struct27 x);

void ceramic_28(struct Struct28 x);

void ceramic_29(struct Struct29 x);

void ceramic_30(struct Struct30 x);

//
// return
//

uint32_t ceramic_return_int(void);

bool ceramic_return_bool(void);

float ceramic_return_float(void);

double ceramic_return_double(void);

struct Struct1 ceramic_return_1(void);

struct Struct2 ceramic_return_2(void);

struct Struct3 ceramic_return_3(void);

struct Struct4 ceramic_return_4(void);

struct Struct5 ceramic_return_5(void);

struct Struct6 ceramic_return_6(void);

struct Struct7 ceramic_return_7(void);

struct Struct8 ceramic_return_8(void);

struct Struct9 ceramic_return_9(void);

struct Struct10 ceramic_return_10(void);

struct Struct11 ceramic_return_11(void);

struct Struct12 ceramic_return_12(void);

struct Struct13 ceramic_return_13(void);

struct Struct14 ceramic_return_14(void);

struct Struct15 ceramic_return_15(void);

struct Struct16 ceramic_return_16(void);

struct Struct17 ceramic_return_17(void);

struct Struct18 ceramic_return_18(void);

struct Struct19 ceramic_return_19(void);

struct Struct20 ceramic_return_20(void);

struct Struct21 ceramic_return_21(void);

union Union22 ceramic_return_22(int tag);

union Union23 ceramic_return_23(int tag);

struct Struct26 ceramic_return_26(void);

struct Struct27 ceramic_return_27(void);

struct Struct28 ceramic_return_28(void);

struct Struct29 ceramic_return_29(void);

struct Struct30 ceramic_return_30(void);

void c_to_ceramic(void) {
    printf("\nPassing C arguments to Ceramic:\n");
    fflush(stdout);

    ceramic_scalar(c_return_int(), c_return_bool(), c_return_float(), c_return_double());
    ceramic_1(c_return_1());
    ceramic_2(c_return_2());
    ceramic_3(c_return_3());
    ceramic_4(c_return_4());
    ceramic_5(c_return_5());
    ceramic_6(c_return_6());
    ceramic_7(c_return_7());
    ceramic_8(c_return_8());
    ceramic_9(c_return_9());
    ceramic_10(c_return_10());
    ceramic_11(c_return_11());
    ceramic_12(c_return_12());
    ceramic_13(c_return_13());
    ceramic_14(c_return_14());
    ceramic_15(c_return_15());
    ceramic_16(c_return_16());
    ceramic_17(c_return_17());
    ceramic_18(c_return_18());
    ceramic_19(c_return_19());
    ceramic_20(c_return_20());
    ceramic_21(c_return_21());

    ceramic_22(c_return_22(0), 0);
    ceramic_22(c_return_22(1), 1);

    ceramic_23(c_return_23(0), 0);
    ceramic_23(c_return_23(1), 1);

    ceramic_26(c_return_26());
    ceramic_27(c_return_27());
    ceramic_28(c_return_28());
    ceramic_29(c_return_29());
    ceramic_30(c_return_30());

    printf("\nPassing Ceramic return values to C:\n");
    fflush(stdout);

    c_scalar(ceramic_return_int(), ceramic_return_bool(), ceramic_return_float(), ceramic_return_double());

    c_1(ceramic_return_1());

    c_2(ceramic_return_2());

    c_3(ceramic_return_3());

    c_4(ceramic_return_4());

    c_5(ceramic_return_5());

    c_6(ceramic_return_6());

    c_7(ceramic_return_7());

    c_8(ceramic_return_8());

    c_9(ceramic_return_9());

    c_10(ceramic_return_10());

    c_11(ceramic_return_11());

    c_12(ceramic_return_12());

    c_13(ceramic_return_13());

    c_14(ceramic_return_14());

    c_15(ceramic_return_15());

    c_16(ceramic_return_16());

    c_17(ceramic_return_17());

    c_18(ceramic_return_18());

    c_19(ceramic_return_19());

    c_20(ceramic_return_20());

    c_21(ceramic_return_21());

    c_22(ceramic_return_22(0), 0);
    c_22(ceramic_return_22(1), 1);

    c_23(ceramic_return_23(0), 0);
    c_23(ceramic_return_23(1), 1);

    c_26(ceramic_return_26());
    c_27(ceramic_return_27());
    c_28(ceramic_return_28());
    c_29(ceramic_return_29());
    c_30(ceramic_return_30());
}

}
