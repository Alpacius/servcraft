#ifndef     MISCUTILS_H_
#define     MISCUTILS_H_

#define     unlikely(expr_)     __builtin_expect(!!(expr_), 0)
#define     likely(expr_)       __builtin_expect(!!(expr_), 1)

// XXX Beware! Arguments may be evaluated more than once
// XXX Yes they are ugly. 

#define     _primitive_cat(_arg_, ...) _arg_ ## __VA_ARGS__
#define     _if(_cond_) _primitive_cat(_if_, _cond_)
#define     _if_1(_term_, ...) __VA_ARGS__
#define     _if_2(_term_, ...) _term_
#define     _if_(_cond_) _primitive_cat(_if__, _cond_)
#define     _if__2(_term_, ...) __VA_ARGS__
#define     _if__3(_term_, ...) _term_
#define     _if__(_cond_) _primitive_cat(_if___, _cond_)
#define     _if___0(_term_, ...) __VA_ARGS__
#define     _if___1(_term_, ...) _term_

#define     cat_(x, y) x ## y
#define     cat2_(x, y) cat_(x, y)

#define __VA_NARG__(...) \
    __VA_NARG_(_0, ## __VA_ARGS__, __RSEQ_N())
#define __VA_NARG_(...) \
    __VA_ARG_N(__VA_ARGS__)
#define __VA_ARG_N( _0, \
    _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
    _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
    _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
    _31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
    _41,_42,_43,_44,_45,_46,_47,_48,_49,_50, \
    _51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
    _61,_62,_63,N,...) N
#define __RSEQ_N() \
    63, 62, 61, 60,                         \
    59, 58, 57, 56, 55, 54, 53, 52, 51, 50, \
    49, 48, 47, 46, 45, 44, 43, 42, 41, 40, \
    39, 38, 37, 36, 35, 34, 33, 32, 31, 30, \
    29, 28, 27, 26, 25, 24, 23, 22, 21, 20, \
    19, 18, 17, 16, 15, 14, 13, 12, 11, 10, \
    9,  8,  7,  6,  5,  4,  3,  2,  1,  0 

#define    offset_of(type, member) ((size_t) &((type *) 0)->member)
#define    container_of(ptr, type, member) ({ \
     const typeof(((type *) 0)->member) *__mptr = (ptr); \
     (type *) ((char *) __mptr - offset_of(type, member)); })

#endif      // MISCUTILS_H_
