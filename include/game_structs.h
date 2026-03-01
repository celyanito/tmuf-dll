#pragma once

struct CFastString {
    int size;
    char* psz;
};

struct CMwId {
    int value;
};

template<typename T>
struct CFastBuffer {
    int size;
    T* pElems;
    int capacity;
};