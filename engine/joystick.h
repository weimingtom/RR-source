// Game Controller Support by dbox (dwbox@hotmail.com) (04/16/2007)
// Updated for Cube Engine 2 by Leviscus Tempris (l._tempris@hotmail.com) (11/04/2012)
// -------------------------------------------------------
// If would like to use this code you are free to do so.
// The only condition placed on its use use is that you
// credit the author.
//
// Last Modified: 12/31/2012


#include "engine.h"
#include "SDL_events.h"
#include "SDL.h"
#include "malloc.h"

// define starting offsets of each type of joystick keymap in the keymap.cfg file
#define JOYBUTTONKEYMAP 320
#define JOYAXISKEYMAP   340
#define JOYHATKEYMAP    360

struct joyaxis
{
	int value;
	bool min_pressed;
	bool max_pressed;
};

namespace joystick
{
	int HAT_POS[9] = { SDL_HAT_CENTERED, SDL_HAT_UP, SDL_HAT_RIGHTUP, SDL_HAT_RIGHT, SDL_HAT_RIGHTDOWN, SDL_HAT_DOWN, SDL_HAT_LEFTDOWN, SDL_HAT_LEFT, SDL_HAT_LEFTUP };
	const char* HAT_POS_NAME[9] = { "CENTER", "NORTH", "NE", "EAST", "SE", "SOUTH", "SW", "WEST", "NW" };

	bool enabled = false;
	int axis_count = 0;
	int hat_count = 0;
	int button_count;
	int* last_hat_move = NULL;
	joyaxis* jmove = NULL;

	int jxaxis = 0;
	int jyaxis = 0;
	int jmove_xaxis;
	int jmove_yaxis;
	bool jmove_left_pressed;
	bool jmove_right_pressed;
	bool jmove_up_pressed;
	bool jmove_down_pressed;

    int autoscrollmillis = -1;

    // look up the joystick variables from the config.cfg file
    VARP(joyhatmove, 0, 1, 1);
    VARP(joyfovxaxis, 1, 3, 10);
    VARP(joyfovyaxis, 1, 4, 10);
    VARP(joyxsensitivity, 1, 10, 30);
    VARP(joyysensitivity, 1, 7, 30);
    VARP(joyaxisminmove, 1, 6, 30);
    VARP(joyfovinvert, 0, 0, 1);
    VARP(joyshowevents, 0, 0, 1);
    VARP(joymousebutton, 0, 12, 0xFFFFFFF);
    VARP(joymousecenter, 0, 11, 0xFFFFFFF);
    VARP(joymousealt, 0, 4, 0xFFFFFFF);
    VARP(joymousescrollup, 0, 5, 0xFFFFFFF);
    VARP(joymousescrolldown, 0, 7, 0xFFFFFFF);

    void resetjoystick()
    {
        // clean up
        if(jmove != NULL) free(jmove);
        if(last_hat_move != NULL) free(last_hat_move);
    }

    void init()
    {
        // initialize joystick support
        if(SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0)
        {
            enabled = false;
            return;
        }

        // check for any joysticks
        if(SDL_NumJoysticks() < 1 )
        {
            enabled = false;
            return;
        }

        // open the first registered joystick
        SDL_Joystick* stick = SDL_JoystickOpen(0);
        if(stick == NULL)
        {
            // there was some sort of problem, bail
            enabled = false;
            return;
        }

        // how many axes are there?
        axis_count = SDL_JoystickNumAxes(stick);
        if(axis_count < 2)
        {
            // sorry, we need at least one set of axes to control fov
            enabled = false;
            return;
        }

        // initialize the array of joyaxis structures used to maintain
        // axis position information
        if(jmove != NULL) free(jmove);
        jmove = (joyaxis*)malloc(sizeof(joyaxis)*axis_count);

        // initialize any hat controls
        hat_count = SDL_JoystickNumHats(stick);
        if(hat_count > 0)
        {
            if(last_hat_move != NULL) free(last_hat_move);
            last_hat_move = (int*)malloc(sizeof(int)*hat_count);
            for(int i=0; i < hat_count; i++) last_hat_move[i] = 0;
        }

        // how many buttons do we have to work with
        button_count = SDL_JoystickNumButtons(stick);

        // if we made it this far, everything must be ok
        enabled = true;

        // initialize the joystick settings from the joystick.cfg file
        //execfile("data/joystick.cfg");
    }

    int get_hat_pos(int hat)
    {
        for(int i=0; i < 9; i++) if(hat == HAT_POS[i]) return i;
        return 0;
    }

    // translate any hat movements into keypress events
    void process_hat(int index, int current)
    {
        if(last_hat_move[index] > 0)
        {
            int key = JOYHATKEYMAP+index*8+last_hat_move[index]-1;
            keypress(key, false, 0);
        }
        last_hat_move[index] = current;
        if(current > 0)
        {
            int key = JOYHATKEYMAP+index*8+last_hat_move[index]-1;
            keypress(key, true, 0);
        }
    }

