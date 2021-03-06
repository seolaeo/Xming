/*
 * Copyright © 2006 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */
/*
 * This Xinerama implementation comes from the SiS driver which has
 * the following notice:
 */
/* 
 * SiS driver main code
 *
 * Copyright (C) 2001-2005 by Thomas Winischhofer, Vienna, Austria.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1) Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3) The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Thomas Winischhofer <thomas@winischhofer.net>
 *	- driver entirely rewritten since 2001, only basic structure taken from
 *	  old code (except sis_dri.c, sis_shadow.c, sis_accel.c and parts of
 *	  sis_dga.c; these were mostly taken over; sis_dri.c was changed for
 *	  new versions of the DRI layer)
 *
 * This notice covers the entire driver code unless indicated otherwise.
 *
 * Formerly based on code which was
 * 	     Copyright (C) 1998, 1999 by Alan Hourihane, Wigan, England.
 * 	     Written by:
 *           Alan Hourihane <alanh@fairlite.demon.co.uk>,
 *           Mike Chapman <mike@paranoia.com>,
 *           Juanjo Santamarta <santamarta@ctv.es>,
 *           Mitani Hiroshi <hmitani@drl.mei.co.jp>,
 *           David Thomas <davtom@dream.org.uk>.
 */

#include "randrstr.h"
#include "swaprep.h"
#include <X11/extensions/panoramiXproto.h>

#define RR_XINERAMA_MAJOR_VERSION   1
#define RR_XINERAMA_MINOR_VERSION   1

/* Xinerama is not multi-screen capable; just report about screen 0 */
#define RR_XINERAMA_SCREEN  0

static int ProcRRXineramaQueryVersion(ClientPtr client);
static int ProcRRXineramaGetState(ClientPtr client);
static int ProcRRXineramaGetScreenCount(ClientPtr client);
static int ProcRRXineramaGetScreenSize(ClientPtr client);
static int ProcRRXineramaIsActive(ClientPtr client);
static int ProcRRXineramaQueryScreens(ClientPtr client);
static int SProcRRXineramaDispatch(ClientPtr client);

/* Proc */

int
ProcRRXineramaQueryVersion(ClientPtr client)
{
    xPanoramiXQueryVersionReply	  rep;
    register int		  n;

    REQUEST_SIZE_MATCH(xPanoramiXQueryVersionReq);
    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.majorVersion = RR_XINERAMA_MAJOR_VERSION;
    rep.minorVersion = RR_XINERAMA_MINOR_VERSION;
    if(client->swapped) {
        swaps(&rep.sequenceNumber, n);
        swapl(&rep.length, n);
        swaps(&rep.majorVersion, n);
        swaps(&rep.minorVersion, n);
    }
    WriteToClient(client, sizeof(xPanoramiXQueryVersionReply), (char *)&rep);
    return (client->noClientException);
}

int
ProcRRXineramaGetState(ClientPtr client)
{
    REQUEST(xPanoramiXGetStateReq);
    WindowPtr			pWin;
    xPanoramiXGetStateReply	rep;
    register int		n, rc;
    ScreenPtr			pScreen;
    rrScrPrivPtr		pScrPriv;
    Bool			active = FALSE;

    REQUEST_SIZE_MATCH(xPanoramiXGetStateReq);
    rc = dixLookupWindow(&pWin, stuff->window, client, DixUnknownAccess);
    if(rc != Success)
	return rc;

    pScreen = pWin->drawable.pScreen;
    pScrPriv = rrGetScrPriv(pScreen);
    if (pScrPriv)
    {
	/* XXX do we need more than this? */
	active = TRUE;
    }

    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.state = active;
    if(client->swapped) {
       swaps (&rep.sequenceNumber, n);
       swapl (&rep.length, n);
       swaps (&rep.state, n);
    }
    WriteToClient(client, sizeof(xPanoramiXGetStateReply), (char *)&rep);
    return client->noClientException;
}

static Bool
RRXineramaScreenActive (ScreenPtr pScreen)
{
    return rrGetScrPriv(pScreen) != NULL;
}

static Bool
RRXineramaCrtcActive (RRCrtcPtr crtc)
{
    return crtc->mode != NULL && crtc->numOutputs > 0;
}

static int
RRXineramaScreenCount (ScreenPtr pScreen)
{
    int	i, n;
    
    n = 0;
    if (RRXineramaScreenActive (pScreen))
    {
	rrScrPriv(pScreen);
	for (i = 0; i < pScrPriv->numCrtcs; i++)
	    if (RRXineramaCrtcActive (pScrPriv->crtcs[i]))
		n++;
    }
    return n;
}

