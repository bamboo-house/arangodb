////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for ClusterComm
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"

#include <iostream>

#include <velocypack/vpack.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Result.h"
#include "Cluster/MaintenanceAction.h"
#include "Cluster/MaintenanceFeature.h"

//
// structure used to store expected states of action properties
//
struct Expected {
  int _id;
  int _result;
  int _state;
  int _progress;
};

typedef std::vector<Expected> ExpectedVec_t;

//
// TestProgressHandler lets us know once ApplicationServer is ready
//
class TestProgressHandler : public arangodb::application_features::ProgressHandler {
public:
  TestProgressHandler() {
    _serverReady=false;

    using std::placeholders::_1;
    _state = std::bind(& TestProgressHandler::StateChange, this, _1);

    using std::placeholders::_2;
    _feature = std::bind(& TestProgressHandler::FeatureChange, this, _1, _2);
  }


  void StateChange(arangodb::application_features::ServerState newState) {
    if (arangodb::application_features::ServerState::IN_WAIT == newState) {
      CONDITION_LOCKER(clock, _serverReadyCond);
      _serverReady = true;
      _serverReadyCond.broadcast();
    }
  }

  void FeatureChange(arangodb::application_features::ServerState newState, std::string const &) {
  }

  arangodb::basics::ConditionVariable _serverReadyCond;
  std::atomic_bool _serverReady;

};// class TestProgressHandler


//
// TestFeature wraps MaintenanceFeature to all test specific action objects
//  by overriding the actionFactory() virtual function.  Two versions:
//  1. default constructor for non-threaded actions
//  2. constructor with ApplicationServer pointer for threaded actions
//
class TestMaintenanceFeature : public arangodb::MaintenanceFeature {
public:
  TestMaintenanceFeature() {

  };

  TestMaintenanceFeature(arangodb::application_features::ApplicationServer * as)
    : arangodb::MaintenanceFeature(as) {

    // begin with no threads to allow queue validation
    _maintenanceThreadsMax = 0;
    as->addReporter(_progressHandler);
  };

  virtual ~TestMaintenanceFeature() {

  };

  virtual arangodb::maintenance::MaintenanceActionPtr_t actionFactory(std::string & name,
                                                                      std::shared_ptr<arangodb::maintenance::ActionDescription_t> const & description,
                                                            std::shared_ptr<VPackBuilder> const & properties) override;

  void setSecondsActionsBlock(uint32_t seconds) {_secondsActionsBlock = seconds;};


  /// @brief set thread count, then activate the threads via start().  One time usage only.
  ///   Code waits until background ApplicationServer known to have fully started.
  void setMaintenanceThreadsMax(uint32_t threads) {
    CONDITION_LOCKER(clock, _progressHandler._serverReadyCond);
    while(!_progressHandler._serverReady) {
      _progressHandler._serverReadyCond.wait();
    } // while

    _maintenanceThreadsMax = threads;
    start();
  } // setMaintenanceThreadsMax


  bool verifyRegistryState(ExpectedVec_t & expected) {
    bool good(true);

    VPackArrayIterator registry(toVelocityPack().slice());
    REQUIRE(registry.size() == expected.size());

    auto action = registry.begin();
    auto check = expected.begin();

    for ( ; registry.end() != action && expected.end()!=check; ++action, ++check) {
      VPackSlice id = (*action).get("id");
      if (!(id.isInteger() && id.getInt() == check->_id)) {
        std::cerr << "Id mismatch: action has " << id.getInt() << " expected " << check->_id << std::endl;
        good = false;
      } // if

      VPackSlice result = (*action).get("result");
      if (!(result.isInteger() && check->_result == result.getInt())) {
        std::cerr << "Result mismatch: action has " << result.getInt() << " expected " << check->_result << std::endl;
        good = false;
      } // if

      VPackSlice state = (*action).get("state");
      if (!(state.isInteger() && check->_state == state.getInt())) {
        std::cerr << "State mismatch: action has " << state.getInt() << " expected " << check->_state << std::endl;
        good = false;
      } // if

      VPackSlice progress = (*action).get("progress");
      if (!(progress.isInteger() && check->_progress == progress.getInt())) {
        std::cerr << "Progress mismatch: action has " << progress.getInt() << " expected " << check->_progress << std::endl;
        good = false;
      } // if
    } // for

    return good;

  } // verifyRegistryState



public:
  arangodb::maintenance::MaintenanceActionPtr_t _recentAction;
  TestProgressHandler _progressHandler;






};// TestMaintenanceFeature


