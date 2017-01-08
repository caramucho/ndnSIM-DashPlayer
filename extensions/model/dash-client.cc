/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Copyright (c) 2014 TEI of Western Macedonia, Greece
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
* Author: Dimitrios J. Vergados <djvergad@gmail.com>
*/

// #include <ns3/log.h>
// #include <ns3/uinteger.h>
// #include <ns3/tcp-socket-factory.h>
// #include <ns3/simulator.h>
// #include <ns3/inet-socket-address.h>
// #include <ns3/inet6-socket-address.h>
// #include "http-header.h"

#include "dash-client.h"



NS_LOG_COMPONENT_DEFINE("DashClient");

namespace ns3{
  namespace ndn{


    NS_OBJECT_ENSURE_REGISTERED(DashClient);

    int DashClient::m_countObjs = 0;

    TypeId
    DashClient::GetTypeId(void)
    {
      static TypeId tid =
      TypeId("ns3::DashClient")
      .SetGroupName("Ndn")
      .SetParent<Consumer>()
      .AddConstructor<DashClient>()
      .AddAttribute("VideoId","The Id of the video that is played.", UintegerValue(0),MakeUintegerAccessor(&DashClient::m_videoId),MakeUintegerChecker<uint32_t>(1))
      .AddAttribute("TargetDt", "The target buffering time", TimeValue(Time("35s")),MakeTimeAccessor(&DashClient::m_target_dt), MakeTimeChecker())
      .AddAttribute("window", "The window for measuring the average throughput (Time)",TimeValue(Time("10s")), MakeTimeAccessor(&DashClient::m_window),MakeTimeChecker())
      .AddTraceSource("Tx", "A new packet is created and is sent",MakeTraceSourceAccessor(&DashClient::m_txTrace));
      return tid;
    }

    DashClient::DashClient() :
    m_rateChanges(0), m_firstTime(true), m_payloadSize(8192), m_target_dt("35s"), m_bitrateEstimate(0.0), m_segmentId(0),  m_totBytes(0), m_startedReceiving(Seconds(0)), m_sumDt(Seconds(0)), m_lastDt(Seconds(-1)),m_id(m_countObjs++), m_requestTime("0s"), m_segment_bytes(0), m_bitRate(45000), m_window(Seconds(10)), m_segmentFetchTime(Seconds(0))
    {
      NS_LOG_FUNCTION(this);
      // m_parser.SetApp(this); // So the parser knows where to send the received messages
    }

    DashClient::~DashClient()
    {
      NS_LOG_FUNCTION(this);
    }

    // Private helpers
    // Request Next Segment
    void
    DashClient::RequestSegment()
    {
      // if (m_seq > m_seqMax + 1) {
      //   return; // we are totally done
      // }
      m_seq = 0;
      CalcSegMax();
      m_requestTime = Simulator::Now(); // the time to request the first packet of the segment
      m_segment_bytes = 0;
      SetInterestName();
      m_firstTime = true;
      ScheduleNextPacket();
    }
    // void
    // DashClient::RequestSegment()
    // {
    // NS_LOG_FUNCTION(this);
    //
    // if (m_connected == false)
    //   {
    //     return;
    //   }
    //
    // Ptr<Packet> packet = Create<Packet>(100);
    //
    // HTTPHeader httpHeader;
    // httpHeader.SetSeq(1);
    // httpHeader.SetMessageType(HTTP_REQUEST);
    // httpHeader.SetVideoId(m_videoId);
    // httpHeader.SetResolution(m_bitRate);
    // httpHeader.SetSegmentId(m_segmentId++);
    // packet->AddHeader(httpHeader);
    //
    // int res = 0;
    // if (((unsigned) (res = m_socket->Send(packet))) != packet->GetSize())
    //   {
    //     NS_FATAL_ERROR(
    //         "Oh oh. Couldn't send packet! res=" << res << " size=" << packet->GetSize());
    //   }
    //
    // m_requestTime = Simulator::Now();
    // m_segment_bytes = 0;
    // }

    void
    DashClient::SetInterestName()
    {
      m_interestName = DashName();
      m_interestName.SetProducerDomain(m_producerDomain);
      m_interestName.SetVideoId(m_videoId);
      m_interestName.SetRrepresentation(m_bitRate);
      m_interestName.SetAdaptationSet(m_adaptationSetId);
      m_interestName.SetPeriod(m_periodId);
      m_interestName.SetSegmentId(m_segmentId++);
      m_interestName.Update();
    }

    void
    DashClient::SendPacket()
    {
      if (!m_active)
      return;

      NS_LOG_FUNCTION_NOARGS();
      //

      uint32_t seq = m_seq;
      m_seq++;

      shared_ptr<Name> nameWithSequence = make_shared<Name>(m_interestName);
      nameWithSequence->appendSequenceNumber(seq);
      //

      shared_ptr<Interest> interest = make_shared<Interest>();
      interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
      interest->setName(*nameWithSequence);
      time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
      interest->setInterestLifetime(interestLifeTime);

      // NS_LOG_INFO ("Requesting Interest: \n" << *interest);
      NS_LOG_INFO("> Interest for " << seq);

      WillSendOutInterest(seq);

      m_transmittedInterests(interest, this, m_face);
      // send out interest
      m_appLink->onReceiveInterest(*interest);

      ScheduleNextPacket();
    }