int
ProcRRXineramaGetScreenCount(ClientPtr client)
{
    REQUEST(xPanoramiXGetScreenCountReq);
    WindowPtr				pWin;
    xPanoramiXGetScreenCountReply	rep;
    register int			n, rc;

    REQUEST_SIZE_MATCH(xPanoramiXGetScreenCountReq);
    rc = dixLookupWindow(&pWin, stuff->window, client, DixUnknownAccess);
    if (rc != Success)
	return rc;
    
    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.ScreenCount = RRXineramaScreenCount (pWin->drawable.pScreen);
    if(client->swapped) {
       swaps(&rep.sequenceNumber, n);
       swapl(&rep.length, n);
       swaps(&rep.ScreenCount, n);
    }
    WriteToClient(client, sizeof(xPanoramiXGetScreenCountReply), (char *)&rep);
    return client->noClientException;
}

int
ProcRRXineramaGetScreenSize(ClientPtr client)
{
    REQUEST(xPanoramiXGetScreenSizeReq);
    WindowPtr				pWin, pRoot;
    ScreenPtr				pScreen;
    xPanoramiXGetScreenSizeReply	rep;
    register int			n, rc;

    REQUEST_SIZE_MATCH(xPanoramiXGetScreenSizeReq);
    rc = dixLookupWindow(&pWin, stuff->window, client, DixUnknownAccess);
    if (rc != Success)
	return rc;

    pScreen = pWin->drawable.pScreen;
    pRoot = WindowTable[pScreen->myNum];
    
    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.width  = pRoot->drawable.width;
    rep.height = pRoot->drawable.height;
    if(client->swapped) {
       swaps(&rep.sequenceNumber, n);
       swapl(&rep.length, n);
       swaps(&rep.width, n);
       swaps(&rep.height, n);
    }
    WriteToClient(client, sizeof(xPanoramiXGetScreenSizeReply), (char *)&rep);
    return client->noClientException;
}

int
ProcRRXineramaIsActive(ClientPtr client)
{
    xXineramaIsActiveReply	rep;

    REQUEST_SIZE_MATCH(xXineramaIsActiveReq);
	
    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.state = RRXineramaScreenActive (screenInfo.screens[RR_XINERAMA_SCREEN]);
    if(client->swapped) {
	register int n;
	swaps(&rep.sequenceNumber, n);
	swapl(&rep.length, n);
	swapl(&rep.state, n);
    }
    WriteToClient(client, sizeof(xXineramaIsActiveReply), (char *) &rep);
    return client->noClientException;
}

int
ProcRRXineramaQueryScreens(ClientPtr client)
{
    xXineramaQueryScreensReply	rep;
    ScreenPtr	pScreen = screenInfo.screens[RR_XINERAMA_SCREEN];

    REQUEST_SIZE_MATCH(xXineramaQueryScreensReq);

    if (RRXineramaScreenActive (pScreen))
    {
	rrScrPriv(pScreen);
	if (pScrPriv->numCrtcs == 0 || pScrPriv->numOutputs == 0)
	    RRGetInfo (pScreen);
    }
    
    rep.type = X_Reply;
    rep.sequenceNumber = client->sequence;
    rep.number = RRXineramaScreenCount (pScreen);
    rep.length = rep.number * sz_XineramaScreenInfo >> 2;
    if(client->swapped) {
	register int n;
	swaps(&rep.sequenceNumber, n);
	swapl(&rep.length, n);
	swapl(&rep.number, n);
    }
    WriteToClient(client, sizeof(xXineramaQueryScreensReply), (char *)&rep);

    if(rep.number) {
	rrScrPriv(pScreen);
	xXineramaScreenInfo scratch;
	int i;

	for(i = 0; i < pScrPriv->numCrtcs; i++) {
	    RRCrtcPtr	crtc = pScrPriv->crtcs[i];
	    if (RRXineramaCrtcActive (crtc))
	    {
	        int width, height;
		RRCrtcGetScanoutSize (crtc, &width, &height);
		scratch.x_org  = crtc->x;
		scratch.y_org  = crtc->y;
		scratch.width  = width;
		scratch.height = height;
		if(client->swapped) {
		    register int n;
		    swaps(&scratch.x_org, n);
		    swaps(&scratch.y_org, n);
		    swaps(&scratch.width, n);
		    swaps(&scratch.height, n);
		}
		WriteToClient(client, sz_XineramaScreenInfo, (char *)&scratch);
	    }
	}
    }

    return client->noClientException;
}

