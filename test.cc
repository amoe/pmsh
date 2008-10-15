#include <cppunit/TestFixture.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

#include <libprojectM/projectM.hpp>

#include "pmsh.hh"

class Test : public CppUnit::TestFixture {
    CPPUNIT_TEST_SUITE(Test);

    CPPUNIT_TEST(test0);

    CPPUNIT_TEST_SUITE_END();

public:
    void test0() {
        CPPUNIT_ASSERT(
            strcmp(append_error_conversion("foo"), "foo: %s") == 0
        );
    }
};


CPPUNIT_TEST_SUITE_REGISTRATION(Test);

int main(int argc, char **argv) {
    CppUnit::TextUi::TestRunner runner;
    bool success;

    CppUnit::TestFactoryRegistry &registry
      = CppUnit::TestFactoryRegistry::getRegistry();
    runner.addTest(registry.makeTest());
    
    success = runner.run("", false);

    return !success;
}
    
