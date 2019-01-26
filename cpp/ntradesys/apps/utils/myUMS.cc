
#include "Util/Socket.h"
#include "Util/StrUtil.h"
#include "Util/SMSByteOrder.h"
#include "c_util/Time.h"
using trc::compat::util::TimeVal;

#include "myUMS.h"

UMS::UMS (const char *_addr)
    : addr (StrCanon::get (_addr)), socket (new UDPSocket ()), index (0)
{
    // AddPort (ORDER_PORT_ALL);
}

bool UMS::Send (int strategy, int code, const char *msg1, const char *msg2)
{
    // clear out struct
    memset (&sms, 0, sizeof (sms));

    // fill in struct header
    sms.msgtype = USER_MESSAGE;
    sms.index = index++;
    TimeVal right_now (TimeVal::now);
    sms.time.tv_sec = right_now.sec ();
    sms.time.tv_usec = right_now.usec ();

    // fill in actual user message part of struct
    UserMessageStruct &ums = sms.msg.ums;
    ums.strategy = strategy;
    ums.code = code;
    strncpy (ums.msg1, msg1, 32);
    strncpy (ums.msg2, msg2, 32);

    // reverse bytes
    SMSByteOrder::makeHostIntoNet (&sms);

    bool res = true;
    for (port_set::const_iterator p = ports.begin ();
	 p != ports.end (); p++) {
	socket->SetPeer (GetInterface (), *p);
	socket->mcastSetTTL (McastDefTTL);
	res = socket->write (&sms, sizeof (sms)) == sizeof (sms)
	    && res;
    }
    return res;
}

void UMS::AddPort (int port)
{
    ports.push_back (port);
}

void UMS::ClearPorts ()
{
    ports.clear ();
}

void UMS::SetInterface (const char *_addr)
{
    addr = StrCanon::get (_addr);
}

/*
 * Local variables:
 * c-basic-offset: 4
 */
