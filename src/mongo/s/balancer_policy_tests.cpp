/*    Copyright 2012 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "mongo/unittest/unittest.h"
#include "mongo/s/config.h"
#include "mongo/s/balancer_policy.h"

namespace mongo {

    // these are all crutch and hopefully will eventually go away
    CmdLine cmdLine;
    bool inShutdown() { return false; }
    void setupSignals( bool inFork ) {}
    DBClientBase *createDirectClient() { return 0; }
    void dbexit( ExitCode rc, const char *why ){
        log()  << "dbexit called? :(" << endl;
        ::_exit(-1);
    }


    namespace {
        
        TEST( BalancerPolicyTests , SizeMaxedShardTest ) {
            ASSERT( ! BalancerPolicy::ShardInfo( 0, 0, false, false ).isSizeMaxed() );
            ASSERT( ! BalancerPolicy::ShardInfo( 100LL, 80LL, false, false ).isSizeMaxed() );
            ASSERT( BalancerPolicy::ShardInfo( 100LL, 110LL, false, false ).isSizeMaxed() );
        }

        TEST( BalancerPolicyTests , BalanceNormalTest  ) {
            // 2 chunks and 0 chunk shards
            BalancerPolicy::ShardToChunksMap chunkMap;
            vector<BSONObj> chunks;
            chunks.push_back(BSON( "min" << BSON( "x" << BSON( "$minKey"<<1) ) <<
                                   "max" << BSON( "x" << 49 )));
            chunks.push_back(BSON( "min" << BSON( "x" << 49 ) <<
                                   "max" << BSON( "x" << BSON( "$maxkey"<<1 ))));
            chunkMap["shard0"] = chunks;
            chunks.clear();
            chunkMap["shard1"] = chunks;

            // no limits
            BalancerPolicy::ShardInfoMap info;
            info["shard0"] = BalancerPolicy::ShardInfo( 0, 2, false, false );
            info["shard1"] = BalancerPolicy::ShardInfo( 0, 0, false, false );

            BalancerPolicy::MigrateInfo* c = NULL;
            c = BalancerPolicy::balance( "ns", info, chunkMap, 1 );
            ASSERT( c );
        }

        TEST( BalanceNormalTests ,  BalanceDrainingTest ) {
            // one normal, one draining
            // 2 chunks and 0 chunk shards
            BalancerPolicy::ShardToChunksMap chunkMap;
            vector<BSONObj> chunks;
            chunks.push_back(BSON( "min" << BSON( "x" << BSON( "$minKey"<<1) ) <<
                                   "max" << BSON( "x" << 49 )));
            chunkMap["shard0"] = chunks;
            chunks.clear();
            chunks.push_back(BSON( "min" << BSON( "x" << 49 ) <<
                                   "max" << BSON( "x" << BSON( "$maxkey"<<1 ))));
            chunkMap["shard1"] = chunks;

            // shard0 is draining
            BalancerPolicy::ShardInfoMap limitsMap;
            limitsMap["shard0"] = BalancerPolicy::ShardInfo( 0LL, 2LL, true, false );
            limitsMap["shard1"] = BalancerPolicy::ShardInfo( 0LL, 0LL, false, false );

            BalancerPolicy::MigrateInfo* c = NULL;
            c = BalancerPolicy::balance( "ns", limitsMap, chunkMap, 0 );
            ASSERT( c );
            ASSERT_EQUALS( c->to , "shard1" );
            ASSERT_EQUALS( c->from , "shard0" );
            ASSERT( ! c->chunk.min.isEmpty() );
        }

        TEST( BalancerPolicyTests , BalanceEndedDrainingTest ) {
            // 2 chunks and 0 chunk (drain completed) shards
            BalancerPolicy::ShardToChunksMap chunkMap;
            vector<BSONObj> chunks;
            chunks.push_back(BSON( "min" << BSON( "x" << BSON( "$minKey"<<1) ) <<
                                   "max" << BSON( "x" << 49 )));
            chunks.push_back(BSON( "min" << BSON( "x" << 49 ) <<
                                   "max" << BSON( "x" << BSON( "$maxkey"<<1 ))));
            chunkMap["shard0"] = chunks;
            chunks.clear();
            chunkMap["shard1"] = chunks;

            // no limits
            BalancerPolicy::ShardInfoMap limitsMap;
            limitsMap["shard0"] = BalancerPolicy::ShardInfo( 0, 2, false, false );
            limitsMap["shard1"] = BalancerPolicy::ShardInfo( 0, 0, true, false );

            BalancerPolicy::MigrateInfo* c = NULL;
            c = BalancerPolicy::balance( "ns", limitsMap, chunkMap, 0 );
            ASSERT( ! c );
        }

        TEST( BalancerPolicyTests , BalanceImpasseTest ) {
            // one maxed out, one draining
            // 2 chunks and 0 chunk shards
            BalancerPolicy::ShardToChunksMap chunkMap;
            vector<BSONObj> chunks;
            chunks.push_back(BSON( "min" << BSON( "x" << BSON( "$minKey"<<1) ) <<
                                   "max" << BSON( "x" << 49 )));
            chunkMap["shard0"] = chunks;
            chunks.clear();
            chunks.push_back(BSON( "min" << BSON( "x" << 49 ) <<
                                   "max" << BSON( "x" << BSON( "$maxkey"<<1 ))));
            chunkMap["shard1"] = chunks;

            // shard0 is draining, shard1 is maxed out, shard2 has writebacks pending
            BalancerPolicy::ShardInfoMap limitsMap;
            limitsMap["shard0"] = BalancerPolicy::ShardInfo( 0, 2, true, false );
            limitsMap["shard1"] = BalancerPolicy::ShardInfo( 1, 1, false, false );
            limitsMap["shard2"] = BalancerPolicy::ShardInfo( 0, 1, true, false );

            BalancerPolicy::MigrateInfo* c = NULL;
            c = BalancerPolicy::balance( "ns", limitsMap, chunkMap, 0 );
            ASSERT( ! c );
        }
    }
}
