#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace player {

/**
 * SystemInterface - 系统调用接口
 *
 * 用于封装 open, close, ioctl, mmap 等系统调用，方便单元测试进行 Mock
 */
class SystemInterface
{
public:
    virtual ~SystemInterface() = default;

    virtual int open(const char *pathname, int flags) = 0;
    virtual int close(int fd) = 0;
    virtual int ioctl(int fd, unsigned long request, void *arg) = 0;
    virtual void *mmap(
        void *addr,
        size_t length,
        int prot,
        int flags,
        int fd,
        off_t offset
    ) = 0;
    virtual int munmap(void *addr, size_t length) = 0;
};

/**
 * LinuxSystem - 真实的 Linux 系统调用实现
 */
class LinuxSystem : public SystemInterface
{
public:
    int open(const char *pathname, int flags) override;
    int close(int fd) override;
    int ioctl(int fd, unsigned long request, void *arg) override;
    void *mmap(
        void *addr,
        size_t length,
        int prot,
        int flags,
        int fd,
        off_t offset
    ) override;
    int munmap(void *addr, size_t length) override;
};

} // namespace player
