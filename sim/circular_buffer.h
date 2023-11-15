// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef CIRC_BUF_H
#define CIRC_BUF_H

/*
 * A resizable circular buffer intended to replace a List as a queue
 * structure where we don't care about reclaiming space afterwards
 */

#include <vector>

template<typename T>
class CircularBuffer {
public:
    CircularBuffer()
    {
        _count = 0;
        _next_push = 0;
        _next_pop = 0;
        _size = 8; // initial size; we'll resize if needed
        _queue.resize(_size);
    }
    CircularBuffer(int starting_size) 
    {

        _count = 0;
        _next_push = 0;
        _next_pop = 0;
        _size = starting_size; // initial size; we'll resize if needed
        _queue.resize(_size);
    }

    void push(T& item)
    {
        //validate();
        _count++;
        if (_count == _size) {
            size_t newsize = _size*2;
            _queue.resize(newsize);
            if (_next_push < _next_pop) {
                //   456789*123
                // NI *, NP 1
                for (int i=0; i < _next_push; i++) {
                    // move 4-9 into new space
                    _queue[_size+i] = _queue[i];
                }
                _next_push += _size;
            } else {
                // 123456789*
                // nothing to do
            }
            _size = newsize;
        }
        _queue[_next_push] = item;
        _next_push = (_next_push +1) % _size;
        //validate();
    }

    T& pop() {
        //validate();
        assert(_count > 0);
        int old_index = _next_pop;
        _next_pop = (_next_pop +1) % _size;
        _count--;
        //validate();
        return _queue[old_index];
    }

    T& pop_front() {
        //validate();
        assert(_count > 0);
        int old_index = (_next_push+_size-1)%_size;
        _next_push = old_index;
        _count--;
        //validate();
        return _queue[old_index];
    }

    T& back() {  // badly named - prefer next_to_pop()
        assert(_count > 0);
        return _queue.at(_next_pop);
    }

    T& next_to_pop() {
        assert(_count > 0);
        return _queue.at(_next_pop);
    }

    bool empty() {return _count == 0;}
    int size() {return _count;}
private:
    void validate() {
        assert(_count < _size);
        assert(_next_push < _size);
        assert(_next_pop < _size);
        if (_next_push > _next_pop) {
            assert(_next_push - _next_pop == _count);
        } else if (_next_push == _next_pop) {
            assert(_count == 0);
        } else {
            assert(_next_push + _size - _next_pop == _count);
        }
    }
    std::vector<T> _queue;
    int _next_push, _next_pop, _count, _size;
};



#endif
