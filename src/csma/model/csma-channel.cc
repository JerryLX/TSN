/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 Emmanuelle Laprise
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Emmanuelle Laprise <emmanuelle.laprise@bluekazoo.ca>
 */

#include "csma-channel.h"
#include "csma-net-device.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("CsmaChannel");

NS_OBJECT_ENSURE_REGISTERED (CsmaChannel);

TypeId
CsmaChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::CsmaChannel")
    .SetParent<Channel> ()
    .SetGroupName ("Csma")
    .AddConstructor<CsmaChannel> ()
    .AddAttribute ("DataRate", 
                   "The transmission data rate to be provided to devices connected to the channel",
                   DataRateValue (DataRate (0xffffffff)),
                   MakeDataRateAccessor (&CsmaChannel::m_bps),
                   MakeDataRateChecker ())
    .AddAttribute ("Delay", "Transmission delay through the channel",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&CsmaChannel::m_delay),
                   MakeTimeChecker ())
    .AddAttribute ("FullDuplex", "Whether the channel is full duplex",
                   TypeId::ATTR_CONSTRUCT,
                   BooleanValue (false),
                   MakeBooleanAccessor (&CsmaChannel::m_fullDuplex),
                   MakeBooleanChecker ())
  ;
  return tid;
}

CsmaChannel::CsmaChannel ()
  :
    Channel ()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_deviceList.clear ();
}

CsmaChannel::~CsmaChannel ()
{
  NS_LOG_FUNCTION (this);
  m_deviceList.clear ();
}

int32_t
CsmaChannel::Attach (Ptr<CsmaNetDevice> device)
{
  NS_LOG_FUNCTION (this << device);
  NS_ASSERT (device != 0);

  // For full-duplex links we can only attach two devices to a channel
  // since there is no backoff
  if (m_fullDuplex && m_deviceList.size () > 2)
    {
      NS_LOG_DEBUG ("Falling back to half-duplex");
      m_fullDuplex = false;
    }

  CsmaDeviceRec rec (device);

  m_deviceList.push_back (rec);
  SetState (m_deviceList.size () - 1, IDLE);
  SetCurrentSrc (m_deviceList.size () - 1, m_deviceList.size () - 1);
  return (m_deviceList.size () - 1);
}

bool
CsmaChannel::Reattach (Ptr<CsmaNetDevice> device)
{
  NS_LOG_FUNCTION (this << device);
  NS_ASSERT (device != 0);

  std::vector<CsmaDeviceRec>::iterator it;
  for (it = m_deviceList.begin (); it < m_deviceList.end ( ); it++)
    {
      if (it->devicePtr == device) 
        {
          if (!it->active) 
            {
              it->active = true;
              return true;
            } 
          else 
            {
              return false;
            }
        }
    }
  return false;
}

bool
CsmaChannel::Reattach (uint32_t deviceId)
{
  NS_LOG_FUNCTION (this << deviceId);

  if (deviceId >= m_deviceList.size ())
    {
      return false;
    }

  if (m_deviceList[deviceId].active)
    {
      return false;
    } 
  else 
    {
      m_deviceList[deviceId].active = true;
      return true;
    }
}

bool
CsmaChannel::Detach (uint32_t deviceId)
{
  NS_LOG_FUNCTION (this << deviceId);

  if (deviceId < m_deviceList.size ())
    {
      if (!m_deviceList[deviceId].active)
        {
          NS_LOG_WARN ("CsmaChannel::Detach(): Device is already detached (" << deviceId << ")");
          return false;
        }

      m_deviceList[deviceId].active = false;

      if ((GetState (deviceId) == TRANSMITTING) && (GetCurrentSrc (deviceId) == deviceId))
        {
          NS_LOG_WARN ("CsmaChannel::Detach(): Device is currently" << "transmitting (" << deviceId << ")");
        }

      return true;
    } 
  else 
    {
      return false;
    }
}

bool
CsmaChannel::Detach (Ptr<CsmaNetDevice> device)
{
  NS_LOG_FUNCTION (this << device);
  NS_ASSERT (device != 0);

  std::vector<CsmaDeviceRec>::iterator it;
  for (it = m_deviceList.begin (); it < m_deviceList.end (); it++) 
    {
      if ((it->devicePtr == device) && (it->active)) 
        {
          it->active = false;
          return true;
        }
    }
  return false;
}

bool
CsmaChannel::TransmitStart (Ptr<const Packet> p, uint32_t srcId)
{
  NS_LOG_FUNCTION (this << p << srcId);
  NS_LOG_INFO ("UID is " << p->GetUid () << ")");

  if (GetState (srcId) != IDLE)
    {
      NS_LOG_WARN ("CsmaChannel::TransmitStart(): State is not IDLE");
      return false;
    }

  if (!IsActive (srcId))
    {
      NS_LOG_ERROR ("CsmaChannel::TransmitStart(): Seclected source is not currently attached to network");
      return false;
    }

  NS_LOG_LOGIC ("switch to TRANSMITTING");
  SetCurrentPkt (srcId, p);
  SetCurrentSrc (srcId, srcId);
  SetState (srcId, TRANSMITTING);
  return true;
}

bool
CsmaChannel::IsActive (uint32_t deviceId)
{
  return (m_deviceList[deviceId].active);
}

bool
CsmaChannel::IsFullDuplex (void) const
{
  return m_fullDuplex;
}

