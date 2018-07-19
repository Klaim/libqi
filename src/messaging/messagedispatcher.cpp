/*
**  Copyright (C) 2012 Aldebaran Robotics
**  See COPYING for the license
*/
#include "messagedispatcher.hpp"

#include "boundobject.hpp"
#include "messagesocket.hpp"

qiLogCategory("qimessaging.messagedispatcher");

namespace qi {

#if 0
  //Needed for handling message Timeout
  static int _gst()
  {
    static const std::string st = qi::os::getenv("QI_MESSAGE_TIMEOUT");
    if (st != "")
    {
      return atoi(st.c_str());
    }
    else
    {
      // Default timeout in NAOqi 1
      return 5 * 60;
    }
  }

  static inline unsigned int getSocketTimeout()
  {
    static int _socket_timeout = _gst();
    return _socket_timeout;
  }
#endif


  MessageDispatcher::MessageDispatcher(ExecutionContext* execContext)
    : _execContext{ execContext }
  {
  }

  void MessageDispatcher::dispatch(const qi::Message& msg) {
    //remove the address from the messageSent map
    if (msg.type() == qi::Message::Type_Reply)
    {
      boost::mutex::scoped_lock sl(_messageSentMutex);
      MessageSentMap::iterator it;
      it = _messageSent.find(msg.id());
      if (it != _messageSent.end())
        _messageSent.erase(it);
      else
        qiLogDebug() << "Message " << msg.id() <<  " is not in the messageSent map";
    }

    {
      QI_ASSERT(_owner);
      if (dispatchToAnyBoundObject(msg, _owner))
      {
        return;
      }


      //boost::shared_ptr<OnMessageSignal> sig;
      //bool hit = false;
      //{
      //  boost::recursive_mutex::scoped_lock sl(_signalMapMutex);
      //  SignalMap::iterator it;
      //  it = _signalMap.find(Target(msg.service(), msg.object()));
      //  if (it != _signalMap.end())
      //  {
      //    hit = true;
      //    sig = it->second;
      //  }
      //}
      //if (sig)
      //  (*sig)(msg);
      //if (!hit) // FIXME: that should probably never happen, raise log level
      //  qiLogDebug() << "No listener for service " << msg.service();
    }
  }

  qi::SignalLink
  MessageDispatcher::messagePendingConnect(unsigned int serviceId, unsigned int objectId, boost::function<void (const qi::Message&)> fun) {
    boost::recursive_mutex::scoped_lock sl(_signalMapMutex);
    boost::shared_ptr<OnMessageSignal> &sig = _signalMap[Target(serviceId, objectId)];
    if (!sig)
      sig.reset(new OnMessageSignal(_execContext));
    sig->setCallType(MetaCallType_Direct);
    return sig->connect(fun);
  }

  void MessageDispatcher::messagePendingDisconnect(unsigned int serviceId, unsigned int objectId, qi::SignalLink linkId)
  {
    boost::shared_ptr<OnMessageSignal> sig;
    boost::recursive_mutex::scoped_lock sl(_signalMapMutex);
    SignalMap::iterator it = _signalMap.find(Target(serviceId, objectId));
    if (it != _signalMap.end())
      sig = it->second;
    else
      return;

    if (sig)
      sig->disconnectAsync(linkId);

    if (!sig || !sig->hasSubscribers())
    {
      _signalMap.erase(it);
    }
  }

  void MessageDispatcher::cleanPendingMessages()
  {
    //we are deleting the Socket and want to timeout all pending request
    //or the cleanup timer ask us to remove pending request that timed out
    while (true)
    {
      MessageAddress ma;
      {
        boost::mutex::scoped_lock l(_messageSentMutex);
        MessageSentMap::iterator it = _messageSent.begin();
        if (it == _messageSent.end())
          break;
        ma = it->second;
        _messageSent.erase(it);
      }
      //generate an error message for the caller.
      qi::Message msg(qi::Message::Type_Error, ma);
      msg.setError("Endpoint disconnected, message dropped.");
      dispatch(msg);
    }
  }

  void MessageDispatcher::sent(const qi::Message& msg) {
    //store Call id, we can use them later to notify the client
    //if the call did not succeed. (network disconnection, message lost)
    if (msg.type() == qi::Message::Type_Call)
    {
      boost::mutex::scoped_lock l(_messageSentMutex);
      MessageSentMap::iterator it = _messageSent.find(msg.id());
      if (it != _messageSent.end()) {
        qiLogInfo() << "Message ID conflict. A message with the same Id is already in flight" << msg.id();
        return;
      }
      _messageSent[msg.id()] = msg.address();
    }
    return;
  }



}
