#pragma once
#include <WinSock2.h>
#include <windows.h>
#include "asio.hpp"
#include <functional>
class MsgQueue
{
public:
	using Msg = std::function<void()>;

	~MsgQueue();
	void addMsg(Msg&& m);
	void run();

	// stop after all tasks executing
	void close();
	// stop and clear all tasks
	void stop();

private:
	asio::io_context mContext;
	asio::io_context::strand mStrand{mContext};
};