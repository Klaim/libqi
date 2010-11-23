#pragma once
/*
** Author(s):
**  - Chris Kilner <ckilner@aldebaran-robotics.com>
**
** Copyright (C) 2010 Aldebaran Robotics
*/
#ifndef   __QI_MESSAGING_DETAIL_MASTER_IMPL_HPP__
#define   __QI_MESSAGING_DETAIL_MASTER_IMPL_HPP__

#include <string>
#include <qi/messaging/detail/server_impl.hpp>
#include <qi/messaging/detail/mutexednamelookup.hpp>
#include <qi/messaging/detail/address_manager.hpp>
#include <qi/messaging/context.hpp>
#include <qi/transport/detail/network/endpoint_context.hpp>

namespace qi {
  namespace detail {

    class MasterImpl {
    public:
      explicit MasterImpl(const std::string& masterAddress);

      ~MasterImpl();

      void registerService(const std::string& methodSignature,
                           const std::string& serverID);

      int registerServer(const std::string& name,
                          const std::string& id,
                          const std::string& contextID,
                          const std::string& machineID,
                          const int&         platformID,
                          const std::string& publicIPAddress);

      void unregisterServer(const std::string& id);

      void registerClient(const std::string& name,
                          const std::string& id,
                          const std::string& contextID,
                          const std::string& machineID,
                          const int& plarformID);

      void unregisterClient(const std::string& id);

      std::string locateService(const std::string& methodSignature, const std::string& clientID);

      const std::map<std::string, std::string>& listServices();

    private:
      std::string _name;
      std::string _address;
      ServerImpl  _server;

      void xInit();

      // Helper method
      template <typename METHOD_TYPE>
      void xAddMasterMethod(std::string name, METHOD_TYPE method) {
        const EndpointContext& serverContext = _server.getEndpointContext();
        std::string sig = makeSignature(name, method);
        _server.addService(makeSignature(name, method), makeFunctor(this, method));
        registerService(sig, serverContext.endpointID);
      }

      // map from methodSignature to nodeAddress
      MutexedNameLookup<std::string> _knownServices;

      // map from id to EndpointContext
      MutexedNameLookup<qi::detail::EndpointContext> _knownServers;

      // map from id to EndpointContext
      MutexedNameLookup<qi::detail::EndpointContext> _knownClients;

      AddressManager addressManager;
    };
  }
}

#endif // __QI_MESSAGING_DETAIL_MASTER_IMPL_HPP__

