/*
*
* Copyright (c) 2008, Jan Ciger (jan.ciger (at) gmail.com)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * The name of its contributors may not be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY Jan Ciger ''AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL Jan Ciger BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
    * Drop-in replacement for the 3D Connexion SDK library not available on Linux
    *
    * Uses native Linux input (evdev) device for SpaceNavigator, otherwise SDL for
    * regular joysticks.
    *
    * Evdev could be used for joysticks as well, but higher level logic would have to be
    * re-implemented (calibration, filtering, etc.) - SDL includes it already.
    *
    * Release 0.9
*/

#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef USE_SDL3
#include <SDL3/SDL.h>
#elif defined(USE_SDL2)
#include <SDL2/SDL.h>
#else
#include <SDL/SDL.h>
#endif


#include "ndofdev_external.h"
#include "ndofdev_version.h"

// Hack - we need to store the descriptor
// of the SpaceNavigator in order to be able to
// turn its LED off in the ndof_cleanup() function :(
static int spacenav_fd = -1;

// SpaceMouse button remapping table - SpaceMouse Enterprise has only
// 32 buttons but reports 9bit button codes, likely for compatibility
// with old hw.
#define SPACE_MOUSE_BUTTON_SPACE 256
static long SPACE_MOUSE_BUTTON_MAPPING[SPACE_MOUSE_BUTTON_SPACE];

const int SPACE_NAVIG_THRESHOLD = 20; // minimum change threshold

typedef struct
{
    // native HID interface or SDL?
    int USE_SDL;

    // SpaceNavigator
    int fd;             // file descriptor of the device
    long int *axes;     // last state
    long int *buttons;

    // SDL joysticks
    SDL_Joystick *j;

    int num_hats;       // how many hats are on this joystick

} LinJoystickPrivate;

NDOF_Device *ndof_create()
{
    NDOF_Device *d = (NDOF_Device *) calloc(1, sizeof(NDOF_Device));
    return d;
}

#define test_bit(nr, addr) \
     (((1UL << ((nr) % (sizeof(long) * 8))) & ((addr)[(nr) / (sizeof(long) * 8)])) != 0)
#define NBITS(x) ((((x)-1)/(sizeof(long) * 8))+1)

