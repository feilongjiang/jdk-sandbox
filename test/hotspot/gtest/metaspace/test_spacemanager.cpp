/*
 * Copyright (c) 2019, SAP SE. All rights reserved.
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */



#include "precompiled.hpp"

//#define LOG_PLEASE

#include "metaspace/metaspaceTestsCommon.hpp"
#include "metaspace/metaspace_sparsearray.hpp"
#include "metaspace/metaspace_testhelper.hpp"
#include "utilities/ostream.hpp"


// TODO: this class is very similar to SpaceManagerTestBed in test_spacemanager_stress.cpp.
// should be unified.
class SpaceManagerTestHelper {

  MetaspaceTestHelper& _helper;

  Mutex* _lock;
  const ArenaGrowthPolicy* _growth_policy;
  SizeAtomicCounter _used_words_counter;
  SpaceManager* _sm;

public:

  SpaceManagerTestHelper(MetaspaceTestHelper& helper, metaspace::MetaspaceType space_type, bool is_class,
                         const char* name = "gtest-SpaceManager")
    : _helper(helper),
      _lock(NULL),
      _growth_policy(NULL),
      _used_words_counter(),
      _sm(NULL)
  {
    _growth_policy = ArenaGrowthPolicy::policy_for_space_type(space_type, is_class);
    _lock = new Mutex(Monitor::native, "gtest-SpaceManagerTest-lock", false, Monitor::_safepoint_check_never);
    // Lock during space creation, since this is what happens in the VM too
    //  (see ClassLoaderData::metaspace_non_null(), which we mimick here).
    {
      MutexLocker ml(_lock,  Mutex::_no_safepoint_check_flag);
      _sm = new SpaceManager(&_helper.cm(), _growth_policy, _lock, &_used_words_counter, name, false);
    }
    DEBUG_ONLY(_sm->verify(true));
  }

  ~SpaceManagerTestHelper() {
    delete_sm_with_tests();
    delete _lock;
  }

  const CommitLimiter& limiter() const { return _helper.commit_limiter(); }
  SpaceManager* sm() const { return _sm; }
  SizeAtomicCounter& used_words_counter() { return _used_words_counter; }

  void delete_sm_with_tests() {
    if (_sm != NULL) {
      size_t used_words_before = _used_words_counter.get();
      size_t committed_words_before = limiter().committed_words();
      DEBUG_ONLY(_sm->verify(true));
      delete _sm;
      _sm = NULL;
      size_t used_words_after = _used_words_counter.get();
      size_t committed_words_after = limiter().committed_words();
      EXPECT_0(used_words_after);
      if (Settings::uncommit_on_return()) {
        EXPECT_LE(committed_words_after, committed_words_before);
      } else {
        EXPECT_EQ(committed_words_after, committed_words_before);
      }
    }
  }

  void usage_numbers_with_test(size_t* p_used, size_t* p_committed, size_t* p_capacity) const {
    _sm->usage_numbers(p_used, p_committed, p_capacity);
    if (p_used != NULL) {
      if (p_committed != NULL) {
        ASSERT_GE(*p_committed, *p_used);
      }
      // Since we own the used words counter, it should reflect our usage number 1:1
      ASSERT_EQ(_used_words_counter.get(), *p_used);
    }
    if (p_committed != NULL && p_capacity != NULL) {
      ASSERT_GE(*p_capacity, *p_committed);
    }
  }

  void allocate_from_sm_with_tests_expect_success(size_t word_size) {
    bool b = allocate_from_sm_with_tests(word_size);
    ASSERT_TRUE(b);
  }

  void allocate_from_sm_with_tests_expect_failure(size_t word_size) {
    bool b = allocate_from_sm_with_tests(word_size);
    ASSERT_FALSE(b);
  }

