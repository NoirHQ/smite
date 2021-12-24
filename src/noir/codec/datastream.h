#pragma once
#include <noir/common/check.h>
#include <cstdlib>
#include <vector>

namespace noir::codec {

template<typename Codec, typename T>
class datastream {
public:
  datastream( T start, size_t s )
    :_start(start),_pos(start),_end(start+s) {}

  inline void skip(size_t s) {
    _pos += s;
  }

  inline bool read(char* d, size_t s) {
    check(size_t(_end - _pos) >= (size_t)s, "datastream attempted to read past the end");
    std::copy(_pos, _pos + s, d);
    _pos += s;
    return true;
  }

  inline bool write(const char* d, size_t s) {
    check(_end - _pos >= (int32_t)s, "datastream attempted to write past the end");
    std::copy(d, d + s, _pos);
    _pos += s;
    return true;
  }

  inline bool write(char d) {
    check(_end - _pos >= 1, "datastream attempted to write past the end");
    *_pos++ = d;
    return true;
  }

  inline bool write(const void* d, size_t s) {
    check(_end - _pos >= (int32_t)s, "datastream attempted to write past the end");
    auto _d = reinterpret_cast<const char*>(d);
    std::copy(_d, _d + s, _pos);
    _pos += s;
    return true;
  }

  inline bool put(char c) {
    check(_pos < _end, "put");
    *_pos = c;
    ++_pos;
    return true;
  }

  inline bool get(unsigned char& c) {
    return get(*(char*)&c);
  }

  inline bool get( char& c ) {
    check(_pos < _end, "get");
    c = *_pos;
    ++_pos;
    return true;
  }

  T pos() const {
    return _pos;
  }

  inline bool valid() const {
    return _pos <= _end && _pos >= _start;
  }

  inline bool seekp(size_t p) {
    _pos = _start + p;
    return _pos <= _end;
  }

  inline size_t tellp() const {
    return size_t(_pos - _start);
  }

  inline size_t remaining() const {
    return _end - _pos;
  }

private:
  T _start;
  T _pos;
  T _end;
};

template<typename Codec>
class datastream<Codec, size_t> {
public:
  datastream(size_t init_size = 0)
    :_size(init_size) {}

  inline bool skip(size_t s) {
    _size += s;
    return true;
  }

  inline bool write(const char*, size_t s)  {
    _size += s;
    return true;
  }

  inline bool write(char) {
    _size++;
    return true;
  }

  inline bool write(const void*, size_t s) {
    _size += s;
    return true;
  }

  inline bool put(char) {
    ++_size;
    return true;
  }

  inline bool valid() const {
    return true;
  }

  inline bool seekp(size_t p) {
    _size = p;
    return true;
  }

  inline size_t tellp() const {
    return _size;
  }

  inline size_t remaining() const {
    return 0;
  }

private:
  size_t _size;
};

template<typename Codec, typename T, typename U>
datastream<Codec,T>& operator<<(datastream<Codec,T>& ds, const U& v);

template<typename Codec, typename T, typename U>
datastream<Codec,T>& operator>>(datastream<Codec,T>& ds, U& v);

template<typename Codec, typename T>
size_t encode_size(const T& v) {
  datastream<Codec,size_t> ds;
  ds << v;
  return ds.tellp();
}

template<typename Codec, typename T>
std::vector<char> encode(const T& v) {
  auto size = encode_size<Codec>(v);
  std::vector<char> result(size);
  if (size) {
    datastream<Codec,char*> ds(result.data(), result.size());
    ds << v;
  }
  return result;
}

template<typename Codec, typename T>
T decode(const char* buffer, size_t size) {
  T result;
  datastream<Codec,const char*> ds(buffer, size);
  ds >> result;
  return result;
}

template<typename Codec, typename T>
void decode(T& result, const char* buffer, size_t size) {
  datastream<Codec,const char*> ds(buffer, size);
  ds >> result;
}

template<typename Codec, typename T>
T decode(const std::vector<char>& bytes) {
  return decode<Codec,T>(bytes.data(), bytes.size());
}

}
