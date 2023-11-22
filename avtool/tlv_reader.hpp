//
//  tlv_reader.hpp
//  avtool
//
//  Created by zhanwang-sky on 2023/11/20.
//

#ifndef tlv_reader_hpp
#define tlv_reader_hpp

#include <string>
#include <fcntl.h>
#include <unistd.h>

class avTLVReader {
 public:
  static constexpr int header_len = 15;

  avTLVReader(const avTLVReader&) = delete;
  avTLVReader& operator=(const avTLVReader&) = delete;

  avTLVReader(const std::string& filename) noexcept
      : fd_(open(filename.c_str(), O_RDONLY)) { }

  avTLVReader(avTLVReader&& rhs) noexcept
      : fd_(rhs.fd_), pos_(rhs.pos_) {
    rhs.fd_ = -1;
    rhs.pos_ = 0;
  }

  avTLVReader& operator=(avTLVReader&& rhs) noexcept {
    if (this != &rhs) {
      do_clean();

      fd_ = rhs.fd_;
      pos_ = rhs.pos_;
      rhs.fd_ = -1;
      rhs.pos_ = 0;
    }

    return *this;
  }

  virtual ~avTLVReader() {
    do_clean();
  }

  bool is_open() const {
    return fd_ >= 0;
  }

  int read(uint8_t* buf, int buf_len,
           uint8_t& marker, uint16_t& seq, uint32_t& rtp_ts, uint64_t& cap_ts) {
    uint16_t tlv_len = 0;
    ssize_t nbytes = 0;

    // sanity check
    if (!buf || buf_len < header_len) {
      return INT_MIN;
    }

    if (!is_open()) {
      // not opened
      return INT_MIN + 1;
    }

    if ((nbytes = pread(fd_, buf, buf_len, pos_)) < 0) {
      // read error
      return INT_MIN + 2;
    }

    if (nbytes == 0) {
      // EOF
      return 0;
    }

    if (nbytes < header_len) {
      // malformed TLV header
      return -1;
    }

    tlv_len = buf[0] | (buf[1] << 8);

    if (nbytes < tlv_len) {
      if (nbytes < buf_len) {
        // early EOF
        return -2;
      }
      // buffer too short
      return -3;
    }

    marker = buf[2];

    seq = buf[3] | (buf[4] << 8);

    rtp_ts = buf[8]; rtp_ts <<= 8;
    rtp_ts |= buf[7]; rtp_ts <<= 8;
    rtp_ts |= buf[6]; rtp_ts <<= 8;
    rtp_ts |= buf[5];

    cap_ts = buf[14]; cap_ts <<= 8;
    cap_ts |= buf[13]; cap_ts <<= 8;
    cap_ts |= buf[12]; cap_ts <<= 8;
    cap_ts |= buf[11]; cap_ts <<= 8;
    cap_ts |= buf[10]; cap_ts <<= 8;
    cap_ts |= buf[9];

    pos_ += tlv_len;

    return tlv_len;
  }

 private:
  void do_clean() {
    if (is_open()) {
      close(fd_);
      fd_ = -1;
    }
    pos_ = 0;
  }

  int fd_ = -1;
  int pos_ = 0;
};

#endif /* tlv_reader_hpp */