static int
ProcRRXineramaDispatch(ClientPtr client)
{
    REQUEST(xReq);
    switch (stuff->data) {
	case X_PanoramiXQueryVersion:
	     return ProcRRXineramaQueryVersion(client);
	case X_PanoramiXGetState:
	     return ProcRRXineramaGetState(client);
	case X_PanoramiXGetScreenCount:
	     return ProcRRXineramaGetScreenCount(client);
	case X_PanoramiXGetScreenSize:
	     return ProcRRXineramaGetScreenSize(client);
	case X_XineramaIsActive:
	     return ProcRRXineramaIsActive(client);
	case X_XineramaQueryScreens:
	     return ProcRRXineramaQueryScreens(client);
    }
    return BadRequest;
}

/* SProc */

static int
SProcRRXineramaQueryVersion (ClientPtr client)
{
    REQUEST(xPanoramiXQueryVersionReq);
    register int n;
    swaps(&stuff->length,n);
    REQUEST_SIZE_MATCH (xPanoramiXQueryVersionReq);
    return ProcRRXineramaQueryVersion(client);
}

static int
SProcRRXineramaGetState(ClientPtr client)
{
    REQUEST(xPanoramiXGetStateReq);
    register int n;
    swaps (&stuff->length, n);
    REQUEST_SIZE_MATCH(xPanoramiXGetStateReq);
    return ProcRRXineramaGetState(client);
}

static int
SProcRRXineramaGetScreenCount(ClientPtr client)
{
    REQUEST(xPanoramiXGetScreenCountReq);
    register int n;
    swaps (&stuff->length, n);
    REQUEST_SIZE_MATCH(xPanoramiXGetScreenCountReq);
    return ProcRRXineramaGetScreenCount(client);
}

static int
SProcRRXineramaGetScreenSize(ClientPtr client)
{
    REQUEST(xPanoramiXGetScreenSizeReq);
    register int n;
    swaps (&stuff->length, n);
    REQUEST_SIZE_MATCH(xPanoramiXGetScreenSizeReq);
    return ProcRRXineramaGetScreenSize(client);
}

static int
SProcRRXineramaIsActive(ClientPtr client)
{
    REQUEST(xXineramaIsActiveReq);
    register int n;
    swaps (&stuff->length, n);
    REQUEST_SIZE_MATCH(xXineramaIsActiveReq);
    return ProcRRXineramaIsActive(client);
}

static int
SProcRRXineramaQueryScreens(ClientPtr client)
{
    REQUEST(xXineramaQueryScreensReq);
    register int n;
    swaps (&stuff->length, n);
    REQUEST_SIZE_MATCH(xXineramaQueryScreensReq);
    return ProcRRXineramaQueryScreens(client);
}

int
SProcRRXineramaDispatch(ClientPtr client)
{
    REQUEST(xReq);
    switch (stuff->data) {
	case X_PanoramiXQueryVersion:
	     return SProcRRXineramaQueryVersion(client);
	case X_PanoramiXGetState:
	     return SProcRRXineramaGetState(client);
	case X_PanoramiXGetScreenCount:
	     return SProcRRXineramaGetScreenCount(client);
	case X_PanoramiXGetScreenSize:
	     return SProcRRXineramaGetScreenSize(client);
	case X_XineramaIsActive:
	     return SProcRRXineramaIsActive(client);
	case X_XineramaQueryScreens:
	     return SProcRRXineramaQueryScreens(client);
    }
    return BadRequest;
}

static void
RRXineramaResetProc(ExtensionEntry* extEntry)
{
}

void
RRXineramaExtensionInit(void)
{
#ifdef PANORAMIX
    if(!noPanoramiXExtension)
	return;
#endif

    /*
     * Xinerama isn't capable enough to have multiple protocol screens each
     * with their own output geometry.  So if there's more than one protocol
     * screen, just don't even try.
     */
    if (screenInfo.numScreens > 1)
	return;

    (void) AddExtension(PANORAMIX_PROTOCOL_NAME, 0,0,
			ProcRRXineramaDispatch,
			SProcRRXineramaDispatch,
			RRXineramaResetProc,
			StandardMinorOpcode);
}
