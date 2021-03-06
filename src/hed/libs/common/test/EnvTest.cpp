#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cppunit/extensions/HelperMacros.h>

#include <time.h>
#include <arc/Utils.h>

class EnvTest
  : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(EnvTest);
  CPPUNIT_TEST(TestEnv);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void TestEnv();

};

void EnvTest::setUp() {
}

void EnvTest::tearDown() {
}

void EnvTest::TestEnv() {
  std::string val;
  bool found = false;

  Arc::SetEnv("TEST_ENV_VAR","TEST_ENV_VALUE");
  CPPUNIT_ASSERT_EQUAL(std::string("TEST_ENV_VALUE"), Arc::GetEnv("TEST_ENV_VAR",found));
  CPPUNIT_ASSERT_EQUAL(true, found);
  Arc::UnsetEnv("TEST_ENV_VAR");
  CPPUNIT_ASSERT_EQUAL(std::string(""), Arc::GetEnv("TEST_ENV_VAR",found));
  CPPUNIT_ASSERT_EQUAL(false, found);

  Arc::SetEnv("TEST_ENV_VAR","TEST_ENV_VALUE2");
  Arc::SetEnv("TEST_ENV_VAR","TEST_ENV_VALUE3");
  Arc::SetEnv("TEST_ENV_VAR","TEST_ENV_VALUE4", false);
  CPPUNIT_ASSERT_EQUAL(std::string("TEST_ENV_VALUE3"), Arc::GetEnv("TEST_ENV_VAR",found));

  time_t start = ::time(NULL);
  for(int n = 0; n < 1000000; ++n) {
    Arc::SetEnv("TEST_ENV_VAR1","TEST_ENV_VALUE");
    Arc::UnsetEnv("TEST_ENV_VAR1");
    Arc::SetEnv("TEST_ENV_VAR1","TEST_ENV_VALUE");
    Arc::SetEnv("TEST_ENV_VAR2","TEST_ENV_VALUE");
    Arc::UnsetEnv("TEST_ENV_VAR2");
    Arc::SetEnv("TEST_ENV_VAR2","TEST_ENV_VALUE");
    Arc::SetEnv("TEST_ENV_VAR3","TEST_ENV_VALUE");
    Arc::UnsetEnv("TEST_ENV_VAR3");
    Arc::SetEnv("TEST_ENV_VAR3","TEST_ENV_VALUE");
    // Limit duration by reasonable value
    if(((unsigned int)(time(NULL)-start)) > 300) break;
  }
}

CPPUNIT_TEST_SUITE_REGISTRATION(EnvTest);