  bool allocate_from_sm_with_tests(size_t word_size) {

    // Note: usage_numbers walks all chunks in use and counts.
    size_t used = 0, committed = 0, capacity = 0;
    usage_numbers_with_test(&used, &committed, &capacity);

    size_t possible_expansion = limiter().possible_expansion_words();

    MetaWord* p = _sm->allocate(word_size);

    size_t used2 = 0, committed2 = 0, capacity2 = 0;
    usage_numbers_with_test(&used2, &committed2, &capacity2);

    if (p == NULL) {
      // Allocation failed. We expect a too small expansion size the cause. Nothing should have changed.
      EXPECT_LT(possible_expansion, word_size);
      EXPECT_EQ(used, used2);
      EXPECT_EQ(committed, committed2);
      EXPECT_EQ(capacity, capacity2);
      return false;
    } else {
      // Allocation succeeded. Should be correctly aligned.
      EXPECT_TRUE(is_aligned(p, sizeof(MetaWord)));
      // used: may go up or may not (since our request may have been satisfied from the freeblocklist
      //   whose content already counts as used).
      // committed: may go up, may not
      // capacity: ditto
      EXPECT_GE(used2, used);
      EXPECT_GE(committed2, committed);
      EXPECT_GE(capacity2, capacity);
      return true;
    }
    return false;
  }

};


static void test_basics(size_t commit_limit, bool is_micro) {
  MetaspaceTestHelper msthelper(commit_limit);
  SpaceManagerTestHelper helper(msthelper, is_micro ? metaspace::ReflectionMetaspaceType : metaspace::StandardMetaspaceType, false);

  helper.allocate_from_sm_with_tests(1);
  helper.allocate_from_sm_with_tests(128);
  helper.allocate_from_sm_with_tests(128 * K);
  helper.allocate_from_sm_with_tests(1);
  helper.allocate_from_sm_with_tests(128);
  helper.allocate_from_sm_with_tests(128 * K);
}

TEST_VM(metaspace, spacemanager_basics_micro_nolimit) {
  test_basics(max_uintx, true);
}

TEST_VM(metaspace, spacemanager_basics_micro_limit) {
  test_basics(256 * K, true);
}

TEST_VM(metaspace, spacemanager_basics_standard_nolimit) {
  test_basics(max_uintx, false);
}

TEST_VM(metaspace, spacemanager_basics_standard_limit) {
  test_basics(256 * K, false);
}


TEST_VM(metaspace, spacemanager_test_enlarge_in_place) {
  // Test: in a single undisturbed SpaceManager (so, we should have chunks enlarged in place)
  // we allocate a small amount, then the full amount possible. The sum of first and second
  // allocation bring us above root chunk size. This should work - chunk enlargement should
  // fail and a new root chunk should be allocated instead.
  MetaspaceTestHelper msthelper;
  SpaceManagerTestHelper helper(msthelper, metaspace::StandardMetaspaceType, false);
  helper.allocate_from_sm_with_tests_expect_success(1);
  helper.allocate_from_sm_with_tests_expect_success(MAX_CHUNK_WORD_SIZE);
  helper.allocate_from_sm_with_tests_expect_success(MAX_CHUNK_WORD_SIZE / 2);
  helper.allocate_from_sm_with_tests_expect_success(MAX_CHUNK_WORD_SIZE);
}

TEST_VM(metaspace, spacemanager_test_enlarge_in_place_ladder_1) {
  MetaspaceTestHelper msthelper;
  SpaceManagerTestHelper helper(msthelper, metaspace::StandardMetaspaceType, false);
  // Test allocating from smallest to largest chunk size, and one step beyond.
  // The first n allocations should happen in place, the ladder should open a new chunk.
  size_t size = MIN_CHUNK_WORD_SIZE;
  while (size <= MAX_CHUNK_WORD_SIZE) {
    helper.allocate_from_sm_with_tests_expect_success(size);
    size *= 2;
  }
  helper.allocate_from_sm_with_tests_expect_success(MAX_CHUNK_WORD_SIZE);
}