    // translate joystick events into keypress events
    void process_event(SDL_Event* event)
    {
        if(!enabled)
            return;

        int hat_pos;

		//@todo: fix UI
        bool menuon = /*uimillis >*/ 0;
        ///*if(((joymousescrollup-1) == event->button.button || (joymousescrolldown-1) == event->button.button) && event->button.state == SDL_PRESSED)
        //{
        //    conoutf("Passed: SDL_PRESSED");
        //    if(autoscrollmillis < 0) { autoscrollmillis = totalmillis+1000; conoutf("Passed: autoscrollmillis < 0"); }
        //    if(autoscrollmillis-totalmillis > 0)
        //    {
        //        if((joymousescrollup-1) == event->button.button) UI::keypress(GUI_M_SCROLLUP, event->button.state == SDL_PRESSED, 0);
        //        else UI::keypress(GUI_M_SCROLLDOWN, event->button.state == SDL_PRESSED, 0);
        //        autoscrollmillis = totalmillis+250;
        //        conoutf("Passed: autoscrollmillis-totalmillis > 0");
        //    }
        //}
        //else if(event->button.state != SDL_PRESSED) { autoscrollmillis = -1; conoutf("failed: != SDL_PRESSED"); }*/
        //#define joymenu(a, b) \
        //    if((a-1) == event->button.button && (event->type == SDL_JOYBUTTONDOWN || event->type == SDL_JOYBUTTONUP) && \
        //        ((a != joymousescrollup && a != joymousescrolldown) || autoscrollmillis < 0)) \
        //        UI::keypress(b, event->button.state!=0, 0);
        //if(menuon)
        //{
        //    joymenu(joymousebutton, GUI_M_MOUSE1);
        //    joymenu(joymousecenter, GUI_M_CENTER);
        //    joymenu(joymousealt, GUI_M_MOUSE2);
        //    joymenu(joymousescrollup, GUI_M_SCROLLUP);
        //    joymenu(joymousescrolldown, GUI_M_SCROLLDOWN);
        //}

        switch (event->type)
        {
            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
                if(joyshowevents) conoutf("JOYBUTTON%d: %s", event->button.button+1, (event->button.state == SDL_PRESSED) ? "DOWN" : "UP");
                if(!menuon) keypress(event->button.button + JOYBUTTONKEYMAP, event->button.state==SDL_PRESSED, 0);
                break;

            case SDL_JOYAXISMOTION:
                if(joyshowevents) conoutf("JOYAXIS%d: %d", event->jaxis.axis+1, event->jaxis.value);

                if(event->jaxis.axis == joyfovxaxis-1) jxaxis = event->jaxis.value;
                else if(event->jaxis.axis == joyfovyaxis-1) jyaxis = event->jaxis.value;

                jmove[event->jaxis.axis].value = event->jaxis.value;
                break;

            case SDL_JOYHATMOTION:
                hat_pos = get_hat_pos(event->jhat.value);
                if(joyshowevents && hat_pos) conoutf("JOYHAT%d%s", event->jhat.hat+1, HAT_POS_NAME[hat_pos]);
                process_hat(event->jhat.hat, hat_pos);
                break;
        }
    }

    void move()
    {
        if(!enabled)
            return;

        // move the player's field of view
        float dx = jxaxis/3200.0;
        float dy = jyaxis/3200.0;

        float jxsensitivity = .015f * joyxsensitivity,
				jysensitivity = .015f * joyysensitivity;

        if(dx < 0.0f) dx *= dx * jxsensitivity * -1;
        else dx *= dx * jxsensitivity;

        if(dy < 0.0f)  dy *= dy * jysensitivity * -1;
        else dy *= dy * jysensitivity;

        if(joyfovinvert) dy *= -1;

		//@todo: fix
        if((abs(dx) >= 1.0 || abs(dy) >= 1.0)) mousemove(dx, dy);
            //resetcursor(true, false); // game controls engine cursor

        // translate any axis movements into keypress events
        for(int i=0; i < axis_count; i++)
        {
            int key = JOYAXISKEYMAP + i * 2;
            if(jmove[i].value > joyaxisminmove * 1000)
            {
                if(jmove[i].min_pressed) keypress(key, false, 0);
                jmove[i].min_pressed = false;
                if(!jmove[i].max_pressed) keypress(key+1, true, 0);
                jmove[i].max_pressed = true;
            }
            else if(jmove[i].value < joyaxisminmove * -1000)
            {
                if(jmove[i].max_pressed) keypress(key+1, false, 0);
                jmove[i].max_pressed = false;
                if(!jmove[i].min_pressed) keypress(key, true, 0);
                jmove[i].min_pressed = true;
            }
            else
            {
                if(jmove[i].max_pressed) keypress(key+1, false, 0);
                jmove[i].max_pressed = false;
                if(jmove[i].min_pressed) keypress(key, false, 0);
                jmove[i].min_pressed = false;
            }
        }
    }

    ICOMMAND(getjoyaxis, "", (), intret(axis_count));
    ICOMMAND(getjoybuttons, "", (), intret(button_count));
    ICOMMAND(getjoyhats, "", (), intret(hat_count));
	ICOMMAND(usejoystick, "", (), intret(axis_count||button_count||hat_count));
}