//
// TestActionBasic simulates a multistep action by counting down
//  on each call to first() and next() until iteration counter is zero.
//  Returns false upon reaching zero
//
class TestActionBasic : public arangodb::maintenance::MaintenanceAction {
public:
    TestActionBasic(arangodb::MaintenanceFeature & feature,
                    std::shared_ptr<arangodb::maintenance::ActionDescription_t> const & description,
                    std::shared_ptr<VPackBuilder> const & properties)
      : MaintenanceAction(feature, description, properties), _iteration(1), _resultCode(0)
  {
    auto des_it = description->find("iterate_count");

    if (description->end() != des_it) {
      _iteration = atol(des_it->second.c_str());
      // safety check
      if (_iteration < 0) {
        _iteration = 1;
      } // if
    } // if

    auto res_it = description->find("result_code");

    if (description->end() != res_it) {
      _resultCode = atol(res_it->second.c_str());
    } // if

  };

  virtual ~TestActionBasic() {};

  bool first() override {
    // time to set result?
    if (0==_iteration) {
      _result.reset(_resultCode);
    } // if

    // verify first() called once
    if (0!=getProgress()) {
      _result.reset(2);
    } // if

    return(0 < _iteration-- && _result.ok());
  };

  bool next() override {
    // time to set result?
    if (0==_iteration) {
      _result.reset(_resultCode);
    } // if

    // verify next() called properly
    if (0==getProgress()) {
      _result.reset(2);
    } // if

    return(0 < _iteration-- && _result.ok());
  };

  int _iteration;
  int _resultCode;

};// TestActionBasic


arangodb::maintenance::MaintenanceActionPtr_t TestMaintenanceFeature::actionFactory(std::string & name,
                                                                         std::shared_ptr<arangodb::maintenance::ActionDescription_t> const & description,
                                                               std::shared_ptr<VPackBuilder> const & properties) {
  arangodb::maintenance::MaintenanceActionPtr_t newAction;

    // walk list until we find the object of our desire
  if (name == "TestActionBasic") {
    newAction.reset(new TestActionBasic(*this, description, properties));
  }

  // make test access to this new action easy ... and prevent its deletion to soon
  _recentAction = newAction;

  return newAction;

} // TestMaintenanceFeature::actionFactory


TEST_CASE("MaintenanceFeatureUnthreaded", "[cluster][maintenance][devel]") {

  SECTION("Iterate Action 0 times - ok") {
    TestMaintenanceFeature tf;
    tf.setSecondsActionsBlock(0);  // disable retry wait for now
    arangodb::maintenance::ActionDescription_t desc={{"name","TestActionBasic"},{"iterate_count","0"}};
    auto desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    auto prop_ptr = std::make_shared<VPackBuilder>();

    arangodb::Result result = tf.addAction(desc_ptr, prop_ptr, true);

    REQUIRE(result.ok());
    REQUIRE(tf._recentAction->result().ok());
    REQUIRE(0==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == arangodb::maintenance::MaintenanceAction::COMPLETE);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());
  }

  SECTION("Iterate Action 0 times - fail") {
    TestMaintenanceFeature tf;
    tf.setSecondsActionsBlock(0);  // disable retry wait for now
    arangodb::maintenance::ActionDescription_t desc={{"name","TestActionBasic"},{"iterate_count","0"},
                                                     {"result_code","1"}};
    auto desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    auto prop_ptr = std::make_shared<VPackBuilder>();

    arangodb::Result result = tf.addAction(desc_ptr, prop_ptr, true);

    REQUIRE(!result.ok());
    REQUIRE(!tf._recentAction->result().ok());
    REQUIRE(0==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == arangodb::maintenance::MaintenanceAction::FAILED);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());
  }

  SECTION("Iterate Action 1 time - ok") {
    TestMaintenanceFeature tf;
    tf.setSecondsActionsBlock(0);  // disable retry wait for now
    arangodb::maintenance::ActionDescription_t desc={{"name","TestActionBasic"},{"iterate_count","1"}};
    auto desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    auto prop_ptr = std::make_shared<VPackBuilder>();

    arangodb::Result result = tf.addAction(desc_ptr, prop_ptr, true);

    REQUIRE(result.ok());
    REQUIRE(tf._recentAction->result().ok());
    REQUIRE(1==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == arangodb::maintenance::MaintenanceAction::COMPLETE);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());
  }

  SECTION("Iterate Action 1 time - fail") {
    TestMaintenanceFeature tf;
    tf.setSecondsActionsBlock(0);  // disable retry wait for now
    arangodb::maintenance::ActionDescription_t desc={{"name","TestActionBasic"},{"iterate_count","1"},
                                                     {"result_code","1"}};
    auto desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    auto prop_ptr = std::make_shared<VPackBuilder>();

    arangodb::Result result = tf.addAction(desc_ptr, prop_ptr, true);

    REQUIRE(!result.ok());
    REQUIRE(!tf._recentAction->result().ok());
    REQUIRE(1==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == arangodb::maintenance::MaintenanceAction::FAILED);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());
  }

  SECTION("Iterate Action 2 times - ok") {
    TestMaintenanceFeature tf;
    tf.setSecondsActionsBlock(0);  // disable retry wait for now
    arangodb::maintenance::ActionDescription_t desc={{"name","TestActionBasic"},{"iterate_count","2"}};
    auto desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    auto prop_ptr = std::make_shared<VPackBuilder>();

    arangodb::Result result = tf.addAction(desc_ptr, prop_ptr, true);

    REQUIRE(result.ok());
    REQUIRE(tf._recentAction->result().ok());
    REQUIRE(2==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == arangodb::maintenance::MaintenanceAction::COMPLETE);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());
  }

  SECTION("Iterate Action 100 times - ok") {
    TestMaintenanceFeature tf;
    tf.setSecondsActionsBlock(0);  // disable retry wait for now
    arangodb::maintenance::ActionDescription_t desc={{"name","TestActionBasic"},{"iterate_count","100"}};
    auto desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    auto prop_ptr = std::make_shared<VPackBuilder>();

    arangodb::Result result = tf.addAction(desc_ptr, prop_ptr, true);

    REQUIRE(result.ok());
    REQUIRE(tf._recentAction->result().ok());
    REQUIRE(100==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == arangodb::maintenance::MaintenanceAction::COMPLETE);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());
  }

  SECTION("Iterate Action 100 times - fail") {
    TestMaintenanceFeature tf;
    tf.setSecondsActionsBlock(0);  // disable retry wait for now
    arangodb::maintenance::ActionDescription_t desc={{"name","TestActionBasic"},{"iterate_count","100"},
                                                     {"result_code","1"}};
    auto desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    auto prop_ptr = std::make_shared<VPackBuilder>();

    arangodb::Result result = tf.addAction(desc_ptr, prop_ptr, true);

    REQUIRE(!result.ok());
    REQUIRE(!tf._recentAction->result().ok());
    REQUIRE(100==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == arangodb::maintenance::MaintenanceAction::FAILED);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());
  }
} // MaintenanceFeatureUnthreaded

