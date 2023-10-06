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
			SidAllocation allocation;
			allocation.parse_string(get_allocation_file());
			return allocation;
		}

	public:
		TEST_METHOD(TestParse)
		{
			const auto allocation_file = get_allocation_file();
			const SidAllocation allocation;
			const size_t parsed = allocation.parse_string(allocation_file);

			Assert::AreEqual(static_cast<size_t>(167), parsed);
		}

		TEST_METHOD(TestKNotOnWeekdays)
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

			const std::vector<std::string> areas;
			const auto maybe_sid = allocation.find("EBBR", "LNO", "EKCH",
			                                       2, "25R", fake_now, areas);
			Assert::IsTrue(maybe_sid.has_value());
			Assert::AreNotEqual(std::string("LNO3K"), maybe_sid.value().sid);
		}

		TEST_METHOD(AssignsForFourEngineOnWeekdays)
		{
			const auto allocator = get_filled_allocator();
			tm fake_now{};
			fake_now.tm_wday = 1; // Monday
			fake_now.tm_mday = 25;
			fake_now.tm_mon = 8; // September
			fake_now.tm_year = 2023;
			fake_now.tm_hour = 12;

			const std::vector<std::string> areas;
			const auto maybe_sid = allocator.find("EBBR", "LNO", "EKCH",
				4, "25R", fake_now, areas);

			Assert::IsTrue(maybe_sid.has_value());
			Assert::AreEqual(std::string("LNO3K"), maybe_sid->sid);

		}

		TEST_METHOD(RespectsTSAHoevene)
		{
			const auto allocator = get_filled_allocator();
			tm fake_now{};
			fake_now.tm_wday = 1; // Monday
			fake_now.tm_mday = 25;
			fake_now.tm_mon = 8; // September
			fake_now.tm_year = 2023;
			fake_now.tm_hour = 12;

			const std::vector<std::string> areas = { "PJE_HOEVENE" };

			const auto maybe_sid = allocator.find("EBAW", "PUTTY", "EKCH", 2, "29", fake_now, areas);

			Assert::IsTrue(maybe_sid.has_value());
			Assert::AreNotEqual(std::string("PUTTY6C"), maybe_sid->sid);
			Assert::AreEqual(std::string("PUTTY2B"), maybe_sid->sid);

		}

		TEST_METHOD(RespectsMultipleTSA)
		{
			const auto allocator = get_filled_allocator();
			tm fake_now{};
			fake_now.tm_wday = 1; // Monday
			fake_now.tm_mday = 25;
			fake_now.tm_mon = 8; // September
			fake_now.tm_year = 2023;
			fake_now.tm_hour = 12;

			const std::vector<std::string> areas = {"EBTRA23", "EBTRAS6", "EBTSA29A"};
			for (const auto& area: areas)
			{
				const std::vector<std::string> active = { area };
				const auto maybe_sid = allocator.find("EBLG", "LNO", "EKCH", 2, "22L", fake_now, active);

				Assert::IsTrue(maybe_sid.has_value());
				Assert::AreNotEqual(std::string("LNO9S"), maybe_sid->sid);
				Assert::AreEqual(std::string("LNO7E"), maybe_sid->sid);
			}
		}
	};
}