int ndof_init_first(NDOF_Device *in_out_dev, void *param)
{
    // try to find 3DConnexion SpaceNavigator first
    int i = 0;
    int fd = -1;
    char *fname = NULL;
    struct input_id ID;

    printf("libndofdef version %d.%d, (c) Jan Ciger https://github.com/janoc/libndofdev\n", NDOFDEV_MAJOR, NDOFDEV_MINOR);

    fname = (char *)malloc(1000 * sizeof(char));
    while (i < 32)
    {
        sprintf(fname, "/dev/input/event%d", i++);
        fd = open(fname, O_RDWR | O_NONBLOCK);
        if (fd > 0)
        {
            ioctl(fd, EVIOCGID, &ID);        // get device ID
            if (                             // For a nice list see http://spacemice.org/index.php?title=Dev
                ((ID.vendor == 0x046d) &&    // Logitech's Vendor ID, used by 3DConnexion until they got their own.
                    (
                        (ID.product == 0xc603) || // SpaceMouse (untested)
                        (ID.product == 0xc605) || // CADMan (untested)
                        (ID.product == 0xc606) || // SpaceMouse Classic (untested)
                        (ID.product == 0xc621) || // SpaceBall 5000
                        (ID.product == 0xc623) || // SpaceTraveler (untested)
                        (ID.product == 0xc625) || // SpacePilot (untested)
                        (ID.product == 0xc626) || // SpaceNavigators
                        (ID.product == 0xc627) || // SpaceExplorer (untested)
                        (ID.product == 0xc628) || // SpaceNavigator for Notebooks (untested)
                        (ID.product == 0xc629) || // SpacePilot Pro (untested)
                        (ID.product == 0xc62b) || // SpaceMousePro
                        0
                    )
                ) ||
                ((ID.vendor == 0x256F) &&    // 3Dconnexion's Vendor ID
                    (
                        (ID.product == 0xc62E) || // SpaceMouse Wireless (cable) (untested)
                        (ID.product == 0xc62F) || // SpaceMouse Wireless (receiver) (untested)
                        (ID.product == 0xc631) || // Spacemouse Wireless (untested)
                        (ID.product == 0xc632) || // Spacemouse Pro Wireless (untested)
                        (ID.product == 0xc633) || // Spacemouse Enterprise
                        (ID.product == 0xc635) || // Spacemouse Compact (untested)
                        0
                    )
                ))
            {
                printf("Using evdev device: %s\n", fname);
                break;
            } else {
                close(fd);
                fd = -1;
            }
        }
    }

    if(fd > 0)
    {
        // We have SpaceNavigator, use it
        spacenav_fd = fd;

        // default to sane values for these devices
        unsigned int axes_count = 6;
        unsigned int button_count = 32;

        // Get the actual number of axes for this device.
        int detected_axes_count = 0;

        // first absolute axes
        unsigned long absbit[NBITS(ABS_MAX)] = { 0 };
        if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) >= 0)
        {
            for (i = 0; i < ABS_MISC; ++i)
            {
                /* Skip hats */
                if (i == ABS_HAT0X)
                {
                    i = ABS_HAT3Y;
                    continue;
                }

                if (test_bit(i, absbit))
                    detected_axes_count++;
            }
        } else {
            perror("Failed to obtain the number of absolute axes for device:\n");
        }

        // now relative axes
        unsigned long relbit[NBITS(REL_MAX)] = { 0 };
        if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relbit)), relbit) >= 0)
        {
            for (i = 0; i < REL_MISC; ++i)
            {
                if (test_bit(i, relbit))
                    detected_axes_count++;
            }
        } else {
            perror("Failed to obtain the number of relative axes for device:\n");
        }

        if (detected_axes_count != 0)
            axes_count = detected_axes_count;

        // Get the actual number of buttons for this device.
        int detected_button_count = 0;
        unsigned long keybit[NBITS(KEY_MAX)] = { 0 };
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) >= 0)
        {
            for (i = BTN_JOYSTICK; i < KEY_MAX; ++i)
            {
                if (test_bit(i, keybit))
                    detected_button_count++;
            }

            for (i = BTN_MISC; i < BTN_JOYSTICK; ++i)
            {
                if (test_bit(i, keybit))
                    detected_button_count++;
            }

            if (detected_button_count != 0)
                button_count = detected_button_count;
        } else {
            perror("Failed to obtain the number of buttons for device:\n");
        }

        // clamp number of axes & buttons to what is actually
        // supported by the original SDK to avoid corrupting
        // memory. 3DConnexion devices routinely report 255+ buttons
        // even though they have only 32.
        in_out_dev->axes_count = (axes_count <= NDOF_MAX_AXES_COUNT) ? axes_count : NDOF_MAX_AXES_COUNT;
        in_out_dev->btn_count  = (button_count <= NDOF_MAX_BUTTONS_COUNT) ? button_count : NDOF_MAX_BUTTONS_COUNT;
        in_out_dev->absolute   = 0;
        in_out_dev->valid      = 1;
        in_out_dev->axes_max   = 512;
        in_out_dev->axes_min   = -512;
        ioctl(fd, EVIOCGNAME(255), &in_out_dev->product); // copy name of the device

        // private data
        LinJoystickPrivate *priv = (LinJoystickPrivate *) malloc (sizeof(LinJoystickPrivate));
        priv->fd = fd;
        priv->axes = (long int *) calloc(axes_count, sizeof(long int));
        priv->buttons = (long int *) calloc(button_count, sizeof(long int));
        priv->USE_SDL = 0;
        priv->j = NULL;
        in_out_dev->private_data = priv;

        // turn the LED on
        struct input_event led_ev;

        led_ev.type = EV_LED;
        led_ev.code = LED_MISC;
        led_ev.value = 1;
        if(write(spacenav_fd, &led_ev, sizeof(struct input_event)) < 0)
            perror("Failed to write LED_ON command:\n");

        return 0;

    } else {
#ifdef USE_SDL3
        // SpaceNavigator not found, use SDL Joystick
        SDL_Joystick *j = SDL_OpenJoystick(0);
        if(j)
        {
            in_out_dev->axes_count = SDL_GetNumJoystickAxes(j) + SDL_GetNumJoystickHats(j) * 2; // each hat has 2 axes
            in_out_dev->btn_count = SDL_GetNumJoystickButtons(j);
            in_out_dev->absolute = 0; // always relative on Linux
            in_out_dev->valid = 1;
            in_out_dev->axes_max = 32767;
            in_out_dev->axes_min = -32767;

            strncpy(in_out_dev->product, SDL_GetJoystickName(j), 255);

            // private data
            LinJoystickPrivate *priv = (LinJoystickPrivate *) malloc (sizeof(LinJoystickPrivate));
            priv->j = j;
            priv->fd = -1;
            priv->USE_SDL = 1;
            // remember number of hats for later axis mapping in ndof_update()
            priv->num_hats = SDL_GetNumJoystickHats(j);
            in_out_dev->private_data = priv;

            return 0;
        } 
#else
        // SpaceNavigator not found, use SDL Joystick
        SDL_Joystick *j = SDL_JoystickOpen(0);
        if(j)
        {
            in_out_dev->axes_count = SDL_JoystickNumAxes(j) + SDL_JoystickNumHats(j) * 2; // each hat has 2 axes
            in_out_dev->btn_count = SDL_JoystickNumButtons(j);
            in_out_dev->absolute = 0; // always relative on Linux
            in_out_dev->valid = 1;
            in_out_dev->axes_max = 32767;
            in_out_dev->axes_min = -32767;
#if defined(USE_SDL2)
            strncpy(in_out_dev->product, SDL_JoystickName(j), 255);
#else
            strncpy(in_out_dev->product, SDL_JoystickName(0), 255);
#endif
            // private data
            LinJoystickPrivate *priv = (LinJoystickPrivate *) malloc (sizeof(LinJoystickPrivate));
            priv->j = j;
            priv->fd = -1;
            priv->USE_SDL = 1;
            // remember number of hats for later axis mapping in ndof_update()
            priv->num_hats = SDL_JoystickNumHats(j);
            in_out_dev->private_data = priv;

            return 0;
        } 
#endif
        else {

            // no joysticks found
            return -1;
        }
    }
}

