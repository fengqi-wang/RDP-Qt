#ifndef RDPIMAGEPROVIDER_H
#define RDPIMAGEPROVIDER_H

#include <QObject>

class RdpImageProviderPrivate;

class RdpImageProvider : public QObject
{
	Q_OBJECT
public:
	static RdpImageProvider *instance();
	~RdpImageProvider();

	void start();
	void hide();
	void stop();
	void onStop();

	public slots:
	void onMousePressed(int x, int y, bool leftButton = true);
	void onMouseReleased(int x, int y, bool leftButton = true);
	void onMouseMoving(int x, int y);

signals:
	void imageReady();
	void imageReadyLater();

private:
	RdpImageProvider();

	Q_DECLARE_PRIVATE(RdpImageProvider)
		RdpImageProviderPrivate* const d_ptr;

	static RdpImageProvider *gInstance;
};

#endif