bool
CsmaChannel::TransmitEnd (uint32_t srcId)
{
  NS_LOG_FUNCTION (this << GetCurrentPkt (srcId) << GetCurrentSrc (srcId));
  NS_LOG_INFO ("UID is " << GetCurrentPkt (srcId)->GetUid () << ")");

  NS_ASSERT (GetState (srcId) == TRANSMITTING);
  SetState (srcId, PROPAGATING);

  bool retVal = true;

  if (!IsActive (GetCurrentSrc (srcId)))
    {
      NS_LOG_ERROR ("CsmaChannel::TransmitEnd(): Seclected source was detached before the end of the transmission");
      retVal = false;
    }

  NS_LOG_LOGIC ("Schedule event in " << m_delay.GetSeconds () << " sec");


  NS_LOG_LOGIC ("Receive");

  std::vector<CsmaDeviceRec>::iterator it;
  uint32_t devId = 0;
  for (it = m_deviceList.begin (); it < m_deviceList.end (); it++)
    {
      // In full duplex mode, don't deliver to sender
      if (!m_fullDuplex || (devId != GetCurrentSrc (srcId)))
        {
          if (it->IsActive ())
            {
              // schedule reception events
              Simulator::ScheduleWithContext (it->devicePtr->GetNode ()->GetId (),
                                              m_delay,
                                              &CsmaNetDevice::Receive, it->devicePtr,
                                              GetCurrentPkt (srcId)->Copy (), m_deviceList[GetCurrentSrc (srcId)].devicePtr);
            }
        }
      devId++;
    }

  // also schedule for the tx side to go back to IDLE
  Simulator::Schedule (IsFullDuplex () ? Time (0) : m_delay,
                       &CsmaChannel::PropagationCompleteEvent, this, srcId);
  return retVal;
}

void
CsmaChannel::PropagationCompleteEvent (uint32_t deviceId)
{
  NS_LOG_FUNCTION (this << GetCurrentPkt (deviceId));
  NS_LOG_INFO ("UID is " << GetCurrentPkt (deviceId)->GetUid () << ")");

  NS_ASSERT (GetState (deviceId) == PROPAGATING);
  SetState (deviceId, IDLE);
}

uint32_t
CsmaChannel::GetNumActDevices (void)
{
  int numActDevices = 0;
  std::vector<CsmaDeviceRec>::iterator it;
  for (it = m_deviceList.begin (); it < m_deviceList.end (); it++) 
    {
      if (it->active)
        {
          numActDevices++;
        }
    }
  return numActDevices;
}

std::size_t
CsmaChannel::GetNDevices (void) const
{
  return m_deviceList.size ();
}

Ptr<CsmaNetDevice>
CsmaChannel::GetCsmaDevice (std::size_t i) const
{
  return m_deviceList[i].devicePtr;
}

int32_t
CsmaChannel::GetDeviceNum (Ptr<CsmaNetDevice> device)
{
  std::vector<CsmaDeviceRec>::iterator it;
  int i = 0;
  for (it = m_deviceList.begin (); it < m_deviceList.end (); it++) 
    {
      if (it->devicePtr == device)
        {
          if (it->active) 
            {
              return i;
            } 
          else 
            {
              return -2;
            }
        }
      i++;
    }
  return -1;
}

bool
CsmaChannel::IsBusy (uint32_t deviceId)
{
  if (GetState (deviceId) == IDLE)
    {
      return false;
    } 
  else 
    {
      return true;
    }
}

DataRate
CsmaChannel::GetDataRate (void)
{
  return m_bps;
}

Time
CsmaChannel::GetDelay (void)
{
  return m_delay;
}

WireState
CsmaChannel::GetState (uint32_t deviceId)
{
  return m_state[m_fullDuplex ? deviceId : 0];
}

Ptr<NetDevice>
CsmaChannel::GetDevice (std::size_t i) const
{
  return GetCsmaDevice (i);
}

Ptr<Packet>
CsmaChannel::GetCurrentPkt (uint32_t deviceId)
{
  return m_currentPkt[m_fullDuplex ? deviceId : 0];
}

void
CsmaChannel::SetCurrentPkt (uint32_t deviceId, Ptr<const Packet> pkt)
{
  m_currentPkt[m_fullDuplex ? deviceId : 0] = pkt->Copy ();
}

uint32_t
CsmaChannel::GetCurrentSrc (uint32_t deviceId)
{
  return m_currentSrc[m_fullDuplex ? deviceId : 0];
}

void
CsmaChannel::SetCurrentSrc (uint32_t deviceId, uint32_t transmitterId)
{
  m_currentSrc[m_fullDuplex ? deviceId : 0] = transmitterId;
}

void
CsmaChannel::SetState (uint32_t deviceId, WireState state)
{
  m_state[m_fullDuplex ? deviceId : 0] = state;
}

CsmaDeviceRec::CsmaDeviceRec ()
{
  active = false;
}

CsmaDeviceRec::CsmaDeviceRec (Ptr<CsmaNetDevice> device)
{
  devicePtr = device; 
  active = true;
}

CsmaDeviceRec::CsmaDeviceRec (CsmaDeviceRec const &deviceRec)
{
  devicePtr = deviceRec.devicePtr;
  active = deviceRec.active;
}

bool
CsmaDeviceRec::IsActive () 
{
  return active;
}

} // namespace ns3
