#ifndef __SHANNON_NET_PATH_H__
#define __SHANNON_NET_PATH_H__

#include <dirent.h>
#include <memory>
#include <string>

namespace shannonnet {
class Path {
 public:
  typedef int errno_t;

  class DirIterator {
   public:
    static std::shared_ptr<DirIterator> OpenDirectory(Path *f);

    ~DirIterator();

    bool HasNext();

    Path Next();

   private:
    explicit DirIterator(Path *f);

    Path *dir_ = nullptr;
    DIR *dp_ = nullptr;
    struct dirent *entry_ = nullptr;
  };

  explicit Path(const std::string &);

  explicit Path(const char *);

  ~Path() = default;

  Path(const Path &);

  Path &operator=(const Path &);

  Path(Path &&) noexcept;

  Path &operator=(Path &&) noexcept;

  std::string ToString() const { return path_; }

  Path operator+(const Path &);

  Path operator+(const std::string &);

  Path operator+(const char *);

  Path &operator+=(const Path &rhs);

  Path &operator+=(const std::string &);

  Path &operator+=(const char *);

  Path operator/(const Path &);

  Path operator/(const std::string &);

  Path operator/(const char *);

  bool operator==(const Path &rhs) const { return (path_ == rhs.path_); }

  bool operator!=(const Path &rhs) const { return (path_ != rhs.path_); }

  bool operator<(const Path &rhs) const { return (path_ < rhs.path_); }

  bool operator>(const Path &rhs) const { return (path_ > rhs.path_); }

  bool operator<=(const Path &rhs) const { return (path_ <= rhs.path_); }

  bool operator>=(const Path &rhs) const { return (path_ >= rhs.path_); }

  bool Exists();

  bool IsDirectory();

  bool CreateDirectory(bool is_common_dir = false);

  bool CreateDirectories(bool is_common_dir = false);

  bool CreateCommonDirectories();

  std::string Extension() const;

  std::string ParentPath();

  bool Remove();

  bool CreateFile(int *fd);

  bool OpenFile(int *fd, bool create = false);

  bool CloseFile(int fd) const;

  bool TruncateFile(int fd) const;

  std::string Basename();

  static bool RealPath(const std::string &path, std::string &realpath_str);  // NOLINT

  friend std::ostream &operator<<(std::ostream &os, const Path &s);

 private:
  static char separator_;
  std::string path_;
};
}  // namespace shannonnet

#endif  // __SHANNON_NET_PATH_H__
