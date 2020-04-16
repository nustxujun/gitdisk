#include "MsgQueue.h"

MsgQueue::~MsgQueue()
{
	close();
}

void MsgQueue::addMsg(Msg&& m)
{
	mStrand.post(m);
}

void MsgQueue::close()
{
	addMsg([this]() {
		stop();
	});
}

void MsgQueue::stop()
{
	mContext.stop();
}

void MsgQueue::run()
{
	asio::io_context::work work(mContext);
	mContext.run();
}
