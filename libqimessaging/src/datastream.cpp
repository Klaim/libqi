/*
*  Author(s):
*  - Cedric Gestes <gestes@aldebaran-robotics.com>
*  - Chris  Kilner <ckilner@aldebaran-robotics.com>
*
*  Copyright (C) 2010, 2012 Aldebaran Robotics
*/

#include <qimessaging/message.hpp>
#include <qimessaging/datastream.hpp>
#include <qi/log.hpp>
#include <vector>
#include <cstring>



#if 0

#include <qimessaging/signature.hpp>

#define __QI_DEBUG_SERIALIZATION_DATA_R(x, d) {            \
  std::string sig = qi::signature< x >::value();           \
  std::cout << "read (" << sig << "): " << d << std::endl; \
}

#define __QI_DEBUG_SERIALIZATION_DATA_W(x, d) {            \
  std::string sig = qi::signature< x >::value();           \
  std::cout << "write(" << sig << "): " << d << std::endl; \
}
#else
# define __QI_DEBUG_SERIALIZATION_DATA_R(x, d)
# define __QI_DEBUG_SERIALIZATION_DATA_W(x, d)
#endif

#define QI_SIMPLE_SERIALIZER_IMPL(Type)                           \
  DataStream& DataStream::operator>>(Type &b)                     \
  {                                                               \
    _buffer->read((void *)&b, sizeof(Type));               \
    __QI_DEBUG_SERIALIZATION_DATA_R(Type, b);              \
    return *this;                                          \
  }                                                        \
                                                           \
  DataStream& DataStream::operator<<(Type b)               \
  {                                                        \
    if (_ro) {                                                      \
      qiLogError("datastream", "cant write to a readonly buffer");  \
      return *this;                                                 \
    }                                                               \
    _buffer->write((Type *)&b, sizeof(b));                 \
    __QI_DEBUG_SERIALIZATION_DATA_W(Type, b);              \
    return *this;                                          \
  }


namespace qi {

  QI_SIMPLE_SERIALIZER_IMPL(bool);
  QI_SIMPLE_SERIALIZER_IMPL(char);
  QI_SIMPLE_SERIALIZER_IMPL(int);
  QI_SIMPLE_SERIALIZER_IMPL(unsigned char);
  QI_SIMPLE_SERIALIZER_IMPL(unsigned int);
  QI_SIMPLE_SERIALIZER_IMPL(float);
  QI_SIMPLE_SERIALIZER_IMPL(double);

  DataStream::DataStream(const qi::Buffer *buffer)
    : _buffer(const_cast<qi::Buffer *>(buffer)),
      _ro(true)
  {
  }

  DataStream::DataStream(qi::Buffer *buffer)
    : _buffer(buffer),
      _ro(false)
  {
  }


  // string
  size_t DataStream::read(void *data, size_t len)
  {
    return _buffer->read(data, len);
  }

  void DataStream::writeString(const char *str, size_t len)
  {
    if (_ro) {
      qiLogError("datastream", "cant write to a readonly buffer");
      return;
    }
    *this << (int)len;
    if (len) {
      _buffer->write(str, len)
      __QI_DEBUG_SERIALIZATION_DATA_W(std::string, str);
    }
  }

  // string
  DataStream& DataStream::operator>>(std::string &s)
  {
    int sz;
    *this >> sz;

    s.clear();
    if (sz) {
      char *data = static_cast<char *>(_buffer->read(sz));
      if (!data) {
        qiLogError("datastream", "buffer empty");
        return *this;
      }
      s.append(data, sz);
      __QI_DEBUG_SERIALIZATION_DATA_R(std::string, s);
    }

    return *this;
  }

  DataStream& DataStream::operator<<(const std::string &s)
  {
    if (_ro) {
      qiLogError("datastream", "cant write to a readonly buffer");
      return *this;
    }
    *this << (int)s.size();
    if (!s.empty()) {
      _buffer->write(s.data(), s.size());
      __QI_DEBUG_SERIALIZATION_DATA_W(std::string, s);
    }

    return *this;
  }

  DataStream& DataStream::operator<<(const char *s)
  {
    if (_ro) {
      qiLogError("datastream", "cant write to a readonly buffer");
      return *this;
    }
    int len = strlen(s);
    writeString(s, len);
    __QI_DEBUG_SERIALIZATION_DATA_W(char *, s);
    return *this;
  }


  qi::DataStream &operator>>(qi::DataStream &sd, qi::Value &val)
  {
    std::string sig;
    int type;
    val.clear();
    sd >> type;
    sd >> sig;
    switch(type) {
      case qi::Value::Bool:
        val._private.type = qi::Value::Bool;
        sd >> val._private.data.b;
        return sd;
      case qi::Value::Char:
        val._private.type = qi::Value::Char;
        sd >> val._private.data.c;
        return sd;
      case qi::Value::Int32:
        val._private.type = qi::Value::Int32;
        sd >> val._private.data.i;
        return sd;
      case Value::UInt32:
      case Value::Int64:
      case Value::UInt64:
        qiLogError("datasteam") << "not implemented";
        return sd;
      case qi::Value::Float:
        val._private.type = qi::Value::Float;
        sd >> val._private.data.f;
        return sd;
      case qi::Value::Double:
        val._private.type = qi::Value::Double;
        sd >> val._private.data.d;
        return sd;
      case qi::Value::String:
        val.setType(qi::Value::String);
        sd >> *val.value<std::string>();
        return sd;
      case qi::Value::List:
        val.setType(qi::Value::List);
        sd >> *val.value< std::list<qi::Value> >();
        return sd;
      case qi::Value::Vector:
        val.setType(qi::Value::Vector);
        sd >> *val.value< std::vector<qi::Value> >();
        return sd;
      case qi::Value::Map:
        val.setType(qi::Value::Map);
        sd >> *val.value< std::map<std::string, qi::Value> >();
        return sd;
    };
    return sd;
  }

  qi::DataStream &operator<<(qi::DataStream &sd, const qi::Value &val)
  {
    sd << (int)val.type();
    switch(val.type()) {
      case qi::Value::Bool:
        sd << "b";
        sd << val._private.data.b;
        return sd;
      case qi::Value::Char:
        sd << "c";
        sd << val._private.data.c;
        return sd;
      case qi::Value::Int32:
        sd << "i";
        sd<< val._private.data.i;
        return sd;
      case Value::UInt32:
      case Value::Int64:
      case Value::UInt64:
        qiLogError("datasteam") << "not implemented";
        return sd;
      case qi::Value::Float:
        sd << "f";
        sd << val._private.data.f;
        return sd;
      case qi::Value::Double:
        sd << "d";
        sd << val._private.data.d;
        return sd;
      case qi::Value::String:{
        sd << "s";
        sd << *val.value< std::string >();
        return sd;
      }
      case qi::Value::List:{
        sd << "[m]";
        const std::list<qi::Value> *le = val.value< std::list<qi::Value> >();
        sd << *le;
        return sd;
      }
      case qi::Value::Vector: {
        sd << "[m]";
        const std::vector<qi::Value> *ve = val.value< std::vector<qi::Value> >();
        sd << *ve;
        return sd;
      }
      case qi::Value::Map: {
        sd << "{sm}";
        const std::map<std::string, qi::Value> *me = val.value< std::map<std::string, qi::Value> >();
        sd << *me;
        return sd;
      }
      default:
        return sd;
    };
    return sd;
  }




}

