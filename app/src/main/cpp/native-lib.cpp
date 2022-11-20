#include <jni.h>
#include <string>
#include <sstream>

#include <chrono>

/* Only needed for the sake of this example. */
#include <iostream>
#include <thread>
#include <vector>
#include <numeric>
#include <algorithm>
#include <functional>

/** Check if the argument is non-null, and if so, call obj->ref() and return obj.
 */
template <typename T> static inline T* SkSafeRef(T* obj) {
    if (obj) {
        obj->ref();
    }
    return obj;
}

/** Check if the argument is non-null, and if so, call obj->unref()
 */
template <typename T> static inline void SkSafeUnref(T* obj) {
    if (obj) {
        obj->unref();
    }
}

template <typename T> class sk_sp {
public:
    using element_type = T;

    constexpr sk_sp() : fPtr(nullptr) {}
    constexpr sk_sp(std::nullptr_t) : fPtr(nullptr) {}

    /**
     *  Shares the underlying object by calling ref(), so that both the argument and the newly
     *  created sk_sp both have a reference to it.
     */
    sk_sp(const sk_sp<T>& that) : fPtr(SkSafeRef(that.get())) {}
    template <typename U,
            typename = typename std::enable_if<std::is_convertible<U*, T*>::value>::type>
    sk_sp(const sk_sp<U>& that) : fPtr(SkSafeRef(that.get())) {}

    /**
     *  Move the underlying object from the argument to the newly created sk_sp. Afterwards only
     *  the new sk_sp will have a reference to the object, and the argument will point to null.
     *  No call to ref() or unref() will be made.
     */
    sk_sp(sk_sp<T>&& that) : fPtr(that.release()) {}
    template <typename U,
            typename = typename std::enable_if<std::is_convertible<U*, T*>::value>::type>
    sk_sp(sk_sp<U>&& that) : fPtr(that.release()) {}

    /**
     *  Adopt the bare pointer into the newly created sk_sp.
     *  No call to ref() or unref() will be made.
     */
    explicit sk_sp(T* obj) : fPtr(obj) {}

    /**
     *  Calls unref() on the underlying object pointer.
     */
    ~sk_sp() {
        SkSafeUnref(fPtr);
        fPtr = nullptr;
    }

    sk_sp<T>& operator=(std::nullptr_t) { this->reset(); return *this; }

    /**
     *  Shares the underlying object referenced by the argument by calling ref() on it. If this
     *  sk_sp previously had a reference to an object (i.e. not null) it will call unref() on that
     *  object.
     */
    sk_sp<T>& operator=(const sk_sp<T>& that) {
        if (this != &that) {
            this->reset(SkSafeRef(that.get()));
        }
        return *this;
    }
    template <typename U,
            typename = typename std::enable_if<std::is_convertible<U*, T*>::value>::type>
    sk_sp<T>& operator=(const sk_sp<U>& that) {
        this->reset(SkSafeRef(that.get()));
        return *this;
    }

    /**
     *  Move the underlying object from the argument to the sk_sp. If the sk_sp previously held
     *  a reference to another object, unref() will be called on that object. No call to ref()
     *  will be made.
     */
    sk_sp<T>& operator=(sk_sp<T>&& that) {
        this->reset(that.release());
        return *this;
    }
    template <typename U,
            typename = typename std::enable_if<std::is_convertible<U*, T*>::value>::type>
    sk_sp<T>& operator=(sk_sp<U>&& that) {
        this->reset(that.release());
        return *this;
    }

    T& operator*() const {
        SkASSERT(this->get() != nullptr);
        return *this->get();
    }

    explicit operator bool() const { return this->get() != nullptr; }

    T* get() const { return fPtr; }
    T* operator->() const { return fPtr; }

    /**
     *  Adopt the new bare pointer, and call unref() on any previously held object (if not null).
     *  No call to ref() will be made.
     */
    void reset(T* ptr = nullptr) {
        // Calling fPtr->unref() may call this->~() or this->reset(T*).
        // http://wg21.cmeerw.net/lwg/issue998
        // http://wg21.cmeerw.net/lwg/issue2262
        T* oldPtr = fPtr;
        fPtr = ptr;
        SkSafeUnref(oldPtr);
    }

    /**
     *  Return the bare pointer, and set the internal object pointer to nullptr.
     *  The caller must assume ownership of the object, and manage its reference count directly.
     *  No call to unref() will be made.
     */
    T* release() {
        T* ptr = fPtr;
        fPtr = nullptr;
        return ptr;
    }

    void swap(sk_sp<T>& that) /*noexcept*/ {
        using std::swap;
        swap(fPtr, that.fPtr);
    }

private:
    T*  fPtr;
};