TEST_VM(metaspace, spacemanager_test_enlarge_in_place_ladder_2) {
  MetaspaceTestHelper msthelper;
  SpaceManagerTestHelper helper(msthelper, metaspace::StandardMetaspaceType, false);
  // Same as spacemanager_test_enlarge_in_place_ladder_1, but increase in *4 step size;
  // this way chunk-in-place-enlargement does not work and we should have new chunks at each allocation.
  size_t size = MIN_CHUNK_WORD_SIZE;
  while (size <= MAX_CHUNK_WORD_SIZE) {
    helper.allocate_from_sm_with_tests_expect_success(size);
    size *= 4;
  }
  helper.allocate_from_sm_with_tests_expect_success(MAX_CHUNK_WORD_SIZE);
}

static void test_recover_from_commit_limit_hit() {
  // Test:
  // - Multiple SpaceManager allocate (operating under the same commit limiter).
  // - One, while attempting to commit parts of its current chunk on demand,
  //   triggers the limit and cannot commit its chunk further.
  // - We release the other SpaceManager - its content is put back to the
  //   freelists.
  // - We re-attempt allocation from the first manager. It should now succeed.
  //
  // This means if the first SpaceManager may have to let go of its current chunk and
  // retire it and take a fresh chunk from the freelist.

  const size_t commit_limit = Settings::commit_granule_words() * 10;
  MetaspaceTestHelper msthelper(commit_limit);

  // The first space managers mimick micro loaders. This will fill the free
  //  chunk list with very small chunks. We allocate from them in an interleaved
  //  way to cause fragmentation.
  SpaceManagerTestHelper helper1(msthelper, metaspace::ReflectionMetaspaceType, false);
  SpaceManagerTestHelper helper2(msthelper, metaspace::ReflectionMetaspaceType, false);

  // This SpaceManager should hit the limit. We use BootMetaspaceType here since
  // it gets a large initial chunk which is committed
  // on demand and we are likely to hit a commit limit while trying to expand it.
  SpaceManagerTestHelper helper3(msthelper, metaspace::BootMetaspaceType, false);

  // Allocate space until we have below two but above one granule left
  while (msthelper.commit_limiter().possible_expansion_words() >= Settings::commit_granule_words() * 2) {
    helper1.allocate_from_sm_with_tests(1);
    helper2.allocate_from_sm_with_tests(1);
  }

  // Now, allocating from helper3, creep up on the limit
  size_t allocated_from_3 = 0;
  while (helper3.allocate_from_sm_with_tests(1) &&
         ++allocated_from_3 < Settings::commit_granule_words() * 2);

  EXPECT_LE(allocated_from_3, Settings::commit_granule_words() * 2);

  // We expect the freelist to be empty of committed space...
  EXPECT_0(msthelper.cm().total_committed_word_size());

  //msthelper.cm().print_on(tty);

  // Release the first SpaceManager.
  helper1.delete_sm_with_tests();

  //msthelper.cm().print_on(tty);

  // Should have populated the freelist with committed space
  // We expect the freelist to be empty of committed space...
  EXPECT_GT(msthelper.cm().total_committed_word_size(), (size_t)0);

  // Repeat allocation from helper3, should now work.
  EXPECT_TRUE(helper3.allocate_from_sm_with_tests(1));

}


TEST_VM(metaspace, spacemanager_recover_from_limit_hit) {
  test_recover_from_commit_limit_hit();
}

