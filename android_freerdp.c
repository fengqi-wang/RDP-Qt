/*
   Android poly Client Layer

   Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
   Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
   */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/select.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/channels/channels.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/utils/event.h>
#include <freerdp/constants.h>
#include <freerdp/locale/keyboard.h>

#include "android_freerdp.h"
#include "android_plcmlog.h"
#include "android_cliprdr.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <linux/tcp.h>
#include <sys/ioctl.h>

#include <semaphore.h>

extern void updateImage(void*, void*, int, int, int, int, int, int, int);
extern void endSession(void*);

void *g_Client = 0;
int g_Instance = 0;

//Polycom
//POLYCOM end

int android_context_new(freerdp* instance, rdpContext* context)
{
	rdpLog(UtilLogLevelDebug, "RDP:android_context_new - Create channels");

	context->channels = freerdp_channels_new();
	android_event_queue_init(instance);
	return 0;
}

void android_context_free(freerdp* instance, rdpContext* context)
{
	rdpLog(UtilLogLevelDebug, "RDP:android_context_free");
	freerdp_channels_free(context->channels);
	android_event_queue_uninit(instance);
}


void android_begin_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
	gdi->primary->hdc->hwnd->invalid->null = 1;
	gdi->primary->hdc->hwnd->ninvalid = 0;
}

void android_end_paint(rdpContext* context)
{
	rdpGdi *gdi = context->gdi;
	if (gdi->primary->hdc->hwnd->invalid->null)
		return;

	int x = gdi->primary->hdc->hwnd->invalid->x;
	int y = gdi->primary->hdc->hwnd->invalid->y;
	int w = gdi->primary->hdc->hwnd->invalid->w;
	int h = gdi->primary->hdc->hwnd->invalid->h;
	// rdpLog(UtilLogLevelDebug, "RDP:android_end_paint : x:%d y:%d w:%d h:%d", x, y, w, h);

	updateImage(g_Client, gdi->primary_buffer, gdi->width, gdi->height, gdi->bytesPerPixel * 8, x, y, w, h);
}

void android_desktop_resize(rdpContext* context)
{
	rdpSettings *settings;
	rdpGdi *gdi = context->gdi;

	settings = context->instance->settings;

	if ((gdi->width != settings->DesktopWidth) || (gdi->height != settings->DesktopHeight))
		gdi_resize(gdi, settings->DesktopWidth, settings->DesktopHeight);

	rdpLog(UtilLogLevelEvent2, "RDP:ui_desktop_resize width:%d %d height:%d %d",
			gdi->width, settings->DesktopWidth, gdi->height, settings->DesktopHeight);
	//freerdp_callback("OnGraphicsResize", "(IIII)V", context->instance, settings->DesktopWidth, settings->DesktopHeight, gdi->dstBpp);
}

BOOL android_pre_connect(freerdp* instance)
{
	rdpSettings* settings = instance->settings;
	BOOL bitmap_cache = settings->BitmapCacheEnabled;
	//rdpLog(UtilLogLevelDebug, "RDP:android_pre_connect, settings->OrderSupport= %x", settings->OrderSupport);
	settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_PATBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = FALSE;
	settings->OrderSupport[NEG_LINETO_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEMBLT_INDEX] = bitmap_cache;
	settings->OrderSupport[NEG_MEM3BLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEMBLT_V2_INDEX] = bitmap_cache;
	settings->OrderSupport[NEG_MEM3BLT_V2_INDEX] = FALSE;
	settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = FALSE;
	settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYGON_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_POLYGON_CB_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = FALSE;

	settings->FrameAcknowledge = 10;

	freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0);
	freerdp_client_load_addins(instance->context->channels, instance->settings);

	freerdp_channels_pre_connect(instance->context->channels, instance);

	rdpLog(UtilLogLevelDebug, "RDP: Done android_pre_connect");
	return TRUE;
}

