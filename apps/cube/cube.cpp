/**
 * @file cube.cpp
 * @author Joe Bayer (joexbayer)
 * @brief A simple cube demo
 * @version 0.1
 * @date 2024-01-10
 *
 * @copyright Copyright (c) 2024
 *
 */

#include <utils/Graphics.hpp>
#include <libc.h>
#include <colors.h>

extern float sin_60[60];
extern float cos_60[60];

#define BACKGROUND_COLOR COLOR_VGA_LIGHTEST_GRAY
#define EDGE_COLOR COLOR_VGA_DARKEST_GRAY
#define FRAME_DELAY_MS 50

/* The libc tables are offset by 90 degrees: cos_60[i] is sin(6i degrees), and sin_60[i] is -cos(6i degrees) */
static float sinStep(int step) { return cos_60[step % 60]; }
static float cosStep(int step) { return -sin_60[step % 60]; }

struct Point3D {
    float x, y, z;
};

struct Point2D {
    int x, y;
};

class CubeDemo : public Window {
public:
    CubeDemo(int width, int height) : Window(width, height, "Cube", 1) {
        this->width = width;
        this->height = height;
    }

    void drawCube() {
        if (needsClear) {
            /* First frame, or the window was resized and still holds its old content */
            gfx_draw_rectangle(0, 0, width, height, BACKGROUND_COLOR);
            needsClear = false;
        } else {
            /* Erase the last frame's edges, which is much cheaper than clearing the whole window */
            drawEdges(previous, BACKGROUND_COLOR);
        }

        Point2D current[8];
        for (int i = 0; i < 8; i++) {
            current[i] = project(rotate(vertices[i]));
        }
        drawEdges(current, EDGE_COLOR);

        for (int i = 0; i < 8; i++) {
            previous[i] = current[i];
        }
    }

    /* Advance the rotation by one 6 degree step */
    void rotateCube() {
        step = (step + 1) % 60;
    }

    void setSize(int width, int height) {
        this->width = width;
        this->height = height;
        needsClear = true;
    }

private:
    int step = 0;
    int width, height;
    bool needsClear = true;
    Point2D previous[8];

    const Point3D vertices[8] = {
        {-1, -1, -1},
        {1, -1, -1},
        {1, 1, -1},
        {-1, 1, -1},
        {-1, -1, 1},
        {1, -1, 1},
        {1, 1, 1},
        {-1, 1, 1},
    };

    const int edges[12][2] = {
        /* Back face */
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        /* Front face */
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
         /* Connecting edges */
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    /* Rotate an original vertex around the y-axis, then the x-axis, by the current step */
    Point3D rotate(Point3D point) {
        float s = sinStep(step);
        float c = cosStep(step);

        float x = point.x * c + point.z * s;
        float z = point.z * c - point.x * s;

        float y = point.y * c - z * s;
        z = point.y * s + z * c;

        return {x, y, z};
    }

    /* Project 3D cube to 2D, centered, and small enough that a corner (sqrt(3) from the center) stays inside */
    Point2D project(Point3D point) {
        int scale = (width < height ? width : height) / 4;
        return {(int)(point.x * scale) + width / 2, (int)(point.y * scale) + height / 2};
    }

    void drawEdges(const Point2D* points, unsigned char color) {
        for (int i = 0; i < 12; i++) {
            const Point2D& a = points[edges[i][0]];
            const Point2D& b = points[edges[i][1]];
            gfx_draw_line(a.x, a.y, b.x, b.y, color);
        }
    }

};



int main()
{
    CubeDemo demo(200, 200);
    struct gfx_event e;
    demo.setHeader("3D");

    while (1){
        /* Handle every pending event each frame. Of several resizes, only the latest size matters. */
        while (gfx_get_event(&e, GFX_EVENT_NONBLOCKING) == 0){
            switch (e.event)
            {
            case GFX_EVENT_RESOLUTION:
                demo.setSize(e.data, e.data2);
                break;
            case GFX_EVENT_EXIT:
                return 0;
            default:
                break;
            }
        }

        demo.rotateCube();
        demo.drawCube();
        sleep(FRAME_DELAY_MS);
    }

    return 0;
}
