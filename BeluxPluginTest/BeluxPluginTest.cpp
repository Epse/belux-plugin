#include "CppUnitTest.h"
#include "../BeluxPlugin/SidAllocation.h"
#include "../BeluxPlugin/LaraParser.h"
#include <fstream>
#include <string>
#include <chrono>


using namespace std::chrono;

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

		static LaraParser get_filled_lara()
		{
			LaraParser parser;
			std::ifstream ifs("TopSkyAreasManualAct.txt");
			std::string activation_file((std::istreambuf_iterator<char>(ifs)),
				(std::istreambuf_iterator<char>()));
			parser.parse_string(activation_file);
			return parser;
		}

	public:
		TEST_METHOD(TestParse)
		{
			const auto allocation_file = get_allocation_file();
			const SidAllocation allocation;
			const size_t parsed = allocation.parse_string(allocation_file);

			Assert::AreEqual(static_cast<size_t>(168), parsed);
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
			fake_now.tm_year = 2023 - 1900;
			fake_now.tm_hour = 22; // Late, to perhaps trigger the four-engine case.

			const time_t epoch = mktime(&fake_now);
			Assert::IsTrue(epoch > 0);
			const auto clock = system_clock::from_time_t(epoch);


			const std::vector<std::string> areas;
			const auto maybe_sid = allocation.find("EBBR", "LNO", "EKCH",
				2, "25R", clock, areas, std::vector<std::string>{});
			Assert::IsTrue(maybe_sid.has_value());
			Assert::AreNotEqual(std::string("LNO3K"), maybe_sid.value().sid);
		}

		TEST_METHOD(AssignsForFourEngineOnWeekdays)
		{
			const auto allocator = get_filled_allocator();
			tm fake_now{};
			fake_now.tm_year = 2023 - 1900;
			fake_now.tm_mon = 8; // September
			fake_now.tm_mday = 25;
			fake_now.tm_wday = 1; // Monday
			fake_now.tm_hour = 12;
			fake_now.tm_min = 1;
			fake_now.tm_isdst = -1;
			const time_t epoch = mktime(&fake_now);
			Assert::IsTrue(epoch > 0);
			const auto clock = system_clock::from_time_t(epoch);

			const std::vector<std::string> areas;
			const auto maybe_sid = allocator.find("EBBR", "LNO", "EKCH",
				4, "25R", clock, areas, std::vector<std::string>{});

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
			fake_now.tm_year = 2023 - 1900;
			fake_now.tm_hour = 12;
			const auto clock = system_clock::from_time_t(mktime(&fake_now));

			const std::vector<std::string> areas = { "PJE_HOEVENE" };

			const auto maybe_sid = allocator.find("EBAW", "PUTTY", "EKCH", 2, "29", clock, areas, std::vector<std::string>{});

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
			fake_now.tm_year = 2023 - 1900;
			fake_now.tm_hour = 12;
			const auto clock = system_clock::from_time_t(mktime(&fake_now));

			const std::vector<std::string> areas = { "EBTRA23", "EBTRAS6", "EBTSA29A" };
			for (const auto& area : areas)
			{
				const std::vector<std::string> active = { area };
				const auto maybe_sid = allocator.find("EBLG", "LNO", "EKCH", 2, "22L", clock, active, std::vector<std::string>{});

				Assert::IsTrue(maybe_sid.has_value());
				Assert::AreNotEqual(std::string("LNO9S"), maybe_sid->sid);
				Assert::AreEqual(std::string("LNO7E"), maybe_sid->sid);
			}
		}

		TEST_METHOD(HandlesMultipleAreas)
		{
			const auto allocator = get_filled_allocator();
			// Date based on bug report
			tm fake_now{};
			fake_now.tm_wday = 3; // Wednesday
			fake_now.tm_mday = 25;
			fake_now.tm_mon = 9; // October, somehow
			fake_now.tm_year = 2023 - 1900;
			fake_now.tm_hour = 11; // 13:13
			fake_now.tm_min = 13;
			const auto clock = system_clock::from_time_t(mktime(&fake_now));

			const LaraParser parser = get_filled_lara();
			const auto active = parser.get_active(fake_now);

			const auto maybe_sid = allocator.find("EBLG", "LNO", "EDDF", 4, "22L", clock, active, std::vector<std::string>{});
			Assert::IsTrue(maybe_sid.has_value());
			Assert::AreNotEqual(std::string("LNO9S"), maybe_sid->sid);
			Assert::AreEqual(std::string("LNO7E"), maybe_sid->sid);
		}

		TEST_METHOD(RespectsTimeZonesPositive)
		{
			// DENUT7N gets assigned for RWY19, if between 2345 and 0344L, or 25R is not active.
			// We will test with 25R active of course

			const auto allocator = get_filled_allocator();

			// This is local. October 1st is winter time, so at 01:05L it should be active. This is 2305Z, so not active if incorrectly converted.
			tm fake_now{};
			fake_now.tm_mday = 2;
			fake_now.tm_mon = 9; // October, somehow
			fake_now.tm_year = 2023 - 1900;
			fake_now.tm_hour = 1;
			fake_now.tm_min = 5;
			fake_now.tm_isdst = -1;
			const time_t epoch = mktime(&fake_now);
			Assert::IsTrue(epoch > 0);
			const auto clock = system_clock::from_time_t(epoch);

			const LaraParser parser = get_filled_lara();
			const auto active = parser.get_active(fake_now);

			const auto maybe_sid = allocator.find("EBBR", "DENUT", "EDDF", 2, "19", clock, active, std::vector<std::string>{"25R"});
			Assert::IsTrue(maybe_sid.has_value());
			Assert::AreEqual(std::string("DENUT7N"), maybe_sid->sid);
		}

		TEST_METHOD(RespectsTimeZonesNegative)
		{
			// DENUT7N gets assigned for RWY19, if between 2345 and 0344L, or 25R is not active.
			// We will test with 25R active of course
			// This also tests that it respects the 25R being disallowed, otherwise we'd get that
			// Instead we select DENUT1F, which is F-ictional

			const auto allocator = get_filled_allocator();

			// This is local. October 1st is winter time, so at 03:50L it should not be active. This is 0150Z, so active if incorrectly converted.
			tm fake_now{};
			fake_now.tm_mday = 2;
			fake_now.tm_mon = 9; // October, somehow
			fake_now.tm_year = 2023 - 1900;
			fake_now.tm_hour = 3;
			fake_now.tm_min = 50;
			fake_now.tm_isdst = -1;
			const time_t epoch = mktime(&fake_now);
			Assert::IsTrue(epoch > 0);
			const auto clock = system_clock::from_time_t(epoch);

			const LaraParser parser = get_filled_lara();
			const auto active = parser.get_active(fake_now);

			const auto maybe_sid = allocator.find("EBBR", "DENUT", "EDDF", 2, "19", clock, active, std::vector<std::string>{"25R"});
			Assert::IsTrue(maybe_sid.has_value());
			Assert::AreNotEqual(std::string("DENUT7N"), maybe_sid->sid);
		}
	};
}