BOOL android_post_connect(freerdp* instance)
{
	rdpLog(UtilLogLevelDebug, "RDP:android_post_connect");

	//freerdp_callback("OnSettingsChanged", "(IIII)V", instance, instance->settings->DesktopWidth, instance->settings->DesktopHeight, instance->settings->ColorDepth);

	instance->context->cache = cache_new(instance->settings);

	gdi_init(instance, CLRCONV_ALPHA | ((instance->settings->ColorDepth > 16) ? CLRBUF_32BPP : CLRBUF_16BPP), NULL);

	instance->update->BeginPaint = android_begin_paint;
	instance->update->EndPaint = android_end_paint;
	instance->update->DesktopResize = android_desktop_resize;

	android_cliprdr_init(instance);

	freerdp_channels_post_connect(instance->context->channels, instance);

	return TRUE;
}

BOOL android_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
	return TRUE;
}

BOOL android_verify_certificate(freerdp* instance, char* subject, char* issuer, char* fingerprint)
{
	return TRUE;
}

BOOL android_verify_changed_certificate(freerdp* instance, char* subject, char* issuer, char* new_fingerprint, char* old_fingerprint)
{
	return android_verify_certificate(instance, subject, issuer, new_fingerprint);
}

int android_receive_channel_data(freerdp* instance, int channelId, UINT8* data, int size, int flags, int total_size)
{
	return freerdp_channels_data(instance, channelId, data, size, flags, total_size);
}

void android_process_channel_event(rdpChannels* channels, freerdp* instance)
{
	wMessage* event;

	event = freerdp_channels_pop_event(channels);

	if (event)
	{
		switch (GetMessageClass(event->id))
		{
		case CliprdrChannel_Class:
			android_process_cliprdr_event(instance, event);
			break;

		default:
			break;
		}

		freerdp_event_free(event);
	}
}