void ndof_libcleanup()
{
    if(spacenav_fd > 0)
    {
        // turn the LED off
        struct input_event led_ev;

        led_ev.type = EV_LED;
        led_ev.code = LED_MISC;
        led_ev.value = 0;
        if(write(spacenav_fd, &led_ev, sizeof(struct input_event)) < 0)
            perror("Failed to write LED_OFF command:\n");
    }

    // FIXME: needs to cleanup the memory
}

int ndof_libinit(NDOF_DeviceAddCallback in_add_cb,
                 NDOF_DeviceRemovalCallback in_removal_cb,
                 void *platform_specific)
{
    // Fill the SpaceMouse button mapping table
    //
    // Remap the buttons with high number codes into the unused spaces
    // within the first 32 buttons - likely not how the Windows driver
    // does it but at least they will be usable. We also ignore the
    // 9th bit.

    for(long i = 0; i < SPACE_MOUSE_BUTTON_SPACE; ++i)
        SPACE_MOUSE_BUTTON_MAPPING[i] = i;

    SPACE_MOUSE_BUTTON_MAPPING[282 & 0xff] = 3;  /* Rotation lock */
    SPACE_MOUSE_BUTTON_MAPPING[291 & 0xff] = 6;  /* Enter */
    SPACE_MOUSE_BUTTON_MAPPING[292 & 0xff] = 7;  /* Delete */
    SPACE_MOUSE_BUTTON_MAPPING[430 & 0xff] = 9;  /* Tab */
    SPACE_MOUSE_BUTTON_MAPPING[431 & 0xff] = 26; /* Space */
    SPACE_MOUSE_BUTTON_MAPPING[358 & 0xff] = 27; /* V1 */
    SPACE_MOUSE_BUTTON_MAPPING[359 & 0xff] = 28; /* V2 */
    SPACE_MOUSE_BUTTON_MAPPING[360 & 0xff] = 29; /* V3 */
    SPACE_MOUSE_BUTTON_MAPPING[332 & 0xff] = 30; /* 11 */
    SPACE_MOUSE_BUTTON_MAPPING[333 & 0xff] = 31; /* 12 */

    // Initialize the joystick subsystem
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);

    return 0;
}