    void
    DashClient::ScheduleNextPacket()
    {
      // cout << "ScheduleNextPacket initilizing" << endl;
      double mean = 8.0 * m_payloadSize / m_bitRate;
      // std::cout << "next: " << Simulator::Now().ToDouble(Time::S) + mean << "s\n";

      if (m_firstTime) {
        m_sendEvent = Simulator::Schedule(Seconds(0.0), &SendPacket, this);
        m_firstTime = false;
      }
      else if (!m_sendEvent.IsRunning())

      m_sendEvent = Simulator::Schedule(Seconds(mean), &SendPacket, this);
    }

    void
    DashClient::OnData(shared_ptr<const Data> data)
    {

      // if (!m_active)
      // return;
      Consumer::OnData(data);
      DashName dataname = data->getName();
      uint32_t seq = data->getName().at(-1).toSequenceNumber();
      // std::cout << "seq num " +  to_string(seq) + " received"  << std::endl;
      // std::cout <<  "data: " +  dataname.getPrefix(6).toUri() + " received"  << std::endl;
      // m_ndnParser = ;
      m_player.ReceiveFrame(&data);
      m_segment_bytes += m_payloadSize;
      m_totBytes += m_payloadSize;

      // Calculate the buffering time
      switch (m_player.m_state)
      {
      case MPEG_PLAYER_PLAYING:
        m_sumDt += m_player.GetRealPlayTime(mpegHeader.GetPlaybackTime());
        break;
      case MPEG_PLAYER_PAUSED:
        break;
      case MPEG_PLAYER_DONE:
        return;
      default:
        NS_FATAL_ERROR("WRONG STATE");
      }

      // If we received the last packet of the segment
      if (seq == m_seqMax)
      {
        m_segmentFetchTime = Simulator::Now() - m_requestTime;

        NS_LOG_INFO(
            Simulator::Now().GetSeconds() << " bytes: " << m_segment_bytes << " segmentTime: " << m_segmentFetchTime.GetSeconds() << " segmentRate: " << 8 * m_segment_bytes / m_segmentFetchTime.GetSeconds());

        // Feed the bitrate info to the player
        AddBitRate(Simulator::Now(),
            8 * m_segment_bytes / m_segmentFetchTime.GetSeconds());

        Time currDt = m_player.GetRealPlayTime(mpegHeader.GetPlaybackTime());
        // And tell the player to monitor the buffer level
        LogBufferLevel(currDt);

        uint32_t old = m_bitRate;
        //  double diff = m_lastDt >= 0 ? (currDt - m_lastDt).GetSeconds() : 0;

        Time bufferDelay;

        //m_player.CalcNextSegment(m_bitRate, m_player.GetBufferEstimate(), diff,
        //m_bitRate, bufferDelay);

        uint32_t prevBitrate = m_bitRate;

        CalcNextSegment(prevBitrate, m_bitRate, bufferDelay);
        // @Todo calculate next m_seqMax

        if (prevBitrate != m_bitRate)
         {
           m_rateChanges++;
         }

      //  if (bufferDelay == Seconds(0))
      //    {
      //      RequestSegment();
      //    }
       else
         {
           m_player.SchduleBufferWakeup(bufferDelay, this);
         }

         m_lastDt = currDt;
         m_firstTime = true;
        //  ScheduleNextPacket();

        RequestSegment();
      }

    }

