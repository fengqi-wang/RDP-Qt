#include "callback.h"
#include "RdpImageProvider_p.h"

void updateImage(void *client, void *buffer, int width, int height, int bpp, int dx, int dy, int dw, int dh)
{
	static_cast<RdpImageProviderPrivate*>(client)->onDesktopUpdated((uchar*)buffer, width, height, bpp, dx, dy, dw, dh);
}

void endSession(void *client)
{
	static_cast<RdpImageProviderPrivate*>(client)->onDisconnected();
}