template <typename T> inline void swap(sk_sp<T>& a, sk_sp<T>& b) /*noexcept*/ {
    a.swap(b);
}

template <typename T, typename U> inline bool operator==(const sk_sp<T>& a, const sk_sp<U>& b) {
    return a.get() == b.get();
}
template <typename T> inline bool operator==(const sk_sp<T>& a, std::nullptr_t) /*noexcept*/ {
    return !a;
}
template <typename T> inline bool operator==(std::nullptr_t, const sk_sp<T>& b) /*noexcept*/ {
    return !b;
}

template <typename T, typename U> inline bool operator!=(const sk_sp<T>& a, const sk_sp<U>& b) {
    return a.get() != b.get();
}
template <typename T> inline bool operator!=(const sk_sp<T>& a, std::nullptr_t) /*noexcept*/ {
    return static_cast<bool>(a);
}
template <typename T> inline bool operator!=(std::nullptr_t, const sk_sp<T>& b) /*noexcept*/ {
    return static_cast<bool>(b);
}

template <typename C, typename CT, typename T>
auto operator<<(std::basic_ostream<C, CT>& os, const sk_sp<T>& sp) -> decltype(os << sp.get()) {
    return os << sp.get();
}

/*  Some helper functions for C strings */
static inline bool SkStrStartsWith(const char string[], const char prefixStr[]) {
    return !strncmp(string, prefixStr, strlen(prefixStr));
}
static inline bool SkStrStartsWith(const char string[], const char prefixChar) {
    return (prefixChar == *string);
}

bool SkStrEndsWith(const char string[], const char suffixStr[]) {
    size_t  strLen = strlen(string);
    size_t  suffixLen = strlen(suffixStr);
    return  strLen >= suffixLen &&
            !strncmp(string + strLen - suffixLen, suffixStr, suffixLen);
}

bool SkStrEndsWith(const char string[], const char suffixChar) {
    size_t  strLen = strlen(string);
    if (0 == strLen) {
        return false;
    } else {
        return (suffixChar == string[strLen-1]);
    }
}

int SkStrStartsWithOneOf(const char string[], const char prefixes[]) {
    int index = 0;
    do {
        const char* limit = strchr(prefixes, '\0');
        if (!strncmp(string, prefixes, limit - prefixes)) {
            return index;
        }
        prefixes = limit + 1;
        index++;
    } while (prefixes[0]);
    return -1;
}


template <typename D, typename S> constexpr D SkTo(S s) {
    return static_cast<D>(s);
}
template <typename S> constexpr int      SkToInt(S x)   { return SkTo<int>(x);      }
template <typename S> constexpr uint8_t  SkToU8(S x)    { return SkTo<uint8_t>(x);  }
template <typename S> constexpr uint32_t SkToU32(S x)   { return SkTo<uint32_t>(x); }


static inline int SkStrFind(const char string[], const char substring[]) {
    const char *first = strstr(string, substring);
    if (nullptr == first) return -1;
    return SkToInt(first - &string[0]);
}

static inline int SkStrFindLastOf(const char string[], const char subchar) {
    const char* last = strrchr(string, subchar);
    if (nullptr == last) return -1;
    return SkToInt(last - &string[0]);
}

static inline bool SkStrContains(const char string[], const char substring[]) {
    return (-1 != SkStrFind(string, substring));
}
static inline bool SkStrContains(const char string[], const char subchar) {
    char tmp[2];
    tmp[0] = subchar;
    tmp[1] = '\0';
    return (-1 != SkStrFind(string, tmp));
}

class SkString {
public:
        SkString();
        explicit    SkString(size_t len);
        explicit    SkString(const char text[]);
        SkString(const char text[], size_t len);
        SkString(const SkString&);
        SkString(SkString&&);
        explicit    SkString(const std::string&);
        explicit    SkString(std::string_view);
        ~SkString();

