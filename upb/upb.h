/*
** This file contains shared definitions that are widely used across upb.
**
** This is a mixed C/C++ interface that offers a full API to both languages.
** See the top-level README for more information.
*/

#ifndef UPB_H_
#define UPB_H_

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#include <memory>
namespace upb {
class Arena;
class Status;
template <int N> class InlinedArena;
}
#endif

/* UPB_INLINE: inline if possible, emit standalone code if required. */
#ifdef __cplusplus
#define UPB_INLINE inline
#elif defined (__GNUC__)
#define UPB_INLINE static __inline__
#else
#define UPB_INLINE static
#endif

/* Hints to the compiler about likely/unlikely branches. */
#define UPB_LIKELY(x) __builtin_expect((x),1)

/* Define UPB_BIG_ENDIAN manually if you're on big endian and your compiler
 * doesn't provide these preprocessor symbols. */
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define UPB_BIG_ENDIAN
#endif

/* Macros for function attributes on compilers that support them. */
#ifdef __GNUC__
#define UPB_FORCEINLINE __inline__ __attribute__((always_inline))
#define UPB_NOINLINE __attribute__((noinline))
#define UPB_NORETURN __attribute__((__noreturn__))
#else  /* !defined(__GNUC__) */
#define UPB_FORCEINLINE
#define UPB_NOINLINE
#define UPB_NORETURN
#endif

#if __STDC_VERSION__ >= 199901L || __cplusplus >= 201103L
/* C99/C++11 versions. */
#include <stdio.h>
#define _upb_snprintf snprintf
#define _upb_vsnprintf vsnprintf
#define _upb_va_copy(a, b) va_copy(a, b)
#elif defined __GNUC__
/* A few hacky workarounds for functions not in C89.
 * For internal use only!
 * TODO(haberman): fix these by including our own implementations, or finding
 * another workaround.
 */
#define _upb_snprintf __builtin_snprintf
#define _upb_vsnprintf __builtin_vsnprintf
#define _upb_va_copy(a, b) __va_copy(a, b)
#else
#error Need implementations of [v]snprintf and va_copy
#endif

#ifdef __cplusplus
#if __cplusplus >= 201103L || defined(__GXX_EXPERIMENTAL_CXX0X__) || \
    (defined(_MSC_VER) && _MSC_VER >= 1900)
// C++11 is present
#else
#error upb requires C++11 for C++ support
#endif
#endif

#define UPB_MAX(x, y) ((x) > (y) ? (x) : (y))
#define UPB_MIN(x, y) ((x) < (y) ? (x) : (y))

#define UPB_UNUSED(var) (void)var

/* UPB_ASSERT(): in release mode, we use the expression without letting it be
 * evaluated.  This prevents "unused variable" warnings. */
#ifdef NDEBUG
#define UPB_ASSERT(expr) do {} while (false && (expr))
#else
#define UPB_ASSERT(expr) assert(expr)
#endif

/* UPB_ASSERT_DEBUGVAR(): assert that uses functions or variables that only
 * exist in debug mode.  This turns into regular assert. */
#define UPB_ASSERT_DEBUGVAR(expr) assert(expr)

#ifdef __GNUC__
#define UPB_UNREACHABLE() do { assert(0); __builtin_unreachable(); } while(0)
#else
#define UPB_UNREACHABLE() do { assert(0); } while(0)
#endif

/* upb_status *****************************************************************/

/* upb_status represents a success or failure status and error message.
 * It owns no resources and allocates no memory, so it should work
 * even in OOM situations. */

/* The maximum length of an error message before it will get truncated. */
#define UPB_STATUS_MAX_MESSAGE 127

typedef struct {
  bool ok;
  char msg[UPB_STATUS_MAX_MESSAGE];  /* Error message; NULL-terminated. */
} upb_status;

