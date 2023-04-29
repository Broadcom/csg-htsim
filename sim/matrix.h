// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-     

// 3d matrix template, based loosely on 2d matrix by
// https://stackoverflow.com/users/145587/oneofone
#ifndef MATRIX2_3D
#define MATRIX2_3D

//helper classes for destructor
template <typename T>
void deletep(T &) {}

template <typename T>
void deletep(T* & ptr) {
    delete ptr;
    ptr = 0;
}

//2d matrix class template
template<typename T>
class Matrix2d {
 public:
    typedef T value_type;
    Matrix2d(bool auto_del = true);
    Matrix2d(size_t x, size_t y, bool auto_del = true);

    void set_size(size_t x, size_t y);
    T & operator()(size_t x, size_t y);
    T & at(size_t x, size_t y);
    T operator()(size_t x, size_t y) const;
    virtual ~Matrix2d();

    int size() const { return _xsize * _ysize; }
    int x() const { return _xsize; }
    int y() const { return _ysize; }
 private:
    Matrix2d(const Matrix2d &);
    size_t _xsize, _ysize;
    mutable T * _data;
    const bool _auto_delete;
};

template<typename T>
Matrix2d<T>::Matrix2d(size_t x, size_t y, bool auto_del) 
: _xsize(x), _ysize(y), _auto_delete(auto_del) {
    _data = new T[x * y];
}

template<typename T>
Matrix2d<T>::Matrix2d(bool auto_del) 
: _xsize(0), _ysize(0), _data(new T[0]), _auto_delete(auto_del) {
}

template<typename T>
void Matrix2d<T>::set_size(size_t x, size_t y) {
    _xsize = x;
    _ysize = y;
    delete [] _data;
    _data = new T[x * y];
}


template<typename T>
inline T & Matrix2d<T>::operator()(size_t x, size_t y) {
    // do bounds checking
    assert (x < _xsize && y < _ysize);
    return _data[(x * _ysize) + y];
}

template<typename T>
inline T & Matrix2d<T>::at(size_t x, size_t y) {
    // do bounds checking
    assert (x < _xsize && y < _ysize);
    return _data[(x * _ysize) + y];
}

template<typename T>
inline T Matrix2d<T>::operator()(size_t x, size_t y) const {
    // do bounds checking
    assert (x < _xsize && y < _ysize);
    return _data[(x * _ysize) + y];
}

template<typename T>
Matrix2d<T>::~Matrix2d() {
    if(_auto_delete){
        for(int i = 0, c = size(); i < c; ++i){
            //will do nothing if T isn't a pointer
            deletep(_data[i]);
        }
    }
    delete [] _data;
}

//3d matrix class template
template<typename T>
class Matrix3d {
 public:
    typedef T value_type;
    Matrix3d(bool auto_del = true);
    Matrix3d(size_t x, size_t y, size_t z, bool auto_del = true);

    void set_size(size_t x, size_t y, size_t z);
    T & operator()(size_t x, size_t y, size_t z);
    T & at(size_t x, size_t y, size_t z);
    T operator()(size_t x, size_t y, size_t z) const;
    virtual ~Matrix3d();

    int size() const { return _xsize * _ysize * _zsize; }
    int x() const { return _xsize; }
    int y() const { return _ysize; }
    int z() const { return _zsize; }
 private:
    Matrix3d(const Matrix3d &);
    size_t _xsize, _ysize, _zsize;
    mutable T * _data;
    const bool _auto_delete;
};

template<typename T>
Matrix3d<T>::Matrix3d(size_t x, size_t y, size_t z, bool auto_del) 
: _xsize(x), _ysize(y), _zsize(z), _auto_delete(auto_del) {
    _data = new T[x * y * z];
}

template<typename T>
Matrix3d<T>::Matrix3d(bool auto_del) 
: _xsize(0), _ysize(0), _zsize(0), _data(new T[0]), _auto_delete(auto_del) {
}

template<typename T>
void Matrix3d<T>::set_size(size_t x, size_t y, size_t z) {
    _xsize = x;
    _ysize = y;
    _zsize = z;
    delete [] _data;
    _data = new T[x * y * z];
}


template<typename T>
inline T & Matrix3d<T>::operator()(size_t x, size_t y, size_t z) {
    // do bounds checking
    assert (x < _xsize && y < _ysize && z < _zsize);
    return _data[((x * _ysize) + y) * _zsize + z];
}

template<typename T>
inline T & Matrix3d<T>::at(size_t x, size_t y, size_t z) {
    // do bounds checking
    assert (x < _xsize && y < _ysize && z < _zsize);
    return _data[((x * _ysize) + y) * _zsize + z];
}

template<typename T>
inline T Matrix3d<T>::operator()(size_t x, size_t y, size_t z) const {
    // do bounds checking
    assert (x < _xsize && y < _ysize && z < _zsize);
    return _data[((x * _ysize) + y) * _zsize + z];
}

template<typename T>
Matrix3d<T>::~Matrix3d() {
    if(_auto_delete){
        for(int i = 0, c = size(); i < c; ++i){
            //will do nothing if T isn't a pointer
            deletep(_data[i]);
        }
    }
    delete [] _data;
}

#endif
