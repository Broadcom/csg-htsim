// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-                                                                

#ifndef _DRAWABLE_H
#define _DRAWABLE_H

/*
 * A base class for anything in a topology that is drawable
 */


class Drawable {
public:
    Drawable() : _x(0), _y(0) {}
    void setPos(int x, int y) {
        _x = x;
        _y = y;
    }
private:
    int _x;
    int _y;
};

#endif