#ifdef __cplusplus
extern "C" {
#endif

const char *upb_status_errmsg(const upb_status *status);
bool upb_ok(const upb_status *status);

/* Any of the functions that write to a status object allow status to be NULL,
 * to support use cases where the function's caller does not care about the
 * status message. */
void upb_status_clear(upb_status *status);
void upb_status_seterrmsg(upb_status *status, const char *msg);
void upb_status_seterrf(upb_status *status, const char *fmt, ...);
void upb_status_vseterrf(upb_status *status, const char *fmt, va_list args);

UPB_INLINE void upb_status_setoom(upb_status *status) {
  upb_status_seterrmsg(status, "out of memory");
}

#ifdef __cplusplus
}  /* extern "C" */

class upb::Status {
 public:
  Status() { upb_status_clear(&status_); }

  upb_status* ptr() { return &status_; }

  /* Returns true if there is no error. */
  bool ok() const { return upb_ok(&status_); }

  /* Guaranteed to be NULL-terminated. */
  const char *error_message() const { return upb_status_errmsg(&status_); }

  /* The error message will be truncated if it is longer than
   * UPB_STATUS_MAX_MESSAGE-4. */
  void SetErrorMessage(const char *msg) { upb_status_seterrmsg(&status_, msg); }
  void SetFormattedErrorMessage(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    upb_status_vseterrf(&status_, fmt, args);
    va_end(args);
  }

  /* Resets the status to a successful state with no message. */
  void Clear() { upb_status_clear(&status_); }

 private:
  upb_status status_;
};

#endif  /* __cplusplus */

/** upb_alloc *****************************************************************/

/* A upb_alloc is a possibly-stateful allocator object.
 *
 * It could either be an arena allocator (which doesn't require individual
 * free() calls) or a regular malloc() (which does).  The client must therefore
 * free memory unless it knows that the allocator is an arena allocator. */

struct upb_alloc;
typedef struct upb_alloc upb_alloc;

/* A malloc()/free() function.
 * If "size" is 0 then the function acts like free(), otherwise it acts like
 * realloc().  Only "oldsize" bytes from a previous allocation are preserved. */
typedef void *upb_alloc_func(upb_alloc *alloc, void *ptr, size_t oldsize,
                             size_t size);

struct upb_alloc {
  upb_alloc_func *func;
};

UPB_INLINE void *upb_malloc(upb_alloc *alloc, size_t size) {
  UPB_ASSERT(alloc);
  return alloc->func(alloc, NULL, 0, size);
}

UPB_INLINE void *upb_realloc(upb_alloc *alloc, void *ptr, size_t oldsize,
                             size_t size) {
  UPB_ASSERT(alloc);
  return alloc->func(alloc, ptr, oldsize, size);
}

UPB_INLINE void upb_free(upb_alloc *alloc, void *ptr) {
  assert(alloc);
  alloc->func(alloc, ptr, 0, 0);
}

/* The global allocator used by upb.  Uses the standard malloc()/free(). */

extern upb_alloc upb_alloc_global;

/* Functions that hard-code the global malloc.
 *
 * We still get benefit because we can put custom logic into our global
 * allocator, like injecting out-of-memory faults in debug/testing builds. */

UPB_INLINE void *upb_gmalloc(size_t size) {
  return upb_malloc(&upb_alloc_global, size);
}

UPB_INLINE void *upb_grealloc(void *ptr, size_t oldsize, size_t size) {
  return upb_realloc(&upb_alloc_global, ptr, oldsize, size);
}

UPB_INLINE void upb_gfree(void *ptr) {
  upb_free(&upb_alloc_global, ptr);
}

/* upb_arena ******************************************************************/

/* upb_arena is a specific allocator implementation that uses arena allocation.
 * The user provides an allocator that will be used to allocate the underlying
 * arena blocks.  Arenas by nature do not require the individual allocations
 * to be freed.  However the Arena does allow users to register cleanup
 * functions that will run when the arena is destroyed.
 *
 * A upb_arena is *not* thread-safe.
 *
 * You could write a thread-safe arena allocator that satisfies the
 * upb_alloc interface, but it would not be as efficient for the
 * single-threaded case. */

typedef void upb_cleanup_func(void *ud);

struct upb_arena;
typedef struct upb_arena upb_arena;

