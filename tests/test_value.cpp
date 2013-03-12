/*
**
** Copyright (C) 2012 Aldebaran Robotics
*/


#include <map>
#include <gtest/gtest.h>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <qi/application.hpp>
#include <qitype/genericobject.hpp>
#include <qitype/genericobjectbuilder.hpp>

using namespace qi;
qiLogCategory("test");

TEST(Value, Ref)
{
  std::string s("foo");
  GenericValueRef r(GenericValuePtr::ref(s));
  r = "bar";
  ASSERT_EQ("bar", s);
  ASSERT_EQ("bar", r.toString());
  ASSERT_ANY_THROW(r = 5);
  double d = 12;
  GenericValueRef rd(GenericValuePtr::ref(d));
  rd = 15;
  ASSERT_EQ(d, 15);
  GenericValuePtr p(&d);
  GenericValueRef vr(p);
  vr = 16;
  ASSERT_EQ(d, 16);
}

TEST(Value, InPlaceSet)
{
  std::string s("foo");
  GenericValuePtr v(&s);
  v.setString("bar");
  ASSERT_EQ("bar", s);
  v.setString("pifpouf");
  ASSERT_EQ("pifpouf", s);
  double d = 12;
  v = GenericValuePtr(&d);
  v.setDouble(15.5);
  ASSERT_EQ(15.5, d);
  v.setInt(16);
  ASSERT_EQ(16.0, d);
  int i = 14;
  v = GenericValuePtr(&i);
  v.setInt(13);
  ASSERT_EQ(13, i);
}

TEST(Value, Update)
{
  std::string s("foo");
  GenericValuePtr v(&s);
  v.update(GenericValuePtr::ref("bar"));
  ASSERT_EQ("bar", s);
  v.update(GenericValuePtr::ref(std::string("baz")));
  ASSERT_EQ("baz", s);
  ASSERT_ANY_THROW(v.update(GenericValuePtr::ref(42)));
  double d = 5.0;
  v = GenericValuePtr(&d);
  v.update(GenericValuePtr::ref(42));
  ASSERT_EQ(42, d);
  v.update(GenericValuePtr::ref(42.42));
  ASSERT_DOUBLE_EQ(42.42, d);
  ASSERT_ANY_THROW(v.update(GenericValuePtr::ref("bar")));
}

TEST(Value, As)
{
  std::string s("foo");
  GenericValuePtr v(&s);
  ASSERT_EQ(&v.asString(), &s);
  ASSERT_ANY_THROW(v.asDouble());
  double d = 1.5;
  v = GenericValuePtr(&d);
  ASSERT_EQ(&d, &v.asDouble());
  ASSERT_ANY_THROW(v.asInt32());
  qi::uint32_t ui = 2; // vcxx uint32_t unqualified is ambiguous.
  v = GenericValuePtr(&ui);
  ASSERT_EQ(&ui, &v.asUInt32());
  ASSERT_ANY_THROW(v.asInt32());
  ASSERT_ANY_THROW(v.asInt16());
}

TEST(Value, Basic)
{
  GenericValuePtr v;
  int twelve = 12;
  v = GenericValuePtr::ref(twelve);
  ASSERT_TRUE(v.type);
  ASSERT_TRUE(v.value);
  ASSERT_EQ(v.toInt(), 12);
  ASSERT_EQ(v.toFloat(), 12.0f);
  ASSERT_EQ(v.toDouble(), 12.0);
  double five = 5.0;
  v = GenericValuePtr::ref(five);
  ASSERT_EQ(v.toInt(), 5);
  ASSERT_EQ(v.toFloat(), 5.0f);
  ASSERT_EQ(v.toDouble(), 5.0);
  v = GenericValuePtr::ref("foo");
  ASSERT_EQ("foo", v.toString());
}