        bool        isEmpty() const { return 0 == fRec->fLength; }
        size_t      size() const { return (size_t) fRec->fLength; }
        const char* c_str() const { return fRec->data(); }
        char operator[](size_t n) const { return this->c_str()[n]; }

        bool equals(const SkString&) const;
        bool equals(const char text[]) const;
        bool equals(const char text[], size_t len) const;

        bool startsWith(const char prefixStr[]) const {
            return SkStrStartsWith(fRec->data(), prefixStr);
        }
        bool startsWith(const char prefixChar) const {
            return SkStrStartsWith(fRec->data(), prefixChar);
        }
        bool endsWith(const char suffixStr[]) const {
            return SkStrEndsWith(fRec->data(), suffixStr);
        }
        bool endsWith(const char suffixChar) const {
            return SkStrEndsWith(fRec->data(), suffixChar);
        }
        bool contains(const char substring[]) const {
            return SkStrContains(fRec->data(), substring);
        }
        bool contains(const char subchar) const {
            return SkStrContains(fRec->data(), subchar);
        }
        int find(const char substring[]) const {
            return SkStrFind(fRec->data(), substring);
        }
        int findLastOf(const char subchar) const {
            return SkStrFindLastOf(fRec->data(), subchar);
        }

        friend bool operator==(const SkString& a, const SkString& b) {
            return a.equals(b);
        }
        friend bool operator!=(const SkString& a, const SkString& b) {
            return !a.equals(b);
        }

        // these methods edit the string

        SkString& operator=(const SkString&);
        SkString& operator=(SkString&&);
        SkString& operator=(const char text[]);

        char* writable_str();
        char& operator[](size_t n) { return this->writable_str()[n]; }

        void reset();
        /** String contents are preserved on resize. (For destructive resize, `set(nullptr, length)`.)
         * `resize` automatically reserves an extra byte at the end of the buffer for a null terminator.
         */
        void resize(size_t len);
        void set(const SkString& src) { *this = src; }
        void set(const char text[]);
        void set(const char text[], size_t len);

        void insert(size_t offset, const SkString& src) { this->insert(offset, src.c_str(), src.size()); }
        void insert(size_t offset, const char text[]);
        void insert(size_t offset, const char text[], size_t len);
        void insertS32(size_t offset, int32_t value);
        void insertS64(size_t offset, int64_t value, int minDigits = 0);
        void insertU32(size_t offset, uint32_t value);
        void insertU64(size_t offset, uint64_t value, int minDigits = 0);
        void insertHex(size_t offset, uint32_t value, int minDigits = 0);

        void append(const SkString& str) { this->insert((size_t)-1, str); }
        void append(const char text[]) { this->insert((size_t)-1, text); }
        void append(const char text[], size_t len) { this->insert((size_t)-1, text, len); }
        void appendS32(int32_t value) { this->insertS32((size_t)-1, value); }
        void appendS64(int64_t value, int minDigits = 0) { this->insertS64((size_t)-1, value, minDigits); }
        void appendU32(uint32_t value) { this->insertU32((size_t)-1, value); }
        void appendU64(uint64_t value, int minDigits = 0) { this->insertU64((size_t)-1, value, minDigits); }
        void appendHex(uint32_t value, int minDigits = 0) { this->insertHex((size_t)-1, value, minDigits); }

        void prepend(const SkString& str) { this->insert(0, str); }
        void prepend(const char text[]) { this->insert(0, text); }
        void prepend(const char text[], size_t len) { this->insert(0, text, len); }
        void prependS32(int32_t value) { this->insertS32(0, value); }
        void prependS64(int32_t value, int minDigits = 0) { this->insertS64(0, value, minDigits); }
        void prependHex(uint32_t value, int minDigits = 0) { this->insertHex(0, value, minDigits); }

        void remove(size_t offset, size_t length);

        SkString& operator+=(const SkString& s) { this->append(s); return *this; }
        SkString& operator+=(const char text[]) { this->append(text); return *this; }
        SkString& operator+=(const char c) { this->append(&c, 1); return *this; }

        /**
         *  Swap contents between this and other. This function is guaranteed
         *  to never fail or throw.
         */
        void swap(SkString& other);

