/*
**  Copyright (C) 2012 Aldebaran Robotics
**  See COPYING for the license
*/

#include <unordered_map>
#include <algorithm>
#include <boost/thread/synchronized_value.hpp>
#include <boost/functional/hash.hpp>
#include <boost/container/flat_set.hpp>

#include <qi/actor.hpp>
#include "objecthost.hpp"
#include "remoteobject_p.hpp"

#include "boundobject.hpp"
#include <ka/typetraits.hpp>
#include <ka/algorithm.hpp>

qiLogCategory("qimessaging.objecthost");

namespace qi
{
  namespace {

    template<class T>
    using MaybeObject = boost::weak_ptr<T>;
    using MaybeBoundObject = MaybeObject<BoundObject>;
    using MaybeRemoteObject = MaybeObject<RemoteObject>;
    using EitherBoundOrRemoteObject = boost::variant<MaybeBoundObject, MaybeRemoteObject>;
    using ObjectSet = boost::container::flat_set<EitherBoundOrRemoteObject>;

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
      using Index = std::unordered_map<BoundObjectAddress, ObjectSet, boost::hash<BoundObjectAddress>>;
      using ThreadSafeIndex = boost::synchronized_value<Index>;

      ThreadSafeIndex _index;

      struct IsAlive
      {
        template<class T>
        bool operator()(T&& maybeObject) const {
          return !maybeObject.expired(); // not sure if it's safe to do it that way
        }
      };

      struct IsSame
      {
        EitherBoundOrRemoteObject* object;
        template<class T>
        bool operator()(T&& maybeObject) const {
          return boost::get<T>(*object).lock() == boost::get<T>(maybeObject).lock();
        }
      };

    public:

      void add(BoundObjectAddress address, EitherBoundOrRemoteObject object)
      {
        QI_ASSERT_FALSE(object.empty());
        auto index = _index.synchronize();
        auto& objects = (*index)[address];
        objects.reserve(2);
        auto result = objects.insert(std::move(object)); // We do not move object in because we might need it if the next test fails.
        //if (!result.second) // there is something already stored, we need to erase it if it's dead and store the new one
        //{
        //  QI_ASSERT_FALSE(result.first->second.empty());
        //  const bool storedObjectIsAlive =  boost::apply_visitor(IsAlive{}, result.first->second);
        //  if (storedObjectIsAlive)
        //  {
        //    if (object.which() == result.first->second.which()
        //      && boost::apply_visitor(IsSame{ &object }, result.first->second))
        //    {
        //      return; // It's the same object.
        //    }
        //  }
        //  QI_ASSERT_FALSE(storedObjectIsAlive); // There is more than one object with the same socket.service.id, we did something wrong.
        //  result.first->second = object;
        //}
      }

      boost::optional<ObjectSet> find(BoundObjectAddress address)
      {
        auto index = _index.synchronize();
        auto objectsIt = findImpl(*index, address);
        if (objectsIt != end(*index))
        {
          return objectsIt->second;
        }
        return {};
      }
/*
      void remove(BoundObjectAddress address)
      {
        _index->erase(address);
      }*/

      void remove(BoundObjectAddress address, RemoteObject& object)
      {
        return removeImpl(*_index, address, &object);
      }

      void remove(BoundObjectAddress address, BoundObject& object)
      {
        return removeImpl(*_index, address, &object);
      }

      void remove(RemoteObject& object)
      {
        return removeImpl(*_index, &object);
      }

      void remove(BoundObject& object)
      {
        return removeImpl(*_index, &object);
      }


    private:

      static Index::iterator findImpl(Index& index, BoundObjectAddress address)
      {
        auto find_it = index.find(address);
        if (find_it != end(index))
        {
          auto& objects = find_it->second;

          ka::erase_if(objects, [&](EitherBoundOrRemoteObject& maybeObject) {
            return maybeObject.empty() || !boost::apply_visitor(IsAlive{}, maybeObject);
          });

          if (objects.empty())
          {
            index.erase(find_it);
            return end(index);
          }

          return find_it;
        }

        return end(index);
      }

      template<class T>
      static void eraseDeadOrTargettedObject(ObjectSet& objects, T* object) // TODO: should be a struct
      {
        ka::erase_if(objects, [&](EitherBoundOrRemoteObject& maybeObject) {

          if (maybeObject.empty())
            return true;

          if (typeid(MaybeObject<T>) == maybeObject.type())
          {
            auto maybeRecordedObject = boost::get<MaybeObject<T>>(maybeObject);
            auto recordedObject = maybeRecordedObject.lock(); // TODO: find a way to make it less expensive
            if (!recordedObject || object == recordedObject.get())
            {
              return true;
            }
          }

          return false;
        });
      }

      template<class T>
      static void removeImpl(Index& index, BoundObjectAddress address, T* object)
      {
        auto objectsIt = findImpl(index, address);
        if (objectsIt != end(index))
        {
          auto& objects = objectsIt->second;
          eraseDeadOrTargettedObject(objects, object);
          if (objects.empty())
            index.erase(objectsIt);
        }
      }

      template<class T>
      static void removeImpl(Index& index, T* object)
      {
        QI_ASSERT(object);

        for (auto it = begin(index); it != end(index); )
        {
          auto& objects = it->second;
          eraseDeadOrTargettedObject(objects, object);

          if(objects.empty())
          {
            it = index.erase(it);
            continue;
          }
          ++it;
        }

      }


    };

    BoundObjectIndex allBoundObjectsIndex;

  }

  void addToGlobalIndex(BoundObjectAddress address, BoundAnyObject object)
  {
    QI_ASSERT_TRUE(object);
    MaybeBoundObject maybeObject = object;
    object.reset();
    allBoundObjectsIndex.add(address, maybeObject);
  }

  void addToGlobalIndex(BoundObjectAddress address, RemoteObjectPtr object)
  {
    QI_ASSERT_TRUE(object);
    RemoteObjectPtr maybeObject = object;
    object.reset();
    allBoundObjectsIndex.add(address, maybeObject);
  }

  void removeFromGlobalIndex(RemoteObject& object)
  {
    allBoundObjectsIndex.remove(object);
  }

  void removeFromGlobalIndex(BoundObject& object)
  {
    allBoundObjectsIndex.remove(object);
  }

  void removeFromGlobalIndex(BoundObjectAddress address, BoundObject& object)
  {
    allBoundObjectsIndex.remove(address, object);
  }

  void removeFromGlobalIndex(BoundObjectAddress address, RemoteObject& object)
  {
    allBoundObjectsIndex.remove(address, object);
  }

  namespace {
    struct PassMessageIfObjectAlive
    {
      const Message& message;
      const MessageSocketPtr& socket;

      template<class T>
      bool operator()(T&& maybeObject) const {
        if (auto object = maybeObject.lock())
        {
          passMessage(message, *object, socket);
          return true;
        }
        else
        {
          return false;
        }
      }
    };
  }

  bool dispatchToAnyBoundObject(const Message& message, MessageSocketPtr socket)
  {
    const BoundObjectAddress address{ socket.get(), message.service(), message.object() };
    auto foundObjects = allBoundObjectsIndex.find(address);
    if (foundObjects)
    {
      bool dispatched = false;
      for (auto& maybeObject : *foundObjects)
      {
        dispatched = dispatched || boost::apply_visitor(PassMessageIfObjectAlive{message, socket}, maybeObject);
      }
      return dispatched;
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
  allBoundObjectsIndex.add({ remoteRef, _service, id}, obj);
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

