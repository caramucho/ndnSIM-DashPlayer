/*
 * aaash-client.h
 *
 *  Created on: Jun 16, 2014
 *      Author: dimitriv
 */

#ifndef AAASH_CLIENT_H_
#define AAASH_CLIENT_H_

#include "../dash-client-zipf.h"
namespace ns3
{
  namespace ndn{

  class AaashClient : public DashClientZipf
  {
  public:
    static TypeId
    GetTypeId(void);

    AaashClient();

    virtual
    ~AaashClient();

    virtual void
    CalcNextSegment(uint32_t currRate, uint32_t & nextRate, Time & delay);

  private:
    bool
    BufferInc();

    bool m_running_fast_start;
  };
}
} /* namespace ns3 */

#endif /* AAASH_CLIENT_H_ */