    // void
    // DashClient::MessageReceived(Packet message)
    // {
    //   NS_LOG_FUNCTION(this << message);
    //
    //   MPEGHeader mpegHeader;
    //   HTTPHeader httpHeader;
    //
    //   // Send the frame to the player
    //   m_player.ReceiveFrame(&message);
    //   m_segment_bytes += message.GetSize();
    //   m_totBytes += message.GetSize();
    //
    //   message.RemoveHeader(mpegHeader);
    //   message.RemoveHeader(httpHeader);
    //
    //   // Calculate the buffering time
    //   switch (m_player.m_state)
    //     {
    //   case MPEG_PLAYER_PLAYING:
    //     m_sumDt += m_player.GetRealPlayTime(mpegHeader.GetPlaybackTime());
    //     break;
    //   case MPEG_PLAYER_PAUSED:
    //     break;
    //   case MPEG_PLAYER_DONE:
    //     return;
    //   default:
    //     NS_FATAL_ERROR("WRONG STATE");
    //     }
    //
    //   // If we received the last frame of the segment
    //   if (mpegHeader.GetFrameId() == MPEG_FRAMES_PER_SEGMENT - 1)
    //     {
    //       m_segmentFetchTime = Simulator::Now() - m_requestTime;
    //
    //       NS_LOG_INFO(
    //           Simulator::Now().GetSeconds() << " bytes: " << m_segment_bytes << " segmentTime: " << m_segmentFetchTime.GetSeconds() << " segmentRate: " << 8 * m_segment_bytes / m_segmentFetchTime.GetSeconds());
    //
    //       // Feed the bitrate info to the player
    //       AddBitRate(Simulator::Now(),
    //           8 * m_segment_bytes / m_segmentFetchTime.GetSeconds());
    //
    //       Time currDt = m_player.GetRealPlayTime(mpegHeader.GetPlaybackTime());
    //       // And tell the player to monitor the buffer level
    //       LogBufferLevel(currDt);
    //
    //       uint32_t old = m_bitRate;
    //       //  double diff = m_lastDt >= 0 ? (currDt - m_lastDt).GetSeconds() : 0;
    //
    //       Time bufferDelay;
    //
    //       //m_player.CalcNextSegment(m_bitRate, m_player.GetBufferEstimate(), diff,
    //       //m_bitRate, bufferDelay);
    //
    //       uint32_t prevBitrate = m_bitRate;
    //
    //       CalcNextSegment(prevBitrate, m_bitRate, bufferDelay);
    //
    //       if (prevBitrate != m_bitRate)
    //         {
    //           m_rateChanges++;
    //         }
    //
    //       if (bufferDelay == Seconds(0))
    //         {
    //           RequestSegment();
    //         }
    //       else
    //         {
    //           m_player.SchduleBufferWakeup(bufferDelay, this);
    //         }
    //
    //       std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id
    //           << " newBitRate: " << m_bitRate << " oldBitRate: " << old
    //           << " estBitRate: " << GetBitRateEstimate() << " interTime: "
    //           << m_player.m_interruption_time.GetSeconds() << " T: "
    //           << currDt.GetSeconds() << " dT: "
    //           << (m_lastDt >= 0 ? (currDt - m_lastDt).GetSeconds() : 0)
    //           << " del: " << bufferDelay << std::endl;
    //
    //       NS_LOG_INFO(
    //           "==== Last frame received. Requesting segment " << m_segmentId);
    //
    //       (void) old;
    //       NS_LOG_INFO(
    //           "!@#$#@!$@#\t" << Simulator::Now().GetSeconds() << " old: " << old << " new: " << m_bitRate << " t: " << currDt.GetSeconds() << " dt: " << (currDt - m_lastDt).GetSeconds());
    //
    //       m_lastDt = currDt;
    //
    //     }
    //
    // }

    void
    DashClient::CalcNextSegment(uint32_t currRate, uint32_t & nextRate,Time & delay)
    {
      nextRate = currRate;
      delay = Seconds(0);
    }

    void
    DashClient::GetStats()
    {
      std::cout << " InterruptionTime: "
      << m_player.m_interruption_time.GetSeconds() << " interruptions: "
      << m_player.m_interrruptions << " avgRate: "
      << (1.0 * m_player.m_t  void
    CalcSegMax();otalRate) / m_player.m_framesPlayed
      << " minRate: " << m_player.m_minRate << " AvgDt: "
      << m_sumDt.GetSeconds() / m_player.m_framesPlayed << " changes: "
      << m_rateChanges << std::endl;

    }


    void
    DashClient::LogBufferLevel(Time t)
    {
      m_bufferState[Simulator::Now()] = t;
      for (std::map<Time, Time>::iterator it = m_bufferState.begin();
      it != m_bufferState.end(); ++it)
      {
        if (it->first < (Simulator::Now() - m_window))
        {
          m_bufferState.erase(it->first);
        }
      }
    }

    double
    DashClient::GetBufferEstimate()
    {
      double sum = 0;
      int count = 0;
      for (std::map<Time, Time>::iterator it = m_bufferState.begin();
      it != m_bufferState.end(); ++it)
      {
        sum += it->second.GetSeconds();
        count++;
      }
      return sum / count;
    }

    double
    DashClient::GetBufferDifferential()
    {
      std::map<Time, Time>::iterator it = m_bufferState.end();

      if (it == m_bufferState.begin())
      {
        // Empty buffer
        return 0;
      }
      it--;
      Time last = it->second;

      if (it == m_bufferState.begin())
      {
        // Only one element
        return 0;
      }
      it--;
      Time prev = it->second;
      return (last - prev).GetSeconds();
    }

    double
    DashClient::GetSegmentFetchTime()
    {
      return m_segmentFetchTime.GetSeconds();
    }

    void
    DashClient::AddBitRate(Time time, double bitrate)
    {
      m_bitrates[time] = bitrate;
      double sum = 0;
      int count = 0;
      for (std::map<Time, double>::iterator it = m_bitrates.begin();
      it != m_bitrates.end(); ++it)
      {
        if (it->first < (Simulator::Now() - m_window))
        {
          m_bitrates.erase(it->first);
        }
        else
        {
          sum += it->second;
          count++;
        }
      }
      m_bitrateEstimate = sum / count;
    }

    void
    DashClient::CalcSegMax(){
      m_seqMax =  8 * m_bitRate * m_segmentLength.GetSeconds()  / m_payloadSize;
    }

  } // Namespace ndn
} // Namespace ns3