#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <libwebsockets.h>
#include <jansson.h>

#define EPSILON 0.00001

static struct lws *wsi_xmas = NULL;
json_error_t error;
json_t *root;

static int callback_xmas(struct lws *wsi, enum lws_callback_reasons reason,
	void *user, void *in, size_t len) {
	fprintf(stderr, "callback %d\n", reason);
	switch (reason) {
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			wsi_xmas = NULL;
			lwsl_err("error msg length %ld\n", len);
			lwsl_err("CLIENT_CONNECTION_ERROR: %s: %s %p\n" ,"xmas", &in);
			break;


		case LWS_CALLBACK_CLIENT_RECEIVE:
			((char *)in)[len] = '\0';
			lwsl_info("rx %d '%s'\n", (int)len, (char *)in);
			
			root = json_loads(in, 0, &error);		
	
			break;

		default:
			break;
	}
	return 0;
}

static struct lws_protocols protocols[] = {
	{
		"xmas-protocol",
		callback_xmas,
		0,
		4096
	},
	{ NULL, NULL, 0, 0 } 
};

struct pos {
	double x;
	double y;
};

struct gps {
	double distance;
	double x;
	double y;
};

void getCurrentGpsPos(struct gps retval[3]) {
	json_t *gpss;
	if (root != NULL) {
		gpss = json_object_get(root, "gpss");
		for (int i = 0; i < json_array_size(gpss); i++) {
			json_t *gps, *coords;
			gps = json_array_get(gpss, i);
			retval[i].distance = json_real_value(json_object_get(gps, "distance"));
			coords = json_object_get(gps, "position");
			retval[i].x = json_integer_value(json_object_get(coords, "x"));
			retval[i].y = json_integer_value(json_object_get(coords, "y"));
// lwsl_info("gps: d %f, x %f, y %f\n", retval[i].distance, retval[i].x, retval[i].y);
		}		

	}
}

bool calculateThreeCircleIntersection(struct pos *retval, double x0, double y0, double r0,
                                                           double x1, double y1, double r1,
                                                           double x2, double y2, double r2) {
	double dx, dy, d, d1, d2, a, point2_x, point2_y, h, rx, ry;
	double intersectionPoint1_x, intersectionPoint1_y, intersectionPoint2_x, intersectionPoint2_y;

	/* dx and dy are the vertical and horizontal distances between
	* the circle centers.
	*/
        dx = x1 - x0;
        dy = y1 - y0;

    	/* Determine the straight-line distance between the centers. */
        d = sqrt((dy*dy) + (dx*dx));

	/* Check for solvability. */
        if (d > (r0 + r1))
        {
	    lwsl_info("false 1: %f, %f", r0, r1);
            return false;
        }
        if (d < fabs(r0 - r1))
        {
	    lwsl_info("false 2: %f < %f - %f  dx=%f dy=%f\n", d, r0, r1, x1, y1);
            return false;
        }

	/* 'point 2' is the point where the line through the circle
    	* intersection points crosses the line between the circle
    	* centers.
    	*/

    	/* Determine the distance from point 0 to point 2. */
        a = ((r0*r0) - (r1*r1) + (d*d)) / (2.0 * d);

        /* Determine the coordinates of point 2. */
        point2_x = x0 + (dx * a/d);
        point2_y = y0 + (dy * a/d);

	/* Determine the distance from point 2 to either of the
	* intersection points. 
	*/
        h = sqrt((r0*r0) - (a*a));

	/* Now determine the offsets of the intersection points from
	* point 2.
	*/
        rx = -dy * (h/d);
        ry = dx * (h/d);

	/* Determine the absolute intersection points. */
        intersectionPoint1_x = point2_x + rx;
        intersectionPoint2_x = point2_x - rx;
        intersectionPoint1_y = point2_y + ry;
        intersectionPoint2_y = point2_y - ry;

	/* Lets determine if circle 3 intersects at either of the above intersection points. */
        dx = intersectionPoint1_x - x2;
        dy = intersectionPoint1_y - y2;
        d1 = sqrt((dy*dy) + (dx*dx));

        dx = intersectionPoint2_x - x2;
        dy = intersectionPoint2_y - y2;
        d2 = sqrt((dy*dy) + (dx*dx));

        if(fabs(d1 - r2) < EPSILON) {
		retval->x = intersectionPoint1_x;
		retval->y = intersectionPoint1_y;
		return true;
        }
        else if(fabs(d2 - r2) < EPSILON) {
		retval->x = intersectionPoint2_x;
		retval->y = intersectionPoint2_y;
		return true;
        }
        else {
            return false;
        }
    }

