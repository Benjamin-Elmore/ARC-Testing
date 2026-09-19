#pragma once  // Include this header at most once per translation unit.

// ============================================================================
// COLOR-FRIENDLY COMMENT KEY
// * [FLOW]      Important runtime behavior or the main path through the code.
// ? [C++/C]     Syntax or language concept worth pausing to understand.
// ! [CAUTION]   Lifetime, safety, portability, or correctness concern.
// TODO: [FIX]   Existing code issue that must be resolved before this header builds.
//
// Editors with a "Better Comments"-style extension usually color `*`, `?`, `!`,
// and `TODO` differently. Without an extension, the labels still make each kind
// of annotation easy to scan.
// ============================================================================

/*
 * Andrew_Sensor.h
 * ----------------
 * Purpose
 *   This header is a small, reusable sensor framework. It does not talk to a
 *   particular ADC, GPIO pin, I2C peripheral, or SPI peripheral by itself.
 *   Instead, a concrete sensor supplies those hardware-specific operations as
 *   callbacks (most importantly through reader()). The framework then provides
 *   the common life cycle around that callback:
 *
 *       construct/configure -> begin -> read -> validate -> filter -> publish
 *                                      |                         |
 *                                      +---- error/fault path ---+
 *
 * Architecture at a glance
 *   1. Status and State describe the result of an operation and the sensor's
 *      longer-lived condition.
 *   2. Reply is a fixed-buffer text writer for command responses.
 *   3. detail::parse and detail::Binder adapt string command arguments to
 *      strongly typed C++ callback arguments.
 *   4. Sensor<T> owns configuration, callbacks, the latest reading, counters,
 *      and the state machine. T is the numeric type of one reading.
 *   5. Ref erases T so Sensor<float>, Sensor<int>, and Sensor<bool> can be kept
 *      in one non-templated collection.
 *
 * Embedded design choices
 *   - Fixed-size arrays avoid heap allocation after construction. Their limits
 *     are selected with macros below.
 *   - Function pointers keep callbacks inexpensive and predictable, but they
 *     only accept non-capturing lambdas (a lambda that captures local state is
 *     an object and cannot convert to a plain function pointer).
 *   - ctx and user[] provide small per-sensor storage without requiring a new
 *     subclass for every physical device.
 *   - update() is synchronous. Calling code is responsible for invoking it at
 *     the desired sample interval, normally from an ESP-IDF task or main loop.
 *   - No mutex/critical section protects mutable state. If multiple tasks access
 *     one Sensor, the application must serialize those calls. Reply formatting
 *     and arbitrary callbacks also make update()/call() unsuitable for an ISR.
 *
 * Reading note
 *   The file mixes a C-style API (sensor_status_t, SENSOR_OK, and similar names)
 *   with a later C++-style API (Status::Ok, State::Ready, tag::NONE, now_us()).
 *   The latter names are not declared in this file, and __cpluspluc below is a
 *   misspelling of __cplusplus, and the linkage block has no closing brace if it
 *   were enabled. These are existing integration/compile issues, not special C++
 *   syntax. The comments document the intended design without changing those
 *   definitions or the file's behavior.
 */

// ? [C++] Angle brackets select compiler/toolchain headers, not project files.
#include <cstdarg>      // C variadic arguments: va_list, va_start, and va_end.
#include <cstdint>      // Fixed-width integers: uint8_t, uint16_t, uint32_t, int64_t.
#include <cstdio>       // Formatted output: snprintf and vsnprintf.
#include <cstdlib>      // Text-to-number conversion: strtod and strtoll.
#include <cstring>      // C-string comparison: strcmp.
#include <strings.h>    // POSIX case-insensitive comparison: strcasecmp.
#include <type_traits>  // Compile-time type questions such as is_arithmetic_v.
#include <utility>      // Compile-time index sequences used by Binder.

// * [EMBEDDED] ESP-IDF timer declarations provide the platform time source.
#include "esp_timer.h"