#ifdef __cplusplus
extern "C" {
#endif

/* Creates an arena from the given initial block (if any -- n may be 0).
 * Additional blocks will be allocated from |alloc|.  If |alloc| is NULL, this
 * is a fixed-size arena and cannot grow. */
upb_arena *upb_arena_init(void *mem, size_t n, upb_alloc *alloc);
void upb_arena_free(upb_arena *a);
bool upb_arena_addcleanup(upb_arena *a, upb_cleanup_func *func, void *ud);
size_t upb_arena_bytesallocated(const upb_arena *a);

UPB_INLINE upb_alloc *upb_arena_alloc(upb_arena *a) { return (upb_alloc*)a; }

/* Convenience wrappers around upb_alloc functions. */

UPB_INLINE void *upb_arena_malloc(upb_arena *a, size_t size) {
  return upb_malloc(upb_arena_alloc(a), size);
}

UPB_INLINE void *upb_arena_realloc(upb_arena *a, void *ptr, size_t oldsize,
                                   size_t size) {
  return upb_realloc(upb_arena_alloc(a), ptr, oldsize, size);
}

UPB_INLINE upb_arena *upb_arena_new() {
  return upb_arena_init(NULL, 0, &upb_alloc_global);
}

#ifdef __cplusplus
}  /* extern "C" */

class upb::Arena {
 public:
  /* A simple arena with no initial memory block and the default allocator. */
  Arena() : ptr_(upb_arena_new(), upb_arena_free) {}

  upb_arena* ptr() { return ptr_.get(); }

  /* Allows this arena to be used as a generic allocator.
   *
   * The arena does not need free() calls so when using Arena as an allocator
   * it is safe to skip them.  However they are no-ops so there is no harm in
   * calling free() either. */
  upb_alloc *allocator() { return upb_arena_alloc(ptr_.get()); }

  /* Add a cleanup function to run when the arena is destroyed.
   * Returns false on out-of-memory. */
  bool AddCleanup(upb_cleanup_func *func, void *ud) {
    return upb_arena_addcleanup(ptr_.get(), func, ud);
  }

  /* Total number of bytes that have been allocated.  It is undefined what
   * Realloc() does to &arena_ counter. */
  size_t BytesAllocated() const { return upb_arena_bytesallocated(ptr_.get()); }

 private:
  std::unique_ptr<upb_arena, decltype(&upb_arena_free)> ptr_;
};

#endif

/* upb::InlinedArena **********************************************************/

/* upb::InlinedArena seeds the arenas with a predefined amount of memory.  No
 * heap memory will be allocated until the initial block is exceeded.
 *
 * These types only exist in C++ */

#ifdef __cplusplus

template <int N> class upb::InlinedArena : public upb::Arena {
 public:
  InlinedArena() : ptr_(upb_arena_new(&initial_block_, N, &upb_alloc_global)) {}

  upb_arena* ptr() { return ptr_.get(); }

 private:
  InlinedArena(const InlinedArena*) = delete;
  InlinedArena& operator=(const InlinedArena*) = delete;

  std::unique_ptr<upb_arena, decltype(&upb_arena_free)> ptr_;
  char initial_block_[N];
};

#endif  /* __cplusplus */

/* Constants ******************************************************************/

/* Generic function type. */
typedef void upb_func();

/* A list of types as they are encoded on-the-wire. */
typedef enum {
  UPB_WIRE_TYPE_VARINT      = 0,
  UPB_WIRE_TYPE_64BIT       = 1,
  UPB_WIRE_TYPE_DELIMITED   = 2,
  UPB_WIRE_TYPE_START_GROUP = 3,
  UPB_WIRE_TYPE_END_GROUP   = 4,
  UPB_WIRE_TYPE_32BIT       = 5
} upb_wiretype_t;

/* The types a field can have.  Note that this list is not identical to the
 * types defined in descriptor.proto, which gives INT32 and SINT32 separate
 * types (we distinguish the two with the "integer encoding" enum below). */
