#include <cstdio>
#include <typeinfo>

namespace {
	struct testconstruction 
	{
		testconstruction()
		{
			std::printf("Testing construction of %p\n", this);
		}

		void throwit(int x)
		{
			throw x;
		}

		~testconstruction()
		{
			std::printf("Testing destruction of %p\n", this);
		}
	};

	testconstruction test;
}

int main()
{
	std::printf("object %p exists, typename %s.\n", &test, typeid(test).name());
	try {
		test.throwit(1);
	} catch (int z) {
		std::printf("object %p threw %d\n", &test, z);
	}
}
