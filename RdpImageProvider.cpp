#include "RdpImageProvider.h"
#include "RdpImageProvider_p.h"
#include "RdpPaintedItem.h"
#include "FrameBuffer.h"

#include <QDebug>
#include <QImage>
#include <QPainter>
#include <QThread>

extern "C" {
#include "android_freerdp.h"
}

RdpImageProviderPrivate::RdpImageProviderPrivate(RdpImageProvider *q)
	: q_ptr(q)
{
	isRunning = false;
	isHidden = false;
	isConnected = false;
	firstFramePassed = false;
	previousFrameTimestamp.restart();
	FrameBuffer::put(FrameBuffer::STOPPED);
}

void RdpImageProviderPrivate::run()
{
	isRunning = true;
	isHidden = false;

	FrameBuffer::put(FrameBuffer::WAITING);
	Q_Q(RdpImageProvider);
	emit q->imageReady();

    firstFramePassed = false;
	poly_rdp_start(this);

	previousFrameTimestamp.restart();
}

void RdpImageProviderPrivate::end()
{
	poly_rdp_stop();
}

void RdpImageProviderPrivate::hide()
{
	isHidden = true;
}

void RdpImageProviderPrivate::show()
{
	Q_Q(RdpImageProvider);
	emit q->imageReady();
	
	isHidden = false;
}


void RdpImageProviderPrivate::onDisconnected()
{
	//rdpLog(UtilLogLevelEvent2, "RdpImageProviderPrivate::onDisconnected");

	Q_Q(RdpImageProvider);
	q->onStop();

	isRunning = false;
	isHidden = false;

}

namespace
{
	// Aim for about 4 fps maximum to avoid CPU issues
	const int MIN_FRAME_PERIOD_MS = 250;
}

void RdpImageProviderPrivate::onDesktopUpdated(uchar *buffer, int width, int height, int bpp, int dx, int dy, int dw, int dh)
{
	Q_Q(RdpImageProvider);
	FrameBuffer::put(buffer, width, height, bpp, dx, dy, dw, dh);

	int timeSincePrevFrameMs = previousFrameTimestamp.elapsed();
	if (firstFramePassed && timeSincePrevFrameMs < MIN_FRAME_PERIOD_MS)
	{
		emit q->imageReadyLater();
		return;
	}

    firstFramePassed = true;

	if (! isHidden)
	{
		emit q->imageReady();
	}

	previousFrameTimestamp.restart();
}

int PTRFLAGS_LBUTTON = 0x1000;
int PTRFLAGS_RBUTTON = 0x2000;
int PTRFLAGS_DOWN = 0x8000;
int PTRFLAGS_MOVE = 0x0800;

void RdpImageProviderPrivate::handleMouseDown(int x, int y, bool leftButton)
{
	int flag = leftButton ? PTRFLAGS_LBUTTON | PTRFLAGS_DOWN : PTRFLAGS_RBUTTON | PTRFLAGS_DOWN;
	// Account for any cropping done when sharing an application window rather than an entire desktop.
	QRect cropRect = FrameBuffer::getCropRect();
	if (!cropRect.isNull())
	{
		x += cropRect.x();
		y += cropRect.y();
	}
	poly_rdp_send_cursor_event(x, y, flag);
}

void RdpImageProviderPrivate::handleMouseUp(int x, int y, bool leftButton)
{
	int flag = leftButton ? PTRFLAGS_LBUTTON : PTRFLAGS_RBUTTON;
	// Account for any cropping done when sharing an application window rather than an entire desktop.
	QRect cropRect = FrameBuffer::getCropRect();
	if (!cropRect.isNull())
	{
		x += cropRect.x();
		y += cropRect.y();
	}
	poly_rdp_send_cursor_event(x, y, flag);
}

void RdpImageProviderPrivate::handleMouseMoving(int x, int y)
{
	int flag = PTRFLAGS_MOVE;
	// Account for any cropping done when sharing an application window rather than an entire desktop.
	QRect cropRect = FrameBuffer::getCropRect();
	if (!cropRect.isNull())
	{
		x += cropRect.x();
		y += cropRect.y();
	}
	poly_rdp_send_cursor_event(x, y, flag);
}

typedef RdpImageProviderPrivate Pimpl;

RdpImageProvider * RdpImageProvider::gInstance = NULL;

RdpImageProvider * RdpImageProvider::instance()
{
	if (NULL == gInstance)
	{
		//rdpLog(UtilLogLevelEvent2, "POLY create RdpImageProvider instance");
		poly_rdp_module_init();
		gInstance = new RdpImageProvider();

	}
	return gInstance;
}

RdpImageProvider::RdpImageProvider()
	: d_ptr(new RdpImageProviderPrivate(this))
{
}

RdpImageProvider::~RdpImageProvider()
{
	rdpLog(UtilLogLevelEvent2, "POLY destroying RdpImageProvider");

	stop();

	delete d_ptr;
}

void RdpImageProvider::start()
{
	Q_D(RdpImageProvider);

	if (!(d->isRunning))
	{
		rdpLog(UtilLogLevelEvent2, "Starting RDP Session");
		d->run();
	}
	else
	{
		rdpLog(UtilLogLevelMinorError, "RDP Session is still living");
		
		if (d->isHidden)
		{
			d->show();
		}
	}
}

void RdpImageProvider::hide()
{
	Q_D(RdpImageProvider);

	if (!(d->isHidden))
	{
		rdpLog(UtilLogLevelEvent2, "Hiding RDP Session");
		d->hide();		
	}
	else
	{
		rdpLog(UtilLogLevelMinorError, "RDP Session is already hidden");
	}
}

void RdpImageProvider::stop()
{
	Q_D(RdpImageProvider);

	if (d->isRunning)
	{
		rdpLog(UtilLogLevelEvent2, "Stopping RDP Session");
		d->end();
	}
	else
	{
		rdpLog(UtilLogLevelMinorError, "No living RDP session");
	}
}

void RdpImageProvider::onStop()
{
	rdpLog(UtilLogLevelEvent2, "RDP Session Ended");
}

void RdpImageProvider::onMousePressed(int x, int y, bool leftButton)
{
	Q_D(RdpImageProvider);
	d->handleMouseDown(x, y, leftButton);
}

void RdpImageProvider::onMouseReleased(int x, int y, bool leftButton)
{
	Q_D(RdpImageProvider);
	d->handleMouseUp(x, y, leftButton);
}

void RdpImageProvider::onMouseMoving(int x, int y)
{
	Q_D(RdpImageProvider);
	d->handleMouseMoving(x, y);
}

