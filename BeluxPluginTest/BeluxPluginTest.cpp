#include "CppUnitTest.h"
#include "../BeluxPlugin/SidAllocation.h"
#include <fstream>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace BeluxPluginTest
{
	TEST_CLASS(SidAllocationTest)
	{
	private:
		static std::string get_allocation_file()
		{
			std::ifstream ifs("SID_ALLOCATION.txt"); // Copied by build
			// Non-const to allow a move out
			std::string allocation_file((std::istreambuf_iterator<char>(ifs)),
			                            (std::istreambuf_iterator<char>()));
			return allocation_file;
		}

		static SidAllocation get_filled_allocator()
		{
			const SidAllocation allocation;
			allocation.parse_string(get_allocation_file());
			return allocation;
		}

	public:
		TEST_METHOD(TestParse)
		{
			const auto allocation_file = get_allocation_file();
			const SidAllocation allocation;
			const size_t parsed = allocation.parse_string(allocation_file);

			Assert::AreEqual(static_cast<size_t>(168), parsed);
		}

		TEST_METHOD(Test2DNotOnWeekdays)
		{
			const auto allocation_file = get_allocation_file();
			const SidAllocation allocation;
			const size_t parsed = allocation.parse_string(allocation_file);
			Assert::IsTrue(parsed > 0);

			tm fake_now{};
			fake_now.tm_wday = 1; // Monday
			fake_now.tm_mday = 25;
			fake_now.tm_mon = 8; // September
			fake_now.tm_year = 2023;
			fake_now.tm_hour = 22; // Late, to perhaps trigger the four-engine case.

			const auto maybe_sid = allocation.find("EBBR", "CIV", "EKCH",
			                                       2, "25R", fake_now);
			Assert::IsTrue(maybe_sid.has_value());
			Assert::AreNotEqual(std::string("CIV2D"), maybe_sid.value().sid);
		}

		TEST_METHOD(AssignsCIV2DForFourEngineOnWeekdays)
		{
			const auto allocator = get_filled_allocator();
			tm fake_now{};
			fake_now.tm_wday = 1; // Monday
			fake_now.tm_mday = 25;
			fake_now.tm_mon = 8; // September
			fake_now.tm_year = 2023;
			fake_now.tm_hour = 22; // Late, to perhaps trigger the four-engine case.

			const auto maybe_sid = allocator.find("EBBR", "CIV", "EKCH",
				4, "25R", fake_now);

			Assert::IsTrue(maybe_sid.has_value());
			Assert::AreEqual(std::string("CIV2D"), maybe_sid->sid);

		}
	};
}
