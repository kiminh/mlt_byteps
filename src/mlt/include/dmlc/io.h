/*!
 *  Copyright (c) 2015 by Contributors
 * \file io.h
 * \brief defines serializable interface of dmlc
 */
#ifndef DMLC_IO_H_
#define DMLC_IO_H_
#include <cstdio>
#include <string>
#include <cstring>
#include <vector>
#include <istream>
#include <ostream>
#include <streambuf>
#include "./base.h"

// include uint64_t only to make io standalone
#ifdef _MSC_VER
/*! \brief uint64 */
typedef unsigned __int64 uint64_t;
#else
#include <inttypes.h>
#endif

/*! \brief namespace for dmlc */
namespace dmlc {
/*!
 * \brief interface of stream I/O for serialization
 */
class Stream {  // NOLINT(*)
 public:
  /*!
   * \brief reads data from a stream
   * \param ptr pointer to a memory buffer
   * \param size block size
   * \return the size of data read
   */
  virtual size_t Read(void *ptr, size_t size) = 0;
  /*!
   * \brief writes data to a stream
   * \param ptr pointer to a memory buffer
   * \param size block size
   */
  virtual void Write(const void *ptr, size_t size) = 0;
  /*! \brief virtual destructor */
  virtual ~Stream(void) {}
  /*!
   * \brief generic factory function
   *  create an stream, the stream will close the underlying files upon deletion
   *
   * \param uri the uri of the input currently we support
   *            hdfs://, s3://, and file:// by default file:// will be used
   * \param flag can be "w", "r", "a"
   * \param allow_null whether NULL can be returned, or directly report error
   * \return the created stream, can be NULL when allow_null == true and file do not exist
   */
  static Stream *Create(const char *uri,
                        const char* const flag,
                        bool allow_null = false);
  // helper functions to write/read different data structures
  /*!
   * \brief writes a data to stream.
   *
   * dmlc::Stream support Write/Read of most STL composites and base types.
   * If the data type is not supported, a compile time error will be issued.
   *
   * This function is endian-aware,
   * the output endian defined by DMLC_IO_USE_LITTLE_ENDIAN
   *
   * \param data data to be written
   * \tparam T the data type to be written
   */
  template<typename T>
  inline void Write(const T &data);
  /*!
   * \brief loads a data from stream.
   *
   * dmlc::Stream support Write/Read of most STL composites and base types.
   * If the data type is not supported, a compile time error will be issued.
   *
   * This function is endian-aware,
   * the input endian defined by DMLC_IO_USE_LITTLE_ENDIAN
   *
   * \param out_data place holder of data to be deserialized
   * \return whether the load was successful
   */
  template<typename T>
  inline bool Read(T *out_data);
  /*!
   * \brief Endian aware write array of data.
   * \param data The data pointer
   * \param num_elems Number of elements
   * \tparam T the data type.
   */
  template<typename T>
  inline void WriteArray(const T* data, size_t num_elems);
  /*!
   * \brief Endian aware read array of data.
   * \param data The data pointer
   * \param num_elems Number of elements
   * \tparam T the data type.
   * \return whether the load was successful
   */
  template<typename T>
  inline bool ReadArray(T* data, size_t num_elems);
};

/*! \brief interface of i/o stream that support seek */
class SeekStream: public Stream {
 public:
  // virtual destructor
  virtual ~SeekStream(void) {}
  /*! \brief seek to certain position of the file */
  virtual void Seek(size_t pos) = 0;
  /*! \brief tell the position of the stream */
  virtual size_t Tell(void) = 0;
  /*!
   * \brief generic factory function
   *  create an SeekStream for read only,
   *  the stream will close the underlying files upon deletion
   *  error will be reported and the system will exit when create failed
   * \param uri the uri of the input currently we support
   *            hdfs://, s3://, and file:// by default file:// will be used
   * \param allow_null whether NULL can be returned, or directly report error
   * \return the created stream, can be NULL when allow_null == true and file do not exist
   */
  static SeekStream *CreateForRead(const char *uri,
                                   bool allow_null = false);
};

/*! \brief interface for serializable objects */
class Serializable {
 public:
  /*! \brief virtual destructor */
  virtual ~Serializable() {}
  /*!
  * \brief load the model from a stream
  * \param fi stream where to load the model from
  */
  virtual void Load(Stream *fi) = 0;
  /*!
  * \brief saves the model to a stream
  * \param fo stream where to save the model to
  */
  virtual void Save(Stream *fo) const = 0;
};

#ifndef _LIBCPP_SGX_NO_IOSTREAMS
/*!
 * \brief a std::ostream class that can can wrap Stream objects,
 *  can use ostream with that output to underlying Stream
 *
 * Usage example:
 * \code
 *
 *   Stream *fs = Stream::Create("hdfs:///test.txt", "w");
 *   dmlc::ostream os(fs);
 *   os << "hello world" << std::endl;
 *   delete fs;
 * \endcode
 */
class ostream : public std::basic_ostream<char> {
 public:
  /*!
   * \brief construct std::ostream type
   * \param stream the Stream output to be used
   * \param buffer_size internal streambuf size
   */
  explicit ostream(Stream *stream,
                   size_t buffer_size = (1 << 10))
      : std::basic_ostream<char>(NULL), buf_(buffer_size) {
    this->set_stream(stream);
  }
  // explictly synchronize the buffer
  virtual ~ostream() DMLC_NO_EXCEPTION {
    buf_.pubsync();
  }
  /*!
   * \brief set internal stream to be stream, reset states
   * \param stream new stream as output
   */
  inline void set_stream(Stream *stream) {
    buf_.set_stream(stream);
    this->rdbuf(&buf_);
  }