int android_freerdp_run(freerdp* instance)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
	int wcount;
	void* rfds[32];
	void* wfds[32];
	fd_set rfds_set;
	fd_set wfds_set;
	int select_status;
	struct timeval timeout;

	rdpSettings * settings = instance->settings;

	rdpLog(UtilLogLevelEvent2, "RDP:android_freerdp_run - Start %s", settings->ServerHostname);

	memset(rfds, 0, sizeof(rfds));
	memset(wfds, 0, sizeof(wfds));

	//Polycom - Open server socket and wait for codec engine to connect
	while (rdpRunning && (plcmServSock = plcmOpenTunnelSocket(44444)) == 0)
	{
		Sleep(100);
	}

	//Wait for Codec engine to say we are connected
	while (rdpRunning && (plcmCheckTunnel(plcmServSock, &plcmSock) == 0))
	{
		Sleep(100);
	}

	if (!rdpRunning)
	{
		return 0;
	}

	settings->ServerPort = plcmSock;
	rdpLog(UtilLogLevelEvent1, "RDP:android_freerdp_run - Tunnel connected sock %d", plcmSock);

	if (!freerdp_connect(instance))
	{
		//freerdp_callback("OnConnectionFailure", "(I)V", instance);
		rdpLog(UtilLogLevelMinorError, "RDP:android_freerdp_run - Failed to Connect");
		return 0;
	}
	rdpLog(UtilLogLevelEvent1, "RDP:android_freerdp_run - Is Connected");
	((androidContext*)instance->context)->is_connected = TRUE;

	while (rdpRunning && !freerdp_shall_disconnect(instance))
	{
		rcount = 0;
		wcount = 0;

		if (freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			rdpLog(UtilLogLevelMinorError, "RDP:android_freerdp_run - Failed to get FreeRDP file descriptor");
			break;
		}
		if (freerdp_channels_get_fds(instance->context->channels, instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			rdpLog(UtilLogLevelMinorError, "RDP:android_freerdp_run - Failed to get channel manager file descriptor");
			break;
		}
		if (android_get_fds(instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			rdpLog(UtilLogLevelMinorError, "RDP:android_freerdp_run - Failed to get android file descriptor");
			break;
		}

		max_fds = 0;
		FD_ZERO(&rfds_set);
		FD_ZERO(&wfds_set);

		for (i = 0; i < rcount; i++)
		{
			fds = (int)(long)(rfds[i]);

			if (fds > max_fds)
				max_fds = fds;

			FD_SET(fds, &rfds_set);
		}

		if (max_fds == 0)
			break;

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		select_status = select(max_fds + 1, &rfds_set, NULL, NULL, &timeout);

		if (select_status == 0)
			continue;
		else if (select_status == -1)
		{
			if (!((errno == EAGAIN) ||
				(errno == EWOULDBLOCK) ||
				(errno == EINPROGRESS) ||
				(errno == EINTR)))
			{
				rdpLog(UtilLogLevelMinorError, "RDP:android_run - select failed");
				break;
			}
		}

		if (freerdp_check_fds(instance) != TRUE)
		{
			rdpLog(UtilLogLevelMinorError, "RDP:android_run - Failed to check FreeRDP file descriptor");
			break;
		}
		if (android_check_fds(instance) != TRUE)
		{
			rdpLog(UtilLogLevelMinorError, "RDP:android_run - Failed to check android file descriptor");
			break;
		}
		if (freerdp_channels_check_fds(instance->context->channels, instance) != TRUE)
		{
			rdpLog(UtilLogLevelMinorError, "RDP:android_run - Failed to check channel manager file descriptor");
			break;
		}
		android_process_channel_event(instance->context->channels, instance);
	}

	return 0;
}

void* android_thread_func(void* param)
{
	rdpLog(UtilLogLevelEvent1, "RDP:android_thread_func");
	android_freerdp_run(g_Instance);
	
	Sleep(250); //workaround, wait until the last frame is rendered.

	endSession(g_Client);
	
	poly_rdp_exit();

	rdpLog(UtilLogLevelEvent1, "RDP:Ended RDP thread");

	rdpRunning = 0;

	if (sem_post(&polyRdpSem) != 0)
	{
		rdpLog(UtilLogLevelMinorError, "RDP:android_thread_func: semaphore error");
	}

	return NULL;
}

void poly_rdp_module_init()
{
	static int initialized = 0;

	if (!initialized)
	{
		if (sem_init(&polyRdpSem, 0, 1) != 0)
		{
			rdpLog(UtilLogLevelMinorError, "RDP: semaphore creation error");
		}

		initialized = 1;
	}
}


int poly_freerdp_new()
{
	rdpLog(UtilLogLevelDebug, "RDP:poly_freerdp_new");

	freerdp_channels_global_init();

	freerdp* instance;
	// create instance
	instance = freerdp_new();
	instance->PreConnect = android_pre_connect;
	instance->PostConnect = android_post_connect;
	instance->Authenticate = android_authenticate;
	instance->VerifyCertificate = android_verify_certificate;
	instance->VerifyChangedCertificate = android_verify_changed_certificate;
	instance->ReceiveChannelData = android_receive_channel_data;

	// create context
	instance->ContextSize = sizeof(androidContext);
	instance->ContextNew = android_context_new;
	instance->ContextFree = android_context_free;
	freerdp_context_new(instance);

	return (int)instance;
}

void poly_rdp_init(void *client)
{
	g_Client = client;

	g_Instance = poly_freerdp_new();
	rdpSettings * settings = ((freerdp*)g_Instance)->settings;
	settings->DesktopWidth = 1920;
	settings->DesktopHeight = 1080;
	settings->ColorDepth = 32;
	settings->ServerPort = 3389;
	settings->DesktopResize = TRUE;

	//if (color_depth <= 16)
	//	settings->DesktopWidth &= (~1);
}

void poly_rdp_start(void *client)
{
	if (sem_wait(&polyRdpSem) != 0)
	{
		rdpLog(UtilLogLevelMinorError, "RDP:poly_rdp_start: semaphore error");
		return;
	}

	rdpRunning = 1;
	poly_rdp_init(client);

	rdpSettings * settings = ((freerdp*)g_Instance)->settings;

	settings->ServerHostname = strdup("polycomLync");
	settings->Domain = strdup("");

	//if(certname && strlen(certname) > 0)
	//	settings->CertificateName = strdup(certname);

	settings->ConsoleSession = FALSE;

	settings->SoftwareGdi = TRUE;
	settings->BitmapCacheV3Enabled = TRUE;

	settings->RdpSecurity = TRUE;
	settings->TlsSecurity = FALSE;
	settings->NlaSecurity = FALSE;
	settings->ExtSecurity = FALSE;
	settings->DisableEncryption = TRUE;
	settings->EncryptionMethods = ENCRYPTION_METHOD_40BIT | ENCRYPTION_METHOD_128BIT | ENCRYPTION_METHOD_FIPS;
	settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;

	// set US keyboard layout
	settings->KeyboardLayout = 0x0409;
	settings->EmbeddedWindow = TRUE;

	freerdp* inst = (freerdp*)g_Instance;
	androidContext* ctx = (androidContext*)inst->context;

	rdpLog(UtilLogLevelEvent1, "RDP: Creating freerdp working thread");
	if (pthread_create(&ctx->thread, 0, android_thread_func, NULL) == 0)
	{
		pthread_detach(ctx->thread);
	}
	else
	{
		rdpLog(UtilLogLevelMinorError, "RDP: Error creating freerdp working thread");
		(void)sem_post(&polyRdpSem);
	}
}

void poly_rdp_stop()
{
	rdpRunning = 0;

	//ANDROID_EVENT* event = (ANDROID_EVENT*)android_event_disconnect_new();
	//android_push_event(g_Instance, event);
}

void poly_rdp_exit()
{
	freerdp* instance = (freerdp*)g_Instance;
	freerdp_channels_close(instance->context->channels, instance);
	freerdp_disconnect(instance);
	android_cliprdr_uninit(instance);
	gdi_free(instance);
	cache_free(instance->context->cache);

	freerdp_context_free(instance);
	freerdp_free(instance);

	rdpLog(UtilLogLevelDebug, "RDP: Closing RDP socket");
	//closesocket(plcmSock);	
	closesocket(plcmServSock);
}

void poly_rdp_send_key_event(int keycode, int down)
{
	DWORD scancode = GetVirtualScanCodeFromVirtualKeyCode(keycode, 4);
	int flags = (down == TRUE) ? KBD_FLAGS_DOWN : KBD_FLAGS_RELEASE;
	flags |= (scancode & KBDEXT) ? KBD_FLAGS_EXTENDED : 0;

	ANDROID_EVENT* event = (ANDROID_EVENT*)android_event_key_new(flags, scancode & 0xFF);
	android_push_event(g_Instance, event);

	rdpLog(UtilLogLevelEvent1, "RDP:send_key_event: %d, %d", (int)scancode, flags);
}

void poly_rdp_send_unicodekey_event(int keycode)
{
	ANDROID_EVENT* event = (ANDROID_EVENT*)android_event_unicodekey_new(keycode);
	android_push_event(g_Instance, event);

	rdpLog(UtilLogLevelEvent1, "RDP:send_unicodekey_event: %d", keycode);
}

void poly_rdp_send_cursor_event(int x, int y, int flags)
{
	ANDROID_EVENT* event = (ANDROID_EVENT*)android_event_cursor_new(flags, x, y);
	android_push_event(g_Instance, event);

	rdpLog(UtilLogLevelEvent1, "RDP:send_cursor_event: (%d, %d), %x", x, y, flags);
}