        private:
        struct Rec {
            public:
            constexpr Rec(uint32_t len, int32_t refCnt) : fLength(len), fRefCnt(refCnt) {}
            static sk_sp<Rec> Make(const char text[], size_t len);
            char* data() { return fBeginningOfData; }
            const char* data() const { return fBeginningOfData; }
            void ref() const {
                if (this == &SkString::gEmptyRec) {
                    return;
                }
                this->fRefCnt.fetch_add(+1, std::memory_order_relaxed);
            }
            void unref() const {
                if (this == &SkString::gEmptyRec) {
                    return;
                }
                int32_t oldRefCnt = this->fRefCnt.fetch_add(-1, std::memory_order_acq_rel);
                if (1 == oldRefCnt) {
                    delete this;
                }
            }
            bool unique() const {
                return fRefCnt.load(std::memory_order_acquire) == 1;
            }
#ifdef SK_DEBUG
            int32_t getRefCnt() const;
#endif
            uint32_t fLength; // logically size_t, but we want it to stay 32 bits

            private:
            mutable std::atomic<int32_t> fRefCnt;
            char fBeginningOfData[1] = {'\0'};

            // Ensure the unsized delete is called.
            void operator delete(void* p) { ::operator delete(p); }
        };
        sk_sp<Rec> fRec;

#ifdef SK_DEBUG
        const SkString& validate() const;
#else
        const SkString& validate() const { return *this; }
#endif

        static const Rec gEmptyRec;
};


const SkString::Rec SkString::gEmptyRec(0, 0);

class SkSafeMath {
public:
    SkSafeMath() = default;

    bool ok() const { return fOK; }
    explicit operator bool() const { return fOK; }

    size_t mul(size_t x, size_t y) {
        return sizeof(size_t) == sizeof(uint64_t) ? mul64(x, y) : mul32(x, y);
    }

    size_t add(size_t x, size_t y) {
        size_t result = x + y;
        fOK &= result >= x;
        return result;
    }

    /**
     *  Return a + b, unless this result is an overflow/underflow. In those cases, fOK will
     *  be set to false, and it is undefined what this returns.
     */
    int addInt(int a, int b) {
        if (b < 0 && a < std::numeric_limits<int>::min() - b) {
            fOK = false;
            return a;
        } else if (b > 0 && a > std::numeric_limits<int>::max() - b) {
            fOK = false;
            return a;
        }
        return a + b;
    }

    size_t alignUp(size_t x, size_t alignment) {
        return add(x, alignment - 1) & ~(alignment - 1);
    }

    template <typename T> T castTo(size_t value) {
        return static_cast<T>(value);
    }

    // These saturate to their results
    static size_t Add(size_t x, size_t y);
    static size_t Mul(size_t x, size_t y);
    static size_t Align4(size_t x) {
        SkSafeMath safe;
        return safe.alignUp(x, 4);
    }

private:
    uint32_t mul32(uint32_t x, uint32_t y) {
        uint64_t bx = x;
        uint64_t by = y;
        uint64_t result = bx * by;
        fOK &= result >> 32 == 0;
        return result;
    }

    uint64_t mul64(uint64_t x, uint64_t y) {
        if (x <= std::numeric_limits<uint64_t>::max() >> 32
            && y <= std::numeric_limits<uint64_t>::max() >> 32) {
            return x * y;
        } else {
            auto hi = [](uint64_t x) { return x >> 32; };
            auto lo = [](uint64_t x) { return x & 0xFFFFFFFF; };

            uint64_t lx_ly = lo(x) * lo(y);
            uint64_t hx_ly = hi(x) * lo(y);
            uint64_t lx_hy = lo(x) * hi(y);
            uint64_t hx_hy = hi(x) * hi(y);
            uint64_t result = 0;
            result = this->add(lx_ly, (hx_ly << 32));
            result = this->add(result, (lx_hy << 32));
            fOK &= (hx_hy + (hx_ly >> 32) + (lx_hy >> 32)) == 0;

#if defined(SK_DEBUG) && defined(__clang__) && defined(__x86_64__)
            auto double_check = (unsigned __int128)x * y;
                SkASSERT(result == (double_check & 0xFFFFFFFFFFFFFFFF));
                SkASSERT(!fOK || (double_check >> 64 == 0));
#endif

            return result;
        }
    }
    bool fOK = true;
};

#define SizeOfRec()     (gEmptyRec.data() - (const char*)&gEmptyRec)