json_t *getPlayer(const char *name) {
        json_t *players;
        players  = json_object_get(root, "players");
        for (int i = 0; i < json_array_size(players); i++) {
                json_t *player;
                player = json_array_get(players, i);
                if (!strcmp(json_string_value(json_object_get(player, "name") ), name)) return player;
        }
	return NULL;
}

void checkName(const char *name) { 
	char nameMsg[255];
	if (getPlayer(name) != NULL) return;

	lwsl_info("need to set name\n");	
	sprintf(nameMsg, "{\"tag\":\"SetName\", \"contents\":\"%s\"}", name);
	lws_write(wsi_xmas, nameMsg, strlen(nameMsg), LWS_WRITE_TEXT);
}

void moveMeTo(const char *name, struct pos dest) {
	json_t *me, *position;
	double x, y;
	char moveMsg[255];
	int dx, dy;

	me = getPlayer(name);
	position = json_object_get(me, "position");
	x = json_real_value(json_object_get(position, "x"));
	y = json_real_value(json_object_get(position, "y"));

	dx = round(dest.x - x);
	dy = round(dest.y - y);

	sprintf(moveMsg, "{\"tag\":\"Move\",\"contents\":{\"x\":%d,\"y\":%d}}", dx, dy);
	lwsl_info("%s\n", moveMsg);

	lws_write(wsi_xmas, moveMsg, strlen(moveMsg), LWS_WRITE_TEXT);
}

int main(int argc, const char **argv) {

	struct lws_context_creation_info info;
	struct lws_client_connect_info i;
	struct lws_context *context;
	bool force_exit = false;
	const char *prot, *p, *add;
	char uri[] = "ws://localhost:8000/"; 
	struct gps satellites[3];
	const char name[] = "Snake";

	memset(&info, 0, sizeof(info));
	memset(&i, 0, sizeof(i));

	info.port = CONTEXT_PORT_NO_LISTEN;
	info.protocols = protocols;
	info.ws_ping_pong_interval = 10;
	info.gid = -1;
	info.uid = -1;
	
	lws_set_log_level(LLL_DEBUG | LLL_INFO | LLL_NOTICE | LLL_ERR, NULL);
	
	context = lws_create_context(&info);
	if (context == NULL) {
		lwsl_info("creating context fail\n");
		return 1;
	}

	if (lws_parse_uri(uri, &prot, &i.address, &i.port, &p)) {
		lwsl_info("error parsing url");
		return 1;
	}

	fprintf(stderr, "connecting to %s:%d\n", i.address, i.port);

	i.context = context;	
	i.path = "/";
	i.ssl_connection = 0;
	i.origin = i.address;
	i.host = i.address;
	i.protocol = "xmas-protocol";
	i.ietf_version_or_minus_one = -1;
	
	i.pwsi = &wsi_xmas;
	wsi_xmas = lws_client_connect_via_info(&i);


	while (!force_exit) {
		lws_service(context, 500);
		if (root != NULL) {
			struct pos intersect;
			checkName(name);
			getCurrentGpsPos(&satellites);
			if (calculateThreeCircleIntersection(&intersect, 
				satellites[0].x, satellites[0].y, satellites[0].distance,
				satellites[1].x, satellites[1].y, satellites[1].distance,
				satellites[2].x, satellites[2].y, satellites[2].distance)) {
				lwsl_info("intersect (%f, %f)\n", intersect.x, intersect.y);
				moveMeTo(name, intersect);
			}
		}
	}

	lws_context_destroy(context);
	return 0;
}