typedef enum {
  /* Types stored in 1 byte. */
  UPB_TYPE_BOOL     = 1,
  /* Types stored in 4 bytes. */
  UPB_TYPE_FLOAT    = 2,
  UPB_TYPE_INT32    = 3,
  UPB_TYPE_UINT32   = 4,
  UPB_TYPE_ENUM     = 5,  /* Enum values are int32. */
  /* Types stored as pointers (probably 4 or 8 bytes). */
  UPB_TYPE_STRING   = 6,
  UPB_TYPE_BYTES    = 7,
  UPB_TYPE_MESSAGE  = 8,
  /* Types stored as 8 bytes. */
  UPB_TYPE_DOUBLE   = 9,
  UPB_TYPE_INT64    = 10,
  UPB_TYPE_UINT64   = 11
} upb_fieldtype_t;

/* The repeated-ness of each field; this matches descriptor.proto. */
typedef enum {
  UPB_LABEL_OPTIONAL = 1,
  UPB_LABEL_REQUIRED = 2,
  UPB_LABEL_REPEATED = 3
} upb_label_t;

/* How integers should be encoded in serializations that offer multiple
 * integer encoding methods. */
typedef enum {
  UPB_INTFMT_VARIABLE = 1,
  UPB_INTFMT_FIXED = 2,
  UPB_INTFMT_ZIGZAG = 3   /* Only for signed types (INT32/INT64). */
} upb_intfmt_t;

/* Descriptor types, as defined in descriptor.proto. */
typedef enum {
  UPB_DESCRIPTOR_TYPE_DOUBLE   = 1,
  UPB_DESCRIPTOR_TYPE_FLOAT    = 2,
  UPB_DESCRIPTOR_TYPE_INT64    = 3,
  UPB_DESCRIPTOR_TYPE_UINT64   = 4,
  UPB_DESCRIPTOR_TYPE_INT32    = 5,
  UPB_DESCRIPTOR_TYPE_FIXED64  = 6,
  UPB_DESCRIPTOR_TYPE_FIXED32  = 7,
  UPB_DESCRIPTOR_TYPE_BOOL     = 8,
  UPB_DESCRIPTOR_TYPE_STRING   = 9,
  UPB_DESCRIPTOR_TYPE_GROUP    = 10,
  UPB_DESCRIPTOR_TYPE_MESSAGE  = 11,
  UPB_DESCRIPTOR_TYPE_BYTES    = 12,
  UPB_DESCRIPTOR_TYPE_UINT32   = 13,
  UPB_DESCRIPTOR_TYPE_ENUM     = 14,
  UPB_DESCRIPTOR_TYPE_SFIXED32 = 15,
  UPB_DESCRIPTOR_TYPE_SFIXED64 = 16,
  UPB_DESCRIPTOR_TYPE_SINT32   = 17,
  UPB_DESCRIPTOR_TYPE_SINT64   = 18
} upb_descriptortype_t;

typedef enum {
  UPB_SYNTAX_PROTO2 = 2,
  UPB_SYNTAX_PROTO3 = 3
} upb_syntax_t;

/* All the different kind of well known type messages. For simplicity of check,
 * number wrappers and string wrappers are grouped together. Make sure the
 * order and merber of these groups are not changed.
 */
typedef enum {
  UPB_WELLKNOWN_UNSPECIFIED,
  UPB_WELLKNOWN_ANY,
  UPB_WELLKNOWN_DURATION,
  UPB_WELLKNOWN_TIMESTAMP,
  /* number wrappers */
  UPB_WELLKNOWN_DOUBLEVALUE,
  UPB_WELLKNOWN_FLOATVALUE,
  UPB_WELLKNOWN_INT64VALUE,
  UPB_WELLKNOWN_UINT64VALUE,
  UPB_WELLKNOWN_INT32VALUE,
  UPB_WELLKNOWN_UINT32VALUE,
  /* string wrappers */
  UPB_WELLKNOWN_STRINGVALUE,
  UPB_WELLKNOWN_BYTESVALUE,
  UPB_WELLKNOWN_BOOLVALUE,
  UPB_WELLKNOWN_VALUE,
  UPB_WELLKNOWN_LISTVALUE,
  UPB_WELLKNOWN_STRUCT
} upb_wellknowntype_t;

extern const uint8_t upb_desctype_to_fieldtype[];

#endif  /* UPB_H_ */