sk_sp<SkString::Rec> SkString::Rec::Make(const char text[], size_t len) {
    if (0 == len) {
        return sk_sp<SkString::Rec>(const_cast<Rec*>(&gEmptyRec));
    }

    SkSafeMath safe;
    // We store a 32bit version of the length
    uint32_t stringLen = safe.castTo<uint32_t>(len);
    // Add SizeOfRec() for our overhead and 1 for null-termination
    size_t allocationSize = safe.add(len, SizeOfRec() + sizeof(char));
    // Align up to a multiple of 4
    allocationSize = safe.alignUp(allocationSize, 4);

    safe.ok();

    void* storage = ::operator new (allocationSize);
    sk_sp<Rec> rec(new (storage) Rec(stringLen, 1));
    if (text) {
        memcpy(rec->data(), text, len);
    }
    rec->data()[len] = 0;
    return rec;
}


SkString::SkString() : fRec(const_cast<Rec*>(&gEmptyRec)) {
}

SkString::SkString(size_t len) {
    fRec = Rec::Make(nullptr, len);
}

SkString::SkString(const char text[]) {
    size_t  len = text ? strlen(text) : 0;

    fRec = Rec::Make(text, len);
}

SkString::SkString(const char text[], size_t len) {
    fRec = Rec::Make(text, len);
}

SkString::SkString(const SkString& src) : fRec(src.validate().fRec) {}

SkString::SkString(SkString&& src) : fRec(std::move(src.validate().fRec)) {
    src.fRec.reset(const_cast<Rec*>(&gEmptyRec));
}

SkString::SkString(const std::string& src) {
    fRec = Rec::Make(src.c_str(), src.size());
}

SkString::SkString(std::string_view src) {
    fRec = Rec::Make(src.data(), src.length());
}

SkString::~SkString() {
    this->validate();
}

bool SkString::equals(const SkString& src) const {
    return fRec == src.fRec || this->equals(src.c_str(), src.size());
}

bool SkString::equals(const char text[]) const {
    return this->equals(text, text ? strlen(text) : 0);
}

static inline int sk_careful_memcmp(const void* a, const void* b, size_t len) {
    // When we pass >0 len we had better already be passing valid pointers.
    // So we just need to skip calling memcmp when len == 0.
    if (len == 0) {
        return 0;   // we treat zero-length buffers as "equal"
    }
    return memcmp(a, b, len);
}

bool SkString::equals(const char text[], size_t len) const {
    return fRec->fLength == len && !sk_careful_memcmp(fRec->data(), text, len);
}

SkString& SkString::operator=(const SkString& src) {
    this->validate();
    fRec = src.fRec;  // sk_sp<Rec>::operator=(const sk_sp<Ref>&) checks for self-assignment.
    return *this;
}

SkString& SkString::operator=(SkString&& src) {
    this->validate();

    if (fRec != src.fRec) {
        this->swap(src);
    }
    return *this;
}

SkString& SkString::operator=(const char text[]) {
    this->validate();
    return *this = SkString(text);
}

void SkString::reset() {
    this->validate();
    fRec.reset(const_cast<Rec*>(&gEmptyRec));
}

char* SkString::writable_str() {
    this->validate();

    if (fRec->fLength) {
        if (!fRec->unique()) {
            fRec = Rec::Make(fRec->data(), fRec->fLength);
        }
    }
    return fRec->data();
}

static uint32_t trim_size_t_to_u32(size_t value) {
    if (sizeof(size_t) > sizeof(uint32_t)) {
        if (value > UINT32_MAX) {
            value = UINT32_MAX;
        }
    }
    return (uint32_t)value;
}

static size_t check_add32(size_t base, size_t extra) {
    if (sizeof(size_t) > sizeof(uint32_t)) {
        if (base + extra > UINT32_MAX) {
            extra = UINT32_MAX - base;
        }
    }
    return extra;
}

void SkString::resize(size_t len) {
    len = trim_size_t_to_u32(len);
    if (0 == len) {
        this->reset();
    } else if (fRec->unique() && ((len >> 2) <= (fRec->fLength >> 2))) {
        // Use less of the buffer we have without allocating a smaller one.
        char* p = this->writable_str();
        p[len] = '\0';
        fRec->fLength = SkToU32(len);
    } else {
        SkString newString(len);
        char* dest = newString.writable_str();
        int copyLen = std::min<uint32_t>(len, this->size());
        memcpy(dest, this->c_str(), copyLen);
        dest[copyLen] = '\0';
        this->swap(newString);
    }
}

