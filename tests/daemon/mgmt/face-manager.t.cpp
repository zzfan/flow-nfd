/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2015,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mgmt/face-manager.hpp"
#include "manager-common-fixture.hpp"
#include "../face/dummy-face.hpp"
#include "face/tcp-factory.hpp"
#include "face/udp-factory.hpp"

#include <ndn-cxx/util/random.hpp>
#include <ndn-cxx/encoding/tlv.hpp>
#include <ndn-cxx/management/nfd-channel-status.hpp>
#include <ndn-cxx/management/nfd-face-event-notification.hpp>

namespace nfd {
namespace tests {

class FaceManagerFixture : public ManagerCommonFixture
{
public:
  FaceManagerFixture()
    : m_faceTable(m_forwarder.getFaceTable())
    , m_manager(m_faceTable, m_dispatcher, m_validator)
  {
    setTopPrefixAndPrivilege("/localhost/nfd", "faces");
  }

public:
  template<typename Face>
  shared_ptr<Face>
  addFace(bool wantRemoveLastNotification = false)
  {
    auto face = make_shared<Face>();
    m_faceTable.add(face);
    advanceClocks(time::milliseconds(1), 10); // wait for notification posted
    if (wantRemoveLastNotification) {
      m_responses.pop_back();
    }
    return face;
  }

protected:
  FaceTable& m_faceTable;
  FaceManager m_manager;
};

BOOST_FIXTURE_TEST_SUITE(Mgmt, FaceManagerFixture)
BOOST_AUTO_TEST_SUITE(TestFaceManager)

BOOST_AUTO_TEST_SUITE(DestroyFace)

BOOST_AUTO_TEST_CASE(Existing)
{
  auto addedFace = addFace<DummyFace>(true); // clear notification for creation

  auto parameters = ControlParameters().setFaceId(addedFace->getId());
  auto command = makeControlCommandRequest("/localhost/nfd/faces/destroy", parameters);

  receiveInterest(command);

  BOOST_REQUIRE_EQUAL(m_responses.size(), 2); // one response and one notification
  // notification is already tested, so ignore it

  BOOST_CHECK_EQUAL(checkResponse(1, command->getName(), makeResponse(200, "OK", parameters)),
                    CheckResponseResult::OK);

  BOOST_CHECK_EQUAL(addedFace->getId(), -1);
}

BOOST_AUTO_TEST_CASE(NonExisting)
{
  auto parameters = ControlParameters().setFaceId(65535);
  auto command = makeControlCommandRequest("/localhost/nfd/faces/destroy", parameters);

  receiveInterest(command);

  BOOST_REQUIRE_EQUAL(m_responses.size(), 1);

  BOOST_CHECK_EQUAL(checkResponse(0, command->getName(), makeResponse(200, "OK", parameters)),
                    CheckResponseResult::OK);
}

BOOST_AUTO_TEST_SUITE_END() // DestroyFace

BOOST_AUTO_TEST_CASE(FaceEvents)
{
  auto addedFace = addFace<DummyFace>(); // trigger FACE_EVENT_CREATED notification
  BOOST_CHECK_NE(addedFace->getId(), -1);
  int64_t faceId = addedFace->getId();

  // check notification
  {
    Block payload;
    ndn::nfd::FaceEventNotification notification;
    BOOST_REQUIRE_EQUAL(m_responses.size(), 1);
    BOOST_CHECK_NO_THROW(payload = m_responses[0].getContent().blockFromValue());
    BOOST_CHECK_EQUAL(payload.type(), ndn::tlv::nfd::FaceEventNotification);
    BOOST_CHECK_NO_THROW(notification.wireDecode(payload));
    BOOST_CHECK_EQUAL(notification.getKind(), ndn::nfd::FACE_EVENT_CREATED);
    BOOST_CHECK_EQUAL(notification.getFaceId(), faceId);
    BOOST_CHECK_EQUAL(notification.getRemoteUri(), addedFace->getRemoteUri().toString());
    BOOST_CHECK_EQUAL(notification.getLocalUri(), addedFace->getLocalUri().toString());
    BOOST_CHECK_EQUAL(notification.getFaceScope(), ndn::nfd::FACE_SCOPE_NON_LOCAL);
    BOOST_CHECK_EQUAL(notification.getFacePersistency(), ndn::nfd::FACE_PERSISTENCY_PERSISTENT);
    BOOST_CHECK_EQUAL(notification.getLinkType(), ndn::nfd::LinkType::LINK_TYPE_POINT_TO_POINT);
  }

  addedFace->close(); // trigger FaceDestroy FACE_EVENT_DESTROYED
  advanceClocks(time::milliseconds(1), 10);

  // check notification
  {
    Block payload;
    ndn::nfd::FaceEventNotification notification;
    BOOST_REQUIRE_EQUAL(m_responses.size(), 2);
    BOOST_CHECK_NO_THROW(payload = m_responses[1].getContent().blockFromValue());
    BOOST_CHECK_EQUAL(payload.type(), ndn::tlv::nfd::FaceEventNotification);
    BOOST_CHECK_NO_THROW(notification.wireDecode(payload));
    BOOST_CHECK_EQUAL(notification.getKind(), ndn::nfd::FACE_EVENT_DESTROYED);
    BOOST_CHECK_EQUAL(notification.getFaceId(), faceId);
    BOOST_CHECK_EQUAL(notification.getRemoteUri(), addedFace->getRemoteUri().toString());
    BOOST_CHECK_EQUAL(notification.getLocalUri(), addedFace->getLocalUri().toString());
    BOOST_CHECK_EQUAL(notification.getFaceScope(), ndn::nfd::FACE_SCOPE_NON_LOCAL);
    BOOST_CHECK_EQUAL(notification.getFacePersistency(), ndn::nfd::FACE_PERSISTENCY_PERSISTENT);
    BOOST_CHECK_EQUAL(notification.getLinkType(), ndn::nfd::LinkType::LINK_TYPE_POINT_TO_POINT);
  }
  BOOST_CHECK_EQUAL(addedFace->getId(), -1);
}

BOOST_AUTO_TEST_CASE(EnableDisableLocalControl)
{
  auto nonLocalFace = addFace<DummyFace>(true); // clear notification
  auto localFace = addFace<DummyLocalFace>(true); // clear notification
  BOOST_CHECK(localFace->isLocal());
  BOOST_CHECK(!nonLocalFace->isLocal());

  std::vector<Name> commandNames;
  auto testLocalControl = [&] (const Name& name, const ControlParameters& params, uint64_t faceId) {
    auto command = makeControlCommandRequest(name, params,
                                             [faceId] (shared_ptr<Interest> interest) {
                                               interest->setIncomingFaceId(faceId);
                                             });
    receiveInterest(command);
    commandNames.push_back(command->getName());
  };

  auto paramsF = ControlParameters().setLocalControlFeature(LOCAL_CONTROL_FEATURE_INCOMING_FACE_ID);
  auto paramsN = ControlParameters().setLocalControlFeature(LOCAL_CONTROL_FEATURE_NEXT_HOP_FACE_ID);

  // non-existing face: 0~3
  testLocalControl("/localhost/nfd/faces/enable-local-control", paramsF, FACEID_NULL);
  testLocalControl("/localhost/nfd/faces/disable-local-control", paramsF, FACEID_NULL);
  testLocalControl("/localhost/nfd/faces/enable-local-control", paramsN, FACEID_NULL);
  testLocalControl("/localhost/nfd/faces/disable-local-control", paramsN, FACEID_NULL);

  // non-local face: 4~7
  testLocalControl("/localhost/nfd/faces/enable-local-control", paramsF, nonLocalFace->getId());
  testLocalControl("/localhost/nfd/faces/disable-local-control", paramsF, nonLocalFace->getId());
  testLocalControl("/localhost/nfd/faces/enable-local-control", paramsN, nonLocalFace->getId());
  testLocalControl("/localhost/nfd/faces/disable-local-control", paramsN, nonLocalFace->getId());

  // enableLocalControl for Incoming FaceId on existing local face:
  testLocalControl("/localhost/nfd/faces/enable-local-control", paramsF, localFace->getId()); // 8
  BOOST_CHECK(localFace->isLocalControlHeaderEnabled(LOCAL_CONTROL_FEATURE_INCOMING_FACE_ID));
  BOOST_CHECK(!localFace->isLocalControlHeaderEnabled(LOCAL_CONTROL_FEATURE_NEXT_HOP_FACE_ID));

  // disableLocalControl for Incoming FaceId on existing local face
  testLocalControl("/localhost/nfd/faces/disable-local-control", paramsF, localFace->getId()); // 9
  BOOST_CHECK(!localFace->isLocalControlHeaderEnabled(LOCAL_CONTROL_FEATURE_INCOMING_FACE_ID));
  BOOST_CHECK(!localFace->isLocalControlHeaderEnabled(LOCAL_CONTROL_FEATURE_NEXT_HOP_FACE_ID));

  // enableLocalControl for NextHop ID on existing local face
  testLocalControl("/localhost/nfd/faces/enable-local-control", paramsN, localFace->getId()); // 10
  BOOST_CHECK(!localFace->isLocalControlHeaderEnabled(LOCAL_CONTROL_FEATURE_INCOMING_FACE_ID));
  BOOST_CHECK(localFace->isLocalControlHeaderEnabled(LOCAL_CONTROL_FEATURE_NEXT_HOP_FACE_ID));

  // disableLocalControl for NextHop ID on existing local face
  testLocalControl("/localhost/nfd/faces/disable-local-control", paramsN, localFace->getId()); // 11
  BOOST_CHECK(!localFace->isLocalControlHeaderEnabled(LOCAL_CONTROL_FEATURE_INCOMING_FACE_ID));
  BOOST_CHECK(!localFace->isLocalControlHeaderEnabled(LOCAL_CONTROL_FEATURE_NEXT_HOP_FACE_ID));

  // check responses
  BOOST_REQUIRE_EQUAL(m_responses.size(), 12);
  BOOST_CHECK_EQUAL(checkResponse(0,  commandNames[0],  ControlResponse(410, "Face not found")),
                    CheckResponseResult::OK);
  BOOST_CHECK_EQUAL(checkResponse(1,  commandNames[1],  ControlResponse(410, "Face not found")),
                    CheckResponseResult::OK);
  BOOST_CHECK_EQUAL(checkResponse(2,  commandNames[2],  ControlResponse(410, "Face not found")),
                    CheckResponseResult::OK);
  BOOST_CHECK_EQUAL(checkResponse(3,  commandNames[3],  ControlResponse(410, "Face not found")),
                    CheckResponseResult::OK);
  BOOST_CHECK_EQUAL(checkResponse(4,  commandNames[4],  ControlResponse(412, "Face is non-local")),
                    CheckResponseResult::OK);
  BOOST_CHECK_EQUAL(checkResponse(5,  commandNames[5],  ControlResponse(412, "Face is non-local")),
                    CheckResponseResult::OK);
  BOOST_CHECK_EQUAL(checkResponse(6,  commandNames[6],  ControlResponse(412, "Face is non-local")),
                    CheckResponseResult::OK);
  BOOST_CHECK_EQUAL(checkResponse(7,  commandNames[7],  ControlResponse(412, "Face is non-local")),
                    CheckResponseResult::OK);
  BOOST_CHECK_EQUAL(checkResponse(8,  commandNames[8],  makeResponse(200, "OK", paramsF)),
                    CheckResponseResult::OK);
  BOOST_CHECK_EQUAL(checkResponse(9,  commandNames[9],  makeResponse(200, "OK", paramsF)),
                    CheckResponseResult::OK);
  BOOST_CHECK_EQUAL(checkResponse(10, commandNames[10], makeResponse(200, "OK", paramsN)),
                    CheckResponseResult::OK);
  BOOST_CHECK_EQUAL(checkResponse(11, commandNames[11], makeResponse(200, "OK", paramsN)),
                    CheckResponseResult::OK);
}

class TestFace : public DummyFace
{
public:
  explicit
  TestFace(const std::string& uri = "test://")
    : DummyFace(uri, uri)
  {
    getMutableCounters().getNInInterests().set(ndn::random::generateWord64());
    getMutableCounters().getNInDatas().set(ndn::random::generateWord64());
    getMutableCounters().getNOutInterests().set(ndn::random::generateWord64());
    getMutableCounters().getNOutDatas().set(ndn::random::generateWord64());
    getMutableCounters().getNInBytes().set(ndn::random::generateWord64());
    getMutableCounters().getNOutBytes().set(ndn::random::generateWord64());
  }
};

// @todo Refactor when ndn::nfd::FaceStatus implementes operator!= and operator<<
class FaceStatus : public ndn::nfd::FaceStatus
{
public:
  FaceStatus(const ndn::nfd::FaceStatus& s)
    : ndn::nfd::FaceStatus(s)
  {
  }
};

bool
operator!=(const FaceStatus& left, const FaceStatus& right)
{
  return left.getRemoteUri() != right.getRemoteUri() ||
    left.getLocalUri() != right.getLocalUri() ||
    left.getFaceScope() != right.getFaceScope() ||
    left.getFacePersistency() != right.getFacePersistency() ||
    left.getLinkType() != right.getLinkType() ||
    left.getNInInterests() != right.getNInInterests() ||
    left.getNInDatas() != right.getNInDatas() ||
    left.getNOutInterests() != right.getNOutInterests() ||
    left.getNOutDatas() != right.getNOutDatas() ||
    left.getNInBytes() != right.getNInBytes() ||
    left.getNOutBytes() != right.getNOutBytes();
}

std::ostream&
operator<<(std::ostream &os, const FaceStatus& status)
{
  os << "[" << status.getRemoteUri() << ", "
     << status.getLocalUri() << ", "
     << status.getFacePersistency() << ", "
     << status.getLinkType() << ", "
     << status.getNInInterests() << ", "
     << status.getNInDatas() << ", "
     << status.getNOutInterests() << ", "
     << status.getNOutDatas() << ", "
     << status.getNInBytes() << ", "
     << status.getNOutBytes() << "]";
  return os;
}

BOOST_AUTO_TEST_CASE(FaceDataset)
{
  size_t nEntries = 303;
  for (size_t i = 0 ; i < nEntries ; i ++) {
    addFace<TestFace>(true);
  }

  receiveInterest(makeInterest("/localhost/nfd/faces/list"));

  Block content;
  BOOST_CHECK_NO_THROW(content = concatenateResponses());
  BOOST_CHECK_NO_THROW(content.parse());
  BOOST_REQUIRE_EQUAL(content.elements().size(), nEntries);

  std::vector<FaceStatus> expectedStatuses, receivedStatuses;
  std::set<FaceId> faceIds;
  for (size_t idx = 0; idx < nEntries; ++idx) {
    BOOST_TEST_MESSAGE("processing element: " << idx);

    ndn::nfd::FaceStatus decodedStatus;
    BOOST_REQUIRE_NO_THROW(decodedStatus.wireDecode(content.elements()[idx]));
    BOOST_REQUIRE(m_faceTable.get(decodedStatus.getFaceId()) != nullptr);
    faceIds.insert(decodedStatus.getFaceId());
    receivedStatuses.push_back(decodedStatus);
    expectedStatuses.push_back(m_faceTable.get(decodedStatus.getFaceId())->getFaceStatus());
  }

  BOOST_CHECK_EQUAL(faceIds.size(), nEntries);
  BOOST_CHECK_EQUAL_COLLECTIONS(receivedStatuses.begin(), receivedStatuses.end(),
                                expectedStatuses.begin(), expectedStatuses.end());
}

BOOST_AUTO_TEST_CASE(FaceQuery)
{
  auto face1 = addFace<DummyFace>(true); // dummy://
  auto face2 = addFace<DummyLocalFace>(true); // dummy://, local
  auto face3 = addFace<TestFace>(true); // test://

  auto generateQueryName = [] (const ndn::nfd::FaceQueryFilter& filter) {
    return Name("/localhost/nfd/faces/query").append(filter.wireEncode());
  };

  auto querySchemeName =
    generateQueryName(ndn::nfd::FaceQueryFilter().setUriScheme("dummy"));
  auto queryIdName =
    generateQueryName(ndn::nfd::FaceQueryFilter().setFaceId(face1->getId()));
  auto queryScopeName =
    generateQueryName(ndn::nfd::FaceQueryFilter().setFaceScope(ndn::nfd::FACE_SCOPE_NON_LOCAL));
  auto invalidQueryName =
    Name("/localhost/nfd/faces/query").append(ndn::makeStringBlock(tlv::Content, "invalid"));

  receiveInterest(makeInterest(querySchemeName)); // face1 and face2 expected
  receiveInterest(makeInterest(queryIdName)); // face1 expected
  receiveInterest(makeInterest(queryScopeName)); // face1 and face3 expected
  receiveInterest(makeInterest(invalidQueryName)); // nack expected

  BOOST_REQUIRE_EQUAL(m_responses.size(), 4);

  Block content;
  ndn::nfd::FaceStatus status;

  content = m_responses[0].getContent();
  BOOST_CHECK_NO_THROW(content.parse());
  BOOST_CHECK_EQUAL(content.elements().size(), 2); // face1 and face2
  BOOST_CHECK_NO_THROW(status.wireDecode(content.elements()[0]));
  BOOST_CHECK_EQUAL(face1->getId(), status.getFaceId());
  BOOST_CHECK_NO_THROW(status.wireDecode(content.elements()[1]));
  BOOST_CHECK_EQUAL(face2->getId(), status.getFaceId());

  content = m_responses[1].getContent();
  BOOST_CHECK_NO_THROW(content.parse());
  BOOST_CHECK_EQUAL(content.elements().size(), 1); // face1
  BOOST_CHECK_NO_THROW(status.wireDecode(content.elements()[0]));
  BOOST_CHECK_EQUAL(face1->getId(), status.getFaceId());

  content = m_responses[2].getContent();
  BOOST_CHECK_NO_THROW(content.parse());
  BOOST_CHECK_EQUAL(content.elements().size(), 2); // face1 and face3
  BOOST_CHECK_NO_THROW(status.wireDecode(content.elements()[0]));
  BOOST_CHECK_EQUAL(face1->getId(), status.getFaceId());
  BOOST_CHECK_NO_THROW(status.wireDecode(content.elements()[1]));
  BOOST_CHECK_EQUAL(face3->getId(), status.getFaceId());

  ControlResponse expectedResponse(400, "Malformed filter"); // nack, 400, malformed filter
  BOOST_CHECK_EQUAL(checkResponse(3, invalidQueryName, expectedResponse, tlv::ContentType_Nack),
                    CheckResponseResult::OK);
}

class TestChannel : public Channel
{
public:
  TestChannel(const std::string& uri)
  {
    setUri(FaceUri(uri));
  }
};

class TestProtocolFactory : public ProtocolFactory
{
public:
  virtual void
  createFace(const FaceUri& uri,
             ndn::nfd::FacePersistency persistency,
             const FaceCreatedCallback& onCreated,
             const FaceCreationFailedCallback& onConnectFailed) DECL_OVERRIDE
  {
  }