TEST(Value, Map)
{
  std::map<std::string, double> map;
  map["foo"] = 1;
  map["bar"] = 2;
  map["baz"] = 3;
  GenericValuePtr v(&map);
  ASSERT_EQ(3u, v.size());

  ASSERT_EQ(v["foo"].toInt(), 1);
  ASSERT_EQ(v[std::string("bar")].toInt(), 2);

  v["baz"].setInt(4);
  ASSERT_EQ(v["baz"].toInt(), 4);

  // write to ref
  v["baz"] = 5;
  ASSERT_EQ(v["baz"].toInt(), 5);

  // Create a new element
  qiLogDebug() << "Insert bimm";
  ASSERT_ANY_THROW(v[GenericValue::from("bimm")].setString("foo"));
  v["bimm"].setInt(42);
  qiLogDebug() << "Check bimm";
  ASSERT_EQ(v["bimm"].toInt(), 42);
  ASSERT_EQ(v[std::string("bimm")].toInt(), 42);


  // Create a new element of an existing string length
  qiLogDebug() << "Insert pif";
  v[GenericValue::from("pif")].setInt(43);
  qiLogDebug() << "Check pif";
  ASSERT_EQ(v["pif"].toInt(), 43);
  ASSERT_EQ(v[std::string("pif")].toInt(), 43);

  // insert existing
  v.insert("pif", 63);
  ASSERT_EQ(v["pif"].toInt(), 63);

  // insert new
  v.insert("pouf", 65);
  ASSERT_EQ(v["pouf"].toInt(), 65);

  ASSERT_EQ(6u, v.size());
  ASSERT_TRUE(!v.find("nokey").type);
  // wrong keytype
  ASSERT_ANY_THROW(v.find(42));
  // append on map
  ASSERT_ANY_THROW(v.append("foo"));
}

static bool triggered = false;
static void nothing(GenericObject*) {triggered = true;}

TEST(Value, ObjectPtr)
{
  {
    ObjectPtr o((GenericObject*)1, &nothing);
    ASSERT_TRUE(o);
    ASSERT_TRUE(o.get());
    GenericValuePtr v = GenericValuePtr::ref(o);
    qi::ObjectPtr out = v.to<ObjectPtr>();
    ASSERT_TRUE(out);
    ASSERT_EQ(o.get(), out.get());
    out = v.toObject();
    ASSERT_TRUE(out);
    ASSERT_EQ(o.get(), out.get());
  }
  ASSERT_TRUE(triggered);
}


TEST(Value, list)
{
  int one = 1;
  GenericValuePtr v = GenericValuePtr::ref(one);
  v.set(5);
  ASSERT_ANY_THROW(v.set("foo"));
  ASSERT_ANY_THROW(v.set(std::vector<int>()));
  std::vector<int> vint;
  vint.push_back(12);
  v = GenericValuePtr(&vint);
  v.append(7);
  ASSERT_EQ(7, v[1].toInt());
  ASSERT_EQ(7, v[1].toFloat());
  ASSERT_EQ(7, v.element<int>(1));
  v[1].setInt(8);
  ASSERT_EQ(8, v[1].toInt());
  v.element<int>(1) = 9;
  ASSERT_EQ(9, v[1].toInt());
  ASSERT_ANY_THROW(v.element<double>(1)); // wrong type
  ASSERT_ANY_THROW(v.element<int>(17));   // out of bound
  EXPECT_EQ(v.as<std::vector<int> >().size(), v.size());
}

struct TStruct
{
  double d;
  std::string s;
  bool operator ==(const TStruct& b) const { return d == b.d && s == b.s;}
};
struct Point
{
  int x,y;
  bool operator ==(const Point& b) const { return x==b.x && y == b.y;}
};

QI_TYPE_STRUCT(TStruct, d, s);
QI_TYPE_STRUCT(Point, x, y);

