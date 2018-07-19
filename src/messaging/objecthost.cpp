/*
**  Copyright (C) 2012 Aldebaran Robotics
**  See COPYING for the license
*/

#include <unordered_map>
#include <boost/thread/synchronized_value.hpp>
#include <boost/functional/hash.hpp>

#include <qi/actor.hpp>
#include "objecthost.hpp"
#include "remoteobject_p.hpp"

#include "boundobject.hpp"
#include <ka/typetraits.hpp>

qiLogCategory("qimessaging.objecthost");

namespace qi
{
  namespace {

    using EitherBoundOrRemoteObject = boost::variant<BoundObject*, RemoteObject*>;

    void passMessage(const Message& message, BoundObject& object, MessageSocketPtr socket)
    {
      object.onMessage(message, socket);
    }

    void passMessage(const Message& message, RemoteObject& object, MessageSocketPtr)
    {
      object.onMessagePending(message);
    }


    class BoundObjectIndex
    {
      using Index = std::unordered_map<BoundObjectAddress, EitherBoundOrRemoteObject, boost::hash<BoundObjectAddress>>;
      using ThreadSafeIndex = boost::synchronized_value<Index>;

      ThreadSafeIndex _index;

    public:

      void add(BoundObjectAddress address, EitherBoundOrRemoteObject object)
      {
        auto result = _index->emplace(std::move(address), std::move(object));
        QI_ASSERT_TRUE(result.second); // There is more than one object with the same socket.service.id, we did something wrong.
      }

      boost::optional<EitherBoundOrRemoteObject> find(BoundObjectAddress address)
      {

        auto index = _index.synchronize();
        auto find_it = index->find(address);
        if (find_it != end(*index))
        {
          return find_it->second;
        }

        return {};
      }

      void remove(BoundObjectAddress address)
      {
        _index->erase(address);
      }

      void remove(RemoteObject& object){ return removeImpl(&object); }
      void remove(BoundObject& object) { return removeImpl(&object); }


    private:
      template<class T>
      void removeImpl(T* object)
      {
        QI_ASSERT(object);

        auto index = _index.synchronize();
        for (auto it = index->begin(); it != end(*index); )
        {
          if (typeid(T*) == it->second.type())
          {
            auto* recordedObject = boost::get<T*>(it->second);
            if (object == recordedObject)
            {
              it = index->erase(it);
              continue;
            }
          }

          ++it;
        }
      }


    };

    BoundObjectIndex allBoundObjectsIndex;

  }

  void addToGlobalIndex(BoundObjectAddress address, BoundObject& object)
  {
    allBoundObjectsIndex.add(address, &object);
  }

  void addToGlobalIndex(BoundObjectAddress address, RemoteObject& object)
  {
    allBoundObjectsIndex.add(address, &object);
  }

  void removeFromGlobalIndex(RemoteObject& object)
  {
    allBoundObjectsIndex.remove(object);
  }

  void removeFromGlobalIndex(BoundObject& object)
  {
    allBoundObjectsIndex.remove(object);
  }

  void removeFromGlobalIndex(BoundObjectAddress address)
  {
    allBoundObjectsIndex.remove(address);
  }

  bool dispatchToAnyBoundObject(const Message& message, MessageSocketPtr socket)
  {

    auto foundObject = allBoundObjectsIndex.find({ socket.get(), message.service(), message.object()});
    if (foundObject)
    {
      boost::apply_visitor([&](auto&& object) {
        passMessage(message, *object, socket);
      }, *foundObject);
      return true;
    }
    else
      return false;
  }

  BoundObject::~BoundObject()
  {
    allBoundObjectsIndex.remove(*this);
  }


ObjectHost::ObjectHost(unsigned int service)
 : _service(service)
 {
 }

 ObjectHost::~ObjectHost()
 {
   // deleting our map will trigger calls to removeObject
   // so does clear() while iterating
   ObjectMap map;
   std::swap(map, _objectMap);
   map.clear();
 }

