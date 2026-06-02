#include "Pif.h"

#include "common/Console.h"

#include <netinet/in.h>
#include <queue>
#include <thread>
#include <array>
#include <unistd.h>
#include "IopDma.h"

namespace Pif
{
	enum class RcvState
	{
		Accept,
		Header,
		Data,
		Deliver,
	};

	struct DECI2_HDR
	{
		u16 len;
		u16 rsvd;
		u16 proto;
		u8 src;
		u8 dst;
	};


	static RcvState s_state = RcvState::Accept;
	static int s_sock = -1;
	static int s_msg = -1;

	static DECI2_HDR s_cur_hdr;

	static int s_rcvpos = 0;
	static std::array<u8, 0x40000> s_recvbuf;
	static int s_sendpos = 0;
	static std::array<u8, 0x40000> s_sendbuf;

	std::queue<u32> fifo;

	int rx_count;
	int tx_count;

	void Initialize()
	{
		struct sockaddr_in sin = {};

		s_cur_hdr = {};
		s_state = RcvState::Accept;
		s_rcvpos = 0;
		s_recvbuf.fill(0);
		s_sendbuf.fill(0);
		s_state = RcvState::Accept;
		fifo = {};

		rx_count = 0;
		tx_count = 0;

		if (s_sock >= 0)
		{
			close(s_sock);
			s_sock = -1;
		}

		if (s_msg >= 0)
		{
			close(s_msg);
			s_msg = -1;
		}

		Console.WriteLn("Pif: Init");

		sin.sin_family = AF_INET;
		sin.sin_port = htons(8888);
		sin.sin_addr.s_addr = 0;

		int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_IP);
		if (fd < 0)
		{
			Console.WriteLn("Pif: socket failed");
		}

		int hold = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&hold, sizeof(hold)) < 0)
		{
			Console.WriteLn("Pif: failed to set SO_REUSEADDR");
		}

		if (bind(fd, (const struct sockaddr*)&sin, 0x10u) < 0)
		{
			Console.WriteLn("Pif: failed to bind address");
		}

		if (listen(fd, 5) < 0)
		{
			Console.WriteLn("Pif: failed to listen");
			close(fd);
		}

		s_sock = fd;
	}

	u32 FifoRx()
	{
		u32 ret = fifo.front();
		fifo.pop();

		return ret;
	}

	void FifoTx(u32 val)
	{
		memcpy(&s_sendbuf[s_sendpos], &val, sizeof(val));
		s_sendpos += 4;
	}

	void PacketComplete()
	{
		DECI2_HDR* hdr = (DECI2_HDR*)s_sendbuf.data();

		write(s_msg, s_sendbuf.data(), hdr->len);
		s_sendbuf.fill(0);
		s_sendpos = 0;
		tx_count--;
	}

	bool RcvPacket(DECI2_HDR& hdr, void* data)
	{
		int count = (s_cur_hdr.len + 3) >> 2;

		for (int i = 0; i < count; i++)
		{
			u32 val;

			memcpy(&val, &s_recvbuf[i * 4], sizeof(val));
			fifo.push(val);
		}

		rx_count++;
		s_recvbuf.fill(0);
		return true;
	}

	bool Accept()
	{
		if (s_msg >= 0)
		{
			return true;
		}

		if ((s_msg = accept4(s_sock, 0, 0, SOCK_NONBLOCK)) >= 0)
		{
			Console.WriteLn("Pif: accepted connection");
			return true;
		}

		return false;
	}

	void Poll()
	{
		switch (s_state)
		{
			case RcvState::Accept:
				if (Accept())
				{
					s_state = RcvState::Header;
				}
				break;

			case RcvState::Header:
			{
				int ret = read(s_msg, &s_recvbuf.at(s_rcvpos), sizeof(DECI2_HDR));
				if (ret < 0)
				{
					break;
				}

				if (ret == 0)
				{
					s_msg = -1;
					s_rcvpos = 0;
					s_state = RcvState::Accept;
					break;
				}

				memcpy(&s_cur_hdr, &s_recvbuf.at(s_rcvpos), sizeof(s_cur_hdr));
				s_rcvpos += ret;

				s_state = RcvState::Data;
			}
			break;

			case RcvState::Data:
			{
				int ret = read(s_msg, &s_recvbuf.at(s_rcvpos), s_cur_hdr.len - sizeof(DECI2_HDR));
				if (ret < 0)
				{
					break;
				}

				if (ret == 0)
				{
					s_msg = -1;
					s_rcvpos = 0;
					s_state = RcvState::Accept;
					break;
				}

				s_rcvpos += ret;
				s_state = RcvState::Deliver;
			}
			break;

			case RcvState::Deliver:
				if (RcvPacket(s_cur_hdr, s_recvbuf.data()))
				{
					s_rcvpos = 0;
					s_state = RcvState::Header;
				}

				break;
		}


		if (rx_count)
			iopIntcIrq(23);
	}

} // namespace Pif
