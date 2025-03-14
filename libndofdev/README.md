libndofdev
==========

[![CI](https://github.com/janoc/libndofdev/actions/workflows/CI.yml/badge.svg)](https://github.com/janoc/libndofdev/actions/workflows/CI.yml)

Linux support for the 3Dconnexion SpaceBall, SpaceNavigator and joysticks in the SecondLife clients

Currently supported are the following devices (VID:PID):

  * 0x046d:0xc603   3Dconnexion SpaceMouse (untested)
  * 0x046d:0xc605   CADMan (untested)
  * 0x046d:0xc606   SpaceMouse Classic (untested)
  * 0x046d:0xc621   SpaceBall 5000 (tested, works)
  * 0x046d:0xc623   SpaceTraveller (untested)
  * 0x046d:0xc625   SpacePilot (untested)
  * 0x046d:0xc626   SpaceNavigator (tested, works)
  * 0x046d:0xc627   SpaceExplorer (untested - from Armin Weatherwax <Armin.Weatherwax (at) gmail.com>)
  * 0x046d:0xc628   SpaceNavigator for Notebooks (untested)
  * 0x046d:0xc629   SpacePilot Pro (untested)
  * 0x046d:0xc62b   SpaceMouse Pro (untested - from David Seikel < http://onefang.net/ >

  * 0x256f:0xc62e   SpaceMouse Wireless (cable) (untested)
  * 0x256f:0xc62f   SpaceMouse Wireless (receiver) (untested)
  * 0x256f:0xc631   SpaceMouse Wireless (untested)
  * 0x256f:0xc632   SpaceMouse Pro Wireless (untested)
  * 0x256f:0xc633   SpaceMouse Enterprise (tested, works)
  * 0x256f:0xc635   SpaceMouse Compact (untested - from Kai Werthwein < https://github.com/KaiWerthwein >

The following devices were contributed by Ricky Curtice (http://rwcproductions.com/):
(https://bitbucket.org/lindenlab/3p-libndofdev-linux/pull-request/1/open-277-linux-support-for-spacemousepro/diff)

CADMan, SpaceM (classic & wireless) except SpaceMouse Pro
(non-wireless) and SpaceMouse Entreprise, SpaceNavigator for Notebooks

The devices marked as "untested" are expected to work and their
VID:PIDs were sent to me by external contributors. I haven't tested
them personally because I don't have that hardware, they could work,
could be completely broken or anything in-between. Caveat
emptor. However, please do report any working or not working devices
from the list above - that's the only way to get the problems fixed.

In addition, the library supports every joystick/gamepad device
supported by the SDL library. SDL2 is supported by passing a flag to
the makefile: `USE_SDL2=1 make` SDL3 is supported by passing USE_SDL3


C A U T I O N !!!
-----------------

DO NOT USE THIS LIBRARY FOR ADDING SUPPORT FOR THE 3Dconnexion HARDWARE TO NEW APPLICATIONS!

This library is a messy hack emulating the proprietary 3Dconnexion SDK, in order to simplify the SecondLife
client integration. If you are looking to integrate support for these devices into a new application,
a much better solution is to use the event interface directly. An example of how to do this can be found
here:
  * http://jciger.com/files/software/linux/spacenav/spacenavig.c

If you are building a virtual reality application, then the best option is to use the VRPN library:
  * http://www.cs.unc.edu/Research/vrpn/

VRPN supports all the devices above and many more in a transparent and abstract manner, so you don't have
to add support for each different interaction device or tracker
specially.

Another option simulating the official 3DConnexion driver is the
Spacenav project:
   * https://spacenav.sourceforge.net/
