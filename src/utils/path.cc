#include "src/utils/path.h"

#include <fcntl.h>
#include <new>
#include <sstream>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "glog/logging.h"

namespace shannonnet {
#if defined(_WIN32) || defined(_WIN64)
char Path::separator_ = '\\';
#else
char Path::separator_ = '/';
#endif

#ifndef EOK
#define EOK 0
#endif

Path::Path(const std::string &s) : path_(s) {}

Path::Path(const char *p) : path_(p) {}

Path::Path(const Path &p) : path_(p.path_) {}

Path &Path::operator=(const Path &p) {
  if (&p != this) {
    this->path_ = p.path_;
  }
  return *this;
}

Path &Path::operator=(Path &&p) noexcept {
  if (&p != this) {
    this->path_ = std::move(p.path_);
  }
  return *this;
}

Path::Path(Path &&p) noexcept { this->path_ = std::move(p.path_); }

Path Path::operator+(const Path &p) {
  std::string q = path_ + p.ToString();
  return Path(q);
}

Path Path::operator+(const std::string &p) {
  std::string q = path_ + p;
  return Path(q);
}

Path Path::operator+(const char *p) {
  std::string q = path_ + p;
  return Path(q);
}

Path &Path::operator+=(const Path &rhs) {
  path_ += rhs.ToString();
  return *this;
}

Path &Path::operator+=(const std::string &p) {
  path_ += p;
  return *this;
}

Path &Path::operator+=(const char *p) {
  path_ += p;
  return *this;
}

Path Path::operator/(const Path &p) {
  std::string q = path_ + separator_ + p.ToString();
  return Path(q);
}

Path Path::operator/(const std::string &p) {
  std::string q = path_ + separator_ + p;
  return Path(q);
}

Path Path::operator/(const char *p) {
  std::string q = path_ + separator_ + p;
  return Path(q);
}

std::string Path::Extension() const {
  std::size_t found = path_.find_last_of('.');
  if (found != std::string::npos) {
    return path_.substr(found);
  } else {
    return std::string("");
  }
}

bool Path::Exists() {
  struct stat sb;
  int rc = stat(path_.data(), &sb);
  if (rc == -1 && errno != ENOENT) {
    std::string msg = "Unable to query the status of " + path_ + ". Errno = " + std::to_string(errno) + ".";
    LOG(WARNING) << msg;
  }
  return (rc == 0);
}

bool Path::IsDirectory() {
  struct stat sb;
  int rc = stat(path_.data(), &sb);
  if (rc == 0) {
    return S_ISDIR(sb.st_mode);
  } else {
    return false;
  }
}

bool Path::CreateDirectory(bool is_common_dir) {
  if (!Exists()) {
#if defined(_WIN32) || defined(_WIN64)
    int rc = mkdir(path_.data());
#else
    int rc = mkdir(path_.data(), S_IRUSR | S_IWUSR | S_IXUSR);
    if (rc == 0 && is_common_dir) {
      rc = chmod(path_.data(), S_IRWXU | S_IRWXG | S_IRWXO);
    }
#endif
    if (rc) {
      LOG(ERROR) << "Unable to create directory " << path_ << ". Errno = " << errno;
      return false;
    }
    return true;
  } else {
    if (IsDirectory()) {
      return true;
    } else {
      LOG(ERROR) << "Unable to create directory " << path_ << ". It exists but is not a directory";
      return false;
    }
  }
}

std::string Path::ParentPath() {
  std::string r("");
  std::size_t found = path_.find_last_of(separator_);
  if (found != std::string::npos) {
    if (found == 0) {
      r += separator_;
    } else {
      r = std::string(path_.substr(0, found));
    }
  }
  return r;
}

bool Path::CreateDirectories(bool is_common_dir) {
  if (IsDirectory()) {
    LOG(WARNING) << "Directory " << ToString() << " already exists.";
    return true;
  } else {
    LOG(INFO) << "Creating directory " << ToString() << ".";
    std::string parent = ParentPath();
    if (!parent.empty()) {
      if (Path(parent).CreateDirectories(is_common_dir)) {
        return CreateDirectory(is_common_dir);
      }
    } else {
      return CreateDirectory(is_common_dir);
    }
  }
  return true;
}

bool Path::CreateCommonDirectories() { return CreateDirectories(true); }

bool Path::Remove() {
  if (Exists()) {
    if (IsDirectory()) {
      errno_t err = rmdir(path_.data());
      if (err == -1) {
        LOG(ERROR) << "Unable to delete directory " << path_ << ". Errno = " << errno;
        return false;
      }
    } else {
      errno_t err = unlink(path_.data());
      if (err == -1) {
        LOG(ERROR) << "Unable to delete file " << path_ << ". Errno = " << errno;
        return false;
      }
    }
  }
  return true;
}

bool Path::CreateFile(int *file_descriptor) { return OpenFile(file_descriptor, true); }

bool Path::OpenFile(int *file_descriptor, bool create) {
  int fd;
  if (file_descriptor == nullptr) {
    LOG(ERROR) << "null pointer";
    return false;
  }
  if (IsDirectory()) {
    LOG(ERROR) << "Unable to create file " << path_ << " which is a directory.";
  }
  // Convert to canonical form.
  if (strlen(path_.data()) >= PATH_MAX) {
    LOG(ERROR) << strerror(errno);
    return false;
  }
  char canonical_path[PATH_MAX] = {0x00};
#if defined(_WIN32) || defined(_WIN64)
  auto err = _fullpath(canonical_path, path_, PATH_MAX);
#else
  auto err = realpath(path_.data(), canonical_path);
#endif
  if (err == nullptr) {
    if (errno == ENOENT && create) {
      // File doesn't exist and we are to create it. Let's break it down.
      auto file_part = Basename();
      auto parent_part = ParentPath();
#if defined(_WIN32) || defined(_WIN64)
      auto parent_err = _fullpath(canonical_path, parent_part.data(), PATH_MAX);
#else
      auto parent_err = realpath(parent_part.data(), canonical_path);
#endif
      if (parent_err == nullptr) {
        LOG(ERROR) << strerror(errno);
        return false;
      }
      auto cur_inx = strlen(canonical_path);
      if (cur_inx + file_part.length() >= PATH_MAX) {
        LOG(ERROR) << strerror(errno);
        return false;
      }
      canonical_path[cur_inx++] = separator_;
      if (strncpy(canonical_path + cur_inx, file_part.data(), file_part.length()) != EOK) {
        LOG(ERROR) << strerror(errno);
        return false;
      }
    } else {
      LOG(ERROR) << strerror(errno);
      return false;
    }
  }
  if (create) {
    fd = open(canonical_path, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP);
  } else {
    fd = open(canonical_path, O_RDWR);
  }
  if (fd == -1) {
    LOG(ERROR) << strerror(errno);
    return false;
  }
  *file_descriptor = fd;
  return true;
}

bool Path::CloseFile(int fd) const {
  if (close(fd) < 0) {
    LOG(ERROR) << strerror(errno);
    return false;
  }
  return true;
}

bool Path::TruncateFile(int fd) const {
  int rc = ftruncate(fd, 0);
  if (rc == 0) {
    return true;
  } else {
    LOG(ERROR) << strerror(errno);
    return false;
  }
}

std::string Path::Basename() {
  std::size_t found = path_.find_last_of(separator_);
  if (found != std::string::npos) {
    return path_.substr(found + 1);
  } else {
    return path_;
  }
}

std::string Path::FileName() {
  std::size_t found = path_.find_last_of(separator_);
  if (found != std::string::npos) {
    std::string basename = path_.substr(found + 1);
    std::size_t found = basename.find_last_of('.');
    if (found != std::string::npos) {
      return basename.substr(0, found);
    } else {
      return basename;
    }
  } else {
    return path_;
  }
}

std::shared_ptr<Path::DirIterator> Path::DirIterator::OpenDirectory(Path *f) {
  auto it = new (std::nothrow) DirIterator(f);

  if (it == nullptr) {
    return nullptr;
  }

  if (it->dp_) {
    return std::shared_ptr<DirIterator>(it);
  } else {
    delete it;
    return nullptr;
  }
}

Path::DirIterator::~DirIterator() {
  if (dp_) {
    (void)closedir(dp_);
  }
  dp_ = nullptr;
  dir_ = nullptr;
  entry_ = nullptr;
}

Path::DirIterator::DirIterator(Path *f) : dir_(f), dp_(nullptr), entry_(nullptr) {
  LOG(INFO) << "Open directory " << f->ToString() << ".";
  dp_ = opendir(f->ToString().c_str());
}

bool Path::DirIterator::HasNext() {
  do {
    entry_ = readdir(dp_);
    if (entry_) {
      if (strcmp(entry_->d_name, ".") == 0 || strcmp(entry_->d_name, "..") == 0) {
        continue;
      }
    }
    break;
  } while (true);
  return (entry_ != nullptr);
}

Path Path::DirIterator::Next() { return (*(this->dir_) / Path(entry_->d_name)); }

bool Path::RealPath(const std::string &path, std::string &realpath_str) {
  char real_path[PATH_MAX] = {0};
  // input_path is only file_name
#if defined(_WIN32) || defined(_WIN64)
  std::string msg = "The length of path: " + path + " exceeds limit: " + std::to_string(PATH_MAX);
  LOG_ASSERT(path.length() < PATH_MAX) << msg;
  auto ret = _fullpath(real_path, path.data(), PATH_MAX);
  msg = "The file " + path + " does not exist.";
  LOG_ASSERT(ret != nullptr) << msg;
#else
  std::string msg = "The length of path: " + path + " exceeds limit: " + std::to_string(NAME_MAX);
  LOG_ASSERT(path.length() < NAME_MAX) << msg;
  auto ret = realpath(path.data(), real_path);
  msg = "The file " + path + " does not exist.";
  LOG_ASSERT(ret != nullptr) << msg;
#endif
  realpath_str = std::string(real_path);
  return true;
}

std::ostream &operator<<(std::ostream &os, const Path &s) {
  os << s.path_;
  return os;
}
}  // namespace shannonnet