static void test_controlled_growth(metaspace::MetaspaceType type, bool is_class,
                                   size_t expected_starting_capacity,
                                   bool test_in_place_enlargement)
{
  // From a spacemanager in a clean room allocate tiny amounts;
  // watch it grow. Used/committed/capacity should not grow in
  // large jumps. Also, different types of SpaceManager should
  // have different initial capacities.

  MetaspaceTestHelper msthelper;
  SpaceManagerTestHelper smhelper(msthelper, type, is_class, "Grower");

  SpaceManagerTestHelper smhelper_harrasser(msthelper, metaspace::ReflectionMetaspaceType, true, "Harasser");

  size_t used = 0, committed = 0, capacity = 0;
  const size_t alloc_words = 16;

  smhelper.sm()->usage_numbers(&used, &committed, &capacity);
  ASSERT_0(used);
  ASSERT_0(committed);
  ASSERT_0(capacity);

  ///// First allocation //

  ASSERT_NOT_NULL(smhelper.allocate_from_sm_with_tests(alloc_words));

  smhelper.sm()->usage_numbers(&used, &committed, &capacity);

  ASSERT_EQ(used, alloc_words);
  ASSERT_GE(committed, used);
  ASSERT_GE(capacity, committed);

  ASSERT_EQ(capacity, expected_starting_capacity);

  // Initial commit charge should not surpass committed_words_on_fresh_chunks
  ASSERT_LE(committed, Settings::committed_words_on_fresh_chunks());


  ///// subsequent allocations //

  DEBUG_ONLY(const uintx num_chunk_enlarged = metaspace::InternalStats::num_chunks_enlarged();)

  size_t allocated = 0;
  const size_t safety = 6 * M;
  size_t highest_capacity_jump = capacity;
  int num_capacity_jumps = 0;

  while (allocated < safety && num_capacity_jumps < 10) {

    // if we want to test growth with in-place chunk enlargement, leave SpaceManager
    // undisturbed; it will have all the place to grow. Otherwise, allocate from a little
    // side arena to increase fragmentation.
    // (Note that this does not completely prevent in-place chunk enlargement but makes it
    //  rather improbable)
    if (!test_in_place_enlargement) {
      smhelper_harrasser.allocate_from_sm_with_tests(alloc_words * 2);
    }

    ASSERT_NOT_NULL(smhelper.allocate_from_sm_with_tests(alloc_words));
    allocated += alloc_words;

    size_t used2 = 0, committed2 = 0, capacity2 = 0;

    smhelper.sm()->usage_numbers(&used2, &committed2, &capacity2);

    // used should not grow larger than what we allocated, plus possible overhead.
    ASSERT_GE(used2, used);
    ASSERT_LE(used2, used + alloc_words * 2);
    ASSERT_LE(used2, allocated + 100);
    used = used2;

    // A jump in committed words should not be larger than commit granule size.
    // It can be smaller, since the current chunk of the space manager may be
    // smaller than a commit granule.
    ASSERT_GE(committed2, used2);
    ASSERT_GE(committed2, committed);
    const size_t committed_jump = committed2 - committed;
    if (committed_jump > 0) {
      ASSERT_LE(committed_jump, Settings::commit_granule_words());
    }
    committed = committed2;

    // Capacity jumps:
    // (we grow either by enlarging the chunk in place, in which case it can only double;
    //  or by allocating a new chunk. The latter is subject to the chunk growth rate set
    //  with arena growth policy (see memory/metaspace/arenaGrowthPolicy.cpp). There should
    //  not be sudden jumps in chunk sizes.
    // Note that this is fuzzy the moment we share the underlying chunk manager with
    //  other arenas, since the chunk manager will always attempt to hand out committed chunks
    //  first; this may cause us to get small chunks where arena policy would expect larger
    //  chunks.
    ASSERT_GE(capacity2, committed2);
    ASSERT_GE(capacity2, capacity);
    const size_t capacity_jump = capacity2 - capacity;
    if (capacity_jump > 0) {
      LOG(">" SIZE_FORMAT "->" SIZE_FORMAT "(+" SIZE_FORMAT ")", capacity, capacity2, capacity_jump)
      if (capacity_jump > highest_capacity_jump) {
        // Note: if this fails, check arena policies for sudden chunk size jumps.
        ASSERT_LE(capacity_jump, highest_capacity_jump * 2);
        ASSERT_GE(capacity_jump, MIN_CHUNK_WORD_SIZE);
        ASSERT_LE(capacity_jump, MAX_CHUNK_WORD_SIZE);
        highest_capacity_jump = capacity_jump;
      }
      num_capacity_jumps ++;
    }
    capacity = capacity2;

  }

  // After all this work, we should see an increase in number of chunk-in-place-enlargements
  //  ( we test this since this especially is vulnerable to regression: the decisions of when
  //    to do in place enlargements are complicated, see SpaceManager::attempt_enlarge_current_chunk() )
#ifdef ASSERT
  // Note, internal statistics only exists in debug builds
  if (test_in_place_enlargement) {
    const uintx num_chunk_enlarged_2 = metaspace::InternalStats::num_chunks_enlarged();
    ASSERT_GT(num_chunk_enlarged_2, num_chunk_enlarged);
  }
#endif
}