TEST_CASE("MaintenanceFeatureThreaded", "[cluster][maintenance][devel]") {

  SECTION("Populate action queue and validate") {
    std::vector<Expected> pre_thread, post_thread;

    std::shared_ptr<arangodb::options::ProgramOptions> po = std::make_shared<arangodb::options::ProgramOptions>("test", std::string(), std::string(), "path");
    arangodb::application_features::ApplicationServer as(po, nullptr);
    TestMaintenanceFeature * tf = new TestMaintenanceFeature(&as);
    as.addFeature(tf);
    std::thread th(&arangodb::application_features::ApplicationServer::run, &as, 0, nullptr);

    //
    // 1. load up the queue without threads running
    //   a. 100 iterations then fail
    arangodb::maintenance::ActionDescription_t desc={{"name","TestActionBasic"},{"iterate_count","100"},
                                                     {"result_code","1"}};
    auto desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    auto prop_ptr = std::make_shared<VPackBuilder>();

    arangodb::Result result = tf->addAction(desc_ptr, prop_ptr, false);
    REQUIRE(result.ok());   // has not executed, ok() is about parse and list add
    REQUIRE(tf->_recentAction->result().ok());
    pre_thread.push_back({1,0,1,0});
    post_thread.push_back({1,1,6,100});

    //   b. 2 iterations then succeed;
    desc={{"name","TestActionBasic"},{"iterate_count","2"}};
    desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    prop_ptr = std::make_shared<VPackBuilder>();

    result = tf->addAction(desc_ptr, prop_ptr, false);
    REQUIRE(result.ok());   // has not executed, ok() is about parse and list add
    REQUIRE(tf->_recentAction->result().ok());
    pre_thread.push_back({2,0,1,0});
    post_thread.push_back({2,0,5,2});

    //   c. duplicate of 'a', should fail to add
    desc={{"name","TestActionBasic"},{"iterate_count","100"},
                                                     {"result_code","1"}};
    desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    prop_ptr = std::make_shared<VPackBuilder>();

    result = tf->addAction(desc_ptr, prop_ptr, false);
    REQUIRE(!result.ok());   // has not executed, ok() is about parse and list add
    // _recentAction will NOT contain the aborted object ... don't test it

    //
    // 2. see if happy about queue prior to threads running
    REQUIRE(tf->verifyRegistryState(pre_thread));

    //
    // 3. start threads AFTER ApplicationServer known to be running
    tf->setMaintenanceThreadsMax(1);

    //
    // 4. loop while waiting for threads to complete all actions
    bool again;

    do {
      again = false;
      sleep(1);
      VPackArrayIterator ai1(tf->toVelocityPack().slice());
      for (auto ma : ai1 )
      {
        VPackSlice state = ma.get("state");
        again = again || (6 != state.getInt() && 5 != state.getInt());
      } // for
    } while(again);

    //
    // 5. verify completed actions
    REQUIRE(tf->verifyRegistryState(post_thread));

#if 0   // for debugging
    std::string xx = (tf->toVelocityPack()).toJson();
    printf("%s\n", xx.c_str());
#endif

    //
    // 6. bring down the ApplicationServer, i.e. clean up
    as.beginShutdown();
    th.join();
  }
} // MaintenanceFeatureThreaded