  /*! \return how many bytes we written so far */
  inline size_t bytes_written(void) const {
    return buf_.bytes_out();
  }

 private:
  // internal streambuf
  class OutBuf : public std::streambuf {
   public:
    explicit OutBuf(size_t buffer_size)
        : stream_(NULL), buffer_(buffer_size), bytes_out_(0) {
      if (buffer_size == 0) buffer_.resize(2);
    }
    // set stream to the buffer
    inline void set_stream(Stream *stream);

    inline size_t bytes_out() const { return bytes_out_; }
   private:
    /*! \brief internal stream by StreamBuf */
    Stream *stream_;
    /*! \brief internal buffer */
    std::vector<char> buffer_;
    /*! \brief number of bytes written so far */
    size_t bytes_out_;
    // override sync
    inline int_type sync(void);
    // override overflow
    inline int_type overflow(int c);
  };
  /*! \brief buffer of the stream */
  OutBuf buf_;
};

/*!
 * \brief a std::istream class that can can wrap Stream objects,
 *  can use istream with that output to underlying Stream
 *
 * Usage example:
 * \code
 *
 *   Stream *fs = Stream::Create("hdfs:///test.txt", "r");
 *   dmlc::istream is(fs);
 *   is >> mydata;
 *   delete fs;
 * \endcode
 */
class istream : public std::basic_istream<char> {
 public:
  /*!
   * \brief construct std::ostream type
   * \param stream the Stream output to be used
   * \param buffer_size internal buffer size
   */
  explicit istream(Stream *stream,
                   size_t buffer_size = (1 << 10))
      : std::basic_istream<char>(NULL), buf_(buffer_size) {
    this->set_stream(stream);
  }
  virtual ~istream() DMLC_NO_EXCEPTION {}
  /*!
   * \brief set internal stream to be stream, reset states
   * \param stream new stream as output
   */
  inline void set_stream(Stream *stream) {
    buf_.set_stream(stream);
    this->rdbuf(&buf_);
  }
  /*! \return how many bytes we read so far */
  inline size_t bytes_read(void) const {
    return buf_.bytes_read();
  }

 private:
  // internal streambuf
  class InBuf : public std::streambuf {
   public:
    explicit InBuf(size_t buffer_size)
        : stream_(NULL), bytes_read_(0),
          buffer_(buffer_size) {
      if (buffer_size == 0) buffer_.resize(2);
    }
    // set stream to the buffer
    inline void set_stream(Stream *stream);
    // return how many bytes read so far
    inline size_t bytes_read(void) const {
      return bytes_read_;
    }
   private:
    /*! \brief internal stream by StreamBuf */
    Stream *stream_;
    /*! \brief how many bytes we read so far */
    size_t bytes_read_;
    /*! \brief internal buffer */
    std::vector<char> buffer_;
    // override underflow
    inline int_type underflow();
  };
  /*! \brief input buffer */
  InBuf buf_;
};
#endif
}  // namespace dmlc

#include "./serializer.h"

namespace dmlc {
// implementations of inline functions
template<typename T>
inline void Stream::Write(const T &data) {
  serializer::Handler<T>::Write(this, data);
}
template<typename T>
inline bool Stream::Read(T *out_data) {
  return serializer::Handler<T>::Read(this, out_data);
}

template<typename T>
inline void Stream::WriteArray(const T* data, size_t num_elems) {
  for (size_t i = 0; i < num_elems; ++i) {
    this->Write<T>(data[i]);
  }
}

template<typename T>
inline bool Stream::ReadArray(T* data, size_t num_elems) {
  for (size_t i = 0; i < num_elems; ++i) {
    if (!this->Read<T>(data + i)) return false;
  }
  return true;
}

#ifndef _LIBCPP_SGX_NO_IOSTREAMS
// implementations for ostream
inline void ostream::OutBuf::set_stream(Stream *stream) {
  if (stream_ != NULL) this->pubsync();
  this->stream_ = stream;
  this->setp(&buffer_[0], &buffer_[0] + buffer_.size() - 1);
}
inline int ostream::OutBuf::sync(void) {
  if (stream_ == NULL) return -1;
  std::ptrdiff_t n = pptr() - pbase();
  stream_->Write(pbase(), n);
  this->pbump(-static_cast<int>(n));
  bytes_out_ += n;
  return 0;
}
inline int ostream::OutBuf::overflow(int c) {
  *(this->pptr()) = c;
  std::ptrdiff_t n = pptr() - pbase();
  this->pbump(-static_cast<int>(n));
  if (c == EOF) {
    stream_->Write(pbase(), n);
    bytes_out_ += n;
  } else {
    stream_->Write(pbase(), n + 1);
    bytes_out_ += n + 1;
  }
  return c;
}

// implementations for istream
inline void istream::InBuf::set_stream(Stream *stream) {
  stream_ = stream;
  this->setg(&buffer_[0], &buffer_[0], &buffer_[0]);
}
inline int istream::InBuf::underflow() {
  char *bhead = &buffer_[0];
  if (this->gptr() == this->egptr()) {
    size_t sz = stream_->Read(bhead, buffer_.size());
    this->setg(bhead, bhead, bhead + sz);
    bytes_read_ += sz;
  }
  if (this->gptr() == this->egptr()) {
    return traits_type::eof();
  } else {
    return traits_type::to_int_type(*gptr());
  }
}
#endif

}  // namespace dmlc
#endif  // DMLC_IO_H_
