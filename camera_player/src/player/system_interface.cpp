#include "system_interface.hpp"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace player {

int LinuxSystem::open(const char *pathname, int flags)
{
    return ::open(pathname, flags);
}

int LinuxSystem::close(int fd) { return ::close(fd); }

int LinuxSystem::ioctl(int fd, unsigned long request, void *arg)
{
    return ::ioctl(fd, request, arg);
}

void *LinuxSystem::mmap(
    void *addr,
    size_t length,
    int prot,
    int flags,
    int fd,
    off_t offset
)
{
    return ::mmap(addr, length, prot, flags, fd, offset);
}

int LinuxSystem::munmap(void *addr, size_t length)
{
    return ::munmap(addr, length);
}

} // namespace player
