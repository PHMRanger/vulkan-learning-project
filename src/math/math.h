#include <linmath.h>

void swapf(float *a, float *b)
{
    float c;
    c = *a, *a = *b, *b = c;
}

void intersect_point(const vec3 l0, const vec3 l, float t, vec3 p)
{
    vec3_scale(p, l, t);
    vec3_add(p, l0, p);
}

bool intersect_plane(const vec3 l0, const vec3 l, const vec3 p0, const vec3 n, float *t) 
{ 
    // assuming vectors are all normalized
    float denom = vec3_mul_dot(n, l); 
    if (fabs(denom) > 1e-6) { 
        vec3 p0l0;
        vec3_sub(p0l0, p0, l0);
        *t = vec3_mul_dot(p0l0, n) / denom; 
        return (*t >= 1e-6); 
    } 
 
    return false; 
}

//TODO: Make it faster.
bool intersect_cube(const vec3 l0, const vec3 l, vec3 min, vec3 max, float *tmin, float *tmax)
{
    intersect_plane(l0, l, min, vec3{1, 0, 0}, tmin);
    intersect_plane(l0, l, max, vec3{-1, 0, 0}, tmax);

    if (*tmin > *tmax) {
        swapf(tmin, tmax);
    }

    float tymin,
          tymax;

    intersect_plane(l0, l, min, vec3{0, 1, 0}, &tymin);
    intersect_plane(l0, l, max, vec3{0, -1, 0}, &tymax);

    if (tymin > tymax) {
        swapf(&tymin, &tymax);
    }

    if (*tmin > tymax || tymin > *tmax) {
        return false;
    }

    if (tymin > *tmin) {
        *tmin = tymin;
    }

    if (tymax < *tmax) {
        *tmax = tymax;
    }

    float tzmin,
          tzmax;

    intersect_plane(l0, l, min, vec3{0, 0, 1}, &tzmin);
    intersect_plane(l0, l, max, vec3{0, 0, -1}, &tzmax);

    if (tzmin > tzmax) {
        swapf(&tzmin, &tzmax);
    }

    if (*tmin > tzmax || tzmin > *tmax) {
        return false;
    }

    if (tzmin > *tmin) {
        *tmin = tzmin;
    }

    if (tzmax < *tmax) {
        *tmax = tzmax;
    }
    
    return true;
}
