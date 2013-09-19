#ifndef DATTO_CLIENT_BLOCK_DEVICE_BLOCK_DEVICE_H_
#define DATTO_CLIENT_BLOCK_DEVICE_BLOCK_DEVICE_H_

#include <boost/noncopyable.hpp>
#include <boost/cstdint.hpp>
#include <string>
#include <stdexcept>

namespace datto_linux_client {

class BlockDeviceException : public std::runtime_error {
  public: 
    BlockDeviceException(std::string & what) : runtime_error(what) {};
};


class BlockDevice : private boost::noncopyable {

 public:
  // Creates a BlockDevice from the given block_path
  // An exception should be thrown (likely a BlockDeviceException) if the
  // block_path doesn't exist or isn't the path for a block_device
  explicit BlockDevice(std::string block_path);

  //
  //  Do the actual initialization of the object
  //
  void Init();

  // If a method is lowercase, then it is should be just an accessor and
  // nothing more. See
  // google-styleguide.googlecode.com/svn/trunk/cppguide.xml#Function_Names
  std::string block_path() const {
    return block_path_;
  }

  uint32_t major() const {
    return major_;
  };

  uint32_t minor() const {
    return minor_;
  }

// If the block device needs to seek when reading/writing data.
// e.g. HDDs need to seek, SSDs do not.
// This is being cut for now, since the ioctl for this is not in the kernel until 3.3

//  bool DoesSeek() const {
//    return does_seek_;
//  }

  // BLKGETSIZE64
  uint64_t DeviceSizeBytes() const {
    return device_size_bytes_;
  }

  // Use BLKSSZGET not BLKBSZGET
  uint64_t BlockSizeBytes() const {
    return block_size_bytes_;
  }

  // https://www.kernel.org/doc/Documentation/cgroups/blkio-controller.txt
  // scalar is from 0 to 1, but hopefully not 0

  // TODO: Throttle unimplemented at this point
  void Throttle(double scalar);

  double throttle_scalar() const {
    return throttle_scalar_;
  }

  void Unthrottle();

  // Return a file descriptor for the block device
  // Throw an exception if one is already open
  virtual int Open();

  // Close the file descriptor returned
  // Don't throw if one isn't open
  virtual void Close();

  // Should close the file descriptor and Unthrottle
  virtual ~BlockDevice();

 protected:
  // Use this constructor when the block_path doesn't exist yet
  // Note that block_path_, major_, and minor_ must be set before the
  // subclass constructor returns
  BlockDevice();

 private:

  // properties

  std::string block_path_;
  uint32_t major_;
  uint32_t minor_;

  uint64_t device_size_bytes_;
  uint32_t block_size_bytes_;
//  bool     does_seek_;    // removed because not available until kernel 3.3.xxx

  double throttle_scalar_;

  int file_descriptor_;
};

}

#endif //  DATTO_CLIENT_BLOCK_DEVICE_BLOCK_DEVICE_H_
