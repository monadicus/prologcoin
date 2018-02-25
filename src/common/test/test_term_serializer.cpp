#include <iostream>
#include <iomanip>
#include <assert.h>
#include <common/term_env.hpp>
#include <common/term_serializer.hpp>

using namespace prologcoin::common;

static void header( const std::string &str )
{
    std::cout << "\n";
    std::cout << "--- [" + str + "] " + std::string(60 - str.length(), '-') << "\n";
    std::cout << "\n";
}

static void test_term_serializer_simple()
{
    header( "test_term_serializer_simple()" );

    term_env env;
    term t = env.parse("foo(1, bar(kallekula, [1,2,baz]), Foo, kallekula, world, test4711, Foo, Bar).");

    auto str1 = env.to_string(t);

    std::cout << "WRITE TERM: " << str1 << "\n";

    term_serializer ser(env);
    term_serializer::buffer_t buf;
    ser.write(buf, t);

    ser.print_buffer(buf);

    term_env env2;
    term_serializer ser2(env2);

    term t2;
    try {
	t2 = ser2.read(buf);
    } catch (serializer_exception &ex) {
	std::cout << "EXCEPTION WHEN READING: " << ex.what() << "\n";
	std::cout << "Here's the data...\n";
	ser.print_buffer(buf);
	assert("No exception expected" == nullptr);
    }
    auto str2 = env2.to_string(t2);

    std::cout << "READ TERM:  " << str2 << "\n";
    
    assert(str1 == str2);
}

namespace prologcoin { namespace common { namespace test {

class test_term_serializer {
public:

    static void test_exception(const std::string &label,
			       std::initializer_list<cell> init,
			       const std::string &expect_str)
    {
	term_env env;

	term_serializer::buffer_t buffer;
	term_serializer ser(env);
	size_t offset = 0;
	for (auto c : init) {
	    ser.write_cell(buffer, offset, c);
	    offset += sizeof(cell);
	}

	try {
	    static_cast<void>(ser.read(buffer));

	    std::cout << label << ": actual: no exception; expected: " << expect_str << "\n";
	    std::cout << "Here's the data:\n";
	    ser.print_buffer(buffer);

	    assert("No exception as expected" == nullptr);

	} catch (serializer_exception &ex) {
	    std::string actual_str = ex.what();
	    std::cout << label << ": actual: " << actual_str << "; expected: " << expect_str << "\n";
	    bool ok = actual_str.find(expect_str) != std::string::npos;
	    if (!ok) {
		std::cout << "Here's the data:\n";
		ser.print_buffer(buffer);
	    }
	    assert(ok);
	}
    }
};

}}}

using namespace prologcoin::common::test;

static void test_term_serializer_exceptions()
{
    header( "test_term_serializer_exceptions()" );

    test_term_serializer::test_exception("UNSUPPORTED VERSION",
					 {con_cell("ver0",0)},
					 "Unsupported version");

    test_term_serializer::test_exception("WRONG REMAP",
					 {con_cell("ver1",0),
					  con_cell("haha",1)},
					 "remap section");

    test_term_serializer::test_exception("MISSING PAMER",
					 {con_cell("ver1",0),
					  con_cell("remap",0)},
					 "Unexpected end");

    test_term_serializer::test_exception("INDEX1",
					 {con_cell("ver1",0),
					  con_cell("remap",0),
					  str_cell(123)},
					 "ref/con in remap section");

    test_term_serializer::test_exception("INDEX2",
					 {con_cell("ver1",0),
					  con_cell("remap",0),
					  ref_cell(123),
			  		  int(4711)},
					 "expected encoded string");

    test_term_serializer::test_exception("INDEX3",
					 {con_cell("ver1",0),
					  con_cell("remap",0),
					  ref_cell(123),
				 	  int_cell::encode_str("frotzba",true),
					  int_cell::encode_str("f",false),
					  str_cell(456)
					  },
					 "ref/con in remap section");

    test_term_serializer::test_exception("INDEX4",
					 {con_cell("ver1",0),
					  con_cell("remap",0),
					  con_cell("pamer",0),
			 	 	  ref_cell(0),
					  },
					 "Missing index entry for 0:REF");

    test_term_serializer::test_exception("INDEX5",
					 {con_cell("ver1",0),
					  con_cell("remap",0),
					  con_cell("pamer",0),
				 	  con_cell(123,2),
					  },
					 "Missing index entry");

    test_term_serializer::test_exception("DANGLING1",
					 {con_cell("ver1",0),
					  con_cell("remap",0),
					  con_cell("pamer",0),
				 	  str_cell(123),
					  },
					 "Dangling pointer");

    test_term_serializer::test_exception("FUNCTORERR1",
					 {con_cell("ver1",0),
					  con_cell("remap",0),
					  con_cell("pamer",0),
				 	  str_cell(4),
				 	  int_cell(123)
					  },
					 "Illegal functor 123:INT");


    test_term_serializer::test_exception("ARGERR1",
					 {con_cell("ver1",0),
					  con_cell("remap",0),
					  con_cell("pamer",0),
				 	  str_cell(5),
				 	  str_cell(6),
					  con_cell("f",1),
					  con_cell("g",1)
					  },
					 "Missing argument for g/1:CON");

    test_term_serializer::test_exception("DANGLING2",
					 {con_cell("ver1",0),
					  con_cell("remap",0),
					  con_cell("pamer",0),
				 	  str_cell(5),
				 	  str_cell(7),
					  con_cell("f",1),
					  con_cell("g",1)
					  },
					 "Dangling pointer for 7:STR");

    test_term_serializer::test_exception("SELFERR1",
					 {con_cell("ver1",0),
					  con_cell("remap",0),
					  con_cell("pamer",0),
				 	  str_cell(3),
					  con_cell("f",1)
					  },
					 "Illegal functor 3:STR");

    test_term_serializer::test_exception("CYCLIC1",
					 {con_cell("ver1",0),
					  con_cell("remap",0),
					  con_cell("pamer",0),
				 	  ref_cell(4),
					  ref_cell(3)
					  },
					 "Cyclic reference for 4:REF");


}

int main( int argc, char *argv[] )
{
    test_term_serializer_simple();
    test_term_serializer_exceptions();

    return 0;
}