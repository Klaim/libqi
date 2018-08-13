#pragma once
/*
**  Copyright (C) 2012 Aldebaran Robotics
**  See COPYING for the license
*/

#ifndef _SRC_OBJECTHOST_HPP_
#define _SRC_OBJECTHOST_HPP_

#include <map>

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/container/flat_map.hpp>

#include <qi/atomic.hpp>

#include <qi/type/fwd.hpp>

#include "messagesocket.hpp"

#include <ka/macroregular.hpp>

namespace qi
{
  class Message;
  class BoundObject;
  class StreamContext;
  class RemoteObject;
  using BoundAnyObject = boost::shared_ptr<BoundObject>;
  using RemoteObjectPtr = boost::shared_ptr<RemoteObject>;


  class ObjectHost
  {
  public:
    ObjectHost(unsigned int service);
    virtual ~ObjectHost();
    void dispatchToChildren(const qi::Message &msg, MessageSocketPtr socket);
    unsigned int addObject(BoundAnyObject obj, StreamContext* remoteReferencer, unsigned int objId = 0);
    Future<void> removeObject(unsigned int id, Future<void> fut = Future<void>{nullptr});
    void removeRemoteReferences(MessageSocketPtr socket);
    unsigned int service() { return _service;}
    virtual unsigned int nextId() = 0;
    using ObjectMap = boost::container::flat_map<unsigned int, BoundAnyObject>;
    const ObjectMap& objects() const { return _objectMap; }
  protected:
    void clear();
  private:
    /// If an object follows a complex call path, e.g it is passed by argument to a service,
    /// then returned via a signal, and finally used to make a call, it is possible that the
    /// destination of the call (the "service") does not know directly the called object, but instead one of its
    /// (ObjectHost) children knows it.
    BoundAnyObject recursiveFindObject(uint32_t objectId);
    using RemoteReferencesMap = boost::container::flat_map<StreamContext*, std::vector<unsigned int>>;
    boost::recursive_mutex    _mutex;
    unsigned int    _service;
    ObjectMap       _objectMap;
    RemoteReferencesMap _remoteReferences;
  };


  struct BoundObjectAddress
  {
    StreamContext* serializationContext = nullptr;
    unsigned int service = 0;
    unsigned int object = 0;

    BoundObjectAddress() = default;
    BoundObjectAddress(StreamContext* serializationContext, unsigned int service, unsigned int object)
      : serializationContext(serializationContext), object(object), service(service)
    {}

    KA_GENERATE_FRIEND_REGULAR_OP_EQUAL_AND_OP_LESS_3(BoundObjectAddress, serializationContext, service, object);

    friend std::size_t hash_value(const BoundObjectAddress& address)
    {
      std::size_t seed = 0;
      boost::hash_combine(seed, address.serializationContext);
      boost::hash_combine(seed, address.service);
      boost::hash_combine(seed, address.object);
      return seed;
    }

  };


  void addToGlobalIndex(BoundObjectAddress address, BoundAnyObject object);
  void addToGlobalIndex(BoundObjectAddress address, RemoteObjectPtr object);
  void removeFromGlobalIndex(BoundObject& object);
  void removeFromGlobalIndex(RemoteObject& object);
  void removeFromGlobalIndex(BoundObjectAddress address, BoundObject& object);
  void removeFromGlobalIndex(BoundObjectAddress address, RemoteObject& object);
  bool dispatchToAnyBoundObject(const Message& message, MessageSocketPtr socket);


}

#endif  // _SRC_OBJECTHOST_HPP_