// these numbers have to be in sync with arena policy numbers (see memory/metaspace/arenaGrowthPolicy.cpp)
TEST_VM(metaspace, spacemanager_growth_refl_c_inplace) {
  test_controlled_growth(metaspace::ReflectionMetaspaceType, true,
                         word_size_for_level(CHUNK_LEVEL_1K), true);
}

TEST_VM(metaspace, spacemanager_growth_refl_c_not_inplace) {
  test_controlled_growth(metaspace::ReflectionMetaspaceType, true,
                         word_size_for_level(CHUNK_LEVEL_1K), false);
}

TEST_VM(metaspace, spacemanager_growth_anon_c_inplace) {
  test_controlled_growth(metaspace::ClassMirrorHolderMetaspaceType, true,
                         word_size_for_level(CHUNK_LEVEL_1K), true);
}

TEST_VM(metaspace, spacemanager_growth_anon_c_not_inplace) {
  test_controlled_growth(metaspace::ClassMirrorHolderMetaspaceType, true,
                         word_size_for_level(CHUNK_LEVEL_1K), false);
}

TEST_VM(metaspace, spacemanager_growth_standard_c_inplace) {
  test_controlled_growth(metaspace::StandardMetaspaceType, true,
                         word_size_for_level(CHUNK_LEVEL_2K), true);
}

TEST_VM(metaspace, spacemanager_growth_standard_c_not_inplace) {
  test_controlled_growth(metaspace::StandardMetaspaceType, true,
                         word_size_for_level(CHUNK_LEVEL_2K), false);
}

TEST_VM(metaspace, spacemanager_growth_boot_c_inplace) {
  test_controlled_growth(metaspace::BootMetaspaceType, true,
                         word_size_for_level(CHUNK_LEVEL_1M), true);
}

TEST_VM(metaspace, spacemanager_growth_boot_c_not_inplace) {
  test_controlled_growth(metaspace::BootMetaspaceType, true,
                         word_size_for_level(CHUNK_LEVEL_1M), false);
}

TEST_VM(metaspace, spacemanager_growth_refl_nc_inplace) {
  test_controlled_growth(metaspace::ReflectionMetaspaceType, false,
                         word_size_for_level(CHUNK_LEVEL_2K), true);
}

TEST_VM(metaspace, spacemanager_growth_refl_nc_not_inplace) {
  test_controlled_growth(metaspace::ReflectionMetaspaceType, false,
                         word_size_for_level(CHUNK_LEVEL_2K), false);
}

TEST_VM(metaspace, spacemanager_growth_anon_nc_inplace) {
  test_controlled_growth(metaspace::ClassMirrorHolderMetaspaceType, false,
                         word_size_for_level(CHUNK_LEVEL_1K), true);
}

TEST_VM(metaspace, spacemanager_growth_anon_nc_not_inplace) {
  test_controlled_growth(metaspace::ClassMirrorHolderMetaspaceType, false,
                         word_size_for_level(CHUNK_LEVEL_1K), false);
}

TEST_VM(metaspace, spacemanager_growth_standard_nc_inplace) {
  test_controlled_growth(metaspace::StandardMetaspaceType, false,
                         word_size_for_level(CHUNK_LEVEL_4K), true);
}

TEST_VM(metaspace, spacemanager_growth_standard_nc_not_inplace) {
  test_controlled_growth(metaspace::StandardMetaspaceType, false,
                         word_size_for_level(CHUNK_LEVEL_4K), false);
}

TEST_VM(metaspace, spacemanager_growth_boot_nc_inplace) {
  test_controlled_growth(metaspace::BootMetaspaceType, false,
                         word_size_for_level(CHUNK_LEVEL_4M), true);
}

TEST_VM(metaspace, spacemanager_growth_boot_nc_not_inplace) {
  test_controlled_growth(metaspace::BootMetaspaceType, false,
                         word_size_for_level(CHUNK_LEVEL_4M), false);
}
