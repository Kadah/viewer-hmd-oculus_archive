/** 
 * @file llviewerassetstats_tut.cpp
 * @date 2010-10-28
 * @brief Test cases for some of newview/llviewerassetstats.cpp
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 * 
 * Copyright (c) 2010, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include <tut/tut.hpp>
#include <iostream>

#include "lltut.h"
#include "../llviewerassetstats.h"
#include "lluuid.h"
#include "llsdutil.h"

static const char * all_keys[] = 
{
	"duration",
	"fps",
	"get_other",
	"get_texture_temp_http",
	"get_texture_temp_udp",
	"get_texture_non_temp_http",
	"get_texture_non_temp_udp",
	"get_wearable_udp",
	"get_sound_udp",
	"get_gesture_udp"
};

static const char * resp_keys[] = 
{
	"get_other",
	"get_texture_temp_http",
	"get_texture_temp_udp",
	"get_texture_non_temp_http",
	"get_texture_non_temp_udp",
	"get_wearable_udp",
	"get_sound_udp",
	"get_gesture_udp"
};

static const char * sub_keys[] =
{
	"dequeued",
	"enqueued",
	"resp_count",
	"resp_max",
	"resp_min",
	"resp_mean"
};

static const char * mmm_resp_keys[] = 
{
	"fps"
};

static const char * mmm_sub_keys[] =
{
	"count",
	"max",
	"min",
	"mean"
};

static const LLUUID region1("4e2d81a3-6263-6ffe-ad5c-8ce04bee07e8");
static const LLUUID region2("68762cc8-b68b-4e45-854b-e830734f2d4a");
static const U64 region1_handle(0x00000401000003f7ULL);
static const U64 region2_handle(0x000003f800000420ULL);
static const std::string region1_handle_str("00000401000003f7");
static const std::string region2_handle_str("000003f800000420");

#if 0
static bool
is_empty_map(const LLSD & sd)
{
	return sd.isMap() && 0 == sd.size();
}
#endif

static bool
is_single_key_map(const LLSD & sd, const std::string & key)
{
	return sd.isMap() && 1 == sd.size() && sd.has(key);
}

static bool
is_double_key_map(const LLSD & sd, const std::string & key1, const std::string & key2)
{
	return sd.isMap() && 2 == sd.size() && sd.has(key1) && sd.has(key2);
}

static bool
is_no_stats_map(const LLSD & sd)
{
	return is_double_key_map(sd, "duration", "regions");
}

namespace tut
{
	struct tst_viewerassetstats_index
	{};
	typedef test_group<tst_viewerassetstats_index> tst_viewerassetstats_index_t;
	typedef tst_viewerassetstats_index_t::object tst_viewerassetstats_index_object_t;
	tut::tst_viewerassetstats_index_t tut_tst_viewerassetstats_index("tst_viewerassetstats_test");

	// Testing free functions without global stats allocated
	template<> template<>
	void tst_viewerassetstats_index_object_t::test<1>()
	{
		// Check that helpers aren't bothered by missing global stats
		ensure("Global gViewerAssetStatsMain should be NULL", (NULL == gViewerAssetStatsMain));

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_TEXTURE, false, false);

		LLViewerAssetStatsFF::record_dequeue_main(LLViewerAssetType::AT_TEXTURE, false, false);

		LLViewerAssetStatsFF::record_response_main(LLViewerAssetType::AT_GESTURE, false, false, 12300000ULL);
	}

	// Create a non-global instance and check the structure
	template<> template<>
	void tst_viewerassetstats_index_object_t::test<2>()
	{
		ensure("Global gViewerAssetStatsMain should be NULL", (NULL == gViewerAssetStatsMain));

		LLViewerAssetStats * it = new LLViewerAssetStats();

		ensure("Global gViewerAssetStatsMain should still be NULL", (NULL == gViewerAssetStatsMain));

		LLSD sd_full = it->asLLSD();

		// Default (NULL) region ID doesn't produce LLSD results so should
		// get an empty map back from output
		ensure("Stat-less LLSD initially", is_no_stats_map(sd_full));

		// Once the region is set, we will get a response even with no data collection
		it->setRegion(region1_handle);
		sd_full = it->asLLSD();
		ensure("Correct single-key LLSD map root", is_double_key_map(sd_full, "duration", "regions"));
		ensure("Correct single-key LLSD map regions", is_single_key_map(sd_full["regions"], region1_handle_str));
		
		LLSD sd = sd_full["regions"][region1_handle_str];

		delete it;
			
		// Check the structure of the LLSD
		for (int i = 0; i < LL_ARRAY_SIZE(all_keys); ++i)
		{
			std::string line = llformat("Has '%s' key", all_keys[i]);
			ensure(line, sd.has(all_keys[i]));
		}

		for (int i = 0; i < LL_ARRAY_SIZE(resp_keys); ++i)
		{
			for (int j = 0; j < LL_ARRAY_SIZE(sub_keys); ++j)
			{
				std::string line = llformat("Key '%s' has '%s' key", resp_keys[i], sub_keys[j]);
				ensure(line, sd[resp_keys[i]].has(sub_keys[j]));
			}
		}

		for (int i = 0; i < LL_ARRAY_SIZE(mmm_resp_keys); ++i)
		{
			for (int j = 0; j < LL_ARRAY_SIZE(mmm_sub_keys); ++j)
			{
				std::string line = llformat("Key '%s' has '%s' key", mmm_resp_keys[i], mmm_sub_keys[j]);
				ensure(line, sd[mmm_resp_keys[i]].has(mmm_sub_keys[j]));
			}
		}
	}

	// Create a non-global instance and check some content
	template<> template<>
	void tst_viewerassetstats_index_object_t::test<3>()
	{
		LLViewerAssetStats * it = new LLViewerAssetStats();
		it->setRegion(region1_handle);
		
		LLSD sd = it->asLLSD();
		ensure("Correct single-key LLSD map root", is_double_key_map(sd, "regions", "duration"));
		ensure("Correct single-key LLSD map regions", is_single_key_map(sd["regions"], region1_handle_str));
		sd = sd[region1_handle_str];
		
		delete it;

		// Check a few points on the tree for content
		ensure("sd[get_texture_temp_http][dequeued] is 0", (0 == sd["get_texture_temp_http"]["dequeued"].asInteger()));
		ensure("sd[get_sound_udp][resp_min] is 0", (0.0 == sd["get_sound_udp"]["resp_min"].asReal()));
	}

	// Create a global instance and verify free functions do something useful
	template<> template<>
	void tst_viewerassetstats_index_object_t::test<4>()
	{
		gViewerAssetStatsMain = new LLViewerAssetStats();
		LLViewerAssetStatsFF::set_region_main(region1_handle);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_TEXTURE, false, false);
		LLViewerAssetStatsFF::record_dequeue_main(LLViewerAssetType::AT_TEXTURE, false, false);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_BODYPART, false, false);
		LLViewerAssetStatsFF::record_dequeue_main(LLViewerAssetType::AT_BODYPART, false, false);

		LLSD sd = gViewerAssetStatsMain->asLLSD();
		ensure("Correct single-key LLSD map root", is_double_key_map(sd, "regions", "duration"));
		ensure("Correct single-key LLSD map regions", is_single_key_map(sd["regions"], region1_handle_str));
		sd = sd["regions"][region1_handle_str];
		
		// Check a few points on the tree for content
		ensure("sd[get_texture_non_temp_udp][enqueued] is 1", (1 == sd["get_texture_non_temp_udp"]["enqueued"].asInteger()));
		ensure("sd[get_texture_temp_udp][enqueued] is 0", (0 == sd["get_texture_temp_udp"]["enqueued"].asInteger()));
		ensure("sd[get_texture_non_temp_http][enqueued] is 0", (0 == sd["get_texture_non_temp_http"]["enqueued"].asInteger()));
		ensure("sd[get_texture_temp_http][enqueued] is 0", (0 == sd["get_texture_temp_http"]["enqueued"].asInteger()));
		ensure("sd[get_gesture_udp][dequeued] is 0", (0 == sd["get_gesture_udp"]["dequeued"].asInteger()));

		// Reset and check zeros...
		// Reset leaves current region in place
		gViewerAssetStatsMain->reset();
		sd = gViewerAssetStatsMain->asLLSD()["regions"][region1_handle_str];
		
		delete gViewerAssetStatsMain;
		gViewerAssetStatsMain = NULL;

		ensure("sd[get_texture_non_temp_udp][enqueued] is reset", (0 == sd["get_texture_non_temp_udp"]["enqueued"].asInteger()));
		ensure("sd[get_gesture_udp][dequeued] is reset", (0 == sd["get_gesture_udp"]["dequeued"].asInteger()));
	}

	// Create two global instances and verify no interactions
	template<> template<>
	void tst_viewerassetstats_index_object_t::test<5>()
	{
		gViewerAssetStatsThread1 = new LLViewerAssetStats();
		gViewerAssetStatsMain = new LLViewerAssetStats();
		LLViewerAssetStatsFF::set_region_main(region1_handle);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_TEXTURE, false, false);
		LLViewerAssetStatsFF::record_dequeue_main(LLViewerAssetType::AT_TEXTURE, false, false);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_BODYPART, false, false);
		LLViewerAssetStatsFF::record_dequeue_main(LLViewerAssetType::AT_BODYPART, false, false);

		LLSD sd = gViewerAssetStatsThread1->asLLSD();
		ensure("Other collector is empty", is_no_stats_map(sd));
		sd = gViewerAssetStatsMain->asLLSD();
		ensure("Correct single-key LLSD map root", is_double_key_map(sd, "regions", "duration"));
		ensure("Correct single-key LLSD map regions", is_single_key_map(sd["regions"], region1_handle_str));
		sd = sd["regions"][region1_handle_str];
		
		// Check a few points on the tree for content
		ensure("sd[get_texture_non_temp_udp][enqueued] is 1", (1 == sd["get_texture_non_temp_udp"]["enqueued"].asInteger()));
		ensure("sd[get_texture_temp_udp][enqueued] is 0", (0 == sd["get_texture_temp_udp"]["enqueued"].asInteger()));
		ensure("sd[get_texture_non_temp_http][enqueued] is 0", (0 == sd["get_texture_non_temp_http"]["enqueued"].asInteger()));
		ensure("sd[get_texture_temp_http][enqueued] is 0", (0 == sd["get_texture_temp_http"]["enqueued"].asInteger()));
		ensure("sd[get_gesture_udp][dequeued] is 0", (0 == sd["get_gesture_udp"]["dequeued"].asInteger()));

		// Reset and check zeros...
		// Reset leaves current region in place
		gViewerAssetStatsMain->reset();
		sd = gViewerAssetStatsMain->asLLSD()["regions"][region1_handle_str];
		
		delete gViewerAssetStatsMain;
		gViewerAssetStatsMain = NULL;
		delete gViewerAssetStatsThread1;
		gViewerAssetStatsThread1 = NULL;

		ensure("sd[get_texture_non_temp_udp][enqueued] is reset", (0 == sd["get_texture_non_temp_udp"]["enqueued"].asInteger()));
		ensure("sd[get_gesture_udp][dequeued] is reset", (0 == sd["get_gesture_udp"]["dequeued"].asInteger()));
	}

    // Check multiple region collection
	template<> template<>
	void tst_viewerassetstats_index_object_t::test<6>()
	{
		gViewerAssetStatsMain = new LLViewerAssetStats();

		LLViewerAssetStatsFF::set_region_main(region1_handle);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_TEXTURE, false, false);
		LLViewerAssetStatsFF::record_dequeue_main(LLViewerAssetType::AT_TEXTURE, false, false);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_BODYPART, false, false);
		LLViewerAssetStatsFF::record_dequeue_main(LLViewerAssetType::AT_BODYPART, false, false);

		LLViewerAssetStatsFF::set_region_main(region2_handle);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_GESTURE, false, false);
		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_GESTURE, false, false);
		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_GESTURE, false, false);
		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_GESTURE, false, false);

		LLSD sd = gViewerAssetStatsMain->asLLSD();

		// std::cout << sd << std::endl;
		
		ensure("Correct double-key LLSD map root", is_double_key_map(sd, "duration", "regions"));
		ensure("Correct double-key LLSD map regions", is_double_key_map(sd["regions"], region1_handle_str, region2_handle_str));
		LLSD sd1 = sd["regions"][region1_handle_str];
		LLSD sd2 = sd["regions"][region2_handle_str];
		
		// Check a few points on the tree for content
		ensure("sd1[get_texture_non_temp_udp][enqueued] is 1", (1 == sd1["get_texture_non_temp_udp"]["enqueued"].asInteger()));
		ensure("sd1[get_texture_temp_udp][enqueued] is 0", (0 == sd1["get_texture_temp_udp"]["enqueued"].asInteger()));
		ensure("sd1[get_texture_non_temp_http][enqueued] is 0", (0 == sd1["get_texture_non_temp_http"]["enqueued"].asInteger()));
		ensure("sd1[get_texture_temp_http][enqueued] is 0", (0 == sd1["get_texture_temp_http"]["enqueued"].asInteger()));
		ensure("sd1[get_gesture_udp][dequeued] is 0", (0 == sd1["get_gesture_udp"]["dequeued"].asInteger()));

		// Check a few points on the tree for content
		ensure("sd2[get_gesture_udp][enqueued] is 4", (4 == sd2["get_gesture_udp"]["enqueued"].asInteger()));
		ensure("sd2[get_gesture_udp][dequeued] is 0", (0 == sd2["get_gesture_udp"]["dequeued"].asInteger()));
		ensure("sd2[get_texture_non_temp_udp][enqueued] is 0", (0 == sd2["get_texture_non_temp_udp"]["enqueued"].asInteger()));

		// Reset and check zeros...
		// Reset leaves current region in place
		gViewerAssetStatsMain->reset();
		sd = gViewerAssetStatsMain->asLLSD();
		ensure("Correct single-key LLSD map root", is_double_key_map(sd, "regions", "duration"));
		ensure("Correct single-key LLSD map regions", is_single_key_map(sd["regions"], region2_handle_str));
		sd2 = sd["regions"][region2_handle_str];
		
		delete gViewerAssetStatsMain;
		gViewerAssetStatsMain = NULL;

		ensure("sd2[get_texture_non_temp_udp][enqueued] is reset", (0 == sd2["get_texture_non_temp_udp"]["enqueued"].asInteger()));
		ensure("sd2[get_gesture_udp][enqueued] is reset", (0 == sd2["get_gesture_udp"]["enqueued"].asInteger()));
	}

    // Check multiple region collection jumping back-and-forth between regions
	template<> template<>
	void tst_viewerassetstats_index_object_t::test<7>()
	{
		gViewerAssetStatsMain = new LLViewerAssetStats();

		LLViewerAssetStatsFF::set_region_main(region1_handle);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_TEXTURE, false, false);
		LLViewerAssetStatsFF::record_dequeue_main(LLViewerAssetType::AT_TEXTURE, false, false);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_BODYPART, false, false);
		LLViewerAssetStatsFF::record_dequeue_main(LLViewerAssetType::AT_BODYPART, false, false);

		LLViewerAssetStatsFF::set_region_main(region2_handle);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_GESTURE, false, false);
		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_GESTURE, false, false);
		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_GESTURE, false, false);
		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_GESTURE, false, false);

		LLViewerAssetStatsFF::set_region_main(region1_handle);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_TEXTURE, true, true);
		LLViewerAssetStatsFF::record_dequeue_main(LLViewerAssetType::AT_TEXTURE, true, true);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_BODYPART, false, false);
		LLViewerAssetStatsFF::record_dequeue_main(LLViewerAssetType::AT_BODYPART, false, false);

		LLViewerAssetStatsFF::set_region_main(region2_handle);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_GESTURE, false, false);
		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_GESTURE, false, false);
		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_GESTURE, false, false);
		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_GESTURE, false, false);

		LLSD sd = gViewerAssetStatsMain->asLLSD();

		ensure("Correct double-key LLSD map root", is_double_key_map(sd, "duration", "regions"));
		ensure("Correct double-key LLSD map regions", is_double_key_map(sd["regions"], region1_handle_str, region2_handle_str));
		LLSD sd1 = sd["regions"][region1_handle_str];
		LLSD sd2 = sd["regions"][region2_handle_str];
		
		// Check a few points on the tree for content
		ensure("sd1[get_texture_non_temp_udp][enqueued] is 1", (1 == sd1["get_texture_non_temp_udp"]["enqueued"].asInteger()));
		ensure("sd1[get_texture_temp_udp][enqueued] is 0", (0 == sd1["get_texture_temp_udp"]["enqueued"].asInteger()));
		ensure("sd1[get_texture_non_temp_http][enqueued] is 0", (0 == sd1["get_texture_non_temp_http"]["enqueued"].asInteger()));
		ensure("sd1[get_texture_temp_http][enqueued] is 1", (1 == sd1["get_texture_temp_http"]["enqueued"].asInteger()));
		ensure("sd1[get_gesture_udp][dequeued] is 0", (0 == sd1["get_gesture_udp"]["dequeued"].asInteger()));

		// Check a few points on the tree for content
		ensure("sd2[get_gesture_udp][enqueued] is 8", (8 == sd2["get_gesture_udp"]["enqueued"].asInteger()));
		ensure("sd2[get_gesture_udp][dequeued] is 0", (0 == sd2["get_gesture_udp"]["dequeued"].asInteger()));
		ensure("sd2[get_texture_non_temp_udp][enqueued] is 0", (0 == sd2["get_texture_non_temp_udp"]["enqueued"].asInteger()));

		// Reset and check zeros...
		// Reset leaves current region in place
		gViewerAssetStatsMain->reset();
		sd = gViewerAssetStatsMain->asLLSD();
		ensure("Correct single-key LLSD map root", is_double_key_map(sd, "duration", "regions"));
		ensure("Correct single-key LLSD map regions", is_single_key_map(sd["regions"], region2_handle_str));
		sd2 = sd["regions"][region2_handle_str];
		
		delete gViewerAssetStatsMain;
		gViewerAssetStatsMain = NULL;

		ensure("sd2[get_texture_non_temp_udp][enqueued] is reset", (0 == sd2["get_texture_non_temp_udp"]["enqueued"].asInteger()));
		ensure("sd2[get_gesture_udp][enqueued] is reset", (0 == sd2["get_gesture_udp"]["enqueued"].asInteger()));
	}

	// Non-texture assets ignore transport and persistence flags
	template<> template<>
	void tst_viewerassetstats_index_object_t::test<8>()
	{
		gViewerAssetStatsThread1 = new LLViewerAssetStats();
		gViewerAssetStatsMain = new LLViewerAssetStats();
		LLViewerAssetStatsFF::set_region_main(region1_handle);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_TEXTURE, false, false);
		LLViewerAssetStatsFF::record_dequeue_main(LLViewerAssetType::AT_TEXTURE, false, false);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_BODYPART, false, false);
		LLViewerAssetStatsFF::record_dequeue_main(LLViewerAssetType::AT_BODYPART, false, false);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_BODYPART, false, true);
		LLViewerAssetStatsFF::record_dequeue_main(LLViewerAssetType::AT_BODYPART, false, true);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_BODYPART, true, false);
		LLViewerAssetStatsFF::record_dequeue_main(LLViewerAssetType::AT_BODYPART, true, false);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_BODYPART, true, true);
		LLViewerAssetStatsFF::record_dequeue_main(LLViewerAssetType::AT_BODYPART, true, true);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_LSL_BYTECODE, false, false);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_LSL_BYTECODE, false, true);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_LSL_BYTECODE, true, false);

		LLViewerAssetStatsFF::record_enqueue_main(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		LLSD sd = gViewerAssetStatsThread1->asLLSD();
		ensure("Other collector is empty", is_no_stats_map(sd));
		sd = gViewerAssetStatsMain->asLLSD();
		ensure("Correct single-key LLSD map root", is_double_key_map(sd, "regions", "duration"));
		ensure("Correct single-key LLSD map regions", is_single_key_map(sd["regions"], region1_handle_str));
		sd = sd["regions"][region1_handle_str];
		
		// Check a few points on the tree for content
		ensure("sd[get_gesture_udp][enqueued] is 0", (0 == sd["get_gesture_udp"]["enqueued"].asInteger()));
		ensure("sd[get_gesture_udp][dequeued] is 0", (0 == sd["get_gesture_udp"]["dequeued"].asInteger()));

		ensure("sd[get_wearable_udp][enqueued] is 4", (4 == sd["get_wearable_udp"]["enqueued"].asInteger()));
		ensure("sd[get_wearable_udp][dequeued] is 4", (4 == sd["get_wearable_udp"]["dequeued"].asInteger()));

		ensure("sd[get_other][enqueued] is 4", (4 == sd["get_other"]["enqueued"].asInteger()));
		ensure("sd[get_other][dequeued] is 0", (0 == sd["get_other"]["dequeued"].asInteger()));

		// Reset and check zeros...
		// Reset leaves current region in place
		gViewerAssetStatsMain->reset();
		sd = gViewerAssetStatsMain->asLLSD()["regions"][region1_handle_str];
		
		delete gViewerAssetStatsMain;
		gViewerAssetStatsMain = NULL;
		delete gViewerAssetStatsThread1;
		gViewerAssetStatsThread1 = NULL;

		ensure("sd[get_texture_non_temp_udp][enqueued] is reset", (0 == sd["get_texture_non_temp_udp"]["enqueued"].asInteger()));
		ensure("sd[get_gesture_udp][dequeued] is reset", (0 == sd["get_gesture_udp"]["dequeued"].asInteger()));
	}


	// LLViewerAssetStats::merge() basic functions work
	template<> template<>
	void tst_viewerassetstats_index_object_t::test<9>()
	{
		LLViewerAssetStats s1;
		LLViewerAssetStats s2;

		s1.setRegion(region1_handle);
		s2.setRegion(region1_handle);

		s1.recordGetServiced(LLViewerAssetType::AT_TEXTURE, true, true, 5000000);
		s1.recordGetServiced(LLViewerAssetType::AT_TEXTURE, true, true, 6000000);
		s1.recordGetServiced(LLViewerAssetType::AT_TEXTURE, true, true, 8000000);
		s1.recordGetServiced(LLViewerAssetType::AT_TEXTURE, true, true, 7000000);
		s1.recordGetServiced(LLViewerAssetType::AT_TEXTURE, true, true, 9000000);
		
		s2.recordGetServiced(LLViewerAssetType::AT_TEXTURE, true, true, 2000000);
		s2.recordGetServiced(LLViewerAssetType::AT_TEXTURE, true, true, 3000000);
		s2.recordGetServiced(LLViewerAssetType::AT_TEXTURE, true, true, 4000000);

		s2.merge(s1);

		LLSD s2_llsd = s2.asLLSD();
		
		ensure_equals("count after merge", 8, s2_llsd["regions"][region1_handle_str]["get_texture_temp_http"]["resp_count"].asInteger());
		ensure_approximately_equals("min after merge", 2.0, s2_llsd["regions"][region1_handle_str]["get_texture_temp_http"]["resp_min"].asReal(), 22);
		ensure_approximately_equals("max after merge", 9.0, s2_llsd["regions"][region1_handle_str]["get_texture_temp_http"]["resp_max"].asReal(), 22);
		ensure_approximately_equals("max after merge", 5.5, s2_llsd["regions"][region1_handle_str]["get_texture_temp_http"]["resp_mean"].asReal(), 22);

	}

	// LLViewerAssetStats::merge() basic functions work without corrupting source data
	template<> template<>
	void tst_viewerassetstats_index_object_t::test<10>()
	{
		LLViewerAssetStats s1;
		LLViewerAssetStats s2;

		s1.setRegion(region1_handle);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s1.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 23289200);
		s1.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 282900);

		
		s2.setRegion(region2_handle);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s2.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 6500000);
		s2.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 10000);

		{
			s2.merge(s1);
			
			LLSD src = s1.asLLSD();
			LLSD dst = s2.asLLSD();

			// Remove time stamps, they're a problem
			src.erase("duration");
			src["regions"][region1_handle_str].erase("duration");
			dst.erase("duration");
			dst["regions"][region1_handle_str].erase("duration");
			dst["regions"][region2_handle_str].erase("duration");

			ensure_equals("merge src has single region", 1, src["regions"].size());
			ensure_equals("merge dst has dual regions", 2, dst["regions"].size());
			ensure("result from src is in dst", llsd_equals(src["regions"][region1_handle_str],
															dst["regions"][region1_handle_str]));
		}

		s1.setRegion(region1_handle);
		s2.setRegion(region1_handle);
		s1.reset();
		s2.reset();
		
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s1.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 23289200);
		s1.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 282900);

		
		s2.setRegion(region1_handle);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s2.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 6500000);
		s2.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 10000);

		{
			s2.merge(s1);
			
			LLSD src = s1.asLLSD();
			LLSD dst = s2.asLLSD();

			// Remove time stamps, they're a problem
			src.erase("duration");
			src["regions"][region1_handle_str].erase("duration");
			dst.erase("duration");
			dst["regions"][region1_handle_str].erase("duration");

			ensure_equals("src counts okay (enq)", 4, src["regions"][region1_handle_str]["get_other"]["enqueued"].asInteger());
			ensure_equals("src counts okay (deq)", 4, src["regions"][region1_handle_str]["get_other"]["dequeued"].asInteger());
			ensure_equals("src resp counts okay", 2, src["regions"][region1_handle_str]["get_other"]["resp_count"].asInteger());
			ensure_approximately_equals("src respmin okay", 0.2829, src["regions"][region1_handle_str]["get_other"]["resp_min"].asReal(), 20);
			ensure_approximately_equals("src respmax okay", 23.2892, src["regions"][region1_handle_str]["get_other"]["resp_max"].asReal(), 20);
			
			ensure_equals("dst counts okay (enq)", 12, dst["regions"][region1_handle_str]["get_other"]["enqueued"].asInteger());
			ensure_equals("src counts okay (deq)", 11, dst["regions"][region1_handle_str]["get_other"]["dequeued"].asInteger());
			ensure_equals("dst resp counts okay", 4, dst["regions"][region1_handle_str]["get_other"]["resp_count"].asInteger());
			ensure_approximately_equals("dst respmin okay", 0.010, dst["regions"][region1_handle_str]["get_other"]["resp_min"].asReal(), 20);
			ensure_approximately_equals("dst respmax okay", 23.2892, dst["regions"][region1_handle_str]["get_other"]["resp_max"].asReal(), 20);
		}
	}


    // Maximum merges are interesting when one side contributes nothing
	template<> template<>
	void tst_viewerassetstats_index_object_t::test<11>()
	{
		LLViewerAssetStats s1;
		LLViewerAssetStats s2;

		s1.setRegion(region1_handle);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		// Want to test negative numbers here but have to work in U64
		s1.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 0);
		s1.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 0);
		s1.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 0);

		s2.setRegion(region1_handle);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		{
			s2.merge(s1);
			
			LLSD src = s1.asLLSD();
			LLSD dst = s2.asLLSD();

			// Remove time stamps, they're a problem
			src.erase("duration");
			src["regions"][region1_handle_str].erase("duration");
			dst.erase("duration");
			dst["regions"][region1_handle_str].erase("duration");

			ensure_equals("dst counts come from src only", 3, dst["regions"][region1_handle_str]["get_other"]["resp_count"].asInteger());

			ensure_approximately_equals("dst maximum with count 0 does not contribute to merged maximum",
										dst["regions"][region1_handle_str]["get_other"]["resp_max"].asReal(), F64(0.0), 20);
		}

		// Other way around
		s1.setRegion(region1_handle);
		s2.setRegion(region1_handle);
		s1.reset();
		s2.reset();

		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		// Want to test negative numbers here but have to work in U64
		s1.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 0);
		s1.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 0);
		s1.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 0);

		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		{
			s1.merge(s2);
			
			LLSD src = s2.asLLSD();
			LLSD dst = s1.asLLSD();

			// Remove time stamps, they're a problem
			src.erase("duration");
			src["regions"][region1_handle_str].erase("duration");
			dst.erase("duration");
			dst["regions"][region1_handle_str].erase("duration");

			ensure_equals("dst counts come from src only (flipped)", 3, dst["regions"][region1_handle_str]["get_other"]["resp_count"].asInteger());

			ensure_approximately_equals("dst maximum with count 0 does not contribute to merged maximum (flipped)",
										dst["regions"][region1_handle_str]["get_other"]["resp_max"].asReal(), F64(0.0), 20);
		}
	}

    // Minimum merges are interesting when one side contributes nothing
	template<> template<>
	void tst_viewerassetstats_index_object_t::test<12>()
	{
		LLViewerAssetStats s1;
		LLViewerAssetStats s2;

		s1.setRegion(region1_handle);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s1.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 3800000);
		s1.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 2700000);
		s1.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 2900000);

		s2.setRegion(region1_handle);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		{
			s2.merge(s1);
			
			LLSD src = s1.asLLSD();
			LLSD dst = s2.asLLSD();

			// Remove time stamps, they're a problem
			src.erase("duration");
			src["regions"][region1_handle_str].erase("duration");
			dst.erase("duration");
			dst["regions"][region1_handle_str].erase("duration");

			ensure_equals("dst counts come from src only", 3, dst["regions"][region1_handle_str]["get_other"]["resp_count"].asInteger());

			ensure_approximately_equals("dst minimum with count 0 does not contribute to merged minimum",
										dst["regions"][region1_handle_str]["get_other"]["resp_min"].asReal(), F64(2.7), 20);
		}

		// Other way around
		s1.setRegion(region1_handle);
		s2.setRegion(region1_handle);
		s1.reset();
		s2.reset();

		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s1.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s1.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 3800000);
		s1.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 2700000);
		s1.recordGetServiced(LLViewerAssetType::AT_LSL_BYTECODE, true, true, 2900000);

		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetEnqueued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);
		s2.recordGetDequeued(LLViewerAssetType::AT_LSL_BYTECODE, true, true);

		{
			s1.merge(s2);
			
			LLSD src = s2.asLLSD();
			LLSD dst = s1.asLLSD();

			// Remove time stamps, they're a problem
			src.erase("duration");
			src["regions"][region1_handle_str].erase("duration");
			dst.erase("duration");
			dst["regions"][region1_handle_str].erase("duration");

			ensure_equals("dst counts come from src only (flipped)", 3, dst["regions"][region1_handle_str]["get_other"]["resp_count"].asInteger());

			ensure_approximately_equals("dst minimum with count 0 does not contribute to merged minimum (flipped)",
										dst["regions"][region1_handle_str]["get_other"]["resp_min"].asReal(), F64(2.7), 20);
		}
	}

}
