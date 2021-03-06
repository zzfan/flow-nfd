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

#ifndef NFD_TESTS_DAEMON_FACE_DUMMY_LP_FACE_HPP
#define NFD_TESTS_DAEMON_FACE_DUMMY_LP_FACE_HPP

#include "face/lp-face.hpp"

namespace nfd {
namespace face {
namespace tests {

class DummyTransport;

/** \brief a LpFace for unit testing
 *
 *  The DummyLpFace allows observing outgoing network-layer packets,
 *  and allows incoming network-layer packets to be injected from a test suite.
 *  It's primarily used for forwarding test suites, but can be used in other tests as well.
 */
class DummyLpFace : public LpFace
{
public:
  class LinkService;

  DummyLpFace(const std::string& localUri = "dummy://", const std::string& remoteUri = "dummy://",
              ndn::nfd::FaceScope scope = ndn::nfd::FACE_SCOPE_NON_LOCAL,
              ndn::nfd::FacePersistency persistency = ndn::nfd::FACE_PERSISTENCY_PERSISTENT,
              ndn::nfd::LinkType linkType = ndn::nfd::LINK_TYPE_POINT_TO_POINT);

  /** \brief changes face state
   *  \pre current state is not CLOSED or FAILED
   */
  void
  setState(FaceState state);

  /** \brief causes the face to receive an Interest
   */
  void
  receiveInterest(const Interest& interest);

  /** \brief causes the face to receive a Data
   */
  void
  receiveData(const Data& data);

  /** \brief causes the face to receive a Nack
   */
  void
  receiveNack(const lp::Nack& nack);

  /** \brief signals after any network-layer packet is sent
   */
  signal::Signal<LinkService>& afterSend;

private:
  LinkService*
  getLinkServiceInternal();

  DummyTransport*
  getTransportInternal();

public:
  std::vector<Interest>& sentInterests;
  std::vector<Data>& sentData;
  std::vector<lp::Nack>& sentNacks;
};

} // namespace tests
} // namespace face

namespace tests {
using nfd::face::tests::DummyLpFace;
} // namespace tests
} // namespace nfd

#endif // NFD_TESTS_DAEMON_FACE_DUMMY_LP_FACE_HPP
