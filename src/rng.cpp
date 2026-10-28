#include "rng.h"

#include "crypto.h"

#include <algorithm>
#include <cstring>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <bcrypt.h>
#  if defined(_MSC_VER)
#    pragma comment(lib, "bcrypt.lib")
#  endif
#else
#  include <cerrno>
#  include <fcntl.h>
#  include <unistd.h>
#  if defined(__linux__)
#    include <sys/random.h>
#  endif
#endif

namespace kruptos {
namespace {

#if !defined(_WIN32)
Bytes readFromDevUrandom(std::size_t count) {
    Bytes output(count);
    const int fd = ::open("/dev/urandom", O_RDONLY);
    if (fd < 0) throw Error("Could not open /dev/urandom");

    std::size_t offset = 0;
    while (offset < count) {
        const ssize_t got = ::read(fd, output.data() + offset, count - offset);
        if (got <= 0) {
            if (got < 0 && errno == EINTR) continue;
            ::close(fd);
            throw Error("Could not read from /dev/urandom");
        }
        offset += static_cast<std::size_t>(got);
    }
    ::close(fd);
    return output;
}
#endif

void appendBe32(Bytes& buffer, std::uint32_t value) {
    buffer.push_back(static_cast<std::uint8_t>(value >> 24));
    buffer.push_back(static_cast<std::uint8_t>(value >> 16));
    buffer.push_back(static_cast<std::uint8_t>(value >> 8));
    buffer.push_back(static_cast<std::uint8_t>(value));
}

} // namespace

Bytes secureRandom(std::size_t count) {
    Bytes output(count);
    if (count == 0) return output;

#if defined(_WIN32)
    std::size_t offset = 0;
    while (offset < count) {
        const ULONG chunk = static_cast<ULONG>(
            std::min<std::size_t>(count - offset, 0x40000000u));
        const NTSTATUS status = ::BCryptGenRandom(
            nullptr, output.data() + offset, chunk, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (status < 0) {
            throw Error("The system random generator (BCryptGenRandom) failed");
        }
        offset += chunk;
    }
#else
    std::size_t offset = 0;
    bool usedGetRandom = false;
#  if defined(__linux__)
    while (offset < count) {
        const ssize_t got = ::getrandom(output.data() + offset, count - offset, 0);
        if (got < 0) {
            if (errno == EINTR) continue;
            break; // sin getrandom utilizable: se usa /dev/urandom
        }
        if (got == 0) break;
        offset += static_cast<std::size_t>(got);
        usedGetRandom = true;
    }
#  endif
    if (!usedGetRandom || offset < count) {
        return readFromDevUrandom(count);
    }
#endif
    return output;
}

std::uint32_t secureRandomU32() {
    const Bytes bytes = secureRandom(4);
    return (static_cast<std::uint32_t>(bytes[0]) << 24) |
           (static_cast<std::uint32_t>(bytes[1]) << 16) |
           (static_cast<std::uint32_t>(bytes[2]) << 8) |
           static_cast<std::uint32_t>(bytes[3]);
}

KeyStream::KeyStream(ByteSpan seed, std::uint32_t step, std::uint8_t method)
    : seed_(seed.begin(), seed.end()), step_(step), method_(method) {}

void KeyStream::refill() {
    Bytes message = toBytes("Kruptos/keystream/v1");
    appendBe32(message, step_);
    message.push_back(method_);
    appendBe32(message, counter_);
    ++counter_;

    buffer_   = crypto::hmacSha256(seed_, message);
    position_ = 0;
}

Bytes KeyStream::take(std::size_t count) {
    Bytes output;
    output.reserve(count);
    while (output.size() < count) {
        if (position_ >= buffer_.size()) refill();
        const std::size_t available = buffer_.size() - position_;
        const std::size_t wanted = std::min(available, count - output.size());
        output.insert(output.end(),
                      buffer_.begin() + static_cast<std::ptrdiff_t>(position_),
                      buffer_.begin() + static_cast<std::ptrdiff_t>(position_ + wanted));
        position_ += wanted;
    }
    return output;
}

std::uint8_t KeyStream::u8() {
    return take(1)[0];
}

std::uint8_t KeyStream::rangeInclusive(std::uint8_t low, std::uint8_t high) {
    if (low > high) std::swap(low, high);
    const unsigned span = static_cast<unsigned>(high) - low + 1u;
    if (span == 256u) return u8();
    const unsigned limit = 256u - (256u % span);
    for (;;) {
        const unsigned value = u8();
        if (value < limit) return static_cast<std::uint8_t>(low + value % span);
    }
}

} // namespace kruptos