void SkString::insert(size_t offset, const char text[]) {
    this->insert(offset, text, text ? strlen(text) : 0);
}

void SkString::insert(size_t offset, const char text[], size_t len) {
    if (len) {
        size_t length = fRec->fLength;
        if (offset > length) {
            offset = length;
        }

        // Check if length + len exceeds 32bits, we trim len
        len = check_add32(length, len);
        if (0 == len) {
            return;
        }

        /*  If we're the only owner, and we have room in our allocation for the insert,
            do it in place, rather than allocating a new buffer.

            To know we have room, compare the allocated sizes
            beforeAlloc = SkAlign4(length + 1)
            afterAlloc  = SkAligh4(length + 1 + len)
            but SkAlign4(x) is (x + 3) >> 2 << 2
            which is equivalent for testing to (length + 1 + 3) >> 2 == (length + 1 + 3 + len) >> 2
            and we can then eliminate the +1+3 since that doesn't affec the answer
        */
        if (fRec->unique() && (length >> 2) == ((length + len) >> 2)) {
            char* dst = this->writable_str();

            if (offset < length) {
                memmove(dst + offset + len, dst + offset, length - offset);
            }
            memcpy(dst + offset, text, len);

            dst[length + len] = 0;
            fRec->fLength = SkToU32(length + len);
        } else {
            /*  Seems we should use realloc here, since that is safe if it fails
                (we have the original data), and might be faster than alloc/copy/free.
            */
            SkString    tmp(fRec->fLength + len);
            char*       dst = tmp.writable_str();

            if (offset > 0) {
                memcpy(dst, fRec->data(), offset);
            }
            memcpy(dst + offset, text, len);
            if (offset < fRec->fLength) {
                memcpy(dst + offset + len, fRec->data() + offset,
                       fRec->fLength - offset);
            }

            this->swap(tmp);
        }
    }
}

static constexpr int kSkStrAppendU32_MaxSize = 10;
char* SkStrAppendU32(char string[], uint32_t dec) {
    char* start = string;

    char    buffer[kSkStrAppendU32_MaxSize];
    char*   p = buffer + sizeof(buffer);

    do {
        *--p = SkToU8('0' + dec % 10);
        dec /= 10;
    } while (dec != 0);

    char* stop = buffer + sizeof(buffer);
    while (p < stop) {
        *string++ = *p++;
    }
    return string;
}

static constexpr int kSkStrAppendU64_MaxSize = 20;
char* SkStrAppendU64(char string[], uint64_t dec, int minDigits) {
    char* start = string;

    char    buffer[kSkStrAppendU64_MaxSize];
    char*   p = buffer + sizeof(buffer);

    do {
        *--p = SkToU8('0' + (int32_t) (dec % 10));
        dec /= 10;
        minDigits--;
    } while (dec != 0);

    while (minDigits > 0) {
        *--p = '0';
        minDigits--;
    }

    size_t cp_len = buffer + sizeof(buffer) - p;
    memcpy(string, p, cp_len);
    string += cp_len;

    return string;
}

static constexpr int kSkStrAppendS32_MaxSize = kSkStrAppendU32_MaxSize + 1;
char* SkStrAppendS32(char string[], int32_t dec) {
    uint32_t udec = dec;
    if (dec < 0) {
        *string++ = '-';
        udec = ~udec + 1;  // udec = -udec, but silences some warnings that are trying to be helpful
    }
    return SkStrAppendU32(string, udec);
}
static constexpr int kSkStrAppendS64_MaxSize = kSkStrAppendU64_MaxSize + 1;
char* SkStrAppendS64(char string[], int64_t dec, int minDigits) {
    uint64_t udec = dec;
    if (dec < 0) {
        *string++ = '-';
        udec = ~udec + 1;  // udec = -udec, but silences some warnings that are trying to be helpful
    }
    return SkStrAppendU64(string, udec, minDigits);
}

void SkString::insertS32(size_t offset, int32_t dec) {
    char    buffer[kSkStrAppendS32_MaxSize];
    char*   stop = SkStrAppendS32(buffer, dec);
    this->insert(offset, buffer, stop - buffer);
}