// extern "C" requests C linkage for declarations included inside the block.
// It is normally guarded by __cplusplus when a header supports both C and C++.
// ! [CAUTION] The guard is misspelled, so this block is normally inactive.
// TODO: [FIX] Remove this block, or correct the guard and linkage design. There
// is no matching closing brace, and C++ templates/classes cannot have C linkage.
#ifdef __cpluspluc
extern "C" {
#endif

// * [CONFIG] Compile-time capacity limits. A build flag such as
// -DSENSOR_MAX_FILTERS=8 overrides a default before this header is compiled.
// Fixed capacities make RAM use predictable, which matters on a microcontroller.
#ifndef SENSOR_MAX_FILTERS
#define SENSOR_MAX_FILTERS 4  // Maximum filter callbacks stored per sensor.
#endif
#ifndef SENSOR_MAX_METHODS
#define SENSOR_MAX_METHODS 6  // Maximum named command callbacks per sensor.
#endif
#ifndef SENSOR_USER_SLOTS
#define SENSOR_USER_SLOTS  4  // Number of per-sensor float scratch values.
#endif

namespace sensor {

// A namespace groups related C++ names and prevents collisions with names from
// other libraries. A caller would refer to this class as sensor::Sensor<float>,
// or use `using namespace sensor;` in a limited scope.

/*
 * Result of one operation.
 *
 * `typedef enum { ... } sensor_status_t;` is the traditional C spelling for an
 * enumeration plus its type alias. The first enumerator is explicitly zero;
 * the rest increase by one. Status answers "what happened during this call?"
 * and is distinct from State, which answers "what mode is the sensor in?"
 */
typedef enum {
    SENSOR_OK = 0,            // Operation completed successfully.
    SENSOR_ERR_NOT_READY,     // No usable reading/state exists yet.
    SENSOR_ERR_IO,            // Hardware or bus transaction failed.
    SENSOR_ERR_TIMEOUT,       // Operation exceeded its allowed time.
    SENSOR_ERR_RANGE,         // Reading fell outside configured bounds.
    SENSOR_ERR_NO_READER,     // update() has no hardware reader callback.
    SENSOR_ERR_DISABLED,      // Operation refused because sensor is disabled.
    SENSOR_ERR_FAULT,         // Sensor is latched in its fault state.
    SENSOR_ERR_ARG,           // Command argument was missing or invalid.
    SENSOR_ERR_NOT_FOUND,     // Requested named method does not exist.
    SENSOR_ERR_FULL,          // A fixed-capacity registry has no free slot.
} sensor_status_t;

// `static inline` is useful in a header: every translation unit may include its
// own internal copy, while inline permits repeated identical definitions. The
// returned string literals have static lifetime and must not be modified.
static inline const char *sensor_status_str(sensor_status_t s) {
    // ? [C] switch chooses one branch by enum value; each return exits immediately.
    switch (s) {
        case SENSOR_OK:             return "ok";
        case SENSOR_ERR_NOT_READY:  return "not_ready";
        case SENSOR_ERR_IO:         return "io";
        case SENSOR_ERR_TIMEOUT:    return "timeout";
        case SENSOR_ERR_RANGE:      return "range";
        case SENSOR_ERR_NO_READER:  return "no_reader";
        case SENSOR_ERR_DISABLED:   return "disabled";
        case SENSOR_ERR_FAULT:      return "fault";
        case SENSOR_ERR_ARG:        return "bad_arg";
        case SENSOR_ERR_NOT_FOUND:  return "not_found";
        case SENSOR_ERR_FULL:       return "full";
    }
    return "?";  // Defensive fallback for an invalid/out-of-range enum value.
}

/*
 * Persistent sensor state machine.
 *
 *   UNINIT --begin succeeds--> READY --enough failures--> FAULT
 *      ^                         |                         |
 *      |                         +--- enable(false) ------>+ DISABLED
 *      +---------------- enable(true) from DISABLED ------+
 *
 * With automatic recovery enabled, update() may retry a FAULT sensor and move
 * it back to READY after a successful reading.
 */
typedef enum {
    SENSOR_STATE_UNINIT = 0,  // Constructed/reset; initialization still required.
    SENSOR_STATE_READY,       // Initialized and available for normal sampling.
    SENSOR_STATE_FAULT,       // Consecutive failures crossed the configured limit.
    SENSOR_STATE_DISABLED,    // Intentionally prevented from sampling.
} sensor_state_t;

// Human-readable state names are useful for logs, telemetry, and a CLI.
static inline const char *sensor_state_str(sensor_state_t s) {
    // * [DISPLAY] Convert machine state to stable text for logs or telemetry.
    switch (s) {
        case SENSOR_STATE_UNINIT:   return "uninit";
        case SENSOR_STATE_READY:    return "ready";
        case SENSOR_STATE_FAULT:    return "fault";
        case SENSOR_STATE_DISABLED: return "disabled";
    }
    return "?";  // Defensive fallback for a corrupted/unknown state value.
}

/*
 * Sensor classification flags stored together in one uint32_t bitmask.
 *
 * `(1u << n)` shifts one unsigned bit into position n. Flags can be combined
 * with bitwise OR:
 *
 *     SENSOR_TAG_ANALOG | SENSOR_TAG_POWER
 *
 * `has(mask)` later requires every requested bit; `any(mask)` requires at least
 * one. Bits 16 and above are reserved here for project-specific categories.
 */
#define SENSOR_TAG_NONE     0u          // No classification bits set.
#define SENSOR_TAG_ANALOG   (1u << 0)   // Analog source, such as an ADC channel.
#define SENSOR_TAG_DIGITAL  (1u << 1)   // Binary/digital source, such as GPIO.
#define SENSOR_TAG_I2C      (1u << 2)   // Device communicates over I2C.
#define SENSOR_TAG_SPI      (1u << 3)   // Device communicates over SPI.
#define SENSOR_TAG_POWER    (1u << 4)   // Voltage/current/power-related sensor.
#define SENSOR_TAG_ENV      (1u << 5)   // Environmental measurement category.
#define SENSOR_TAG_CRITICAL (1u << 6)   // Application considers failure critical.
#define SENSOR_TAG_USER0    (1u << 16)  // First project-specific extension bit.

/* ------------------------------- Reply -------------------------------
 * A small adapter around a caller-owned character buffer. It appends formatted
 * text without allocating memory and always keeps a terminating '\0' when the
 * capacity is nonzero. The caller owns the buffer and must keep it alive for as
 * long as the Reply object uses it.
 */
class Reply {
public:
    // ? [C++] A constructor initializer list initializes members before the body.
    // constructor body runs. Starting with '\0' makes the buffer an empty C
    // string. `size_t` is the unsigned type used for object/buffer sizes.
    Reply(char* buf, size_t cap) : buf_(buf), cap_(cap) { if (buf_ && cap_) buf_[0] = '\0'; }

    // * [SAFETY] GCC/Clang checks the format string/arguments like printf.
    // Here argument 2 is fmt and argument 3 begins the `...` arguments because
    // the hidden `this` pointer is counted as argument 1 for a member function.
    __attribute__((format(printf, 2, 3)))
    Reply& print(const char* fmt, ...) {
        // ? [C++] Returning Reply& enables: reply.print("x").print("y").
        // ! [BOUNDS] Refuse writes when no buffer or no room remains for '\0'.
        if (!buf_ || len_ + 1 >= cap_) return *this;

        // ? [C] va_list/va_start/va_end are the mechanism for reading `...`.
        // vsnprintf writes at most the remaining capacity and reports how many
        // characters it wanted to write, which lets us detect truncation.
        va_list ap;  // Cursor over the unnamed variadic arguments.
        va_start(ap, fmt);  // Start reading immediately after named arg `fmt`.
        const int w = vsnprintf(buf_ + len_, cap_ - len_, fmt, ap);  // Append safely.
        va_end(ap);  // Required cleanup for every successful va_start call.

        // * [STATE] Advance length; clamp it when vsnprintf reports truncation.
        if (w > 0) len_ = (static_cast<size_t>(w) >= cap_ - len_) ? cap_ - 1 : len_ + static_cast<size_t>(w);
        return *this;  // Return this object, not a copy.
    }

    // ? [C++] Trailing const promises these calls do not mutate Reply.
    const char* c_str() const { return buf_ ? buf_ : ""; }  // Never return nullptr.
    bool        empty() const { return len_ == 0; }           // No text appended yet.

private:
    // ? [STYLE] A trailing underscore marks private object state.
    char*  buf_;      // Non-owning pointer to caller-provided writable bytes.
    size_t cap_;      // Total buffer capacity, including final '\0'.
    size_t len_ = 0;  // Number of meaningful characters currently stored.
};

/* -------------------- argument parsing + signature binding --------------------
 * Everything in detail is an implementation helper, not the public API. This
 * layer turns CLI-style text such as {"3.3", "true"} into the typed arguments
 * expected by a registered method callback.
 */
namespace detail {

/*
 * Parse one null-terminated C string into T.
 *
 * This is a function template: the compiler creates a specialized function for
 * each T that is actually used. `if constexpr` chooses a branch at compile time,
 * so invalid branches for that T are discarded rather than executed at runtime.
 */
template <typename T>
inline bool parse(const char* s, T& out) {
    // ! [INPUT] Reject nullptr and empty text; `!*s` means s[0] == '\0'.
    if (!s || !*s) return false;

    // ? [C++] This branch exists only when T is exactly `const char*`.
    if constexpr (std::is_same_v<T, const char*>) {
        // ! [LIFETIME] No copy: out points into caller-owned argument text.
        out = s;  // Assign the pointer itself, not the characters it references.
        return true;
    } else if constexpr (std::is_same_v<T, bool>) {
        // ? [C] strcasecmp compares case-insensitively; zero means equal.
        if (!strcasecmp(s, "1") || !strcasecmp(s, "on")  || !strcasecmp(s, "true"))  { out = true;  return true; }
        if (!strcasecmp(s, "0") || !strcasecmp(s, "off") || !strcasecmp(s, "false")) { out = false; return true; }
        return false;  // Any other spelling is not accepted as a boolean.
    } else {
        // * [PARSE] strtod/strtoll parse a value and advance `end` past it.
        // the first unparsed character. Integer base 0 accepts decimal, octal,
        // and 0x-prefixed hexadecimal input. `static_cast<T>` makes conversion
        // explicit; this code does not additionally check overflow/range.
        char* end = nullptr;  // The conversion function writes the stopping point.
        if constexpr (std::is_floating_point_v<T>)  // float/double/long double path.
            out = static_cast<T>(strtod(s, &end));
        else                                        // Integral types use signed parse.
            out = static_cast<T>(strtoll(s, &end, 0));
        // * [VALIDATE] A valid argument must consume the entire input string.
        return *end == '\0';
    }
}

// ? [C++] T v{} value-initializes v (normally zero/false). parse_or returns it
// even if parsing fails; callers must run ok_arg first when validity matters.
template <typename T> inline T    parse_or(const char* s) { T v{}; parse(s, v); return v; }
template <typename T> inline bool ok_arg(const char* s)   { T v{}; return parse(s, v); }

/*
 * Convert a non-capturing lambda/callable type into its plain function-pointer
 * type. `decltype(&F::operator())` inspects a lambda's generated call operator;
 * partial template specializations then extract return type R and arguments A.
 * `A...` is a variadic template parameter pack (zero or more types).
 */
template <typename F> struct fn_ptr : fn_ptr<decltype(&F::operator())> {};
// ? [C++] A const lambda call operator has hidden class type C, return R, args A.
template <typename C, typename R, typename... A> struct fn_ptr<R (C::*)(A...) const> { using type = R (*)(A...); };
// ? [C++] A plain function pointer already has the target R (*)(A...) shape.
template <typename R, typename... A>             struct fn_ptr<R (*)(A...)>          { using type = R (*)(A...); };

/*
 * Turn Status(Sensor&, Reply&, A...) into one uniform argv-driven call.
 *
 * Binder is first forward-declared and then specialized only for the supported
 * callback shape. An incompatible method signature therefore fails at compile
 * time instead of being accepted with ambiguous runtime behavior.
 */
template <typename P> struct Binder;

template <typename S, typename... A>
struct Binder<Status (*)(S&, Reply&, A...)> {
    using Ptr = Status (*)(S&, Reply&, A...);  // Preserve the real callback type.

    // I... is the compile-time sequence 0, 1, ... for the argument pack. The
    // fold expression `(condition && ...)` validates every argv entry. The
    // second pack expansion converts argv[I] to each corresponding type in A.
    template <size_t... I>
    static Status expand(Ptr fn, S& s, Reply& r, const char* const* v, std::index_sequence<I...>) {
        // ! [INPUT] Fold with &&: every required argv[I] must parse successfully.
        if (!(ok_arg<std::decay_t<A>>(v[I]) && ...)) return Status::BadArg;
        // * [CALL] Convert each string, then expand all arguments into fn(...).
        return fn(s, r, parse_or<std::decay_t<A>>(v[I])...);
    }

    // All registered callbacks are stored as the same erased void function
    // pointer. reinterpret_cast restores the real type selected at registration.
    // This is compact but low-level: correctness depends on storing and restoring
    // exactly the same signature.
    static Status call(S& s, void (*raw)(), int argc, const char* const* argv, Reply& r) {
        // ! [INPUT] Too few args fail. Extra args are currently accepted/ignored.
        if (argc < static_cast<int>(sizeof...(A))) return Status::BadArg;
        // ! [TYPE] raw must be restored to exactly the type used at registration.
        return expand(reinterpret_cast<Ptr>(raw), s, r, argv, std::index_sequence_for<A...>{});
    }
};

}  // namespace detail

/* ------------------------------- Sensor -------------------------------
 * Sensor<T> is the central abstraction. One instance represents one logical
 * sensor whose processed reading has type T, for example:
 *
 *     Sensor<float> battery("battery", SENSOR_TAG_ANALOG | SENSOR_TAG_POWER);
 *     Sensor<bool>  button("button", SENSOR_TAG_DIGITAL);
 *
 * Because this is a class template and all definitions are in the header, the
 * compiler can generate type-specific code wherever the header is included.
 */
template <typename T>
class Sensor {
    // * [COMPILE TIME] Reject unsupported reading types before firmware runs.
    // Arithmetic includes integral, floating-point, and bool types.
    static_assert(std::is_arithmetic_v<T>, "Sensor<T>: T must be arithmetic");

public:
    /*
     * Callback type aliases.
     *
     * `Status (*)(Sensor&, T&)` reads as "pointer to a function that receives a
     * Sensor reference and an output-value reference, then returns Status."
     * References (`&`) cannot be null and let callbacks modify the real object.
     * These aliases document every extension point accepted by this framework.
     *
     * NOTE: Status and State are intended C++ enum types, but this file currently
     * declares only sensor_status_t and sensor_state_t above. Their use here is
     * therefore an unresolved dependency/inconsistency in the original code.
     */
    using Self     = Sensor;  // Short name for this exact Sensor<T> specialization.
    using InitFn   = Status (*)(Sensor&);             // Initialize hardware.
    using ReadFn   = Status (*)(Sensor&, T&);         // Write one raw sample to T&.
    using CheckFn  = Status (*)(Sensor&, T);          // Validate a raw sample.
    using FilterFn = T      (*)(Sensor&, T);          // Transform one sample.
    using EventFn  = void   (*)(Sensor&, T);          // Observe a good update.
    using ErrorFn  = void   (*)(Sensor&, Status);     // Observe a failed operation.
    using StateFn  = void   (*)(Sensor&, State, State); // Observe old -> new state.
    using CallFn   = Status (*)(Sensor&, void (*)(), int, const char* const*, Reply&);

    // A method is a remotely/locally invokable command attached to this sensor.
    // fn stores the erased user callback; call stores the matching Binder thunk
    // that knows how to parse argv and invoke fn with its original signature.
    struct Method {
        const char* name;  // Non-owning command name, used as the lookup key.
        const char* help;  // Optional non-owning usage/help text.
        void (*fn)();      // User function with its type deliberately erased.
        CallFn call;       // Typed Binder adapter that can safely restore `fn`.
    };

    /*
     * Public scratch storage.
     *
     * ctx is an untyped pointer, commonly aimed at an ESP-IDF driver handle or a
     * project-owned configuration struct. A callback must cast it back to the
     * correct type before use. The compiler cannot verify that cast.
     *
     * user[] is zero-initialized numeric scratch space for values such as filter
     * state, calibration offset, or smoothing coefficient. Named indices (for
     * example ALPHA = 0) are safer and clearer than unexplained numeric indices.
     */
    void* ctx = nullptr;                       // Optional driver/configuration pointer.
    float user[SENSOR_USER_SLOTS] = {};        // Zero-filled filter/calibration slots.

    // ? [C++] The constructor records non-owning pointers to name/context.
    // data must outlive this Sensor. `tag::NONE` is referenced but not declared
    // in this file; SENSOR_TAG_NONE is the C-style constant defined above.
    // TODO: [FIX] Define tag::NONE or replace it with SENSOR_TAG_NONE.
    Sensor(const char* name, uint32_t tags = tag::NONE, void* context = nullptr)
        // ? [C++] Members initialize in declaration order, not visual list order.
        : ctx(context), name_(name), tags_(tags) {}

    // ! [OWNERSHIP] `= delete` forbids copies of non-owning pointers/callbacks.
    // context pointers while appearing to represent an independent sensor.
    Sensor(const Sensor&)            = delete;
    Sensor& operator=(const Sensor&) = delete;

    /* ---------------------- appendable behaviour ----------------------
     * Each setter stores a callback and returns `*this` by reference. Returning
     * the current object enables the fluent/chained configuration style shown in
     * the example at the bottom of the file.
     */
    // * [DRIVER] Install the callback that obtains one raw hardware sample.
    Sensor& reader(ReadFn f)         { read_      = f; return *this; }
    // * [STARTUP] Install optional hardware initialization.
    Sensor& onInit(InitFn f)         { init_      = f; return *this; }
    // * [VALIDATE] Install custom validation beyond the numeric range.
    Sensor& check(CheckFn f)         { check_     = f; return *this; }
    // * [EVENTS] Install observers; each is invoked synchronously inside a call.
    Sensor& onUpdate(EventFn f)      { on_update_ = f; return *this; }
    Sensor& onError(ErrorFn f)       { on_error_  = f; return *this; }
    Sensor& onStateChange(StateFn f) { on_state_  = f; return *this; }

    /*
     * filter(a, b, c) appends any number of filters in source order. The fold
     * expression calls push once per supplied function. static_cast requires
     * each callback to convert to FilterFn, which permits non-capturing generic
     * lambdas when their generated signature is compatible.
     */
    template <typename... F>
    Sensor& filter(F... fns) {
        // ? [C++] Comma-fold: call push(...) once for every item in fns.
        (push(static_cast<FilterFn>(fns)), ...);
        return *this;  // Enable another chained configuration call.
    }

    /*
     * Register a named command, for example:
     *
     *   method("set", [](Self&, Reply&, float value) -> Status { ... })
     *
     * fn_ptr determines the lambda's exact function-pointer type P at compile
     * time. Binder<P> later parses text arguments into P's argument types.
     * Registration fails by setting config_error_ when the name is null, the
     * fixed method array is full, or a method already has that name.
     *
     * `reinterpret_cast` erases the callback signature for storage. This assumes
     * the platform ABI permits converting between function-pointer types and
     * restoring the original type before the call; that is implementation-level
     * behavior and deserves particular care if this code is ported.
     */
    template <typename F>
    Sensor& method(const char* mname, F f, const char* help = nullptr) {
        // ? [C++] Infer the plain function-pointer signature represented by F.
        using P = typename detail::fn_ptr<F>::type;

        // ! [CONFIG] Accept only a name, an available slot, and a unique key.
        if (mname && method_count_ < SENSOR_MAX_METHODS && !find(mname)) {
            // * [REGISTER] Store metadata, erased function, and matching adapter.
            methods_[method_count_++] = Method{mname, help,
                                               reinterpret_cast<void (*)()>(static_cast<P>(f)),
                                               &detail::Binder<P>::call};
        } else {
            config_error_ = true;  // Sticky flag: some requested setup was rejected.
        }
        return *this;  // Keep fluent configuration alive even after an error.
    }

    /* ------------------------------ config ------------------------------
     * These methods describe the sensor and configure framework behavior. They
     * do not access hardware and are normally called once during startup.
     */
    // ! [LIFETIME] Unit/name/help strings are non-owning C-string pointers.
    Sensor& units(const char* u)  { unit_ = u;  return *this; }
    // ? [BITS] `|=` adds flags without removing tags already present.
    Sensor& addTags(uint32_t m)   { tags_ |= m; return *this; }
    // ! [TYPE] Caller and callback must agree on the concrete type behind void*.
    Sensor& context(void* c)      { ctx = c;    return *this; }
    // * [VALIDATE] Inclusive raw-sample bounds checked before filters.
    Sensor& range(T lo, T hi)     { min_ = lo; max_ = hi; has_range_ = true; return *this; }
    // ! [BOUNDS] Store one scratch value; reject an index outside user[].
    Sensor& slot(uint8_t i, float v) { if (i < SENSOR_USER_SLOTS) user[i] = v; else config_error_ = true; return *this; }

    // Enter FAULT after this many consecutive failures. A value of zero disables
    // automatic transition to FAULT. auto_recover controls whether update() may
    // retry reads while currently faulted.
    Sensor& faultAfter(uint16_t consecutive, bool auto_recover = true) {
        fault_after_  = consecutive;   // 0 disables automatic FAULT escalation.
        auto_recover_ = auto_recover;  // true lets FAULT retry on later update().
        return *this;
    }

    /* ------------------------------ runtime ------------------------------ */

    /*
     * Prepare the sensor for sampling.
     *
     * 1. Refuse to initialize a deliberately disabled sensor.
     * 2. Call the optional hardware initialization callback.
     * 3. Route initialization errors through fail().
     * 4. Clear the consecutive-error streak and enter READY.
     *
     * Calling begin() explicitly is optional because update() invokes it while
     * UNINIT. Explicit use can still be useful when startup errors should be
     * reported before the normal sampling loop begins.
     */
    Status begin() {
        // * [GUARD] A deliberate disable takes priority over initialization.
        if (state_ == State::Disabled) return Status::Disabled;

        // ? [C++] `condition ? a : b` chooses one of two expressions.
        // * [STARTUP] No init callback means initialization succeeds by default.
        const Status st = init_ ? init_(*this) : Status::Ok;

        // ! [ERROR] Keep the object uninitialized and record callback failure.
        if (st != Status::Ok) { setState(State::Uninit); return fail(st); }

        consecutive_errors_ = 0;     // A successful begin breaks the error streak.
        setState(State::Ready);       // Notify observers only if state changed.
        return Status::Ok;            // Caller may now proceed with normal use.
    }

    /*
     * Perform one complete synchronous sample cycle.
     *
     * Expected call pattern (timing is owned by the surrounding application):
     *
     *   for (;;) {
     *       sensor.update();
     *       vTaskDelay(pdMS_TO_TICKS(sample_period_ms));
     *   }
     *
     * Data order is significant: reader -> range -> custom check -> filters ->
     * stored value -> update callback. A failed sample leaves the previous good
     * value intact and increments error counters through fail().
     */
    Status update() {
        // * [1: GUARD] DISABLED is intentional, so do not count it as an error.
        // does not count as a sampling error.
        if (state_ == State::Disabled) return Status::Disabled;

        // * [2: INIT] Lazily initialize when the first update arrives.
        if (state_ == State::Uninit) {
            const Status st = begin();       // May invoke the hardware init callback.
            if (st != Status::Ok) return st; // Stop: hardware is not ready to read.
        } else if (state_ == State::Fault && !auto_recover_) {
            return Status::Fault;            // Latched fault: do not touch hardware.
        }

        // * [3: DRIVER] A sample requires a registered hardware reader.
        if (!read_) return fail(Status::NoReader);

        // * [4: READ] Begin with the previous good value as a safe initial value.
        // through T&; its Status indicates whether that output is trustworthy.
        T raw = value_;                    // Local candidate; not committed yet.
        Status st = read_(*this, raw);      // Driver overwrites raw through T&.
        if (st != Status::Ok) return fail(st); // Preserve old value_ after read error.

        // * [5: VALIDATE] Reject impossible samples before stateful filters see them.
        // not contaminate stateful filters such as an exponential moving average.
        if (has_range_ && (raw < min_ || raw > max_)) return fail(Status::OutOfRange);
        // ? [C++] `&&` short-circuits: check_ is called only when non-null.
        if (check_ && (st = check_(*this, raw)) != Status::Ok) return fail(st);

        // * [6: FILTER] Each filter receives the output of the preceding filter.
        for (uint8_t i = 0; i < filter_count_; ++i)
            raw = filters_[i](*this, raw);

        // * [7: COMMIT] Only a fully successful candidate replaces stored state.
        // now_us() is intended to return a monotonically increasing microsecond
        // timestamp but is not defined in this file.
        // TODO: [FIX] Define now_us(), likely around esp_timer_get_time().
        value_               = raw;         // Publish the filtered reading.
        last_update_us_      = now_us();    // Timestamp the successful commit.
        last_status_         = Status::Ok;  // Health now reflects success.
        consecutive_errors_ = 0;           // Break any previous failure streak.
        ++updates_;                         // Count successful samples only.

        // * [8: NOTIFY] Recover state first, then notify the value observer.
        // callback runs last, so observers see fully committed state/counters.
        if (state_ == State::Fault) setState(State::Ready);
        if (on_update_) on_update_(*this, value_);
        return Status::Ok;  // Entire read/validate/filter/commit path succeeded.
    }

    // Look up and execute a registered command. argc is the number of entries in
    // argv; argv points to C strings owned by the caller. Reply receives output.
    Status call(const char* mname, int argc, const char* const* argv, Reply& reply) {
        const Method* m = find(mname);  // nullptr means no matching command.
        // ? [C++] Ternary dispatches the method or produces NotFound.
        return m ? m->call(*this, m->fn, argc, argv, reply) : Status::NotFound;
    }

    // Enabling does not immediately initialize hardware; it moves DISABLED to
    // UNINIT so begin() or the next update() performs initialization.
    Sensor& enable(bool on) {
        if (on) {
            // * [STATE] Re-enabling requires initialization before another read.
            if (state_ == State::Disabled) setState(State::Uninit);
        } else {
            // * [STATE] Disable from any state and notify observers if changed.
            setState(State::Disabled);
        }
        return *this;
    }

    // Clear readings and health history while preserving configuration, callback
    // registration, tags, context, range, and scratch slots. A disabled sensor
    // stays disabled; every other state returns to UNINIT.
    void reset() {
        value_ = T{};                  // Value-initialize: numeric zero or false.
        updates_ = errors_ = 0;        // Chained assignment clears both totals.
        consecutive_errors_ = 0;       // Clear fault-escalation history.
        last_update_us_ = 0;           // No valid success timestamp remains.
        last_status_ = Status::NotReady; // Express that value() is not yet valid.
        // * [STATE] Preserve an intentional disable; reinitialize every other mode.
        if (state_ != State::Disabled) setState(State::Uninit);
    }

    /* ----------------------------- accessors -----------------------------
     * Accessors expose state without giving callers direct write access to the
     * private members. These short functions will usually be inlined by the
     * compiler, so the abstraction has essentially no runtime cost.
     */
    const char* name() const  { return name_; }
    // unit() returns an empty string rather than exposing a null pointer.
    const char* unit() const  { return unit_ ? unit_ : ""; }
    uint32_t    tags() const  { return tags_; }
    State       state() const { return state_; }
    Status      lastStatus() const { return last_status_; }
    bool        configError() const { return config_error_; }

    // value() is meaningful only when hasValue() is true. Before then, value_ is
    // merely T{} (zero/false), which is not necessarily a real sensor reading.
    T        value() const    { return value_; }
    bool     hasValue() const { return updates_ > 0; }
    uint32_t updates() const  { return updates_; }
    uint32_t errors() const   { return errors_; }
    // ageUs() uses -1 as a sentinel for "no successful reading yet." A signed
    // type is required because an unsigned integer cannot represent -1 naturally.
    int64_t  ageUs() const    { return hasValue() ? now_us() - last_update_us_ : -1; }
    // has(m): every bit in m is present. any(m): at least one bit is present.
    bool     has(uint32_t m) const { return (tags_ & m) == m; }
    bool     any(uint32_t m) const { return (tags_ & m) != 0; }

    // methodAt performs no bounds check; caller must ensure i < methodCount().
    uint8_t       methodCount() const       { return method_count_; }
    const Method& methodAt(uint8_t i) const { return methods_[i]; }

    // Compile-time type inspection selects a concise telemetry/display name. An
    // if-constexpr chain emits only the branch applicable to this Sensor<T>.
    static constexpr const char* typeName() {
        if constexpr (std::is_same_v<T, bool>)         return "bool";
        else if constexpr (std::is_floating_point_v<T>) return "float";
        else if constexpr (std::is_signed_v<T>)         return "int";
        else                                            return "uint";
    }

    // Format the current value into caller-owned buffer b of capacity n. Like
    // snprintf, the return value is the number of characters that would have
    // been written, excluding '\0'; it may be >= n when output was truncated.
    int format(char* b, size_t n) const {
        if constexpr (std::is_same_v<T, bool>)          return snprintf(b, n, "%d", value_ ? 1 : 0);
        else if constexpr (std::is_floating_point_v<T>) return snprintf(b, n, "%.4g", static_cast<double>(value_));
        else if constexpr (std::is_signed_v<T>)         return snprintf(b, n, "%ld", static_cast<long>(value_));
        else                                            return snprintf(b, n, "%lu", static_cast<unsigned long>(value_));
    }

private:
    // Append one filter to fixed storage. A null callback or full array records a
    // sticky configuration error instead of writing outside array bounds.
    void push(FilterFn f) {
        if (f && filter_count_ < SENSOR_MAX_FILTERS) filters_[filter_count_++] = f;
        else config_error_ = true;
    }

    // Linear search is appropriate for a deliberately small method array. strcmp
    // returns zero when two null-terminated strings have identical contents.
    const Method* find(const char* mname) const {
        if (!mname) return nullptr;
        for (uint8_t i = 0; i < method_count_; ++i)
            if (strcmp(methods_[i].name, mname) == 0) return &methods_[i];
        return nullptr;
    }

    /*
     * Central failure path. Total errors never decrease except on reset(); the
     * consecutive count resets after success and saturates at UINT16_MAX to avoid
     * wrapping back to zero. Only READY transitions to FAULT here.
     */
    Status fail(Status st) {
        ++errors_;
        if (consecutive_errors_ < UINT16_MAX) ++consecutive_errors_;
        last_status_ = st;
        if (on_error_) on_error_(*this, st);
        if (state_ == State::Ready && fault_after_ && consecutive_errors_ >= fault_after_)
            setState(State::Fault);
        return st;
    }

    // Change state only when necessary and notify after storing the new value.
    // The callback receives both old and new state for logging/telemetry.
    void setState(State to) {
        if (state_ == to) return;
        const State from = state_;
        state_ = to;
        if (on_state_) on_state_(*this, from, to);
    }

    /* Identity and classification. These string pointers are non-owning. */
    const char* name_;
    const char* unit_ = nullptr;
    uint32_t    tags_;

    /* Optional behavior callbacks plus fixed-capacity filter/method tables. */
    InitFn   init_      = nullptr;
    ReadFn   read_      = nullptr;
    CheckFn  check_     = nullptr;
    EventFn  on_update_ = nullptr;
    ErrorFn  on_error_  = nullptr;
    StateFn  on_state_  = nullptr;
    FilterFn filters_[SENSOR_MAX_FILTERS] = {};
    Method   methods_[SENSOR_MAX_METHODS] = {};
    uint8_t  filter_count_ = 0;
    uint8_t  method_count_ = 0;

    /* Validation and fault policy configured before runtime sampling. */
    T        min_{}, max_{};
    bool     has_range_    = false;
    uint16_t fault_after_  = 0;
    bool     auto_recover_ = true;
    bool     config_error_ = false;

    /* Runtime data and health counters. Brace initialization means zero/false. */
    T        value_{};
    State    state_              = State::Uninit;
    Status   last_status_        = Status::NotReady;
    uint32_t updates_            = 0;
    uint32_t errors_             = 0;
    uint16_t consecutive_errors_ = 0;
    int64_t  last_update_us_     = 0;
};

/* ---------------------------- Type-erased Ref ----------------------------
 * Templates with different T are unrelated concrete C++ types, so an ordinary
 * homogeneous array cannot directly contain Sensor<float>, Sensor<int>, and
 * Sensor<bool>. Ref solves that by keeping:
 *
 *   - obj: an untyped pointer to the original Sensor<T>
 *   - one wrapper function pointer for each operation available through Ref
 *
 * This is manual type erasure, conceptually similar to a tiny virtual interface.
 * It is non-owning: the original Sensor<T> must outlive every Ref that points to
 * it. Ref deliberately exposes only common operations that do not mention T.
 */
struct Ref {
    void*       obj;
    const char* (*name)(void*);
    Status      (*update)(void*);
    int         (*format)(void*, char*, size_t);
    Status      (*call)(void*, const char*, int, const char* const*, Reply&);
};

// Build the erased view while T is still known. Each non-capturing lambda becomes
// a function pointer that casts obj back to the correct Sensor<T> and delegates.
// `using S = Sensor<T>` is a local alias that keeps those casts readable.
template <typename T>
inline Ref ref(Sensor<T>& s) {
    using S = Sensor<T>;
    return Ref{
        // Aggregate initialization fills Ref fields in declaration order.
        &s,
        [](void* p) { return static_cast<S*>(p)->name(); },
        [](void* p) { return static_cast<S*>(p)->update(); },
        [](void* p, char* b, size_t n) { return static_cast<S*>(p)->format(b, n); },
        [](void* p, const char* m, int c, const char* const* v, Reply& r) {
            return static_cast<S*>(p)->call(m, c, v, r);
        },
    };
}

} // namespace sensor

/* ---------------------------- Guided usage ----------------------------
 * This block is documentation, not compiled code. It assumes surrounding
 * declarations that are not shown here, including:
 *
 *   using Batt = sensor::Sensor<float>;
 *   Batt batt("battery", SENSOR_TAG_ANALOG | SENSOR_TAG_POWER, &adc_handle);
 *   sensor::Sensor<bool> button("button", SENSOR_TAG_DIGITAL);
 *   enum { ALPHA, EMA, OFFSET };
 *
 * It also uses the intended C++ Status API that is not defined in this header.
 * Read it as the assembly of a processing pipeline, not as a standalone program.
 */
/*
void setup_sensors() {
    // Chained calls all configure and return the same `batt` object:
    //   - display values in volts
    //   - accept only physically plausible values
    //   - enter FAULT after three consecutive failures, with auto-recovery
    //   - initialize scratch slot ALPHA to the EMA smoothing coefficient
    batt.units("V")
        .range(0.0f, 5.5f)
        .faultAfter(3)
        .slot(ALPHA, 0.2f)

        // The reader is the hardware-driver boundary. `auto&` lets the compiler
        // infer Batt& and float& from ReadFn. `out` is an output parameter.
        .reader([](auto& s, auto& out) -> Status {              
            int raw = 0;

            // ctx was configured to point at an ADC handle. static_cast restores
            // that pointer type, and unary * reads the handle value it points to.
            auto unit = *static_cast<adc_oneshot_unit_handle_t*>(s.ctx);
            if (adc_oneshot_read(unit, ADC_CHANNEL_3, &raw) != ESP_OK) return Status::IoError;

            // Convert a 12-bit ADC count to pin voltage, then account for a 2:1
            // divider. The `f` suffix keeps constants in float arithmetic.
            out = (raw / 4095.0f) * 3.3f * 2.0f;
            return Status::Ok;
        })

        // Filters execute left to right. First perform an exponential moving
        // average (EMA), then add a calibration offset.
        .filter(
            [](auto& s, auto in) {                             
                const float a = s.user[ALPHA];

                // On the first good sample, seed EMA directly from input. Later
                // samples move it a fraction `a` toward the newest reading.
                s.user[EMA] = s.updates() ? s.user[EMA] + a * (in - s.user[EMA]) : in;
                return s.user[EMA];
            },
            [](auto& s, auto in) { return in + s.user[OFFSET]; } 
        )

        // Register a typed command: text argv is parsed into float ref before
        // this lambda runs. Calibration adjusts OFFSET so current output equals
        // the supplied reference voltage and writes a response into Reply.
        .method("calibrate", [](Batt& s, Reply& r, float ref) -> Status {
            if (!s.hasValue()) return Status::NotReady;          
            s.user[OFFSET] += ref - s.value();
            r.print("offset=%+.4f", static_cast<double>(s.user[OFFSET]));
            return Status::Ok;
        }, "<ref_v>  trim to a reference")

        // Register another command that validates and updates the EMA factor.
        .method("alpha", [](Batt& s, Reply& r, float a) -> Status {
            if (a <= 0.0f || a > 1.0f) return Status::BadArg;
            s.user[ALPHA] = a;
            r.print("alpha=%.3f", static_cast<double>(a));
            return Status::Ok;
        }, "<0..1>  smoothing factor")

        // Run after every successful value commit. static_cast<double> matches
        // printf-family variadic formatting expectations explicitly.
        .onUpdate([](auto& s, auto v) {
            if (v < 3.4f) ESP_LOGW(s.name(), "low %.2f V", static_cast<double>(v));
        });

    // A digital input uses the same framework with T = bool. Active-low wiring
    // means GPIO level 0 is represented as logical true (button pressed).
    button.reader([](auto&, auto& out) -> Status {
        out = gpio_get_level(GPIO_NUM_0) == 0;
        return Status::Ok;
    });
}
*/