void ndof_update(NDOF_Device *in_dev)
{
    int i;

    LinJoystickPrivate *priv = (LinJoystickPrivate *) in_dev->private_data;
    assert(priv != NULL);

    if(priv->USE_SDL)
    {
#ifdef USE_SDL3
        SDL_UpdateJoysticks();
        SDL_Joystick *j = priv->j;

        for(i = 0; i < in_dev->axes_count - priv->num_hats*2; i++) // hats will get mapped to uppermost axes
        {
            in_dev->axes[i] = (int) (SDL_GetJoystickAxis(j, i));
        }

        for(i = 0; i < priv->num_hats; i++)
        {
            int x = 0;
            int y = 0;

            int value = SDL_GetJoystickHat(j, i);

            // map hat values (1, 2, 4, 8) to axis data (-32768, 32768)
            if(value & 1)
            {
                y = -32767;
            }
            else if(value & 4)
            {
                y = 32768;
            }
            if(value & 2)
            {
                x = 32767;
            }
            else if(value & 8)
            {
                x = -32768;
            }

            // add the hat data to the uppermost axes data
            in_dev->axes[in_dev->axes_count - priv->num_hats*2 + i*2  ] = x;
            in_dev->axes[in_dev->axes_count - priv->num_hats*2 + i*2+1] = y;
        }

        for(i = 0; i < in_dev->btn_count; i++)
        {
            in_dev->buttons[i] = SDL_GetJoystickButton(j, i);
        }
#else
        SDL_JoystickUpdate();
        SDL_Joystick *j = priv->j;

        for(i = 0; i < in_dev->axes_count - priv->num_hats*2; i++) // hats will get mapped to uppermost axes
        {
            in_dev->axes[i] = (int) (SDL_JoystickGetAxis(j, i));
        }

        for(i = 0; i < priv->num_hats; i++)
        {
            int x = 0;
            int y = 0;

            int value = SDL_JoystickGetHat(j, i);

            // map hat values (1, 2, 4, 8) to axis data (-32768, 32768)
            if(value & 1)
            {
                y = -32767;
            }
            else if(value & 4)
            {
                y = 32768;
            }
            if(value & 2)
            {
                x = 32767;
            }
            else if(value & 8)
            {
                x = -32768;
            }

            // add the hat data to the uppermost axes data
            in_dev->axes[in_dev->axes_count - priv->num_hats*2 + i*2  ] = x;
            in_dev->axes[in_dev->axes_count - priv->num_hats*2 + i*2+1] = y;
        }

        for(i = 0; i < in_dev->btn_count; i++)
        {
            in_dev->buttons[i] = SDL_JoystickGetButton(j, i);
        }
#endif
    } else {
        // update SpaceNavigator

        struct input_event ev;

        while(read(priv->fd, &ev, sizeof(struct input_event)) > 0)
        {
            switch (ev.type)
            {
                case EV_KEY:
                    int mapped_code = SPACE_MOUSE_BUTTON_MAPPING[ev.code & 0xff];
                    // printf("Key %d (mapped to %d) pressed %d .\n", ev.code, mapped_code, ev.value);
                    priv->buttons[mapped_code] = ev.value;
                    break;

                case EV_REL:
                case EV_ABS: // 2.6.35 and up, maybe earlier kernels too send EV_ABS instead of EV_REL
                    // printf("%d %g\n", ev.code, ev.value);

                    // clean up small values
                    priv->axes[ev.code] = abs(ev.value) > SPACE_NAVIG_THRESHOLD ? ev.value : 0;
                    break;

                default:
                    break;
            }
        }

        memcpy(in_dev->axes, priv->axes, in_dev->axes_count * sizeof(long int));
        memcpy(in_dev->buttons, priv->buttons, in_dev->btn_count * sizeof(long int));
    }
}

void ndof_dump(FILE *stream, NDOF_Device *dev)
{
    fprintf(stream, "NDOF device: %s, with %d buttons and %d axes\n", dev->product, dev->btn_count, dev->axes_count);
}

void ndof_dump_list(FILE *stream)
{

}