void SkString::insertS64(size_t offset, int64_t dec, int minDigits) {
    char    buffer[kSkStrAppendS64_MaxSize];
    char*   stop = SkStrAppendS64(buffer, dec, minDigits);
    this->insert(offset, buffer, stop - buffer);
}

void SkString::insertU32(size_t offset, uint32_t dec) {
    char    buffer[kSkStrAppendU32_MaxSize];
    char*   stop = SkStrAppendU32(buffer, dec);
    this->insert(offset, buffer, stop - buffer);
}

void SkString::insertU64(size_t offset, uint64_t dec, int minDigits) {
    char    buffer[kSkStrAppendU64_MaxSize];
    char*   stop = SkStrAppendU64(buffer, dec, minDigits);
    this->insert(offset, buffer, stop - buffer);
}

template <typename T>
static constexpr const T& SkTPin(const T& x, const T& lo, const T& hi) {
    return std::max(lo, std::min(x, hi));
}

namespace SkHexadecimalDigits {
    extern const char gUpper[16];  // 0-9A-F
    extern const char gLower[16];  // 0-9a-f
}  // namespace SkHexadecimalDigits

const char SkHexadecimalDigits::gUpper[16] =
        { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
const char SkHexadecimalDigits::gLower[16] =
        { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

void SkString::insertHex(size_t offset, uint32_t hex, int minDigits) {
    minDigits = SkTPin(minDigits, 0, 8);

    char    buffer[8];
    char*   p = buffer + sizeof(buffer);

    do {
        *--p = SkHexadecimalDigits::gUpper[hex & 0xF];
        hex >>= 4;
        minDigits -= 1;
    } while (hex != 0);

    while (--minDigits >= 0) {
        *--p = '0';
    }

    this->insert(offset, p, buffer + sizeof(buffer) - p);
}

///////////////////////////////////////////////////////////////////////////////


void SkString::remove(size_t offset, size_t length) {
    size_t size = this->size();

    if (offset < size) {
        if (length > size - offset) {
            length = size - offset;
        }
        if (length > 0) {
            SkString    tmp(size - length);
            char*       dst = tmp.writable_str();
            const char* src = this->c_str();

            if (offset) {
                memcpy(dst, src, offset);
            }
            size_t tail = size - (offset + length);
            if (tail) {
                memcpy(dst + offset, src + (offset + length), tail);
            }
            this->swap(tmp);
        }
    }
}

void SkString::swap(SkString& other) {
    this->validate();
    other.validate();

    using std::swap;
    swap(fRec, other.fRec);
}









std::string gen_random(const int len) {
    static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz<&";
    std::string tmp_s;
    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i) {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return tmp_s;
}

std::string escape_xml(const std::string& input,
                       const char* before = nullptr,
                       const char* after = nullptr) {
    if (input.empty()) {
        return input;
    }
    // "&" --> "&amp;" and  "<" --> "&lt;"
    // text is assumed to be in UTF-8
    // all strings are xml content, not attribute values.
    std::string output;
    if (before) {
        output += before;
    }
    static const char kAmp[] = "&amp;";
    static const char kLt[] = "&lt;";
    for (char i : input) {
        if (i == '&') {
            output += kAmp;
        } else if (i == '<') {
            output += kLt;
        } else {
            output += i;
        }
    }
    if (after) {
        output += after;
    }
    return output;
}

static int count_xml_escape_size(const SkString& input) {
    int extra = 0;
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '&') {
            extra += 4;  // strlen("&amp;") - strlen("&")
        } else if (input[i] == '<') {
            extra += 3;  // strlen("&lt;") - strlen("<")
        }
    }
    return extra;
}

SkString escape_xml(const SkString& input,
                    const char* before = nullptr,
                    const char* after = nullptr) {
    if (input.size() == 0) {
        return input;
    }
    // "&" --> "&amp;" and  "<" --> "&lt;"
    // text is assumed to be in UTF-8
    // all strings are xml content, not attribute values.
    size_t beforeLen = before ? strlen(before) : 0;
    size_t afterLen = after ? strlen(after) : 0;
    int extra = count_xml_escape_size(input);
    SkString output(input.size() + extra + beforeLen + afterLen);
    char* out = output.writable_str();
    if (before) {
        strncpy(out, before, beforeLen);
        out += beforeLen;
    }
    static const char kAmp[] = "&amp;";
    static const char kLt[] = "&lt;";
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '&') {
            memcpy(out, kAmp, strlen(kAmp));
            out += strlen(kAmp);
        } else if (input[i] == '<') {
            memcpy(out, kLt, strlen(kLt));
            out += strlen(kLt);
        } else {
            *out++ = input[i];
        }
    }
    if (after) {
        strncpy(out, after, afterLen);
        out += afterLen;
    }
    // Validate that we haven't written outside of our string.
    assert (out == &output.writable_str()[output.size()]);
    *out = '\0';
    return output;
}

