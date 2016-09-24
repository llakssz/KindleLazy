/*
KindleLazy 0.5
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
    key_brightness_down
};

struct keycommand {
  int id;
  action_type action;
};

bool reverse = false;
Display *dpy = NULL;
int height = 0;
int width  = 0;
int halfheight = 0;

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
	if (read(fd, &t, sizeof(t)) == sizeof(t))	{
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

void process_key (action_type action)
{
	std::string new_brightness = "";
	int current_brightness = 0;

	switch(action)
	{
	case key_next_page:
		if(reverse){
			mouse_click(dpy, 0, halfheight);
		}
		else{
			mouse_click(dpy, width, halfheight);
		}
	    break;

	case key_prev_page:
		if(reverse){
			mouse_click(dpy, width, halfheight);
		}
		else{
			mouse_click(dpy, 0, halfheight);
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
			//lets us go to 0 and turn off light, amazon doesn't let us
			current_brightness--;
		}
		new_brightness = std::to_string(current_brightness);
		new_brightness = "lipc-set-prop com.lab126.powerd flIntensity " + new_brightness;
		system(new_brightness.c_str());
	    break;
	}

	//keep awake. at least faking touch with xdotool, we needed this.
	//otherwise the kindle sleeps due to no touch - we probably need it here too.
	system("lipc-set-prop com.lab126.powerd -i touchScreenSaverTimeout 1");

}

int main (int argc, char* argv[])
{
	std::vector<keycommand> the_keys;

	std::ifstream ifs("config.json");
    Json::Reader reader;
    Json::Value obj;
    if(!reader.parse(ifs, obj)){
    	printf("Problem with config.json, exiting\n");
    	return -1;
    }
    
	//get whether to reverse page turn direction or not, default to false
	reverse = obj.get("reverse_direction", 0).asBool();

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

	ifs.close();

 	if (argc >= 2) //argc is the int that stores how many arguments. always 1 or more? 1 because the filename is the first argument (argv[0])
 	{
 		if (std::string(argv[1]) == "flip") //if the flag i choose for swapping page direction is true
 		{ 
 			reverse = !reverse;
			std::ofstream ofs("config.json");	
 			obj["reverse_direction"] = reverse;
			Json::StyledWriter styledWriter;
			ofs << styledWriter.write(obj);
			ofs.close();

 			//print message to screen that it was successful, print the current direction
			printf("KindleLazy v0.5 - cearp\n");
			printf("Flipped the direction - ");
			if(reverse){
				printf("now reversed\n");
			}
			else{
				printf("now normal\n");
			}

			return 0;
 		}

	}

	int eventfd;
	int eventfd2;
	struct pollfd mypoll;
	struct pollfd mypoll2;
	int btn = 0;

	printf("KindleLazy v0.5 - cearp\n");
	if(reverse){
		printf("Page direction is reversed\n\n");
	} else{
		printf("Page direction is normal\n\n");
	}

	eventfd = open("/dev/input/event2", O_RDONLY);
	if (eventfd < 0) //failed
	{	
		perror("ERROR Reading evdev device\n");
		return -1;
	}
	else{
		printf("opened event2\n");
	}
	
	eventfd2 = open("/dev/input/event3", O_RDONLY);
	if (eventfd2 < 0) //failed
	{	
		perror("ERROR Reading evdev2 device\n");
		return -1;
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
	//touch Y half way down, X at 0 for back or X at maxwidth for forward
	Screen*  scrn = DefaultScreenOfDisplay(dpy);
	height = scrn->height;
	width  = scrn->width;
	halfheight = height/2;

	mypoll.fd =  eventfd;
	mypoll.events = POLLIN;
	
	mypoll2.fd =  eventfd2;
	mypoll2.events = POLLIN;


	while(1) {
		if(poll(&mypoll, 1, 10) > 0) {
			btn = getbtn(eventfd);
			if (btn <= 0){
				continue;
			}

			//i'm not sure how inefficient it is to cycle through all configured keys to see if the current input matches
			//but we are dealing with small data so it shouldn't matter here :)
			for (const keycommand& i : the_keys)
			{
       			if (btn == i.id)
       			{
       				process_key(i.action);
       			}
       		}

		}
		if(poll(&mypoll2, 1, 10) > 0) {
			btn = getbtn(eventfd2);
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

	close(eventfd);
	close(eventfd2);

	return 0;
}