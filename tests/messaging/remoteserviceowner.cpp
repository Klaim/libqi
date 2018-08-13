
#include <chrono>
#include <thread>

#include <qi/applicationsession.hpp>
#include <qi/anymodule.hpp>
#include <qi/log.hpp>

qiLogCategory("RemoteServiceOwner");

struct PingPongService
{
  qi::AnyObject take()
  {
    return object;
  }

  void give(qi::AnyObject newObject)
  {
    object = newObject;
  }

private:
  qi::AnyObject object;
};

QI_REGISTER_OBJECT(PingPongService, take, give)


struct MyService {

  MyService()
  {
    list.reserve(5000);
  }

  void setObject(qi::AnyObject obj)
  {
    /// storing copies of the original object (those are kept alive)
    list.push_back(obj);

  }

  void setObjectList(std::vector<qi::AnyObject> obj)
  {
    /// storing copies of the original object (those are kept alive)
    list = std::move(obj);

  }

  void test()
  {
    using namespace std::chrono;
    static int callCount = 0;
    ++callCount;
    qi::AnyObject& o = list.back();

    // getting the content of the property
    for (int i = 0; i < 1; ++i)
    {
      const auto startTime = high_resolution_clock::now();
      int cont = o.call<int>("foo");
      const auto endTime = high_resolution_clock::now();

      const auto discussDuration = endTime - startTime;

      qiLogInfo("TEST") << "Test " << callCount << " call " << i << " ID{" << o.ptrUid() << "} : "
        << duration_cast<milliseconds>(discussDuration).count() << " ms ("
        << duration_cast<nanoseconds>(discussDuration).count() << " ns)"
        ;
    }

  }

  qi::AnyObject getIt()
  {
    return list.back();
  }

  void clear()
  {
    list.clear();
  }

  std::vector<qi::AnyObject> list;

};

QI_REGISTER_OBJECT(MyService, setObject, setObjectList, test, clear, getIt)


int main(int argc, char **argv) {
  qi::ApplicationSession app(argc, argv);
  app.session()->setIdentity(qi::path::findData("qi", "server.key"),
                             qi::path::findData("qi", "server.crt"));
  qi::log::addFilter("qi*", qi::LogLevel_Debug);

  qiLogInfo() << "Attempting connection to " << app.url().str();
  app.startSession();
  auto client = app.session();
  assert(client->isConnected());

  auto service = boost::make_shared<PingPongService>();
  qiLogInfo() << "Created PingPongService";
  client->registerService("PingPongService", service);
  client->registerService("MyService", boost::make_shared<MyService>());
  qiLogInfo() << "Registered PingPongService";
  app.run();

  return 0;
}