  thread_local int findDepth = 0;

BoundAnyObject ObjectHost::recursiveFindObject(uint32_t objectId)
{
  ++findDepth;
  auto scopedDepth = ka::scoped([&]{ --findDepth; });
  boost::recursive_mutex::scoped_lock lock(_mutex);
  auto it = _objectMap.find(objectId);
  auto e = end(_objectMap);
  if (it != e)
  {
    return it->second;
  }
  // Object was not found, so search in the children.
  auto b = begin(_objectMap);
  while (b != e)
  {
    BoundObject* obj{b->second.get()};
    // Children are BoundObjects. Unfortunately, BoundObject has no common
    // ancestors with ObjectHost. Nevertheless, some children can indeed
    // be ObjectHost. There is no way to get this information but to
    // perform a dynamic cast. The overhead should be negligible, as
    // this case is rare.
    if (auto* host = dynamic_cast<ObjectHost*>(obj))
    {
      if (auto obj = host->recursiveFindObject(objectId))
      {
        return obj;
      }
    }
    ++b;
  }
  return {};
}

void ObjectHost::dispatchToChildren(const qi::Message &msg, MessageSocketPtr socket)
{
  const auto objectId = msg.object();
  BoundAnyObject obj{recursiveFindObject(objectId)};
  if (!obj)
  {
    // Should we treat this as an error ? Returning without error is the
    // legacy behavior.
    return;
  }
  obj->onMessage(msg, socket);

  // Because of potential dependencies between the object's destruction
  // and the networking resources, we transfer the object's destruction
  // responsability to another thread.
  Promise<void> destructPromise;
  Future<void> destructFuture = destructPromise.future();
  qi::async([obj, destructFuture] { destructFuture.wait(); });
  obj = {};
  destructPromise.setValue(nullptr);
}

unsigned int ObjectHost::addObject(BoundAnyObject obj, StreamContext* remoteRef, unsigned int id)
{
  boost::recursive_mutex::scoped_lock lock(_mutex);
  if (!id)
    id = nextId();
  QI_ASSERT(_objectMap.find(id) == _objectMap.end());
  _objectMap[id] = obj;
  _remoteReferences[remoteRef].push_back(id);
  allBoundObjectsIndex.add({ remoteRef, _service, id}, obj.get());
  return id;
}

void ObjectHost::removeRemoteReferences(MessageSocketPtr socket)
{
  boost::recursive_mutex::scoped_lock lock(_mutex);

  RemoteReferencesMap::iterator it = _remoteReferences.find(socket.get());
  if (it == _remoteReferences.end())
    return;
  Future<void> fut{nullptr};
  for (auto id : it->second)
    fut = removeObject(id, fut);

  _remoteReferences.erase(it);
}

namespace
{
  /// Wraps a shared pointer so that when this class object is copied, the underlying
  /// shared pointer is not and therefore can most likely be reset and destroy the
  /// underlying shared pointer pointee object.
  /// shared_ptr<T> P
  template <typename Ptr>
  struct PointerDeferredResetHack
  {
    template <typename ...Args, typename Enable = ka::EnableIf<std::is_constructible<Ptr, Args...>::value>>
    PointerDeferredResetHack(Args&&... args)
      : wrap(boost::make_shared<Ptr>(std::forward<Args>(args)...))
    {
    }

    KA_GENERATE_FRIEND_REGULAR_OPS_1(PointerDeferredResetHack, wrap)

    /// Tries to destroy the wrapped pointer pointee object if possible
    void operator()() const
    {
      QI_ASSERT(wrap);
      (*wrap).reset();
    }

    boost::shared_ptr<Ptr> wrap;
  };
}

Future<void> ObjectHost::removeObject(unsigned int id, Future<void> fut)
{
  {
    boost::recursive_mutex::scoped_lock lock(_mutex);
    ObjectMap::iterator it = _objectMap.find(id);
    if (it == _objectMap.end())
    {
      qiLogDebug() << this << " No match in host for " << id;
      return fut;
    }
    const auto obj = it->second;
    _objectMap.erase(it);
    qiLogDebug() << this << " count " << obj.use_count();
    // Because of potential dependencies between the object's destruction
    // and the networking resources, we transfer the object's destruction
    // responsability to another thread.
    const auto resetter = PointerDeferredResetHack<BoundAnyObject>(std::move(obj));
    fut = fut.then([resetter](Future<void> f) {
      if (f.hasError())
        qiLogWarning() << "Object destruction failed: " << f.error();
      resetter();
    });
  }
  qiLogDebug() << this << " Object " << id << " removed.";
  return fut;
}

void ObjectHost::clear()
{
  boost::recursive_mutex::scoped_lock lock(_mutex);
  for (ObjectMap::iterator it = _objectMap.begin(); it != _objectMap.end(); ++it)
  {
    ServiceBoundObject* sbo = dynamic_cast<ServiceBoundObject*>(it->second.get());
    if (sbo && sbo->_owner)
      sbo->_owner.reset();
  }
  _objectMap.clear();
}


}

