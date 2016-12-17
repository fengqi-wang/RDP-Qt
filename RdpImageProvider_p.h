#ifndef RDPIMAGEPROVIDER_P_H
#define RDPIMAGEPROVIDER_P_H

#include <QObject>
#include <QRect>
#include <QTime>

class RdpImageProvider;

class RdpImageProviderPrivate : public QObject{
	Q_OBJECT
public:
	explicit RdpImageProviderPrivate(RdpImageProvider *q);

	void run();
	void end();
	void hide();
	void show();

	void handleMouseDown(int, int, bool);
	void handleMouseUp(int, int, bool);
	void handleMouseMoving(int, int);

	Q_DECLARE_PUBLIC(RdpImageProvider)
		RdpImageProvider* const q_ptr;
	bool isRunning;
	bool isHidden;

	void onDesktopUpdated(uchar*, int, int, int, int, int, int, int);
	void onDisconnected();

private:
	bool isConnected;
	QTime previousFrameTimestamp;
	bool firstFramePassed;
};

#endif //RDPIMAGEPROVIDER_P_H