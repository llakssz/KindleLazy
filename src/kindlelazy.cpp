/*
KindleLazy 0.9
cearp 2016

thanks to lucida for usbotgmouse and kindle-evdev
thanks to knc1 and everyone else at the mobileread kindle developer's corner
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <sys/poll.h>
#include <unistd.h>
#include <vector>

#include <linux/input.h>
#include <X11/extensions/XTest.h>

#include "json/json.h"

enum action_type {
    key_next_page,
    key_prev_page,
    key_brightness_up,
    key_brightness_down,
    key_wake
};

struct keycommand {
    int id;
    action_type action;
};

Display *dpy = NULL;
int height = 0;
int width  = 0;
int halfheight = 0;
int halfwidth = 0;
bool reverse = false;

//touch screen device file
int eventfdTOUCH;

std::string new_brightness = "";
int current_brightness = 0;

int next_x = 0;
int next_y = 0;
int prev_x = 0;
int prev_y = 0;


std::string exec(const char* cmd) {
    char buffer[128];
    std::string result = "";
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer, 128, pipe.get()) != NULL)
            result += buffer;
    }
    return result;
}

int getbtn(int fd)
{
    int btn = 0;
    struct input_event t;
    if (read(fd, &t, sizeof(t)) == sizeof(t)){
        if (t.type == EV_KEY && t.value == 0)  //key released
            btn = t.code;
        else if (t.type == EV_REL && t.code == REL_WHEEL && t.value == 1)  //vscroll up
            btn = 9001;
        else if (t.type == EV_REL && t.code == REL_WHEEL && t.value == -1)  //vscroll down
            btn = 9002;
        else if (t.type == EV_REL && t.code == REL_HWHEEL && t.value == -1)  //hscroll left
            btn = 9003;
        else if (t.type == EV_REL && t.code == REL_HWHEEL && t.value == 1)  //hscroll right
            btn = 9004;
    }
    return btn;
}

void mouse_click(Display* dpy, int x, int y)
{
    XTestFakeMotionEvent (dpy, -1, x, y, CurrentTime);
    XTestFakeButtonEvent (dpy, 1, True,  CurrentTime);
    XTestFakeButtonEvent (dpy, 1, False, CurrentTime);
    XFlush(dpy);
}

void write_event(int fd, __u16 type, __u16 code, __s32 value)
{
    int ret;
    struct input_event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;
    ret = write(fd, &event, sizeof(event));
    if (ret < 0)
        perror("ERROR write_event");
    fsync(fd);
}

void write_touch_event(int fd, int x, int y)
{
    //For kindle, get sequence from "evtest /dev/input/eventX"
    write_event(fd, EV_KEY, BTN_TOUCH, 1);
    write_event(fd, EV_ABS, ABS_MT_TRACKING_ID, 0);
    write_event(fd, EV_ABS, ABS_MT_POSITION_X , x);
    write_event(fd, EV_ABS, ABS_MT_POSITION_Y, y);
    write_event(fd, EV_SYN, 0, 0);
    write_event(fd, EV_KEY, BTN_TOOL_FINGER, 1);
    write_event(fd, EV_SYN, 0, 0);
    write_event(fd, EV_ABS, ABS_MT_TRACKING_ID , -1);
    write_event(fd, EV_SYN, 0, 0);
    write_event(fd, EV_KEY, BTN_TOUCH , 0);
    write_event(fd, EV_KEY, BTN_TOOL_FINGER, 0);
    write_event(fd, EV_SYN, 0, 0);
}

void process_key (action_type action)
{
    //when faking a touch event and not just faking a simple mouse click
    //the orientation is ignored, and there is NOT a x,y point that turns/reverses for both portrait and lanscape
    //that turns/reverse the page for both portrait and landscape
    //we could check the orientation via 'lipc-get-prop com.lab126.winmgr orientation'

    //i have no idea about oasis orientation values
    std::string orientation = exec("lipc-get-prop com.lab126.winmgr orientation");
    orientation.pop_back(); //it retuns with a newline... took a while to figure why compares weren't working
    if(orientation.compare("U") == 0){
        //upright/portrait
        next_x = width;
        next_y = halfheight;
        prev_x = 0;
        prev_y = halfheight;
    }
    else{
        //landscape
        next_x = halfwidth;
        next_y = 0;
        prev_x = halfwidth;
        prev_y = height;
    }

    switch(action)
    {
        case key_next_page:
        if(reverse){
            // mouse_click(dpy, 0, halfheight);
            write_touch_event(eventfdTOUCH, prev_x, prev_y);
        }
        else{
            // mouse_click(dpy, width, halfheight);
            write_touch_event(eventfdTOUCH, next_x, next_y);
        }
        break;

        case key_prev_page:
        if(reverse){
            // mouse_click(dpy, width, halfheight);
            write_touch_event(eventfdTOUCH, next_x, next_y);
        }
        else{
            // mouse_click(dpy, 0, halfheight);
            write_touch_event(eventfdTOUCH, prev_x, prev_y);
        }
        break;

        case key_brightness_up:
        current_brightness = std::stoi( exec("lipc-get-prop com.lab126.powerd flIntensity"));
        if(current_brightness < 24){
            current_brightness++;
        }
        new_brightness = std::to_string(current_brightness);
        new_brightness = "lipc-set-prop com.lab126.powerd flIntensity " + new_brightness;
        system(new_brightness.c_str());
        break;

        case key_brightness_down:
        current_brightness = std::stoi(exec("lipc-get-prop com.lab126.powerd flIntensity"));
        if(current_brightness > 0){
            current_brightness--;
        }
        new_brightness = std::to_string(current_brightness);
        new_brightness = "lipc-set-prop com.lab126.powerd flIntensity " + new_brightness;
        system(new_brightness.c_str());
        break;

        case key_wake:
        std::string state = exec("lipc-get-prop com.lab126.powerd state");
        state.pop_back(); //remove newline from string
        if(state.compare("screenSaver") == 0){
            exec("lipc-set-prop com.lab126.powerd wakeUp 1");
        }
        break;
    }

    //keep awake. at least faking touch with xdotool, we needed this.
    //otherwise the kindle sleeps due to no touch - we probably need it here too.
    system("lipc-set-prop com.lab126.powerd -i touchScreenSaverTimeout 1");

}

int main (int argc, char* argv[])
{
    std::vector<keycommand> the_keys;

    printf("KindleLazy v0.9 - cearp\n\n");

    std::ifstream ifs("config.json");
    Json::Reader reader;
    Json::Value obj;
    if(!reader.parse(ifs, obj)){
        printf("Problem with config.json - check it exists and is valid!\n");
        printf("Exiting\n");
        return -1;
    }

    //argc is the int that stores how many arguments. always 1 or more? 1 because the filename is the first argument (argv[0])
    if (argc >= 2)
    {
        if (std::string(argv[1]) == "-reverse")
        { 
            reverse = true;
            printf("Page direction is reversed\n");
        }
    }

    const Json::Value& json_next_page = obj["key_next_page"];
    for (int i = 0; i < json_next_page.size(); i++){
        keycommand temp;
        temp.id = json_next_page[i].asInt();
        temp.action = key_next_page;
        the_keys.push_back(temp);
    }
    const Json::Value& json_prev_page = obj["key_prev_page"];
    for (int i = 0; i < json_prev_page.size(); i++){
        keycommand temp;
        temp.id = json_prev_page[i].asInt();
        temp.action = key_prev_page;
        the_keys.push_back(temp);
    }
    const Json::Value& json_brightness_up = obj["key_brightness_up"];
    for (int i = 0; i < json_brightness_up.size(); i++){
        keycommand temp;
        temp.id = json_brightness_up[i].asInt();
        temp.action = key_brightness_up;
        the_keys.push_back(temp);
    }
    const Json::Value& json_brightness_down = obj["key_brightness_down"];
    for (int i = 0; i < json_brightness_down.size(); i++){;
        keycommand temp;
        temp.id = json_brightness_down[i].asInt();
        temp.action = key_brightness_down;
        the_keys.push_back(temp);
    }
    const Json::Value& json_wake = obj["key_wake"];
    for (int i = 0; i < json_wake.size(); i++){;
        keycommand temp;
        temp.id = json_wake[i].asInt();
        temp.action = key_wake;
        the_keys.push_back(temp);
    }

    ifs.close();


    int eventfd2;
    struct pollfd mypoll2;
    int eventfd3;
    struct pollfd mypoll3;

    int btn = 0;


    eventfdTOUCH = open("/dev/input/event1", O_WRONLY);
    if (eventfdTOUCH < 0) //failed
    {   
        perror("ERROR opening evdev1 (touch screen) device\n");
    }
    else{
        printf("opened evdev1 touch screen\n");
    }

    eventfd2 = open("/dev/input/event2", O_RDONLY);
    if (eventfd2 < 0) //failed
    {	
        perror("ERROR opening evdev2 device\n");
    }
    else{
        printf("opened event2\n");
    }

    eventfd3 = open("/dev/input/event3", O_RDONLY);
    if (eventfd3 < 0) //failed
    {   
        perror("ERROR opening evdev3 device\n");
    }
    else{
        printf("opened event3\n");
    }


    dpy = XOpenDisplay(NULL);
    if ( dpy == NULL) {
        perror("ERROR Opening XDisplay\n");
        return -1;
    }

    //get screen res so we know where to touch
    //if in book view, in portrait/landscape, X will report the correct respective resolution.
    //Although 9% of the time KindleLazy is loaded in portrait (via KUAL) - if run from ssh whilst in landscape book view, the res will be landscape and be a problem.
    //when we write a touch to /dev/input/event1 (touch screen), X's rotation/book rotation does not matter, coordinates are ALWAYS in 'portrait'
    //so, just incase KindleLazy starts up in landscape, we make sure that height < width, if not, swap them.
    Screen*  scrn = DefaultScreenOfDisplay(dpy);
    if(scrn->height > scrn->width){
        height = scrn->height;
        width = scrn->width;
    }
    else
    {
        width = scrn->height;
        height = scrn->width;
    }

    halfheight = height/2;
    halfwidth = width/2;

    mypoll2.fd = eventfd2;
    mypoll2.events = POLLIN;
    mypoll3.fd = eventfd3;
    mypoll3.events = POLLIN;

    while(1) {
        if(poll(&mypoll2, 1, 10) > 0) {
            btn = getbtn(mypoll2.fd);
            if (btn <= 0){
                continue;
            }

            //i'm not sure how inefficient it is to cycle through all configured keys to see if the current input matches
            //but we are dealing with a small amount of data so it shouldn't matter here :)
            for (const keycommand& i : the_keys)
            {
                if (btn == i.id)
                {
                    process_key(i.action);
                }
            }

        }
        if(poll(&mypoll3, 1, 10) > 0) {
            btn = getbtn(mypoll3.fd);
            if (btn <= 0){
                continue;
            }

            for (const keycommand& i : the_keys)
            {
                if (btn == i.id)
                {
                    process_key(i.action);
                }
            }

        }

    }
 

    XCloseDisplay (dpy);

    close(eventfd2);
    close(eventfd3);

    return 0;
}