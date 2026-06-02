#ifndef PIF_H_
#define PIF_H_

#include "common/Pcsx2Types.h"

namespace Pif
{
	extern int tx_count;
	extern int rx_count;

	void Initialize();
	void Poll();
	u32 FifoRx();
	void FifoTx(u32 val);

	void PacketComplete();

} // namespace Pif

#endif // PIF_H_
