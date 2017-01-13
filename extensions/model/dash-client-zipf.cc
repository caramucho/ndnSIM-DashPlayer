
#include "dash-client-zipf.h"

#include <math.h>

NS_LOG_COMPONENT_DEFINE("ndn.DashClientZipf");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(DashClientZipf);

TypeId
DashClientZipf::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::DashClientZipf")
      .SetGroupName("Ndn")
      .SetParent<DashClient>()
      .AddConstructor<DashClientZipf>()

      .AddAttribute("NumberOfContents", "Number of the Contents in total", StringValue("100"),
                    MakeUintegerAccessor(&DashClientZipf::SetNumberOfContents,
                                         &DashClientZipf::GetNumberOfContents),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("q", "parameter of improve rank", StringValue("0.7"),
                    MakeDoubleAccessor(&DashClientZipf::SetQ,
                                       &DashClientZipf::GetQ),
                    MakeDoubleChecker<double>())

      .AddAttribute("s", "parameter of power", StringValue("0.7"),
                    MakeDoubleAccessor(&DashClientZipf::SetS,
                                       &DashClientZipf::GetS),
                    MakeDoubleChecker<double>());

  return tid;
}


DashClientZipf::DashClientZipf()
  : m_N(100) // needed here to make sure when SetQ/SetS are called, there is a valid value of N
  , m_q(0.7)
  , m_s(0.7)
  , m_seqRng(CreateObject<UniformRandomVariable>())
{
  // SetNumberOfContents is called by NS-3 object system during the initialization
}

DashClientZipf::~DashClientZipf()
{
}

void
DashClientZipf::SetNumberOfContents(uint32_t numOfContents)
{
  m_N = numOfContents;

  NS_LOG_DEBUG(m_q << " and " << m_s << " and " << m_N);

  m_Pcum = std::vector<double>(m_N + 1);

  m_Pcum[0] = 0.0;
  for (uint32_t i = 1; i <= m_N; i++) {
    m_Pcum[i] = m_Pcum[i - 1] + 1.0 / std::pow(i + m_q, m_s);
  }

  for (uint32_t i = 1; i <= m_N; i++) {
    m_Pcum[i] = m_Pcum[i] / m_Pcum[m_N];
    NS_LOG_LOGIC("Cumulative probability [" << i << "]=" << m_Pcum[i]);
  }
}


uint32_t
DashClientZipf::GetNumberOfContents() const
{
  return m_N;
}

void
DashClientZipf::SetQ(double q)
{
  m_q = q;
  SetNumberOfContents(m_N);
}

double
DashClientZipf::GetQ() const
{
  return m_q;
}

void
DashClientZipf::SetS(double s)
{
  m_s = s;
  SetNumberOfContents(m_N);
}

double
DashClientZipf::GetS() const
{
  return m_s;
}


uint32_t
DashClientZipf::GetNextContentId()
{
  uint32_t content_index = 1; //[1, m_N]
  double p_sum = 0;

  double p_random = m_seqRng->GetValue();
  while (p_random == 0) {
    p_random = m_seqRng->GetValue();
  }
  // if (p_random == 0)
  NS_LOG_LOGIC("p_random=" << p_random);
  for (uint32_t i = 1; i <= m_N; i++) {
    p_sum = m_Pcum[i]; // m_Pcum[i] = m_Pcum[i-1] + p[i], p[0] = 0;   e.g.: p_cum[1] = p[1],
                       // p_cum[2] = p[1] + p[2]
    if (p_random <= p_sum) {
      content_index = i;
      break;
    } // if
  }   // for
  // content_index = 1;
  NS_LOG_DEBUG("RandomNumber=" << content_index);
  return content_index;
}

} // namespace ndn
} // namespace ns3