  virtual std::vector<shared_ptr<const Channel>>
  getChannels() const DECL_OVERRIDE
  {
    return m_channels;
  }

public:
  shared_ptr<TestChannel>
  addChannel(const std::string& channelUri)
  {
    auto channel = make_shared<TestChannel>(channelUri);
    m_channels.push_back(channel);
    return channel;
  }

private:
  std::vector<shared_ptr<const Channel>> m_channels;
};

BOOST_AUTO_TEST_CASE(ChannelDataset)
{
  auto factory = make_shared<TestProtocolFactory>();
  m_manager.m_factories["test"] = factory;

  std::map<std::string, shared_ptr<TestChannel>> addedChannels;
  size_t nEntries = 404;
  for (size_t i = 0 ; i < nEntries ; i ++) {
    auto channel = factory->addChannel("test" + boost::lexical_cast<std::string>(i) + "://");
    addedChannels[channel->getUri().toString()] = channel;
  }

  receiveInterest(makeInterest("/localhost/nfd/faces/channels"));

  Block content;
  BOOST_CHECK_NO_THROW(content = concatenateResponses());
  BOOST_CHECK_NO_THROW(content.parse());
  BOOST_REQUIRE_EQUAL(content.elements().size(), nEntries);

  for (size_t idx = 0; idx < nEntries; ++idx) {
    BOOST_TEST_MESSAGE("processing element: " << idx);

    ndn::nfd::ChannelStatus decodedStatus;
    BOOST_CHECK_NO_THROW(decodedStatus.wireDecode(content.elements()[idx]));
    BOOST_CHECK(addedChannels.find(decodedStatus.getLocalUri()) != addedChannels.end());
  }
}

BOOST_AUTO_TEST_SUITE_END() // TestFaceManager
BOOST_AUTO_TEST_SUITE_END() // Mgmt

} // namespace tests
} // namespace nfd
