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

#include "dummy-lp-face.hpp"
#include "dummy-transport.hpp"

namespace nfd {
namespace face {
namespace tests {

class DummyLpFace::LinkService : public face::LinkService
{
public:
  void
  receiveInterest(const Interest& interest)
  {
    this->face::LinkService::receiveInterest(interest);
  }

  void
  receiveData(const Data& data)
  {
    this->face::LinkService::receiveData(data);
  }

  void
  receiveNack(const lp::Nack& nack)
  {
    this->face::LinkService::receiveNack(nack);
  }

  signal::Signal<LinkService> afterSend;

private:
  virtual void
  doSendInterest(const Interest& interest) DECL_OVERRIDE
  {
    this->sentInterests.push_back(interest);
    this->afterSend();
  }

  virtual void
  doSendData(const Data& data) DECL_OVERRIDE
  {
    this->sentData.push_back(data);
    this->afterSend();
  }

  virtual void
  doSendNack(const lp::Nack& nack) DECL_OVERRIDE
  {
    this->sentNacks.push_back(nack);
    this->afterSend();
  }

  virtual void
  doReceivePacket(Transport::Packet&& packet) DECL_OVERRIDE
  {
    BOOST_ASSERT(false);
  }

public:
  std::vector<Interest> sentInterests;
  std::vector<Data> sentData;
  std::vector<lp::Nack> sentNacks;
};

DummyLpFace::DummyLpFace(const std::string& localUri, const std::string& remoteUri,
                         ndn::nfd::FaceScope scope, ndn::nfd::FacePersistency persistency,
                         ndn::nfd::LinkType linkType)
  : LpFace(make_unique<LinkService>(),
           make_unique<DummyTransport>(localUri, remoteUri, scope, persistency, linkType))
  , afterSend(this->getLinkServiceInternal()->afterSend)
  , sentInterests(this->getLinkServiceInternal()->sentInterests)
  , sentData(this->getLinkServiceInternal()->sentData)
  , sentNacks(this->getLinkServiceInternal()->sentNacks)
{
}

void
DummyLpFace::setState(FaceState state)
{
  this->getTransportInternal()->setState(state);
}

void
DummyLpFace::receiveInterest(const Interest& interest)
{
  this->getLinkServiceInternal()->receiveInterest(interest);
}

void
DummyLpFace::receiveData(const Data& data)
{
  this->getLinkServiceInternal()->receiveData(data);
}

void
DummyLpFace::receiveNack(const lp::Nack& nack)
{
  this->getLinkServiceInternal()->receiveNack(nack);
}

DummyLpFace::LinkService*
DummyLpFace::getLinkServiceInternal()
{
  return static_cast<LinkService*>(this->getLinkService());
}

DummyTransport*
DummyLpFace::getTransportInternal()
{
  return static_cast<DummyTransport*>(this->getTransport());
}

} // namespace tests
} // namespace face
} // namespace nfd