TEST(Value, Tuple)
{
  // Create a Dynamic tuple from vector
  std::vector<GenericValue> v;
  GenericValuePtr gv(&v);
  gv.append(GenericValue::from(12.0));
  gv.append(GenericValue::from("foo")); // cstring not std::string
  GenericValue gtuple = gv.toTuple(false);
  TStruct t;
  t.d = 12.0;
  t.s = "foo";
  TStruct gtupleconv = gtuple.to<TStruct>();
  ASSERT_EQ(t, gtupleconv);
  gtuple[0].setDouble(15);
  ASSERT_EQ(15, gtuple.to<TStruct>().d);

  // create a static tuple
  std::vector<double> vd;
  vd.push_back(1);
  gv = GenericValuePtr(&vd);
  gv.append(2);
  gtuple = gv.toTuple(true);
  Point p;
  p.x = 1;
  p.y = 2;
  p == p;
  ASSERT_EQ(p , gtuple.to<Point>());
  p.x = 3;
  gtuple[0].setDouble(gtuple[0].toDouble() + 2);
  ASSERT_EQ(p, gtuple.to<Point>());
}


TEST(Value, DefaultMap)
{ // this one has tricky code and deserves a test)
  Type* dmt = Type::fromSignature("{si}");
  GenericValue val = GenericValue(GenericValuePtr(dmt), false, true);
  ASSERT_EQ(0u, val.size());
  val["foo"] = 12;
  ASSERT_EQ(1u, val.size());
  ASSERT_EQ(12, val.find("foo").toInt());
  ASSERT_EQ(0, val.find("bar").type);
  val.insert(std::string("bar"), 13);
  ASSERT_EQ(13, val.element<int>("bar"));
  val.element<int>("foo") = 10;
  ASSERT_EQ(10, val.find("foo").toInt());
  GenericIterator b = val.begin();
  GenericIterator end = val.end();
  int sum = 0;
  while (b != end)
  {
    GenericValueRef deref = *b;
    sum += deref[1].toInt();
    ++b;
  }
  ASSERT_EQ(23, sum);

  GenericValuePtr valCopy = val.clone();
  ASSERT_EQ(13, valCopy.element<int>("bar"));
  ASSERT_EQ(2u, valCopy.size());
  // reset val, checks valCopy still works
  val.reset();
  val = GenericValue::from(5);
  ASSERT_EQ(13, valCopy.element<int>("bar"));
  ASSERT_EQ(2u, valCopy.size());
  valCopy.destroy();
}


TEST(Value, STL)
{
  std::vector<int> v;
  GenericValuePtr gv(&v);
  gv.append(1);
  gv.append(3);
  gv.append(2);
  std::vector<int> w;
  // seems there are overloads for push_back, need to explicitly cast to signature
  std::for_each(gv.begin(), gv.end(),
    boost::lambda::bind((void (std::vector<int>::*)(const int&))&std::vector<int>::push_back,
      boost::ref(w),
      boost::lambda::bind(&GenericValueRef::toInt, boost::lambda::_1)));
  ASSERT_EQ(3u, w.size());
  ASSERT_EQ(v, w);
  GenericIterator mine = std::min_element(gv.begin(), gv.end());
  ASSERT_EQ(1, (*mine).toInt());
  mine = std::find_if(gv.begin(), gv.end(),
    boost::lambda::bind(&GenericValueRef::toInt, boost::lambda::_1) == 3);
  ASSERT_EQ(3, (*mine).toInt());
  (*mine).setInt(4);
  ASSERT_EQ(4, v[1]);
  mine = std::find_if(gv.begin(), gv.end(),
    boost::lambda::bind(&GenericValueRef::toInt, boost::lambda::_1) == 42);
  ASSERT_EQ(mine, gv.end());

  std::vector<int> v2;
  v2.push_back(10);
  v2.push_back(1);
  v2.push_back(100);
  // v has correct size
  std::copy(v2.begin(), v2.end(), gv.begin());
  ASSERT_EQ(v2, v);
  // copy other-way-round requires cast from GenericValueRef to int


  std::vector<GenericValueRef> vg;
  vg.insert(vg.end(), v.begin(), v.end());
  std::sort(vg.begin(), vg.end());
  ASSERT_EQ(321, vg[0].toInt() + vg[1].toInt()*2 + vg[2].toInt() * 3);
}
int main(int argc, char **argv) {
  qi::Application app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