constexpr unsigned NUMBER_OF_RUNS = 200;

void run_test_case(bool include_creation, std::stringstream& result, int length) {
    using std::chrono::high_resolution_clock;
    using std::chrono::duration_cast;
    using std::chrono::duration;
    using std::chrono::milliseconds;

    std::vector<std::string> random_strings(NUMBER_OF_RUNS);
    auto gen_random_with_length = std::bind(gen_random, length);
    std::generate(random_strings.begin(), random_strings.end(), gen_random_with_length);
    std::vector<double> original_durations(NUMBER_OF_RUNS);
    std::vector<double> modified_durations(NUMBER_OF_RUNS);

    for (int i = 0; i < NUMBER_OF_RUNS; i++) {
        auto random_c_string = random_strings[i].c_str();

        auto t1_original = high_resolution_clock::now();
        SkString escaped_xml;
        if (include_creation) {
            escaped_xml = escape_xml(
                    SkString(random_c_string),
                    "<dc:title><rdf:Alt><rdf:li xml:lang=\"x-default\">",
                    "</rdf:li></rdf:Alt></dc:title>\n"
            );
        } else {
            auto skString = SkString(random_c_string);
            t1_original = high_resolution_clock::now();
            escaped_xml = escape_xml(
                    skString,
                    "<dc:title><rdf:Alt><rdf:li xml:lang=\"x-default\">",
                    "</rdf:li></rdf:Alt></dc:title>\n"
            );
        }
        auto t2_original = high_resolution_clock::now();
        if (i >= NUMBER_OF_RUNS / 10) {
            duration<double, std::milli> duration = t2_original - t1_original;
            original_durations[i] = duration.count();
        }
    }

    for (int i = 0; i < NUMBER_OF_RUNS; i++) {
        auto random_c_string = random_strings[i].c_str();

        auto t1_original = high_resolution_clock::now();
        std::string escaped_xml;
        if (include_creation) {
            escaped_xml = escape_xml(
                    std::string(random_c_string),
                    "<dc:title><rdf:Alt><rdf:li xml:lang=\"x-default\">",
                    "</rdf:li></rdf:Alt></dc:title>\n"
            );
        } else {
            auto string = std::string(random_c_string);
            t1_original = high_resolution_clock::now();
            escaped_xml = escape_xml(
                    string,
                    "<dc:title><rdf:Alt><rdf:li xml:lang=\"x-default\">",
                    "</rdf:li></rdf:Alt></dc:title>\n"
            );
        }
        auto t2_original = high_resolution_clock::now();
        if (i >= NUMBER_OF_RUNS / 10) {
            duration<double, std::milli> duration = t2_original - t1_original;
            modified_durations[i] = duration.count();
        }
    }

    double original_duration_average = std::accumulate(
            original_durations.begin(),
            original_durations.end(),
            0.0
    ) / original_durations.size();

    double modified_duration_average = std::accumulate(
            modified_durations.begin(),
            modified_durations.end(),
            0.0
    ) / modified_durations.size();

    result << "Test ";
    if (include_creation) {
        result << "with creation, ";
    } else {
        result << "without creation, ";
    }
    result << "with length ";
    result << length;
    result << " speed of original / modified (ms): ";
    result << original_duration_average << " / ";
    result << modified_duration_average << "\n";
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_nativespeedtest_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::stringstream result;
    run_test_case(true, result, 100);
    run_test_case(false, result, 100);
    run_test_case(true, result, 1000);
    run_test_case(false, result, 1000);
    run_test_case(true, result, 10000);
    run_test_case(false, result, 10000);
    run_test_case(true, result, 100000);
    run_test_case(false, result, 100000);
    run_test_case(true, result, 1000000);
    run_test_case(false, result, 1000000);
    std::string str = result.str();

    return env->NewStringUTF(result.str().c_str());
}